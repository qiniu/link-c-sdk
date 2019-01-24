#include "tsuploader.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <pthread.h>
#include "servertime.h"
#include <time.h>
#include <unistd.h>
#include "b64/urlsafe_b64.h"
#include "libghttp/qupload.h"
#include "httptools.h"
#include "picuploader.h"


size_t getDataCallback(void* buffer, size_t size, size_t n, void* rptr);

#define TS_DIVIDE_LEN 4096

enum LinkTsuCmdType {
        LINK_TSU_UPLOAD = 1,
        LINK_TSU_QUIT = 2,
        LINK_TSU_START_TIME = 3,
        LINK_TSU_END_TIME = 4,
        LINK_TSU_SEG_TIME = 5,
        LINK_TSU_SET_META = 6,
        LINK_TSU_CLR_META = 7,
        LINK_TSU_SET_PLAN_TYPE = 8,
        LINK_TSU_PICTURE = 9
};

typedef struct _TsUploaderMeta {
        LinkKeyFrameMetaInfo metaInfo[10];
        int nMetaInfoLen;
}TsUploaderMeta;

typedef struct _KodoUploader{
        LinkTsUploader uploader;
        
        LinkCircleQueue * pQueue_;
        TsUploaderMeta* pUpMeta;
        LinkCircleQueue * pCommandQueue_;
        CircleQueuePolicy policy;
        int nMaxItemLen;
        int nInitItemCount;
        
        pthread_mutex_t uploadMutex_;
        int nTsCacheNum;
        int nTsMaxCacheNum;

        pthread_t workerId_;
        int isThreadStarted_;
        
        LinkTsUploadArg uploadArg;
        LinkUploadState state;

        LinkEndUploadCallback pTsEndUploadCallback;
        void *pTsEndUploadCallbackArg;
        int nQuit_;
        LinkPlanType planType;
        LinkSession session;
        
        // for restoreDuration
        int64_t nAudioDuration;
        int64_t nVideoDuration;
        
        // for discontinuity
        int64_t nLastSystime;
        int64_t nFirstSystime;
        int64_t nLastSystimeBak;
        
        int64_t nLastTsEndTime;
        
        LinkTsOutput output;
        void *pOutputUserArg;
        LinkMediaArg mediaArg;
        LinkSessionMeta *pSessionMeta;
        LinkPicture picture;
}KodoUploader;

typedef struct _TsUploaderCommandTs {
        void *pData;
        KodoUploader *pKodoUploader;
        TsUploaderMeta* pUpMeta;
}TsUploaderCommandTs;

typedef struct _TsUploaderCommand {
        enum LinkTsuCmdType nCommandType;
        union{
                TsUploaderCommandTs ts;
                LinkReportTimeInfo time;
                LinkSessionMeta *pSessionMeta;
                LinkPlanType planType;
                LinkPicture pic;
        };
}TsUploaderCommand;

static void handleSessionCheck(KodoUploader * pKodoUploader, int64_t nSysTimestamp, int isForceNewSession, int64_t nCurTsDuration);
static void restoreDuration (KodoUploader * pKodoUploader);

// ts pts 33bit, max value 8589934592, 10 numbers, bcd need 5byte to store
static void inttoBCD(int64_t m, char *buf)
{
        int n = 10, a = 0;
        memset(buf, 0, 5);
        while(m) {
                a = (m%10) << ((n%2) * 4);
                m=m/10;
                buf[(n+1)/2 - 1] |= a;
                n--;
        }
        return;
}

