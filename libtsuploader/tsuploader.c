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
#include "segmentmgr.h"

static int64_t lastReportAccDuration  = 0; // for test
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
        unsigned char isThreadStarted_;
        unsigned char isForceSeg;
        
        LinkTsUploadArg uploadArg;
        LinkUploadState state;

        LinkEndUploadCallback pTsEndUploadCallback;
        void *pTsEndUploadCallbackArg;
        int nQuit_;
        LinkPlanType planType;
        LinkSession session;
        LinkSession bakSession;
        int reportType;
        int8_t isSegStartReport; // 是否上报了片段开始
        int8_t isFirstSessionPicReported; // 第一个seg的封面检查
        int8_t nInternalForceSegFlag; // 防止内存强制切片(meta),在短时间内重复多次，必须在有片段上传后才能再次强制分片
        // for restoreDuration
        int64_t nAudioDuration;
        int64_t nVideoDuration;
        
        // for discontinuity
        int64_t nLastSystime;
        int64_t nFirstSystime;
        int64_t nLastSystimeBak;
        
        int64_t nLastTsEndTime;
        int64_t nLastTsDuration;
        
        LinkTsOutput output;
        void *pOutputUserArg;
        LinkMediaArg mediaArg;
        LinkSessionMeta *pSessionMeta;
        LinkPicture picture;
        
        int64_t nTsLastStartTime;
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

