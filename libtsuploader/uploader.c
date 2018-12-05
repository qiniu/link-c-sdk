#include "uploader.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <pthread.h>
#include "servertime.h"
#include <time.h>
#include "fixjson.h"
#include "b64/b64.h"
#include <qupload.h>
#include "httptools.h"

size_t getDataCallback(void* buffer, size_t size, size_t n, void* rptr);

#define TS_DIVIDE_LEN 4096

enum WaitFirstFlag {
        WF_INIT,
        WF_LOCKED,
        WF_FIRST,
        WF_QUIT,
};

typedef struct _KodoUploader{
        LinkTsUploader uploader;
        LinkCircleQueue * pQueue_;

        pthread_t workerId_;
        int isThreadStarted_;
        
        LinkTsUploadArg uploadArg;
        
        int64_t nFirstFrameTimestamp;
        int64_t nLastFrameTimestamp;
        int64_t nTsStartTimestamp;
        LinkUploadState state;
        
        int64_t getDataBytes;
        int nLowSpeedCnt;
        int isTimeoutWithData;
        
        pthread_mutex_t waitFirstMutex_;
        enum WaitFirstFlag nWaitFirstMutexLocked_;
        LinkTsStartUploadCallback tsStartUploadCallback;
        void *pTsStartUploadCallbackArg;
        LinkKeyFrameMetaInfo metaInfo[10];
        int nMetaInfoLen;
}KodoUploader;




static size_t writeResult(void *resp, size_t size,  size_t nmemb,  void *pUserData) {
        char **pResp = (char **)pUserData;
        int len = size * nmemb ;
        char *respTxt = (char *)malloc(len +1);
        memcpy(respTxt, resp, len);
        
        respTxt[len] = 0;
        *pResp = respTxt;
        return len;
}

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
                         LinkKeyFrameMetaInfo *pMetas, int nMetaLen) {
       
        char metaValue[200];
        char metaBcd[150];
        int i = 0;
        for(i = 0; i < nMetaLen; i++) {
                inttoBCD(pMetas[i].nTimestamp90Khz, metaBcd + 15 * i);
                inttoBCD(pMetas[i].nOffset, metaBcd + 15 * i + 5);
                inttoBCD(pMetas[i].nLength, metaBcd + 15 * i + 10);
        }
        int nMetaValueLen = b64_encode(metaBcd, nMetaLen * 15, metaValue, sizeof(metaValue));
        
        LinkPutret putret;
        int ret = LinkUploadBuffer(data, datasize, uphost, token, key, metaValue, nMetaValueLen, /*mimetype*/NULL, &putret);

        

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

#ifdef MULTI_SEG_TEST
static int newSegCount = 0;
#endif
static void * streamUpload(void *_pOpaque)
{
        KodoUploader * pUploader = (KodoUploader *)_pOpaque;
        
        char uptoken[1024] = {0};
        char upHost[192] = {0};
        char suffix[16] = {0};
        char deviceName[33] = {0};
        int ret = 0;
        
        LinkUploadParam param;
        memset(&param, 0, sizeof(param));
        param.pTokenBuf = uptoken;
        param.nTokenBufLen = sizeof(uptoken);
        param.pTypeBuf = suffix;
        param.nTypeBufLen = sizeof(suffix);
        param.pUpHost = upHost;
        param.nUpHostLen = sizeof(upHost);
        param.pDeviceName = deviceName;
        param.nDeviceNameLen = sizeof(deviceName);
        
        char key[128] = {0};
        
        enum CircleQueuePolicy qtype = pUploader->pQueue_->GetType(pUploader->pQueue_);
        // wait for first packet
        if (pUploader->nWaitFirstMutexLocked_ == WF_LOCKED) {
                pthread_mutex_lock(&pUploader->waitFirstMutex_);
                pthread_mutex_unlock(&pUploader->waitFirstMutex_);
        }
        
        if (qtype != TSQ_APPEND && pUploader->nWaitFirstMutexLocked_ != WF_FIRST) {
                LinkLogWarn("nWaitFirstMutexLocked_ status abnormal");
                return NULL;
        }
        LinkUploadResult uploadResult = LINK_UPLOAD_RESULT_FAIL;
        ret = pUploader->uploadArg.getUploadParamCallback(pUploader->uploadArg.pGetUploadParamCallbackArg,
                                                          &param);
        if (ret != LINK_SUCCESS) {
                if (ret == LINK_BUFFER_IS_SMALL) {
                        LinkLogError("token buffer %d is too small. drop file:%s", sizeof(uptoken), key);
                        goto END;
                } else {
                        LinkLogError("not get uptoken yet:%s", key);
                        goto END;
                }
        }
        
        int64_t tsStartTime = pUploader->nTsStartTimestamp;
        
        
        int64_t tsDuration = pUploader->nLastFrameTimestamp - pUploader->nFirstFrameTimestamp;
        
        if (pUploader->uploadArg.nSegmentId_ == 0) {
                pUploader->uploadArg.nSegmentId_ = tsStartTime;
        }
        pUploader->uploadArg.nLastUploadTsTime_ = tsStartTime;
        if (pUploader->uploadArg.UploadSegmentIdUpadate) {
                pUploader->uploadArg.UploadSegmentIdUpadate(pUploader->uploadArg.pUploadArgKeeper_, &pUploader->uploadArg, tsStartTime,
                                                            tsStartTime + tsDuration * 1000000);
        }
        uint64_t nSegmentId = pUploader->uploadArg.nSegmentId_;
        
        int nDeleteAfterDays_ = 0;
        ret = LinkGetDeleteAfterDaysFromUptoken(uptoken, &nDeleteAfterDays_);
        if (ret != LINK_SUCCESS) {
                LinkLogWarn("not get deleteafterdays");
        }
        memset(key, 0, sizeof(key));
        
        
        if (pUploader->tsStartUploadCallback) {
                pUploader->tsStartUploadCallback(pUploader->pTsStartUploadCallbackArg, tsStartTime / 1000000);
        }
        

        if (qtype == TSQ_APPEND) {
                int r, l;
                char *bufData;
                r = LinkGetQueueBuffer(pUploader->pQueue_, &bufData, &l);
                if (r > 0) {
                        //ts/uaid/startts/endts/segment_start_ts/expiry[/type].ts
                        if (suffix[0] != 0) {
                                sprintf(key, "ts/%s/%"PRId64"/%"PRId64"/%"PRId64"/%d/%s.ts", param.pDeviceName,
                                        tsStartTime / 1000000, tsStartTime / 1000000 + tsDuration, nSegmentId / 1000000, nDeleteAfterDays_, suffix);
                        } else {
                                sprintf(key, "ts/%s/%"PRId64"/%"PRId64"/%"PRId64"/%d.ts", param.pDeviceName,
                                        tsStartTime / 1000000, tsStartTime / 1000000 + tsDuration, nSegmentId / 1000000, nDeleteAfterDays_);
                        }
                        LinkLogDebug("upload start:%s q:%p  len:%d", key, pUploader->pQueue_, l);

                        int putRet = linkPutBuffer(upHost, uptoken, key, bufData, l, pUploader->metaInfo,
                                                   pUploader->nMetaInfoLen);
                        if (putRet == LINK_SUCCESS) {
                                uploadResult = LINK_UPLOAD_RESULT_OK;
                                pUploader->state = LINK_UPLOAD_OK;
                        } else {
                                pUploader->state = LINK_UPLOAD_FAIL;
                        }
                        if (pUploader->uploadArg.pUploadStatisticCb) {
                                pUploader->uploadArg.pUploadStatisticCb(pUploader->uploadArg.pUploadStatArg, LINK_UPLOAD_TS, uploadResult);
                        }
                        goto END;
                } else {
                        LinkLogError("LinkGetQueueBuffer get no data:%d", r);
                        goto END;
                }
        }

END:
        if (pUploader->uploadArg.pUploadStatisticCb) {
                pUploader->uploadArg.pUploadStatisticCb(pUploader->uploadArg.pUploadStatArg, LINK_UPLOAD_TS, uploadResult);
        }

        return NULL;
}


static int streamUploadStart(LinkTsUploader * _pUploader)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pUploader;
        int ret = pthread_create(&pKodoUploader->workerId_, NULL, streamUpload, _pUploader);
        if (ret == 0) {
                pKodoUploader->isThreadStarted_ = 1;
                return LINK_SUCCESS;
        } else {
                LinkLogError("start upload thread fail:%d", ret);
                return LINK_THREAD_ERROR;
        }
}