static int linkPutBuffer(const char * uphost, const char *token, const char * key, const char *data, int datasize,
                         LinkKeyFrameMetaInfo *pMetas, int nMetaLen,
                         const char **customMagic, int nCustomMagicLen,
                         int64_t duration, int64_t seqnum, int isDiscontinuity) {
       
        char metaValue[200];
        char metaBuf[250];
        const char *realKey = key;
        int i = 0;
        for(i = 0; i < nMetaLen; i++) {
                inttoBCD(pMetas[i].nTimestamp90Khz, metaBuf + 15 * i);
                inttoBCD(pMetas[i].nOffset, metaBuf + 15 * i + 5);
                inttoBCD(pMetas[i].nLength, metaBuf + 15 * i + 10);
        }
        int nMetaValueLen = urlsafe_b64_encode(metaBuf, nMetaLen * 15, metaValue, sizeof(metaValue));
        
        char **pp = (char **)metaBuf;
        char *pCnt = metaBuf + sizeof(void *) * 8;
        
        pp[0] = pCnt; *pCnt++ = 'o'; *pCnt++ = 0;
        pp[1] = pCnt; memcpy(pCnt, metaValue, nMetaValueLen); pCnt += nMetaValueLen; *pCnt++ = 0;
        pp[2] = pCnt; *pCnt++ = 'd'; *pCnt++ = 0;
        pp[3] = pCnt; pCnt += sprintf(pCnt, "%"PRId64"", duration); *pCnt++ = 0;
        pp[4] = pCnt; *pCnt++ = 's'; *pCnt++ = 0;
        pp[5] = pCnt; pCnt += sprintf(pCnt, "%"PRId64"", seqnum); *pCnt++ = 0;
        pp[6] = pCnt; *pCnt++ = 'c'; *pCnt++ = 0;
        pp[7] = pCnt; pCnt += sprintf(pCnt, "%d", isDiscontinuity); *pCnt++ = 0;
        
        LinkPutret putret;
        if (nCustomMagicLen > 0)
                realKey = NULL;
        int ret = LinkUploadBuffer(data, datasize, uphost, token, realKey, (const char **)pp, 8,
                                   customMagic, nCustomMagicLen, /*mimetype*/NULL, &putret);

        

        int retCode = -1;
        if (ret != 0) { //http error
                LinkLogError("upload.file :%s[%d] errorcode=%d errmsg=%s", key, datasize, ret, putret.error);
                return LINK_GHTTP_FAIL;
        } else {
                if (putret.code / 100 == 2) {
                        retCode = LINK_SUCCESS;
                        LinkLogDebug("upload.file size:exp:%d key:%s success",datasize, key);
                } else {
                        if (putret.body != NULL) {
                                LinkLogError("upload.file :%s[%d] reqid:%s httpcode=%d errmsg=%s",
                                             key, datasize, putret.reqid, putret.code, putret.body);
                        } else {
                                LinkLogError("upload.file :%s[%d] reqid:%s httpcode=%d errmsg={not receive response}",
                                             key, datasize, putret.reqid, putret.code);
                        }
                }
        }

        LinkFreePutret(&putret);
        
        return retCode;
}

static void resetSessionCurrentTsScope(LinkSession *pSession) {
        pSession->nTsStartTime = 0;
}

static void resetSessionReportScope(LinkSession *pSession) {
        pSession->nAudioGapFromLastReport = 0;
        pSession->nVideoGapFromLastReport = 0;
}
static void resizeQueueSize(KodoUploader * pKodoUploader, int nCurLen, int64_t nCurTsDuration) {
        
        if (nCurTsDuration < 4500) {
                return;
        }
        
        if (nCurLen > 1152 * 1024) {
                pKodoUploader->nInitItemCount = 1536 * 1024 / pKodoUploader->nMaxItemLen;
        } else if (nCurLen > 896 * 1024) {
                pKodoUploader->nInitItemCount = 1152 * 1024 / pKodoUploader->nMaxItemLen;
        } else if (nCurLen > 640 * 1024) {
                pKodoUploader->nInitItemCount = 896 * 1024 / pKodoUploader->nMaxItemLen;
        }
        
        if (nCurLen < 640 * 1024) {
                pKodoUploader->nInitItemCount = 640 * 1024 / pKodoUploader->nMaxItemLen;
        } else if (nCurLen < 896 * 1024) {
                pKodoUploader->nInitItemCount = 896 * 1024 / pKodoUploader->nMaxItemLen;
        } else if (nCurLen < 1152 * 1024) {
                pKodoUploader->nInitItemCount = 1152 * 1024 / pKodoUploader->nMaxItemLen;
        }
        LinkLogInfo("resize queue buffer:%dK", (pKodoUploader->nInitItemCount * pKodoUploader->nMaxItemLen)/1024);
        return;
}

