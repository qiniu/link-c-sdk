#include "tsuploader.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <pthread.h>
#include "servertime.h"
#include <time.h>
#include "fixjson.h"
#include "b64/urlsafe_b64.h"
#include <qupload.h>
#include "httptools.h"
#include <unistd.h>

size_t getDataCallback(void* buffer, size_t size, size_t n, void* rptr);

#define TS_DIVIDE_LEN 4096

enum LinkTsuCmdType {
        LINK_TSU_UPLOAD = 1,
        LINK_TSU_QUIT = 2,
        LINK_TSU_AUDIO_TIME = 3,
        LINK_TSU_VIDEO_TIME = 4,
        LINK_TSU_SEG_TIME = 5
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
        enum CircleQueuePolicy policy;
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
        LinkSession session;
        
        // for discontinuity
        int64_t nLastSystime;
        int64_t nFirstSystime;
        int64_t nLastSystimeBak;
}KodoUploader;

typedef struct _TsUploaderCommandTs {
        void *pData;
        KodoUploader *pKodoUploader;
        TsUploaderMeta* pUpMeta;
}TsUploaderCommandTs;
typedef struct _TsUploaderCommandTime {
        int64_t nAvTimestamp;
        int64_t nSysTimestamp;
}TsUploaderCommandTime;

typedef struct _TsUploaderCommand {
        enum LinkTsuCmdType nCommandType;
        union{
                TsUploaderCommandTs ts;
                TsUploaderCommandTime time;
        };
}TsUploaderCommand;

static void handleSessionCheck(KodoUploader * pKodoUploader, int64_t nSysTimestamp, int isForceNewSession);

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
                         LinkKeyFrameMetaInfo *pMetas, int nMetaLen, int64_t duration, int64_t seqnum, int isDiscontinuity) {
       
        char metaValue[200];
        char metaBuf[250];
        int i = 0;
        for(i = 0; i < nMetaLen; i++) {
                inttoBCD(pMetas[i].nTimestamp90Khz, metaBuf + 15 * i);
                inttoBCD(pMetas[i].nOffset, metaBuf + 15 * i + 5);
                inttoBCD(pMetas[i].nLength, metaBuf + 15 * i + 10);
        }
        int nMetaValueLen = urlsafe_b64_encode(metaBuf, nMetaLen * 15, metaValue, sizeof(metaValue));
        nMetaValueLen = snprintf(metaBuf, sizeof(metaBuf), "{\"o\":\"%s\",\"d\":%"PRId64",\"s\":%"PRId64",\"c\":%d}",
                                 metaValue, duration, seqnum, isDiscontinuity);
        
        LinkPutret putret;
        int ret = LinkUploadBuffer(data, datasize, uphost, token, key, metaBuf, nMetaValueLen, /*mimetype*/NULL, &putret);

        

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
        pSession->nFirstFrameTimestamp = 0;
        pSession->nLastFrameTimestamp = 0;
        pSession->nFirstAudioFrameTimestamp = 0;
        pSession->nLastAudioFrameTimestamp = 0;
        pSession->nFirstVideoFrameTimestamp = 0;
        pSession->nLastVideoFrameTimestamp = 0;
}

static void resetSessionReportScope(LinkSession *pSession) {
        pSession->nAudioGapFromLastReport = 0;
        pSession->nVideoGapFromLastReport = 0;
}

