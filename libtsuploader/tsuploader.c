#include "tsuploader.h"
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

typedef struct _TsUploaderMeta {
        int64_t nFirstFrameTimestamp;
        int64_t nLastFrameTimestamp;
        int64_t nTsStartTimestamp;
        LinkUploadState state;
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

        pthread_t workerId_;
        int isThreadStarted_;
        
        LinkTsUploadArg uploadArg;
        LinkUploadState state;

        LinkEndUploadCallback pTsEndUploadCallback;
        void *pTsEndUploadCallbackArg;
        int nQuit_;
}KodoUploader;



typedef struct _TsUploaderCommand {
        int nCommandType;
        void *pData;
        KodoUploader *pKodoUploader;
        TsUploaderMeta* pUpMeta;
}TsUploaderCommand;

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

static void * streamUpload(TsUploaderCommand *pUploadCmd) {
        
        char uptoken[1024] = {0};
        char upHost[192] = {0};
        char suffix[16] = {0};
        char deviceName[LINK_MAX_DEVICE_NAME_LEN+1] = {0};
        char app[LINK_MAX_APP_LEN+1] = {0};
        int ret = 0;
        LinkCircleQueue *pDataQueue = (LinkCircleQueue *)pUploadCmd->pData;
        
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
        param.pApp = app;
        param.nAppLen = sizeof(app);
        
        char key[128+LINK_MAX_DEVICE_NAME_LEN+LINK_MAX_APP_LEN] = {0};
        
       
        LinkUploadResult uploadResult = LINK_UPLOAD_RESULT_FAIL;

        ret = pUploadCmd->pKodoUploader->uploadArg.getUploadParamCallback(
                                        pUploadCmd->pKodoUploader->uploadArg.pGetUploadParamCallbackArg,
                                        &param);
        if (ret != LINK_SUCCESS) {
                if (ret == LINK_BUFFER_IS_SMALL) {
                        LinkLogError("param buffer is too small. drop file");
                        goto END;
                } else {
                        LinkLogError("not get param yet:%d", ret);
                        goto END;
                }
        }
        
        TsUploaderMeta* pUpMeta = pUploadCmd->pUpMeta;
        int64_t tsStartTime = pUpMeta->nTsStartTimestamp;
        
        
        int64_t tsDuration = pUpMeta->nLastFrameTimestamp - pUpMeta->nFirstFrameTimestamp;
        
        if (pUploadCmd->pKodoUploader->uploadArg.nSegmentId_ == 0) {
                pUploadCmd->pKodoUploader->uploadArg.nSegmentId_ = tsStartTime;
        }
        pUploadCmd->pKodoUploader->uploadArg.nLastStartTime_ = tsStartTime;
        if (pUploadCmd->pKodoUploader->uploadArg.UploadSegmentIdUpdate) {
                pUploadCmd->pKodoUploader->uploadArg.UploadSegmentIdUpdate(pUploadCmd->pKodoUploader->uploadArg.pUploadArgKeeper_,
                                                                            &pUploadCmd->pKodoUploader->uploadArg, tsStartTime,
                                                            tsStartTime + tsDuration * 1000000);
        }
        uint64_t nSegmentId = pUploadCmd->pKodoUploader->uploadArg.nSegmentId_;
        
        int nDeleteAfterDays_ = 0;
        ret = LinkGetDeleteAfterDaysFromUptoken(uptoken, &nDeleteAfterDays_);
        if (ret != LINK_SUCCESS) {
                LinkLogWarn("not get deleteafterdays");
        }
        memset(key, 0, sizeof(key));
        
        
        int r, l;
        char *bufData;
        r = LinkGetQueueBuffer(pDataQueue, &bufData, &l);
        if (r > 0) {
#ifdef LINK_USE_OLD_NAME
                //ts/uaid/startts/endts/segment_start_ts/expiry[/type].ts
                if (suffix[0] != 0) {
                        sprintf(key, "ts/%s/%"PRId64"/%"PRId64"/%"PRId64"/%d/%s.ts", param.pDeviceName,
                                tsStartTime / 1000000, tsStartTime / 1000000 + tsDuration, nSegmentId / 1000000, nDeleteAfterDays_, suffix);
                } else {
                        sprintf(key, "ts/%s/%"PRId64"/%"PRId64"/%"PRId64"/%d.ts", param.pDeviceName,
                                tsStartTime / 1000000, tsStartTime / 1000000 + tsDuration, nSegmentId / 1000000, nDeleteAfterDays_);
                }
                
#else
                // app/devicename/ts/startts/endts/segment_start_ts/expiry[/type].ts
                if (suffix[0] != 0) {
                        sprintf(key, "%s/%s/ts/%"PRId64"/%"PRId64"/%"PRId64"/%d/%s.ts", param.pApp, param.pDeviceName,
                                tsStartTime / 1000000, tsStartTime / 1000000 + tsDuration, nSegmentId / 1000000, nDeleteAfterDays_, suffix);
                } else {
                        sprintf(key, "%s/%s/ts/%"PRId64"/%"PRId64"/%"PRId64"/%d.ts", param.pApp, param.pDeviceName,
                                tsStartTime / 1000000, tsStartTime / 1000000 + tsDuration, nSegmentId / 1000000, nDeleteAfterDays_);
                }
#endif
                LinkLogDebug("upload start:%s q:%p  len:%d", key, pUploadCmd->pData, l);
                
                int putRet = linkPutBuffer(upHost, uptoken, key, bufData, l, pUpMeta->metaInfo,
                                           pUpMeta->nMetaInfoLen);
                if (putRet == LINK_SUCCESS) {
                        uploadResult = LINK_UPLOAD_RESULT_OK;
                        pUploadCmd->pKodoUploader->state = LINK_UPLOAD_OK;
                } else {
                        pUploadCmd->pKodoUploader->state = LINK_UPLOAD_FAIL;
                }
                if (pUploadCmd->pKodoUploader->uploadArg.pUploadStatisticCb) {
                        pUploadCmd->pKodoUploader->uploadArg.pUploadStatisticCb(pUploadCmd->pKodoUploader->uploadArg.pUploadStatArg,
                                                                                LINK_UPLOAD_TS, uploadResult);
                }
                goto END;
        } else {
                LinkLogError("LinkGetQueueBuffer get no data:%d", r);
                goto END;
        }


END:
        if (pUploadCmd->pKodoUploader->uploadArg.pUploadStatisticCb) {
                pUploadCmd->pKodoUploader->uploadArg.pUploadStatisticCb(pUploadCmd->pKodoUploader->uploadArg.pUploadStatArg,
                                                                        LINK_UPLOAD_TS, uploadResult);
        }
        if (pUploadCmd->pKodoUploader->pTsEndUploadCallback) {
                pUploadCmd->pKodoUploader->pTsEndUploadCallback(pUploadCmd->pKodoUploader->pTsEndUploadCallbackArg, tsStartTime / 1000000);
        }

        LinkDestroyQueue(&pDataQueue);
        free(pUpMeta);
        return NULL;
}