static void * bufferUpload(TsUploaderCommand *pUploadCmd) {
        
        char uptoken[1024] = {0};
        char upHost[192] = {0};
        int ret = 0, getUploadParamOk = 0;
        int getBufDataRet, lenOfBufData = 0;
        char *bufData = NULL;
        LinkCircleQueue *pDataQueue = (LinkCircleQueue *)pUploadCmd->ts.pData;
        KodoUploader *pKodoUploader = (KodoUploader*)pUploadCmd->ts.pKodoUploader;
        TsUploaderMeta* pUpMeta = pUploadCmd->ts.pUpMeta;
        LinkSession *pSession = &pUploadCmd->ts.pKodoUploader->session;
        
        LinkUploadParam param;
        memset(&param, 0, sizeof(param));
        param.pTokenBuf = uptoken;
        param.nTokenBufLen = sizeof(uptoken);
        param.pUpHost = upHost;
        param.nUpHostLen = sizeof(upHost);
        
        char fprefix[LINK_MAX_DEVICE_NAME_LEN * 2 + 32]={0};
        param.pFilePrefix = fprefix;
        param.nFilePrefix = sizeof(fprefix);

        int nSLen = snprintf(param.sessionId, sizeof(param.sessionId), "%s", pKodoUploader->session.sessionId);
        assert(nSLen < sizeof(param.sessionId));
        
        char key[128+LINK_MAX_DEVICE_NAME_LEN+LINK_MAX_APP_LEN] = {0};
        
        LinkUploadResult uploadResult = LINK_UPLOAD_RESULT_FAIL;

        ret = pKodoUploader->uploadArg.uploadParamCallback(
                                        pKodoUploader->uploadArg.pGetUploadParamCallbackArg,
                                        &param, LINK_UPLOAD_CB_GETTSPARAM);
        if (ret != LINK_SUCCESS) {
                if (ret == LINK_BUFFER_IS_SMALL) {
                        LinkLogError("param buffer is too small. drop file");
                } else {
                        LinkLogError("not get param yet:%d", ret);
                }
        } else {
                getUploadParamOk = 1;
        }
        
        int64_t tsStartTime = pSession->nTsStartTime;
        int64_t tsDuration = pSession->nTsDuration;
        
        int64_t tsEndTime = tsStartTime / 1000000 + tsDuration;
        if (pKodoUploader->nLastTsEndTime > 0) {
                if (tsEndTime <= pKodoUploader->nLastTsEndTime) {
                        LinkLogWarn("ts timestamp not monotonical:%"PRId64" %"PRId64"",tsEndTime, pKodoUploader->nLastTsEndTime);
                }
        }
        pKodoUploader->nLastTsEndTime = tsEndTime;
        
        if (getUploadParamOk)
                handleSessionCheck(pKodoUploader, pKodoUploader->session.nTsStartTime + tsDuration * 1000000LL, 0, tsDuration);
        
        int isDiscontinuity = 0;
        if (pKodoUploader->nLastSystimeBak > 0 && getUploadParamOk) {
                if (pKodoUploader->nFirstSystime - pKodoUploader->nLastSystimeBak > 200000000) {
                        LinkLogDebug("discontinuity:%"PRId64"-%"PRId64"=%"PRId64"\n", pKodoUploader->nFirstSystime, pKodoUploader->nLastSystimeBak, pKodoUploader->nFirstSystime - pKodoUploader->nLastSystimeBak);
                        isDiscontinuity = 1;
                }
        }
        pKodoUploader->nLastSystimeBak = pKodoUploader->nLastSystime;
        pKodoUploader->nFirstSystime = 0;

        memset(key, 0, sizeof(key));
        
        getBufDataRet = LinkGetQueueBuffer(pDataQueue, &bufData, &lenOfBufData);

        if ((pKodoUploader->pSessionMeta || pKodoUploader->planType == LINK_PLAN_TYPE_24) && (getBufDataRet > 0 && getUploadParamOk)) {
                resizeQueueSize(pKodoUploader, lenOfBufData, tsDuration);
                
                sprintf(key, "%s/ts/%"PRId64"-%"PRId64"-%s.ts", param.pFilePrefix,
                        tsStartTime / 1000000, tsStartTime / 1000000 + tsDuration, pSession->sessionId);
                
                LinkLogDebug("upload start:%s q:%p  len:%d", key, pDataQueue, lenOfBufData);
                char startTs[14]={0};
                char endTs[14]={0};
                snprintf(startTs, sizeof(startTs), "%"PRId64"", tsStartTime / 1000000);
                snprintf(endTs, sizeof(endTs), "%"PRId64"", tsStartTime / 1000000+tsDuration);
                const char *cusMagics[6];
                int nCusMagics = 6;
                cusMagics[0]="x:start";
                cusMagics[1]=startTs;
                cusMagics[2]="x:end";
                cusMagics[3]=endTs;
                cusMagics[4]="x:session";
                cusMagics[5] = pSession->sessionId;
                if(param.nFilePrefix > 0)
                        nCusMagics = 0;
                int putRet = linkPutBuffer(upHost, uptoken, key, bufData, lenOfBufData, pUpMeta->metaInfo, pUpMeta->nMetaInfoLen,
                                           cusMagics, nCusMagics,
                                           tsDuration,pKodoUploader->session.nTsSequenceNumber++, isDiscontinuity);
                if (putRet == LINK_SUCCESS) {
                        uploadResult = LINK_UPLOAD_RESULT_OK;
                        pKodoUploader->state = LINK_UPLOAD_OK;
                } else {
                        pKodoUploader->state = LINK_UPLOAD_FAIL;
                }
                if (pKodoUploader->uploadArg.pUploadStatisticCb) {
                        pKodoUploader->uploadArg.pUploadStatisticCb(pKodoUploader->uploadArg.pUploadStatArg,
                                                                                LINK_UPLOAD_TS, uploadResult);
                }
        } else {
                LinkLogDebug("not upload:getbuffer:%d, meta:%p param:%d", getBufDataRet, pKodoUploader->pSessionMeta, getUploadParamOk);
        }

        if (pKodoUploader->uploadArg.pUploadStatisticCb) {
                pKodoUploader->uploadArg.pUploadStatisticCb(pKodoUploader->uploadArg.pUploadStatArg,
                                                                        LINK_UPLOAD_TS, uploadResult);
        }
        if (uploadResult == LINK_UPLOAD_RESULT_OK) {
                if (pKodoUploader->picture.pFilename) {
                        snprintf((char *)pKodoUploader->picture.pFilename + pKodoUploader->picture.nFilenameLen-4, LINK_MAX_SESSION_ID_LEN+5,
                                 "-%s.jpg", pKodoUploader->session.sessionId);
                        LinkSendPictureToPictureUploader(pKodoUploader->picture.pOpaque, pKodoUploader->picture);
                        pKodoUploader->picture.pFilename = NULL;
                }
                //handleSessionCheck(pKodoUploader, LinkGetCurrentNanosecond(), 0, 0);
                pSession->nLastTsEndTime = tsEndTime;
        } else {
                restoreDuration(pKodoUploader);
        }
        resetSessionReportScope(pSession);
        resetSessionCurrentTsScope(pSession);
        if (pKodoUploader->pTsEndUploadCallback) {
                pKodoUploader->pTsEndUploadCallback(pKodoUploader->pTsEndUploadCallbackArg, tsStartTime / 1000000);
        }
        
        if (pKodoUploader->output && lenOfBufData > 0 && bufData) {
                LinkMediaInfo mediaInfo;
                memset(&mediaInfo, 0, sizeof(mediaInfo));
                mediaInfo.startTime = tsStartTime / 1000000;
                mediaInfo.endTime = tsStartTime / 1000000 + tsDuration;
                mediaInfo.pSessionMeta = (const LinkSessionMeta *)pKodoUploader->pSessionMeta;
                memcpy(mediaInfo.sessionId, pSession->sessionId, sizeof(mediaInfo.sessionId) - 1);
                int idx = 0;
                if (pKodoUploader->mediaArg.nAudioFormat != LINK_AUDIO_NONE) {
                        mediaInfo.media[idx].nChannels = pKodoUploader->mediaArg.nChannels;
                        mediaInfo.media[idx].nAudioFormat = pKodoUploader->mediaArg.nAudioFormat;
                        mediaInfo.media[idx].nSamplerate = pKodoUploader->mediaArg.nSamplerate;
                        mediaInfo.mediaType[idx] = LINK_MEDIA_AUDIO;
                        mediaInfo.nCount++;
                        idx++;
                }
                if (pKodoUploader->mediaArg.nVideoFormat != LINK_VIDEO_NONE) {
                        mediaInfo.media[idx].nVideoFormat = pKodoUploader->mediaArg.nVideoFormat;
                        mediaInfo.mediaType[idx] = LINK_MEDIA_VIDEO;
                        mediaInfo.nCount++;
                }
                
                pKodoUploader->output(bufData, lenOfBufData, pKodoUploader->pOutputUserArg, mediaInfo);
        }

        LinkDestroyQueue(&pDataQueue);
        free(pUpMeta);
        if (pKodoUploader->pSessionMeta) {
                if (pKodoUploader->pSessionMeta->isOneShot) {
                        free(pKodoUploader->pSessionMeta);
                        pKodoUploader->pSessionMeta = NULL;
                }
        }
        
        pthread_mutex_lock(&pKodoUploader->uploadMutex_);
        pKodoUploader->nTsCacheNum--;
        pthread_mutex_unlock(&pKodoUploader->uploadMutex_);
        return NULL;
}