static int handleSessionCheck(KodoUploader * pKodoUploader, int64_t nSysTimestamp, int isForceNewSession, int64_t nCurTsDuration, int shouldReport);
static void restoreDuration (KodoUploader * pKodoUploader);
static void handleSegTimeReport(KodoUploader * pKodoUploader, int64_t nCurSysTime, int fromInteral);

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
        
        LinkPutret putret = {0};
        if (nCustomMagicLen > 0)
                realKey = NULL;
        int ret = LinkUploadBuffer(data, datasize, uphost, token, realKey, (const char **)pp, 8,
                                   customMagic, nCustomMagicLen, /*mimetype*/NULL, &putret);
        char resDesc[32] = {0};
        snprintf(resDesc, sizeof(resDesc), "upload.file[%"PRId64"]", LinkGetCurrentNanosecond()/1000000);
        int retCode = -1;
        if (ret != 0) { //http error
                LinkLogError("%s :%s[%d] retcode=%d rcode:%d eno:%d errmsg=%s",resDesc, key, datasize, ret, putret.code, errno, putret.error);
                return LINK_GHTTP_FAIL;
        } else {
                if (putret.code / 100 == 2) {
                        retCode = LINK_SUCCESS;
                        LinkLogDebug("%s size:exp:%d key:%s success",resDesc, datasize, key);
                } else {
                        if (putret.body != NULL) {
                                LinkLogError("%s :%s[%d] reqid:%s xreqid:%s rcope=%d errmsg=%s", resDesc,
                                             key, datasize, putret.reqid, putret.xreqid,putret.code, putret.body);
                        } else {
                                LinkLogError("%s :%s[%d] reqid:%s xreqid:%s rcope=%d errmsg={not receive response}",
                                             resDesc, key, datasize, putret.reqid, putret.xreqid, putret.code);
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

static void resetSessionScope(LinkSession *pSession) {
        resetSessionReportScope(pSession);
        pSession->nAccSessionAudioDuration = 0;
        pSession->nAccSessionVideoDuration = 0;
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

static void setSessionCoverStatus(LinkSession *pSession, int nPicUploadStatus) {
        if (nPicUploadStatus == LINK_SUCCESS) {
                pSession->coverStatus = 0;
        } else if (nPicUploadStatus == LINK_TIMEOUT) {
                pSession->coverStatus = 1;
        } else if (nPicUploadStatus < 0) {
                pSession->coverStatus = -2;
        } else if (nPicUploadStatus < 1000)
                pSession->coverStatus = nPicUploadStatus;
}

static void doTsOutput(KodoUploader * pKodoUploader, int lenOfBufData, char *bufData,
                       int64_t tsStartTime, int64_t tsEndTime, const LinkSessionMeta * pMeta,
                       char *sessionId) {
        
        if (pKodoUploader->output && lenOfBufData > 0 && bufData) {
                LinkMediaInfo mediaInfo;
                memset(&mediaInfo, 0, sizeof(mediaInfo));
                mediaInfo.startTime = tsStartTime / 1000000;
                mediaInfo.endTime = tsEndTime / 1000000;
                mediaInfo.pSessionMeta = pMeta;
                if (sessionId)
                        memcpy(mediaInfo.sessionId, sessionId, LINK_MAX_SESSION_ID_LEN);
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
        
        char key[256] = {0};
        
        LinkPicUploadParam upPicParam;
        upPicParam.getUploadParamCallback = pKodoUploader->uploadArg.uploadParamCallback;
        upPicParam.pParamOpaque = pKodoUploader->uploadArg.pGetUploadParamCallbackArg;
        upPicParam.nRetCode = 1000;
        upPicParam.pUploadStatisticCb = pKodoUploader->uploadArg.pUploadStatisticCb;
        upPicParam.pStatOpauqe = pKodoUploader->uploadArg.pUploadStatArg;
        
        LinkUploadResult uploadResult = LINK_UPLOAD_RESULT_FAIL;
        
        int64_t tsStartTime = pSession->nTsStartTime;
        
        //just for log
        int64_t tsDuration = pSession->nTsDuration;
        if (tsDuration > 30000 && tsDuration < 0) {
                LinkLogWarn("abnormal ts duration:%"PRId64"", tsDuration);
        }
        
        int reportType = pKodoUploader->reportType;
        int64_t tsEndTime = pKodoUploader->nLastSystime - 40 * 1000000;
        if (pKodoUploader->nLastTsEndTime > 0) {
                if (tsStartTime < pKodoUploader->nLastTsEndTime) {
                        LinkLogWarn("ts start timestamp abnormal: le:%"PRId64" cs:%"PRId64" -%"PRId64"-",pKodoUploader->nLastTsEndTime/1000000,
                                    tsStartTime/1000000,pKodoUploader->nLastTsEndTime/1000000 - tsStartTime/1000000);
                }
        }
        pKodoUploader->nLastTsEndTime = tsEndTime;
        pKodoUploader->nLastTsDuration = tsDuration;
        
        getBufDataRet = LinkGetQueueBuffer(pDataQueue, &bufData, &lenOfBufData);
        
        int shouldReport = 0, shouldUpload = 0;
        if (((pKodoUploader->pSessionMeta && pKodoUploader->planType == LINK_PLAN_TYPE_MOVE) || pKodoUploader->planType == LINK_PLAN_TYPE_24) &&
            getBufDataRet > 0) {
                shouldUpload = 1;
                shouldReport = 1;
        }
        
        int nSLen = snprintf(param.sessionId, sizeof(param.sessionId), "%s", pKodoUploader->session.sessionId);
        assert(nSLen < sizeof(param.sessionId));
        
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

        snprintf(key, sizeof(key), "ts/%"PRId64"-%"PRId64"-%s.ts", tsStartTime / 1000000, tsEndTime / 1000000, pSession->sessionId);
        
        LinkLogDebug("upload prepared:[%"PRId64"] %s q:%p  len:%d",LinkGetCurrentNanosecond()/1000000, key, pDataQueue, lenOfBufData);
        if (shouldUpload) {
                if (pKodoUploader->picture.pFilename) {
                        snprintf((char *)pKodoUploader->picture.pFilename + pKodoUploader->picture.nFilenameLen-4, LINK_MAX_SESSION_ID_LEN+5,
                                 "-%s.jpg", pKodoUploader->session.sessionId);
                        LinkUploadPicture(&pKodoUploader->picture, &upPicParam);
                        pKodoUploader->picture.pFilename = NULL;
                }
                
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
                
                resizeQueueSize(pKodoUploader, lenOfBufData, tsDuration);
                if (getUploadParamOk) {
                        char startTs[14]={0};
                        char endTs[14]={0};
                        snprintf(startTs, sizeof(startTs), "%"PRId64"", tsStartTime / 1000000);
                        snprintf(endTs, sizeof(endTs), "%"PRId64"", tsEndTime / 1000000);
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
                                                   tsDuration, pKodoUploader->session.nTsSequenceNumber, isDiscontinuity);
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
                }
        } else {
                LinkLogDebug("not upload:getbuffer:%d, meta:%p param:%d", getBufDataRet, pKodoUploader->pSessionMeta, getUploadParamOk);
        }

        if (pKodoUploader->uploadArg.pUploadStatisticCb) {
                pKodoUploader->uploadArg.pUploadStatisticCb(pKodoUploader->uploadArg.pUploadStatArg,
                                                                        LINK_UPLOAD_TS, uploadResult);
        }
        
        if (pKodoUploader->isSegStartReport && reportType & 0x4) {
                pSession->coverStatus = 1000;
                if (lastReportAccDuration != pKodoUploader->bakSession.nAccSessionVideoDuration)
                        LinkLogWarn("abnormal report duration:%"PRId64" %"PRId64"", lastReportAccDuration,  pKodoUploader->bakSession.nAccSessionVideoDuration);
                LinkLogDebug("=========================4>%s %"PRId64"", pKodoUploader->bakSession.sessionId, pKodoUploader->bakSession.nAccSessionVideoDuration);
                pKodoUploader->bakSession.isNewSessionStarted = 0;
                pKodoUploader->bakSession.nSessionEndResonCode = pSession->nSessionEndResonCode;
                assert(pKodoUploader->bakSession.nSessionEndResonCode != 0);
                if (pKodoUploader->isFirstSessionPicReported)
                        setSessionCoverStatus(&pKodoUploader->bakSession, upPicParam.nRetCode);
                LinkUpdateSegment(pSession->segHandle, &pKodoUploader->bakSession);
                if (pKodoUploader->isFirstSessionPicReported)
                        pKodoUploader->bakSession.coverStatus = 0;
                pKodoUploader->isSegStartReport = 0;
                memset(&pKodoUploader->bakSession, 0, sizeof(pKodoUploader->bakSession));
        }
        
        pSession->nLastTsEndTime = pKodoUploader->nLastTsEndTime;
        if (uploadResult == LINK_UPLOAD_RESULT_OK) {
                if (shouldReport){
                        if(reportType & 0x1) {
                                LinkLogDebug("=========================1>%s %"PRId64"", pSession->sessionId, pSession->nAccSessionVideoDuration);
                                pSession->isNewSessionStarted = 1;
                                setSessionCoverStatus(pSession, upPicParam.nRetCode);
                                LinkUpdateSegment(pSession->segHandle, pSession);
                                pSession->isNewSessionStarted = 0;
                                pKodoUploader->isSegStartReport = 1;
                        } else if(reportType & 0x2) {
                                LinkLogDebug("=========================2>%s %"PRId64" %"PRId64"", pSession->sessionId, pSession->nTsSequenceNumber,
                                             pSession->nAccSessionVideoDuration);
                                if (pKodoUploader->isFirstSessionPicReported)
                                        setSessionCoverStatus(pSession, upPicParam.nRetCode);
                                LinkUpdateSegment(pSession->segHandle, pSession);
                        }
                }
                //handleSessionCheck(pKodoUploader, LinkGetCurrentNanosecond(), 0, 0);
        } else {
                restoreDuration(pKodoUploader);
                if (shouldReport){
                        if(reportType & 0x1) {
                                LinkLogDebug("========================-1>%s %"PRId64"", pSession->sessionId, pSession->nAccSessionVideoDuration);
                                pSession->isNewSessionStarted = 1;
                                setSessionCoverStatus(pSession, upPicParam.nRetCode);
                                LinkUpdateSegment(pSession->segHandle, pSession);
                                pSession->isNewSessionStarted = 0;
                                pKodoUploader->isSegStartReport = 1;
                        }
                }
        }
        
        resetSessionReportScope(pSession);
        resetSessionCurrentTsScope(pSession);
        if (pKodoUploader->pTsEndUploadCallback) {
                pKodoUploader->pTsEndUploadCallback(pKodoUploader->pTsEndUploadCallbackArg, tsStartTime / 1000000);
        }
        
       doTsOutput(pKodoUploader, lenOfBufData, bufData, tsStartTime,  tsEndTime,
                  (const LinkSessionMeta *)pKodoUploader->pSessionMeta, pSession->sessionId);

        LinkDestroyQueue(&pDataQueue);
        free(pUpMeta);
        if (pKodoUploader->pSessionMeta) {
                if (pKodoUploader->pSessionMeta->isOneShot) {
                        free(pKodoUploader->pSessionMeta);
                        pKodoUploader->pSessionMeta = NULL;
                }
        }
        pKodoUploader->reportType = 0;
        pKodoUploader->isFirstSessionPicReported = 0;
        pKodoUploader->nInternalForceSegFlag = 0;
        pSession->nSessionEndResonCode = 0;
        pthread_mutex_lock(&pKodoUploader->uploadMutex_);
        pKodoUploader->nTsCacheNum--;
        pthread_mutex_unlock(&pKodoUploader->uploadMutex_);
        lastReportAccDuration = pSession->nAccSessionVideoDuration;
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

static void notifyDataPrapared(LinkTsUploader *pTsUploader, LinkReportTimeInfo *pTinfo) {
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
                int lenOfBufData = 0, getBufRet = 0;
                char *bufData = NULL;
                if ( (getBufRet = LinkGetQueueBuffer((LinkCircleQueue *)uploadCommand.ts.pData, &bufData, &lenOfBufData)) > 0) {
                        doTsOutput(pKodoUploader, lenOfBufData, bufData, pKodoUploader->nTsLastStartTime, pTinfo->nSystimestamp,
                                   (const LinkSessionMeta *)pKodoUploader->pSessionMeta, NULL); // 得不到准确的session id
                } else {
                        LinkLogError("drop ts file callback not get data:%d", getBufRet);
                }
                
                free(uploadCommand.ts.pUpMeta);
                LinkDestroyQueue((LinkCircleQueue **)(&uploadCommand.ts.pData));
                LinkLogError("ts queue cmd:drop ts file due to ts cache reatch max limit");
        } else {
                TsUploaderCommand tmcmd = {0};
                tmcmd.nCommandType = LINK_TSU_END_TIME;
                tmcmd.time = *pTinfo;
                int ret = 0;
                ret = pKodoUploader->pCommandQueue_->Push(pKodoUploader->pCommandQueue_, (char *)&tmcmd, sizeof(tmcmd));
                if (ret <= 0) {
                        LinkLogError("ts queue error. push report time:%d", ret);
                } else {
                        LinkLogTrace("-------->push ts to  queue\n", nCurCacheNum);
                        ret = pKodoUploader->pCommandQueue_->Push(pKodoUploader->pCommandQueue_, (char *)&uploadCommand, sizeof(TsUploaderCommand));
                        if (ret > 0)
                                pKodoUploader->nTsCacheNum++;
                        else
                                LinkLogError("ts queue error. push ts to upload:%d", ret);
                }
                if (ret <= 0) {
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
                pKodoUploader->nTsLastStartTime = pTinfo->nSystimestamp;
                tmcmd.nCommandType = LINK_TSU_START_TIME;
        }
        else if (tmtype == LINK_TS_END) {
                tmcmd.nCommandType = LINK_TSU_END_TIME;
                notifyDataPrapared(_pTsUploader, pTinfo);
                isNotifyDataPrepared = 1;
        }
        else if (tmtype == LINK_SEG_TIMESTAMP) {
                tmcmd.nCommandType = LINK_TSU_SEG_TIME;
        }
        tmcmd.time = *pTinfo;
        
        if (!isNotifyDataPrepared) {
                int ret;
                ret = pKodoUploader->pCommandQueue_->Push(pKodoUploader->pCommandQueue_, (char *)&tmcmd, sizeof(TsUploaderCommand));
                if (ret <= 0) {
                        LinkLogError("ts queue error. push report time:%d", ret);
                }
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
        //pSession->nSessionEndResonCode = 0;
        pSession->nTsSequenceNumber = 0;
        pSession->isNewSessionStarted = 1;
        
#if 0
        
        //pSession->nAccSessionDuration = 0;
        //pSession->nAccSessionAudioDuration = 0;
        //pSession->nAccSessionVideoDuration = 0;
#else
        pSession->nAccSessionDuration = pSession->nTsDuration;
        pSession->nAccSessionAudioDuration = pSession->nAudioGapFromLastReport;
        pSession->nAccSessionVideoDuration = pSession->nVideoGapFromLastReport;
#endif
        
        pSession->nSessionEndTime = 0;
}

static int handleSessionCheck(KodoUploader * pKodoUploader, int64_t nSysTimestamp, int isForceNewSession, int64_t nCurTsDuration, int shouldReport) {

        if (pKodoUploader->uploadArg.UploadUpdateSegmentId) {
                if (isForceNewSession)
                        return pKodoUploader->uploadArg.UploadUpdateSegmentId(pKodoUploader->uploadArg.pUploadArgKeeper_,
                                                                       &pKodoUploader->session, nSysTimestamp, 0, nCurTsDuration, shouldReport);
                else
                        return pKodoUploader->uploadArg.UploadUpdateSegmentId(pKodoUploader->uploadArg.pUploadArgKeeper_,
                                                                       &pKodoUploader->session,
                                                                       pKodoUploader->session.nTsStartTime, nSysTimestamp, nCurTsDuration, shouldReport);
        }
        return 0;
}

static void handleTsStartTimeReport(KodoUploader * pKodoUploader, LinkReportTimeInfo *pTi) {
        resetSessionReportScope(&pKodoUploader->session); // 可能会丢弃ts，所以这里也需要reset
        int isForceSeg = pKodoUploader->isForceSeg;
        if (pKodoUploader->isForceSeg) {
                pKodoUploader->isForceSeg = 0;
                LinkLogDebug("force to seg");
                handleSegTimeReport(pKodoUploader, pTi->nSystimestamp, 0); // 属于正常的分片逻辑(是强制分片设置的标记)，所以不算强制分片
        }
        if (pKodoUploader->nFirstSystime <= 0)
                pKodoUploader->nFirstSystime = pTi->nSystimestamp;
        if (pKodoUploader->session.nSessionStartTime <= 0)
                pKodoUploader->session.nSessionStartTime =  pTi->nSystimestamp;
        if (pKodoUploader->session.nTsStartTime > 0)
                LinkLogWarn("ts start should be zero:%"PRId64"", pKodoUploader->session.nTsStartTime/1000000);
        pKodoUploader->session.nTsStartTime = pTi->nSystimestamp;
        
        if (pKodoUploader->nLastSystime > 0) {
                if (pTi->nSystimestamp < pKodoUploader->nLastSystime) {
                        LinkLogWarn("setime abnormal:%"PRId64" %"PRId64" =%"PRId64"=", pTi->nSystimestamp/1000000,
                                    pKodoUploader->nLastSystime/1000000, pTi->nSystimestamp/1000000 - pKodoUploader->nLastSystime/1000000);
                }
        }
        pKodoUploader->nLastSystime = pTi->nSystimestamp;
        pKodoUploader->nFirstSystime = pTi->nSystimestamp;
        
        if (pKodoUploader->reportType == 0) {
                pKodoUploader->bakSession = pKodoUploader->session;
                pKodoUploader->session.nTsSequenceNumber++;
                pKodoUploader->reportType = handleSessionCheck(pKodoUploader, pTi->nSystimestamp, 0, pKodoUploader->nLastTsDuration, 0);
                if (pKodoUploader->reportType == 5)
                        pKodoUploader->session.nTsSequenceNumber = 1;
        } else { // 强制片段结束了
                pKodoUploader->session.nTsSequenceNumber++;
                if (isForceSeg == 0) {
                        int xx = handleSessionCheck(pKodoUploader, pTi->nSystimestamp, 0, pKodoUploader->nLastTsDuration, 0);
                        pKodoUploader->reportType |= xx;
                }
        }
        pKodoUploader->bakSession.nSessionEndTime = pKodoUploader->session.nLastTsEndTime;
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
        pKodoUploader->session.nTsDuration = pTi->nVideoDuration;
        if ((pTi->nVideoDuration <= 0 && pTi->nAudioDuration > 0) || pTi->nAudioDuration > pTi->nVideoDuration)
                pKodoUploader->session.nTsDuration = pTi->nAudioDuration;
        pKodoUploader->session.nAccSessionDuration += pKodoUploader->session.nTsDuration;//pTi->nMediaDuation;
        
        pKodoUploader->nAudioDuration = pTi->nAudioDuration;
        pKodoUploader->nVideoDuration = pTi->nVideoDuration;
        
        pKodoUploader->nLastSystime = pTi->nSystimestamp;
}

static void handleSegTimeReport(KodoUploader * pKodoUploader, int64_t nCurSysTime, int fromInternal) {
        if (fromInternal) {
                if(pKodoUploader->nInternalForceSegFlag > 0) {
                        return;
                }
        }
        pKodoUploader->bakSession = pKodoUploader->session;
        pKodoUploader->reportType = handleSessionCheck(pKodoUploader, nCurSysTime, 1, 0, 0);
        if (fromInternal) {
                pKodoUploader->bakSession.nSessionEndTime = pKodoUploader->session.nLastTsEndTime;
                if (pKodoUploader->bakSession.nTsSequenceNumber > 1) {// 当前切片算在新的sessoin，所以之前seq要减1
                        pKodoUploader->bakSession.nTsSequenceNumber--;
                        pKodoUploader->session.nTsSequenceNumber++;
                }
                
                
        }
}

// 第一个切片还没有开始上传任何东西就收到了强制分片相关的命令消息
static void checkAndSetFirstSessionPicReportedStatus(KodoUploader * pKodoUploader) {
        if (pKodoUploader->isFirstSessionPicReported) {
                pKodoUploader->isFirstSessionPicReported = 0;
                pKodoUploader->isSegStartReport = 0;
        }
}

static void * listenTsUpload(void *_pOpaque)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pOpaque;
        handleSessionCheck(pKodoUploader, LinkGetCurrentNanosecond(), 1, 0, 0);
        pKodoUploader->isFirstSessionPicReported = 1;
        pKodoUploader->isSegStartReport = 1;
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
                                        LinkLogWarn("drop picture:%s", pKodoUploader->picture.pFilename);
                                        free((void*)pKodoUploader->picture.pFilename);
                                        pKodoUploader->picture.pFilename = NULL;
                                }
                                pKodoUploader->picture = cmd.pic;
                                break;
                        case LINK_TSU_QUIT:
                                LinkLogInfo("tsuploader required to quit:%"PRId64"", pKodoUploader->session.nAccSessionDuration);
                                if (pKodoUploader->session.nAccSessionDuration > 0) {
                                        LinkLogDebug("=========================4>%s", pKodoUploader->session.sessionId, pKodoUploader->session.nAccSessionVideoDuration);
                                        pKodoUploader->session.nSessionEndResonCode = 4;
                                        pKodoUploader->session.nSessionEndTime = pKodoUploader->session.nLastTsEndTime;
                                        LinkUpdateSegment(pKodoUploader->session.segHandle, &pKodoUploader->session);
                                }
                                break;
                        case LINK_TSU_START_TIME:
                                handleTsStartTimeReport(pKodoUploader, &cmd.time);
                                break;
                        case LINK_TSU_END_TIME:
                                handleTsEndTimeReport(pKodoUploader, &cmd.time);
                                break;
                        case LINK_TSU_SEG_TIME:
                                checkAndSetFirstSessionPicReportedStatus(pKodoUploader);
                                handleSegTimeReport(pKodoUploader, cmd.time.nSystimestamp, 0);
                                break;
                        case LINK_TSU_SET_META:
                                checkAndSetFirstSessionPicReportedStatus(pKodoUploader);
                                if (pKodoUploader->pSessionMeta) {
                                        free(pKodoUploader->pSessionMeta);
                                        pKodoUploader->pSessionMeta = NULL;
                                }
                                
                                if (!pKodoUploader->nInternalForceSegFlag) {
                                        if (pKodoUploader->isForceSeg == 0 && pKodoUploader->nFirstSystime > 0) { // 切片还在缓存中
                                                // 当前切片也算在新的分片中
                                                LinkLogDebug("1force seg cuz meta:%d", pKodoUploader->session.nTsSequenceNumber);
                                                // 可能因为其它原因刚强制分片完成，新的片段一个切片都没有上传完成
                                                if (pKodoUploader->session.nTsSequenceNumber > 1) {
                                                        handleSegTimeReport(pKodoUploader, pKodoUploader->nFirstSystime, 1);
                                                }
                                        } else { // 当前切片已结束，但是下一个切片还没有开始，设置标志到下个切片开始
                                                LinkLogDebug("2force seg cuz meta");
                                                pKodoUploader->isForceSeg = 1;
                                        }
                                }
                                pKodoUploader->nInternalForceSegFlag = 1;
                                
                                pKodoUploader->pSessionMeta = cmd.pSessionMeta;
                                break;
                        case LINK_TSU_CLR_META:
                                checkAndSetFirstSessionPicReportedStatus(pKodoUploader);
                                if (pKodoUploader->pSessionMeta) {
                                        pKodoUploader->pSessionMeta->isOneShot = 1;
                                }
                                if (!pKodoUploader->nInternalForceSegFlag) {
                                        if (pKodoUploader->nFirstSystime > 0) {
                                                LinkLogDebug("3force seg cuz meta");
                                                pKodoUploader->isForceSeg = 1;
                                        }
                                }
                                pKodoUploader->nInternalForceSegFlag = 1;
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
        
        pthread_mutex_lock(&pKodoUploader->uploadMutex_);
        int nCurCacheNum = pKodoUploader->nTsCacheNum;
        if (nCurCacheNum >= pKodoUploader->nTsMaxCacheNum) {
                LinkLogWarn("drop pic:%s", pic.pFilename);
                pthread_mutex_unlock(&pKodoUploader->uploadMutex_);
                return LINK_MAX_CACHE;
        }
        pthread_mutex_unlock(&pKodoUploader->uploadMutex_);
        
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
                LinkLogError("ts queue error. push pic meta:%s", ret, pic.pFilename);
                free(pFileName);
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