static int allocDataQueueAndUploadMeta(KodoUploader * pKodoUploader) {
        pKodoUploader->pUpMeta = (TsUploaderMeta*)malloc(sizeof(TsUploaderMeta));
        if (pKodoUploader->pUpMeta == NULL) {
                LinkLogError("malloc upload meta error");
                return LINK_NO_MEMORY;
        }
        memset(pKodoUploader->pUpMeta, 0, sizeof(TsUploaderMeta));
        pKodoUploader->pUpMeta->nFirstFrameTimestamp = -1;
        pKodoUploader->pUpMeta->nLastFrameTimestamp = -1;
        
        if (pKodoUploader->pUpMeta != NULL) {
                pKodoUploader->pQueue_ = NULL;
                int ret = LinkNewCircleQueue(&pKodoUploader->pQueue_, 0, pKodoUploader->policy,
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
        
        if (pKodoUploader->pQueue_ == NULL) {
                int ret = allocDataQueueAndUploadMeta(pKodoUploader);
                if (ret != LINK_SUCCESS) {
                        return ret;
                }
        }
        
        int ret = pKodoUploader->pQueue_->Push(pKodoUploader->pQueue_, (char *)pData, nDataLen);
        return ret;
}

static void notifyDataPrapared(LinkTsUploader *pTsUploader) {
        KodoUploader * pKodoUploader = (KodoUploader *)pTsUploader;
        
        TsUploaderCommand uploadCommand;
        uploadCommand.nCommandType = 1;
        uploadCommand.pData = pKodoUploader->pQueue_;
        uploadCommand.pKodoUploader = pKodoUploader;
        uploadCommand.pUpMeta = pKodoUploader->pUpMeta;
        
        pKodoUploader->pQueue_ = NULL;
        pKodoUploader->pUpMeta = NULL;
        
        LinkUploaderStatInfo info = {0};
        pKodoUploader->pCommandQueue_->GetStatInfo(pKodoUploader->pCommandQueue_, &info);
        if (info.nLen_ >= 2) {
                free(uploadCommand.pUpMeta);
                LinkDestroyQueue((LinkCircleQueue **)(&uploadCommand.pData));
                LinkLogError("drop ts file due to ts queue is full");
        } else {
                LinkLogDebug("-------->push a queue\n", info.nLen_);
                pKodoUploader->pCommandQueue_->Push(pKodoUploader->pCommandQueue_, (char *)&uploadCommand, sizeof(TsUploaderCommand));
        }
        
        allocDataQueueAndUploadMeta(pKodoUploader);
        
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
        if(pKodoUploader->pUpMeta) {
                if (pKodoUploader->pUpMeta->nFirstFrameTimestamp == -1) {
                        pKodoUploader->pUpMeta->nTsStartTimestamp = nSysNanotime;
                        pKodoUploader->pUpMeta->nFirstFrameTimestamp = _nTimestamp;
                        pKodoUploader->pUpMeta->nLastFrameTimestamp = _nTimestamp;
                }
                pKodoUploader->pUpMeta->nLastFrameTimestamp = _nTimestamp;
        }
        return;
}

LinkUploadState getUploaderState(LinkTsUploader *_pTsUploader)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pTsUploader;
        return pKodoUploader->state;
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
                if (ret == LINK_TIMEOUT) {
                        continue;
                }
                
                switch (cmd.nCommandType) {
                        case 1:
                                streamUpload(&cmd);
                                break;
                        case 2:
                                return NULL;
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
        pKodoUploader->uploader.RecordTimestamp = recordTimestamp;
        pKodoUploader->uploader.GetUploaderState = getUploaderState;
        
        pKodoUploader->policy = _policy;
        pKodoUploader->nMaxItemLen = _nMaxItemLen;
        pKodoUploader->nInitItemCount = _nInitItemCount;

        int ret = allocDataQueueAndUploadMeta(pKodoUploader);
        if (ret != LINK_SUCCESS) {
                free(pKodoUploader);
                return ret;
        }
        
        ret = LinkNewCircleQueue(&pKodoUploader->pCommandQueue_, 0, TSQ_FIX_LENGTH, sizeof(TsUploaderCommand), 2);
        if (ret != 0) {
                LinkDestroyQueue(&pKodoUploader->pQueue_);
                free(pKodoUploader);
                free(pKodoUploader->pUpMeta);
                return ret;
        }
        
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
        uploadCommand.nCommandType = 2;
        pKodoUploader->pCommandQueue_->Push(pKodoUploader->pCommandQueue_, (char *)&uploadCommand, sizeof(int));
        
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