static int allocDataQueueAndUploadMeta(KodoUploader * pKodoUploader) {
        pKodoUploader->pUpMeta = (TsUploaderMeta*)malloc(sizeof(TsUploaderMeta));
        if (pKodoUploader->pUpMeta == NULL) {
                LinkLogError("malloc upload meta error");
                return LINK_NO_MEMORY;
        }
        memset(pKodoUploader->pUpMeta, 0, sizeof(TsUploaderMeta));
        
        if (pKodoUploader->pUpMeta != NULL) {
                pKodoUploader->pQueue_ = NULL;
                int ret = LinkNewCircleQueue(&pKodoUploader->pQueue_, 1, pKodoUploader->policy,
                                             pKodoUploader->nMaxItemLen, pKodoUploader->nInitItemCount);
                if (ret != 0) {
                        free(pKodoUploader->pUpMeta);
                        pKodoUploader->pUpMeta = NULL;
                        LinkLogError("LinkNewCircleQueue error:%d", ret);
                        return ret;
                }
        }
        return LINK_SUCCESS;
}

static int streamPushData(LinkTsUploader *pTsUploader, const char * pData, int nDataLen)
{
        KodoUploader * pKodoUploader = (KodoUploader *)pTsUploader;
        pthread_mutex_lock(&pKodoUploader->uploadMutex_);
        if (pKodoUploader->pQueue_ == NULL) {
                int ret = allocDataQueueAndUploadMeta(pKodoUploader);
                if (ret != LINK_SUCCESS) {
                        pthread_mutex_unlock(&pKodoUploader->uploadMutex_);
                        return ret;
                }
        }
        
        int ret = pKodoUploader->pQueue_->Push(pKodoUploader->pQueue_, (char *)pData, nDataLen);
        pthread_mutex_unlock(&pKodoUploader->uploadMutex_);
        return ret;
}