static void * streamUpload(TsUploaderCommand *pUploadCmd) {
        
        char uptoken[1024] = {0};
        char upHost[192] = {0};
        int ret = 0;
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
        
#ifdef LINK_USE_OLD_NAME
        char deviceName[LINK_MAX_DEVICE_NAME_LEN+1] = {0};
        char app[LINK_MAX_APP_LEN+1] = {0};
        param.pDeviceName = deviceName;
        param.nDeviceNameLen = sizeof(deviceName);
#else
        char fprefix[LINK_MAX_DEVICE_NAME_LEN * 2 + 32];
        param.pFilePrefix = fprefix;
        param.nFilePrefix = sizeof(fprefix);
#endif

        strcpy(param.sessionId, pKodoUploader->session.sessionId);
        
        char key[128+LINK_MAX_DEVICE_NAME_LEN+LINK_MAX_APP_LEN] = {0};
        
        LinkUploadResult uploadResult = LINK_UPLOAD_RESULT_FAIL;

        ret = pKodoUploader->uploadArg.uploadParamCallback(
                                        pKodoUploader->uploadArg.pGetUploadParamCallbackArg,
                                        &param, LINK_UPLOAD_CB_GETPARAM);
        if (ret != LINK_SUCCESS) {
                if (ret == LINK_BUFFER_IS_SMALL) {
                        LinkLogError("param buffer is too small. drop file");
                        goto END;
                } else {
                        LinkLogError("not get param yet:%d", ret);
                        goto END;
                }
        }
        
        int64_t tsStartTime = pSession->nTsStartTime;
        int64_t tsDuration = pSession->nLastFrameTimestamp - pSession->nFirstFrameTimestamp;
        
        handleSessionCheck(pKodoUploader, pKodoUploader->session.nTsStartTime + tsDuration * 1000000LL, 0);
        
        int isDiscontinuity = 0;
        if (pKodoUploader->nLastSystimeBak > 0) {
                if (pKodoUploader->nFirstSystime - pKodoUploader->nLastSystimeBak > 200)
                        isDiscontinuity = 1;
        }
        pKodoUploader->nLastSystimeBak = pKodoUploader->nLastSystime;
        pKodoUploader->nFirstSystime = 0;
        
        resetSessionReportScope(pSession);
        resetSessionCurrentTsScope(pSession);

        memset(key, 0, sizeof(key));
        
        
        int r, l;
        char *bufData;
        r = LinkGetQueueBuffer(pDataQueue, &bufData, &l);
        if (r > 0) {
#ifdef LINK_USE_OLD_NAME
                uint64_t nSegmentId = pSession->nSessionStartTime;
                int nDeleteAfterDays_ = 0;
                ret = LinkGetPolicyFromUptoken(uptoken, &nDeleteAfterDays_, NULL);
                if (ret != LINK_SUCCESS) {
                        LinkLogWarn("not get deleteafterdays");
                }
                //ts/uaid/startts/endts/segment_start_ts/expiry[/type].ts
                sprintf(key, "ts/%s/%"PRId64"/%"PRId64"/%"PRId64"/%d.ts", param.pDeviceName,
                        tsStartTime / 1000000, tsStartTime / 1000000 + tsDuration, nSegmentId / 1000000, nDeleteAfterDays_);
                
#else
                sprintf(key, "%s/ts/%"PRId64"-%"PRId64"-%s.ts", param.pFilePrefix,
                        tsStartTime / 1000000, tsStartTime / 1000000 + tsDuration, pSession->sessionId);
#endif
                LinkLogDebug("upload start:%s q:%p  len:%d", key, pDataQueue, l);
                
                int putRet = linkPutBuffer(upHost, uptoken, key, bufData, l, pUpMeta->metaInfo, pUpMeta->nMetaInfoLen,
                                           tsDuration, pKodoUploader->session.nTsSequenceNumber++, isDiscontinuity);
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
                goto END;
        } else {
                LinkLogError("LinkGetQueueBuffer get no data:%d", r);
                goto END;
        }


END:
        if (pKodoUploader->uploadArg.pUploadStatisticCb) {
                pKodoUploader->uploadArg.pUploadStatisticCb(pKodoUploader->uploadArg.pUploadStatArg,
                                                                        LINK_UPLOAD_TS, uploadResult);
        }
        if (pKodoUploader->pTsEndUploadCallback) {
                pKodoUploader->pTsEndUploadCallback(pKodoUploader->pTsEndUploadCallbackArg, tsStartTime / 1000000);
        }

        LinkDestroyQueue(&pDataQueue);
        free(pUpMeta);
        
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
                LinkLogError("drop ts file due to ts queue is full");
        } else {
                LinkLogDebug("-------->push a queue\n", nCurCacheNum);
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

void reportTimeInfo(LinkTsUploader *_pTsUploader, int64_t _nTimestamp, int64_t nSysNanotime,
                    enum LinkUploaderTimeInfoType tmtype) {
        
        KodoUploader * pKodoUploader = (KodoUploader *)_pTsUploader;
        
        TsUploaderCommand tmcmd;
        if (tmtype == LINK_AUDIO_TIMESTAMP) {
                tmcmd.nCommandType = LINK_TSU_AUDIO_TIME;
        }
        else if (tmtype == LINK_VIDEO_TIMESTAMP){
                tmcmd.nCommandType = LINK_TSU_VIDEO_TIME;
        }
        else if (tmtype == LINK_SEG_TIMESTAMP){
                tmcmd.nCommandType = LINK_TSU_SEG_TIME;
        }
        tmcmd.time.nAvTimestamp = _nTimestamp;
        tmcmd.time.nSysTimestamp = nSysNanotime;
        
        int ret;
        ret = pKodoUploader->pCommandQueue_->Push(pKodoUploader->pCommandQueue_, (char *)&tmcmd, sizeof(TsUploaderCommand));
        if (ret <= 0) {
                LinkLogError("ts queue error. push report time:%d", ret);
        }
        
        return;
}

LinkUploadState getUploaderState(LinkTsUploader *_pTsUploader)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pTsUploader;
        return pKodoUploader->state;
}