static void streamUploadStop(LinkTsUploader * _pUploader)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pUploader;
        if(pKodoUploader->nWaitFirstMutexLocked_ == WF_LOCKED) {
                pKodoUploader->nWaitFirstMutexLocked_ = WF_QUIT;
                pthread_mutex_unlock(&pKodoUploader->waitFirstMutex_);
        }
        pthread_mutex_lock(&pKodoUploader->waitFirstMutex_);
        pKodoUploader->nWaitFirstMutexLocked_ = WF_QUIT;
        pthread_mutex_unlock(&pKodoUploader->waitFirstMutex_);
        
        if (pKodoUploader->isThreadStarted_) {
                pKodoUploader->pQueue_->StopPush(pKodoUploader->pQueue_);
                pthread_join(pKodoUploader->workerId_, NULL);
                pKodoUploader->isThreadStarted_ = 0;
        }
        return;
}

static int streamPushData(LinkTsUploader *pTsUploader, const char * pData, int nDataLen)
{
        KodoUploader * pKodoUploader = (KodoUploader *)pTsUploader;
        
        int ret = pKodoUploader->pQueue_->Push(pKodoUploader->pQueue_, (char *)pData, nDataLen);
        if (pKodoUploader->pQueue_->GetType(pKodoUploader->pQueue_) != TSQ_APPEND && pKodoUploader->nWaitFirstMutexLocked_ == WF_LOCKED) {
                pKodoUploader->nWaitFirstMutexLocked_ = WF_FIRST;
                pthread_mutex_unlock(&pKodoUploader->waitFirstMutex_);
        }
        return ret;
}