static void notifyDataPrapared(LinkTsUploader *pTsUploader) {
        KodoUploader * pKodoUploader = (KodoUploader *)pTsUploader;
        
        pthread_mutex_lock(&pKodoUploader->uploadMutex_);
        TsUploaderCommand uploadCommand;
        uploadCommand.nCommandType = LINK_TSU_UPLOAD;
        uploadCommand.ts.pData = pKodoUploader->pQueue_;
        uploadCommand.ts.pKodoUploader = pKodoUploader;
        uploadCommand.ts.pUpMeta = pKodoUploader->pUpMeta;
       
        pKodoUploader->pQueue_ = NULL;
        pKodoUploader->pUpMeta = NULL;
        
        int nCurCacheNum = 0;
        
        nCurCacheNum = pKodoUploader->nTsCacheNum;
        
        if (nCurCacheNum >= pKodoUploader->nTsMaxCacheNum) {
                free(uploadCommand.ts.pUpMeta);
                LinkDestroyQueue((LinkCircleQueue **)(&uploadCommand.ts.pData));
                LinkLogError("drop ts file due to ts cache reatch max limit");
        } else {
                LinkLogTrace("-------->push ts to  queue\n", nCurCacheNum);
                int ret = pKodoUploader->pCommandQueue_->Push(pKodoUploader->pCommandQueue_, (char *)&uploadCommand, sizeof(TsUploaderCommand));
                if (ret > 0)
                        pKodoUploader->nTsCacheNum++;
                else {
                        LinkLogError("ts queue error. push ts to upload:%d", ret);
                        LinkDestroyQueue((LinkCircleQueue **)(&uploadCommand.ts.pData));
                }
        }
        allocDataQueueAndUploadMeta(pKodoUploader);
        pthread_mutex_unlock(&pKodoUploader->uploadMutex_);
        
        return;
}

static void getStatInfo(LinkTsUploader *pTsUploader, LinkUploaderStatInfo *_pStatInfo)
{
        KodoUploader * pKodoUploader = (KodoUploader *)pTsUploader;

        pKodoUploader->pQueue_->GetStatInfo(pKodoUploader->pQueue_, _pStatInfo);

        return;
}

void reportTimeInfo(LinkTsUploader *_pTsUploader, LinkReportTimeInfo *pTinfo,
                    enum LinkUploaderTimeInfoType tmtype) {
        
        KodoUploader * pKodoUploader = (KodoUploader *)_pTsUploader;
        
        TsUploaderCommand tmcmd;
        int isNotifyDataPrepared = 0;
        if (tmtype == LINK_TS_START) {
                tmcmd.nCommandType = LINK_TSU_START_TIME;
        }
        else if (tmtype == LINK_TS_END) {
                tmcmd.nCommandType = LINK_TSU_END_TIME;
                isNotifyDataPrepared = 1;
        }
        else if (tmtype == LINK_SEG_TIMESTAMP) {
                tmcmd.nCommandType = LINK_TSU_SEG_TIME;
        }
        tmcmd.time = *pTinfo;
        
        int ret;
        ret = pKodoUploader->pCommandQueue_->Push(pKodoUploader->pCommandQueue_, (char *)&tmcmd, sizeof(TsUploaderCommand));
        if (ret <= 0) {
                LinkLogError("ts queue error. push report time:%d", ret);
        }
        if (isNotifyDataPrepared) {
                notifyDataPrapared(_pTsUploader);
        }
        
        return;
}

LinkUploadState getUploaderState(LinkTsUploader *_pTsUploader)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pTsUploader;
        return pKodoUploader->state;
}

void LinkUpdateSessionId(LinkSession *pSession, int64_t nTsStartSystime) {
        char str[20] = {0};
        int strLen = sprintf(str, "%"PRId64"", nTsStartSystime/1000000);
        int nId = urlsafe_b64_encode(str, strLen, pSession->sessionId, sizeof(pSession->sessionId));
        while (pSession->sessionId[nId - 1] == '=') {
                nId--;
                pSession->sessionId[nId] = 0;
        }
        
        pSession->nSessionStartTime =  nTsStartSystime;
        pSession->nSessionEndResonCode = 0;
        pSession->nTsSequenceNumber = 0;
        pSession->isNewSessionStarted = 1;
        
        pSession->nAudioGapFromLastReport = 0;
        pSession->nVideoGapFromLastReport = 0;
        
        pSession->nAccSessionDuration = 0;
        pSession->nAccSessionAudioDuration = 0;
        pSession->nAccSessionVideoDuration = 0;
}

static void handleSessionCheck(KodoUploader * pKodoUploader, int64_t nSysTimestamp, int isForceNewSession, int64_t nCurTsDuration) {

        if (pKodoUploader->uploadArg.UploadUpdateSegmentId) {
                if (isForceNewSession)
                        pKodoUploader->uploadArg.UploadUpdateSegmentId(pKodoUploader->uploadArg.pUploadArgKeeper_,
                                                                       &pKodoUploader->session, nSysTimestamp, 0, nCurTsDuration);
                else
                        pKodoUploader->uploadArg.UploadUpdateSegmentId(pKodoUploader->uploadArg.pUploadArgKeeper_,
                                                                       &pKodoUploader->session,
                                                                       pKodoUploader->session.nTsStartTime, nSysTimestamp, nCurTsDuration);
        }
}