void LinkUpdateSessionId(LinkSession *pSession, int64_t nTsStartSystime) {
        char str[15] = {0};
        sprintf(str, "%"PRId64"", nTsStartSystime/1000000);
        int nId = urlsafe_b64_encode(str, strlen(str), pSession->sessionId, sizeof(pSession->sessionId));
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

static void handleSessionCheck(KodoUploader * pKodoUploader, int64_t nSysTimestamp, int isForceNewSession) {

        if (pKodoUploader->uploadArg.UploadUpdateSegmentId) {
                if (isForceNewSession)
                        pKodoUploader->uploadArg.UploadUpdateSegmentId(pKodoUploader->uploadArg.pUploadArgKeeper_,
                                                                       &pKodoUploader->session, nSysTimestamp, 0);
                else
                        pKodoUploader->uploadArg.UploadUpdateSegmentId(pKodoUploader->uploadArg.pUploadArgKeeper_,
                                                                       &pKodoUploader->session,
                                                                       pKodoUploader->session.nTsStartTime, nSysTimestamp);
        }
}

static void handleAudioTimeReport(KodoUploader * pKodoUploader, TsUploaderCommandTime *pTi) {
        if (pKodoUploader->session.nFirstFrameTimestamp <= 0) {
                pKodoUploader->session.nFirstFrameTimestamp = pTi->nAvTimestamp;
                pKodoUploader->session.nLastFrameTimestamp = pTi->nAvTimestamp;
        }
        
        if (pKodoUploader->session.nFirstAudioFrameTimestamp <= 0) {
                pKodoUploader->session.nFirstAudioFrameTimestamp = pTi->nAvTimestamp;
                pKodoUploader->session.nLastAudioFrameTimestamp = pTi->nAvTimestamp;
        }
        if (pKodoUploader->nFirstSystime <= 0)
                pKodoUploader->nFirstSystime = pTi->nSysTimestamp;
        
        int64_t nDiffAudio = pTi->nAvTimestamp - pKodoUploader->session.nLastAudioFrameTimestamp;
        
        pKodoUploader->session.nAccSessionAudioDuration += nDiffAudio;
        pKodoUploader->session.nAudioGapFromLastReport += nDiffAudio;
        
        nDiffAudio = pTi->nAvTimestamp - pKodoUploader->session.nLastFrameTimestamp;
        if (nDiffAudio > 0)
                pKodoUploader->session.nAccSessionDuration += nDiffAudio;
        
        pKodoUploader->session.nLastAudioFrameTimestamp = pTi->nAvTimestamp;
        pKodoUploader->session.nLastFrameTimestamp = pTi->nAvTimestamp;
        pKodoUploader->nLastSystime = pTi->nSysTimestamp;
        
        handleSessionCheck(pKodoUploader, pTi->nSysTimestamp, 0);
}

static void handleVideoTimeReport(KodoUploader * pKodoUploader, TsUploaderCommandTime *pTi) {
        
        if (pKodoUploader->session.nFirstFrameTimestamp <= 0) {
                pKodoUploader->session.nFirstFrameTimestamp = pTi->nAvTimestamp;
                pKodoUploader->session.nLastFrameTimestamp = pTi->nAvTimestamp;
        }
        
        if (pKodoUploader->session.nFirstVideoFrameTimestamp <= 0) {
                pKodoUploader->session.nFirstVideoFrameTimestamp = pTi->nAvTimestamp;
                pKodoUploader->session.nLastVideoFrameTimestamp = pTi->nAvTimestamp;
        }
        if (pKodoUploader->nFirstSystime <= 0)
                pKodoUploader->nFirstSystime = pTi->nSysTimestamp;
        
        if (pKodoUploader->session.nTsStartTime <= 0)
                pKodoUploader->session.nTsStartTime = pTi->nSysTimestamp;
        
        int64_t nDiffVideo = pTi->nAvTimestamp - pKodoUploader->session.nLastVideoFrameTimestamp;
        
        pKodoUploader->session.nAccSessionVideoDuration += nDiffVideo;
        pKodoUploader->session.nVideoGapFromLastReport += nDiffVideo;
        
        nDiffVideo = pTi->nAvTimestamp - pKodoUploader->session.nLastFrameTimestamp;
        if (nDiffVideo > 0)
                pKodoUploader->session.nAccSessionDuration += nDiffVideo;
        
        pKodoUploader->session.nLastVideoFrameTimestamp = pTi->nAvTimestamp;
        pKodoUploader->session.nLastFrameTimestamp = pTi->nAvTimestamp;
        pKodoUploader->nLastSystime = pTi->nSysTimestamp;
        
        handleSessionCheck(pKodoUploader, pTi->nSysTimestamp, 0);
}

static void handleSegTimeReport(KodoUploader * pKodoUploader, TsUploaderCommandTime *pTi) {
        
        handleSessionCheck(pKodoUploader, pTi->nSysTimestamp, 1);
}

static void * listenTsUpload(void *_pOpaque)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pOpaque;
        LinkUploaderStatInfo info = {0};
        while(!pKodoUploader->nQuit_ || info.nLen_ != 0) {
                TsUploaderCommand cmd;
                int ret = pKodoUploader->pCommandQueue_->PopWithTimeout(pKodoUploader->pCommandQueue_, (char *)(&cmd),
                                                                      sizeof(TsUploaderCommand), 24 * 60 * 60);
                pKodoUploader->pCommandQueue_->GetStatInfo(pKodoUploader->pCommandQueue_, &info);
                //LinkLogDebug("ts queue:%d", info.nLen_);
                if (ret <= 0) {
                        if (ret != LINK_TIMEOUT) {
                                LinkLogError("seg queue error. pop:%d", ret);
                        }
                        continue;
                }
                
                switch (cmd.nCommandType) {
                        case LINK_TSU_UPLOAD:
                                streamUpload(&cmd);
                                break;
                        case LINK_TSU_QUIT:
                                LinkLogInfo("tsuploader required to quit");
                                return NULL;
                        case LINK_TSU_AUDIO_TIME:
                                handleAudioTimeReport(pKodoUploader, &cmd.time);
                                break;
                        case LINK_TSU_VIDEO_TIME:
                                handleVideoTimeReport(pKodoUploader, &cmd.time);
                                break;
                        case LINK_TSU_SEG_TIME:
                                handleSegTimeReport(pKodoUploader, &cmd.time);
                                break;
                        default:
                                break;
                }
                
        }
        return NULL;
}

int LinkNewTsUploader(LinkTsUploader ** _pUploader, const LinkTsUploadArg *_pArg, enum CircleQueuePolicy _policy, int _nMaxItemLen, int _nInitItemCount)
{
        KodoUploader * pKodoUploader = (KodoUploader *) malloc(sizeof(KodoUploader));
        if (pKodoUploader == NULL) {
                return LINK_NO_MEMORY;
        }
        
        memset(pKodoUploader, 0, sizeof(KodoUploader));
        
        pKodoUploader->uploadArg = *_pArg;
        
        pKodoUploader->uploader.Push = streamPushData;
        pKodoUploader->uploader.NotifyDataPrapared = notifyDataPrapared;
        
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
        
        ret = LinkNewCircleQueue(&pKodoUploader->pCommandQueue_, 1, TSQ_FIX_LENGTH, sizeof(TsUploaderCommand), 512);
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

void LinkDestroyTsUploader(LinkTsUploader ** _pUploader)
{
        KodoUploader * pKodoUploader = (KodoUploader *)(*_pUploader);
        
        pKodoUploader->nQuit_ = 1;
        
        TsUploaderCommand uploadCommand;
        uploadCommand.nCommandType = LINK_TSU_QUIT;
        
        int ret = 0;
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