static void notifyDataPrapared(LinkTsUploader *pTsUploader) {
        KodoUploader * pKodoUploader = (KodoUploader *)pTsUploader;
        if (pKodoUploader->nWaitFirstMutexLocked_ == WF_LOCKED)
                pKodoUploader->nWaitFirstMutexLocked_ = WF_FIRST;
        
        pthread_mutex_unlock(&pKodoUploader->waitFirstMutex_);
        
        return;
}

static void getStatInfo(LinkTsUploader *pTsUploader, LinkUploaderStatInfo *_pStatInfo)
{
        KodoUploader * pKodoUploader = (KodoUploader *)pTsUploader;

        pKodoUploader->pQueue_->GetStatInfo(pKodoUploader->pQueue_, _pStatInfo);

        return;
}

void recordTimestamp(LinkTsUploader *_pTsUploader, int64_t _nTimestamp, int64_t nSysNanotime)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pTsUploader;
        if (pKodoUploader->nFirstFrameTimestamp == -1) {
                pKodoUploader->nTsStartTimestamp = nSysNanotime;
                pKodoUploader->nFirstFrameTimestamp = _nTimestamp;
                pKodoUploader->nLastFrameTimestamp = _nTimestamp;
        }
        pKodoUploader->nLastFrameTimestamp = _nTimestamp;
        return;
}

LinkUploadState getUploaderState(LinkTsUploader *_pTsUploader)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pTsUploader;
        return pKodoUploader->state;
}

int LinkNewTsUploader(LinkTsUploader ** _pUploader, const LinkTsUploadArg *_pArg, enum CircleQueuePolicy _policy, int _nMaxItemLen, int _nInitItemCount)
{
        KodoUploader * pKodoUploader = (KodoUploader *) malloc(sizeof(KodoUploader));
        if (pKodoUploader == NULL) {
                return LINK_NO_MEMORY;
        }
        
        memset(pKodoUploader, 0, sizeof(KodoUploader));
        
        int ret = pthread_mutex_init(&pKodoUploader->waitFirstMutex_, NULL);
        if (ret != 0){
                free(pKodoUploader);
                return LINK_MUTEX_ERROR;
        }
        pthread_mutex_lock(&pKodoUploader->waitFirstMutex_);
        pKodoUploader->nWaitFirstMutexLocked_ = WF_LOCKED;

        ret = LinkNewCircleQueue(&pKodoUploader->pQueue_, 0, _policy, _nMaxItemLen, _nInitItemCount);
        if (ret != 0) {
                free(pKodoUploader);
                return ret;
        }

        pKodoUploader->nFirstFrameTimestamp = -1;
        pKodoUploader->nLastFrameTimestamp = -1;
        pKodoUploader->uploadArg = *_pArg;

        pKodoUploader->uploader.UploadStart = streamUploadStart;
        pKodoUploader->uploader.UploadStop = streamUploadStop;
        pKodoUploader->uploader.Push = streamPushData;
        pKodoUploader->uploader.NotifyDataPrapared = notifyDataPrapared;

        pKodoUploader->uploader.GetStatInfo = getStatInfo;
        pKodoUploader->uploader.RecordTimestamp = recordTimestamp;
        pKodoUploader->uploader.GetUploaderState = getUploaderState;
        
        
        *_pUploader = (LinkTsUploader*)pKodoUploader;
        
        return LINK_SUCCESS;
}

void LinkTsUploaderSetTsStartUploadCallback(LinkTsUploader * _pUploader, LinkTsStartUploadCallback cb, void *pOpaque) {
        KodoUploader * pKodoUploader = (KodoUploader *)(_pUploader);
        pKodoUploader->tsStartUploadCallback = cb;
        pKodoUploader->pTsStartUploadCallbackArg = pOpaque;
}

void LinkDestroyTsUploader(LinkTsUploader ** _pUploader)
{
        KodoUploader * pKodoUploader = (KodoUploader *)(*_pUploader);
        
        pthread_mutex_destroy(&pKodoUploader->waitFirstMutex_);

        if (pKodoUploader->isThreadStarted_) {
                pthread_join(pKodoUploader->workerId_, NULL);
        }
        LinkDestroyQueue(&pKodoUploader->pQueue_);

        
        free(pKodoUploader);
        * _pUploader = NULL;
        return;
}

void LinkAppendKeyframeMetaInfo(void *pOpaque, LinkKeyFrameMetaInfo *pMediaInfo) {
        if (pOpaque == NULL || pMediaInfo == NULL) {
                return;
        }
        KodoUploader * pKodoUploader = (KodoUploader *)(pOpaque);
        if (pKodoUploader->nMetaInfoLen < sizeof(pKodoUploader->metaInfo) / sizeof(LinkKeyFrameMetaInfo)) {
                int idx = pKodoUploader->nMetaInfoLen++;
                pKodoUploader->metaInfo[idx] = *pMediaInfo;
                pKodoUploader->metaInfo[idx].nTimestamp90Khz &= 0x00000001FFFFFFFFLL;
                //fprintf(stderr, "==========------->%lld %d %d %x\n", pMediaInfo->nTimestamp, pMediaInfo->nOffset, pMediaInfo->nLength, pOpaque);
        }
}