static void handleTsStartTimeReport(KodoUploader * pKodoUploader, LinkReportTimeInfo *pTi) {
        
        if (pKodoUploader->nFirstSystime <= 0)
                pKodoUploader->nFirstSystime = pTi->nSystimestamp;
        if (pKodoUploader->session.nSessionStartTime <= 0)
                pKodoUploader->session.nSessionStartTime =  pTi->nSystimestamp;
        if (pKodoUploader->session.nTsStartTime <= 0)
                pKodoUploader->session.nTsStartTime = pTi->nSystimestamp;
        
        pKodoUploader->nLastSystime = pTi->nSystimestamp;
        pKodoUploader->nFirstSystime = pTi->nSystimestamp;
        
        handleSessionCheck(pKodoUploader, pTi->nSystimestamp, 0, 0);
}

static void restoreDuration (KodoUploader * pKodoUploader) {
        
        pKodoUploader->session.nAccSessionVideoDuration -= pKodoUploader->nVideoDuration;
        pKodoUploader->session.nAccSessionAudioDuration -= pKodoUploader->nAudioDuration;
        pKodoUploader->session.nVideoGapFromLastReport -= pKodoUploader->nVideoDuration;
        pKodoUploader->session.nAudioGapFromLastReport -= pKodoUploader->nAudioDuration;
        pKodoUploader->session.nAccSessionDuration -= pKodoUploader->session.nTsDuration;
        pKodoUploader->session.nTsDuration = 0;
        pKodoUploader->nVideoDuration = 0;
        pKodoUploader->nAudioDuration = 0;
}

static void handleTsEndTimeReport(KodoUploader * pKodoUploader, LinkReportTimeInfo *pTi) {
        
        if (pKodoUploader->nFirstSystime <= 0)
                pKodoUploader->nFirstSystime = pTi->nSystimestamp;
        
        pKodoUploader->session.nAccSessionVideoDuration += pTi->nVideoDuration;
        pKodoUploader->session.nAccSessionAudioDuration += pTi->nAudioDuration;
        pKodoUploader->session.nVideoGapFromLastReport += pTi->nVideoDuration;
        pKodoUploader->session.nAudioGapFromLastReport += pTi->nAudioDuration;
        pKodoUploader->session.nAccSessionDuration += pTi->nMediaDuation;
        pKodoUploader->session.nTsDuration = pTi->nMediaDuation;
        
        pKodoUploader->nAudioDuration = pTi->nAudioDuration;
        pKodoUploader->nVideoDuration = pTi->nVideoDuration;
        
        
        pKodoUploader->nLastSystime = pTi->nSystimestamp;
        
        //handleSessionCheck(pKodoUploader, pTi->nSystimestamp, 0, 0);
}

static void handleSegTimeReport(KodoUploader * pKodoUploader, LinkReportTimeInfo *pTi) {
        
        handleSessionCheck(pKodoUploader, pTi->nSystimestamp, 1, 0);
}

static void * listenTsUpload(void *_pOpaque)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pOpaque;
        handleSessionCheck(pKodoUploader, LinkGetCurrentNanosecond(), 1, 0);
        LinkUploaderStatInfo info = {0};
        while(!pKodoUploader->nQuit_ || info.nLen_ != 0) {
                TsUploaderCommand cmd;
                int ret = pKodoUploader->pCommandQueue_->PopWithTimeout(pKodoUploader->pCommandQueue_, (char *)(&cmd),
                                                                      sizeof(TsUploaderCommand), 24 * 60 * 60 * 1000000LL);
                pKodoUploader->pCommandQueue_->GetStatInfo(pKodoUploader->pCommandQueue_, &info);
                LinkLogDebug("ts queue:%d cmd:%d", info.nLen_, cmd.nCommandType);
                if (ret <= 0) {
                        if (ret != LINK_TIMEOUT) {
                                LinkLogError("tscmd queue error. pop:%d", ret);
                        }
                        continue;
                }
                
                switch (cmd.nCommandType) {
                        case LINK_TSU_UPLOAD:
                                bufferUpload(&cmd);
                                break;
                        case LINK_TSU_PICTURE:
                                if (pKodoUploader->picture.pFilename) {
                                        free((void*)pKodoUploader->picture.pFilename);
                                        pKodoUploader->picture.pFilename = NULL;
                                }
                                pKodoUploader->picture = cmd.pic;
                                break;
                        case LINK_TSU_QUIT:
                                LinkLogInfo("tsuploader required to quit");
                                handleSegTimeReport(pKodoUploader, &cmd.time);
                                break;
                        case LINK_TSU_START_TIME:
                                handleTsStartTimeReport(pKodoUploader, &cmd.time);
                                break;
                        case LINK_TSU_END_TIME:
                                handleTsEndTimeReport(pKodoUploader, &cmd.time);
                                break;
                        case LINK_TSU_SEG_TIME:
                                handleSegTimeReport(pKodoUploader, &cmd.time);
                                break;
                        case LINK_TSU_SET_META:
                                if (pKodoUploader->pSessionMeta) {
                                        free(pKodoUploader->pSessionMeta);
                                        pKodoUploader->pSessionMeta = NULL;
                                }
                                pKodoUploader->pSessionMeta = cmd.pSessionMeta;
                                break;
                        case LINK_TSU_CLR_META:
                                if (pKodoUploader->pSessionMeta) {
                                        pKodoUploader->pSessionMeta->isOneShot = 1;
                                }
                                break;
                        case LINK_TSU_SET_PLAN_TYPE:
                                pKodoUploader->planType = cmd.planType;
                        default:
                                break;
                }
                
        }
        return NULL;
}

int LinkNewTsUploader(LinkTsUploader ** _pUploader, const LinkTsUploadArg *_pArg, CircleQueuePolicy _policy, int _nMaxItemLen, int _nInitItemCount)
{
        KodoUploader * pKodoUploader = (KodoUploader *) malloc(sizeof(KodoUploader));
        if (pKodoUploader == NULL) {
                return LINK_NO_MEMORY;
        }
        
        memset(pKodoUploader, 0, sizeof(KodoUploader));
        
        pKodoUploader->uploadArg = *_pArg;
        
        pKodoUploader->uploader.Push = streamPushData;
        
        pKodoUploader->uploader.GetStatInfo = getStatInfo;
        pKodoUploader->uploader.ReportTimeInfo = reportTimeInfo;
        pKodoUploader->uploader.GetUploaderState = getUploaderState;
        
        pKodoUploader->policy = _policy;
        pKodoUploader->nMaxItemLen = _nMaxItemLen;
        pKodoUploader->nInitItemCount = _nInitItemCount;

        int ret = allocDataQueueAndUploadMeta(pKodoUploader);
        if (ret != LINK_SUCCESS) {
                free(pKodoUploader);
                return ret;
        }
        
        ret = LinkNewCircleQueue(&pKodoUploader->pCommandQueue_, 1, TSQ_FIX_LENGTH, sizeof(TsUploaderCommand), 64);
        if (ret != 0) {
                LinkDestroyQueue(&pKodoUploader->pQueue_);
                free(pKodoUploader);
                free(pKodoUploader->pUpMeta);
                return ret;
        }
        ret = pthread_mutex_init(&pKodoUploader->uploadMutex_, NULL);
        if (ret != 0){
                LinkDestroyQueue(&pKodoUploader->pQueue_);
                LinkDestroyQueue(&pKodoUploader->pCommandQueue_);
                free(pKodoUploader->pUpMeta);
                free(pKodoUploader);
                return LINK_MUTEX_ERROR;
        }
        pKodoUploader->nTsMaxCacheNum = 2;
        ret = pthread_create(&pKodoUploader->workerId_, NULL, listenTsUpload, pKodoUploader);
        if (ret != 0) {
                LinkDestroyQueue(&pKodoUploader->pQueue_);
                LinkDestroyQueue(&pKodoUploader->pCommandQueue_);
                free(pKodoUploader->pUpMeta);
                free(pKodoUploader);
                return LINK_THREAD_ERROR;
        }
        pKodoUploader->isThreadStarted_ = 1;
        
        *_pUploader = (LinkTsUploader*)pKodoUploader;
        
        return LINK_SUCCESS;
}

void LinkTsUploaderSetTsEndUploadCallback(LinkTsUploader * _pUploader, LinkEndUploadCallback cb, void *pOpaque) {
        KodoUploader * pKodoUploader = (KodoUploader *)(_pUploader);
        pKodoUploader->pTsEndUploadCallback = cb;
        pKodoUploader->pTsEndUploadCallbackArg = pOpaque;
}

void LinkTsUploaderSetTsCallback(IN LinkTsUploader * _pUploader, IN LinkTsOutput output, IN void * pUserArg, IN LinkMediaArg mediaArg) {
        KodoUploader * pKodoUploader = (KodoUploader *)(_pUploader);
        pKodoUploader->output = output;
        pKodoUploader->pOutputUserArg = pUserArg;
        pKodoUploader->mediaArg = mediaArg;
        
        return;
}

void LinkDestroyTsUploader(LinkTsUploader ** _pUploader)
{
        KodoUploader * pKodoUploader = (KodoUploader *)(*_pUploader);
        
        TsUploaderCommand uploadCommand;
        uploadCommand.nCommandType = LINK_TSU_QUIT;
        uploadCommand.time.nSystimestamp = LinkGetCurrentNanosecond();
        int ret = 0;
        LinkLogInfo("tsuploader required to quit:%d", ret);
        pKodoUploader->nQuit_ = 1;
        while(ret <= 0) {
                ret = pKodoUploader->pCommandQueue_->Push(pKodoUploader->pCommandQueue_, (char *)&uploadCommand, sizeof(TsUploaderCommand));
                if (ret <= 0) {
                        LinkLogError("ts queue error. notify quit:%d sleep 1 sec to retry", ret);
                        sleep(1);
                }
        }
        
        if (pKodoUploader->isThreadStarted_) {
                pthread_join(pKodoUploader->workerId_, NULL);
        }
        
        LinkDestroyQueue(&pKodoUploader->pCommandQueue_);
        if (pKodoUploader->pQueue_) {
                LinkDestroyQueue(&pKodoUploader->pQueue_);
        }
        if (pKodoUploader->pUpMeta) {
                free(pKodoUploader->pUpMeta);
        }
        pthread_mutex_destroy(&pKodoUploader->uploadMutex_);

        free(pKodoUploader);
        * _pUploader = NULL;
        return;
}

void LinkAppendKeyframeMetaInfo(void *pOpaque, LinkKeyFrameMetaInfo *pMediaInfo) {
        if (pOpaque == NULL || pMediaInfo == NULL) {
                return;
        }
        KodoUploader * pKodoUploader = (KodoUploader *)(pOpaque);
        if (pKodoUploader->pUpMeta != NULL) {
                if (pKodoUploader->pUpMeta->nMetaInfoLen < sizeof(pKodoUploader->pUpMeta->metaInfo) / sizeof(LinkKeyFrameMetaInfo)) {
                        int idx = pKodoUploader->pUpMeta->nMetaInfoLen++;
                        pKodoUploader->pUpMeta->metaInfo[idx] = *pMediaInfo;
                        pKodoUploader->pUpMeta->metaInfo[idx].nTimestamp90Khz &= 0x00000001FFFFFFFFLL;
                        //fprintf(stderr, "==========------->%lld %d %d %x\n", pMediaInfo->nTimestamp, pMediaInfo->nOffset, pMediaInfo->nLength, pOpaque);
                }
        }
}

void LinkSetSessionMeta(IN LinkTsUploader * _pUploader, LinkSessionMeta *pSessionMeta) {
        
        KodoUploader * pKodoUploader = (KodoUploader *)(_pUploader);
        TsUploaderCommand uploadCommand;
        uploadCommand.nCommandType = LINK_TSU_SET_META;
        uploadCommand.pSessionMeta = pSessionMeta;
        
        int ret = pKodoUploader->pCommandQueue_->Push(pKodoUploader->pCommandQueue_, (char *)&uploadCommand, sizeof(TsUploaderCommand));
        if (ret <= 0) {
                LinkLogError("ts queue error. set meta", ret);
                return;
        }
        return;

}

void LinkClearSessionMeta(IN LinkTsUploader * _pUploader) {
        KodoUploader * pKodoUploader = (KodoUploader *)(_pUploader);
        
        TsUploaderCommand uploadCommand;
        uploadCommand.nCommandType = LINK_TSU_CLR_META;
        
        int ret = pKodoUploader->pCommandQueue_->Push(pKodoUploader->pCommandQueue_, (char *)&uploadCommand, sizeof(TsUploaderCommand));
        if (ret <= 0) {
                LinkLogError("ts queue error. clr meta", ret);
                return;
        }
        return;
}

int LinkTsUploaderPushPic(IN LinkTsUploader * _pUploader, LinkPicture pic) {
        KodoUploader * pKodoUploader = (KodoUploader *)(_pUploader);
        
        TsUploaderCommand uploadCommand;
        uploadCommand.nCommandType = LINK_TSU_PICTURE;
        
        char *pFileName = (char *)malloc(pic.nFilenameLen + 5+LINK_MAX_SESSION_ID_LEN + pic.nBuflen);
        if (pFileName == NULL) {
                return LINK_NO_MEMORY;
        }
        memset(pFileName, 0, pic.nFilenameLen + 5+LINK_MAX_SESSION_ID_LEN);
        memcpy(pFileName, pic.pFilename, pic.nFilenameLen);

        char *pData = pFileName + pic.nFilenameLen + 5+LINK_MAX_SESSION_ID_LEN;
        memcpy(pData, pic.pBuf, pic.nBuflen);
        
        pic.pFilename = (const char *)pFileName;
        pic.pBuf = (const char *)pData;
        uploadCommand.pic = pic;
        
        int ret = pKodoUploader->pCommandQueue_->Push(pKodoUploader->pCommandQueue_, (char *)&uploadCommand, sizeof(TsUploaderCommand));
        if (ret <= 0) {
                LinkLogError("ts queue error. push pic meta", ret);
                return ret;
        }
        return LINK_SUCCESS;
}

void LinkTsUploaderSetPlanType(IN LinkTsUploader * _pUploader, LinkPlanType planType) {
        KodoUploader * pKodoUploader = (KodoUploader *)(_pUploader);
        
        TsUploaderCommand uploadCommand;
        uploadCommand.nCommandType = LINK_TSU_SET_PLAN_TYPE;
        uploadCommand.planType = planType;
        
        int ret = pKodoUploader->pCommandQueue_->Push(pKodoUploader->pCommandQueue_, (char *)&uploadCommand, sizeof(TsUploaderCommand));
        if (ret <= 0) {
                LinkLogError("ts queue error. clr meta", ret);
                return;
        }
        return;
}
