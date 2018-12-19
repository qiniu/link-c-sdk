#include "picuploader.h"
#include "resource.h"
#include "queue.h"
#include "uploader.h"
#include "fixjson.h"
#include <qupload.h>
#include "httptools.h"
 #include <unistd.h>

#define LINK_PIC_UPLOAD_MAX_FILENAME 256
enum LinkPicUploadSignalType {
        LinkPicUploadSignalIgnore = 0,
        LinkPicUploadSignalStop,
        LinkPicUploadGetPicSignalCallback,
        LinkPicUploadSignalUpload
};

typedef struct {
        LinkAsyncInterface asyncWait_;
        pthread_t workerId_;
        LinkQueue *pSignalQueue_;
        int nQuit_;
        int64_t nCount_;
        LinkPicUploadFullArg picUpSettings_;
}PicUploader;

typedef struct {
        LinkAsyncInterface asyncWait_;
        enum LinkPicUploadSignalType signalType_;
        char *pData;
        int nDataLen;
        int64_t nTimestamp; //file name need
        pthread_t uploadPicThread;
        PicUploader *pPicUploader;
        char *pFileName;
}LinkPicUploadSignal;


static void * uploadPicture(void *_pOpaque);

int LinkSendItIsTimeToCaptureSignal(PictureUploader *pPicUploader, int64_t nSysTimestamp) {
        LinkPicUploadSignal sig;
        memset(&sig, 0, sizeof(LinkPicUploadSignal));
        sig.signalType_ = LinkPicUploadGetPicSignalCallback;
        sig.nTimestamp = nSysTimestamp;
       
        PicUploader *pPicUp = (PicUploader*)pPicUploader;
        return pPicUp->pSignalQueue_->Push(pPicUp->pSignalQueue_, (char *)&sig, sizeof(LinkPicUploadSignal));
}

int LinkSendUploadPictureToPictureUploader(PictureUploader *pPicUploader, const char *pFileName, int nFileNameLen, const char *pBuf, int nBuflen) {
        
        PicUploader *pPicUp = (PicUploader *)pPicUploader;
        
        LinkPicUploadSignal sig;
        memset(&sig, 0, sizeof(LinkPicUploadSignal));
        
        sig.pPicUploader = pPicUp;
        sig.signalType_ = LinkPicUploadSignalUpload;
        sig.nDataLen = nBuflen;
        sig.pFileName = (char *)malloc(nFileNameLen + 1 + nBuflen);
        if (sig.pFileName == NULL) {
                return LINK_NO_MEMORY;
        }
        memcpy(sig.pFileName, pFileName, nFileNameLen);
        sig.pFileName[nFileNameLen] = 0;
        sig.pData = sig.pFileName + nFileNameLen + 1;
        memcpy(sig.pData, pBuf, nBuflen);
        int ret = pPicUp->pSignalQueue_->Push(pPicUp->pSignalQueue_, (char *)&sig, sizeof(LinkPicUploadSignal));
        if (ret <= 0) {
                free(sig.pFileName);
                LinkLogError("pic push queue error:%d", ret);
        }

        return ret;
}

static void * listenPicUpload(void *_pOpaque)
{
        PicUploader *pPicUploader = (PicUploader *)_pOpaque;
        LinkQueueInfo info;
        while(!pPicUploader->nQuit_ || info.nCount != 0) {
                LinkPicUploadSignal sig;
                int ret = pPicUploader->pSignalQueue_->Pop(pPicUploader->pSignalQueue_, (char *)(&sig),
                                                                       sizeof(LinkPicUploadSignal), 24 * 60 * 60);
                fprintf(stderr, "----->pu receive a signal:%d %d\n", sig.signalType_, ret);
                memset(&info, 0, sizeof(info));
                pPicUploader->pSignalQueue_->GetInfo(pPicUploader->pSignalQueue_, &info);
                LinkLogDebug("pic queue:%d", info.nCount);
                if (ret <= 0) {
                        if (ret != LINK_TIMEOUT) {
                                LinkLogError("pic queue error. pop:%d", ret);
                        }
                        continue;
                }
                
                LinkPicUploadSignal *pUpInfo;
                if (ret == sizeof(LinkPicUploadSignal)) {

                        switch (sig.signalType_) {
                                case LinkPicUploadSignalIgnore:
                                        LinkLogWarn("ignore signal");
                                        continue;
                                case LinkPicUploadSignalStop:
                                        return NULL;
                                case LinkPicUploadSignalUpload:
                                        pUpInfo = (LinkPicUploadSignal*)malloc(sizeof(LinkPicUploadSignal));
                                        if (pUpInfo == NULL) {
                                                LinkLogWarn("upload picture:%"PRId64" no memory", sig.nTimestamp);
                                        } else {
                                                memcpy(pUpInfo, &sig, sizeof(sig));
                                                ret = pthread_create(&pUpInfo->uploadPicThread, NULL, uploadPicture, pUpInfo);
                                        }
                                
                                        break;
                                case LinkPicUploadGetPicSignalCallback:
                                        if (pPicUploader->picUpSettings_.getPicCallback) {
                                                LinkUploadParam param;
                                                memset(&param, 0, sizeof(param));
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
                                                int r = pPicUploader->picUpSettings_.getUploadParamCallback(pPicUploader->picUpSettings_.pGetUploadParamCallbackOpaque, &param, LINK_UPLOAD_CB_GETPARAM);
                                                if (r != LINK_SUCCESS) {
                                                        LinkLogError("getUploadParamCallback fail:%d", r);
                                                        break;
                                                }
                                                
                                                char key[160+LINK_MAX_DEVICE_NAME_LEN+LINK_MAX_APP_LEN] = {0};
                                                memset(key, 0, sizeof(key));
#ifdef LINK_USE_OLD_NAME
                                                snprintf(key, sizeof(key), "frame,%s,%"PRId64"_0.jpg", deviceName, sig.nTimestamp);
#else
                                                char *tmp = param.pFilePrefix;
                                                while(*tmp != 0) {
                                                        if (*tmp == '/')
                                                            *tmp = ',';
                                                        tmp++;
                                                }
                                                snprintf(key, sizeof(key), "%s,frame,%"PRId64"-%s.jpg", param.pFilePrefix, sig.nTimestamp,
                                                         param.sessionId);
#endif
                                                pPicUploader->picUpSettings_.getPicCallback(
                                                                                            pPicUploader->picUpSettings_.pGetPicCallbackOpaque,
                                                                                            key, strlen(key));
                                               
                                        }
                                        break;
                        }

                }
        }
        return NULL;
}

static int waitUploadMgrThread(void * _pOpaque) {
        PicUploader *p = (PicUploader*)_pOpaque;
        p->nQuit_ = 1;
        
        LinkPicUploadSignal sig;
        memset(&sig, 0, sizeof(sig));
        sig.signalType_ = LinkPicUploadSignalStop;
        
        int ret = 0;
        while(ret <= 0) {
                ret = p->pSignalQueue_->Push(p->pSignalQueue_, (char *)&sig, sizeof(LinkPicUploadSignal));
                if (ret <= 0) {
                        LinkLogError("pic queue error. notify quit:%d sleep 1 sec to retry", ret);
                        sleep(1);
                }
        }
        
        pthread_join(p->workerId_, NULL);
        LinkDestroyQueue(&p->pSignalQueue_);
        free(p);
        return 0;
}

int LinkNewPictureUploader(PictureUploader **_pPicUploader, LinkPicUploadFullArg *pArg) {
        PicUploader * pPicUploader = (PicUploader *) malloc(sizeof(PicUploader));
        if (pPicUploader == NULL) {
                return LINK_NO_MEMORY;
        }
        memset(pPicUploader, 0, sizeof(PicUploader));
        
        int ret = LinkNewCircleQueue(&pPicUploader->pSignalQueue_, sizeof(LinkPicUploadSignal) + sizeof(int), 50, LQP_NONE);
        if (ret != 0) {
                free(pPicUploader);
                return ret;
        }
        pPicUploader->asyncWait_.function = waitUploadMgrThread;
        pPicUploader->picUpSettings_ = *pArg;
        
        ret = pthread_create(&pPicUploader->workerId_, NULL, listenPicUpload, pPicUploader);
        if (ret != 0) {
                LinkDestroyQueue(&pPicUploader->pSignalQueue_);
                free(pPicUploader);
                return LINK_THREAD_ERROR;
        }
        *_pPicUploader = (PictureUploader *)pPicUploader;
        
        return LINK_SUCCESS;
}

static int waitUploadThread(void * _pOpaque) {
        LinkPicUploadSignal *pSig = (LinkPicUploadSignal*)_pOpaque;
        pthread_join(pSig->uploadPicThread, NULL);
        if (pSig->pFileName) {
                free(pSig->pFileName);
        }
        free(pSig);
        return 0;
}

static void * uploadPicture(void *_pOpaque) {
        LinkPicUploadSignal *pSig = (LinkPicUploadSignal*)_pOpaque;
        pSig->asyncWait_.function = waitUploadThread;
        
        // frame/ua/ts_start_timestamp/fragment_start_timestamp.jpeg
        char key[160+LINK_MAX_DEVICE_NAME_LEN+LINK_MAX_APP_LEN] = {0};
        memset(key, 0, sizeof(key));
        if (pSig->pFileName != NULL) {
                char *n = strrchr(pSig->pFileName, '/');
                
                char *r = pSig->pFileName;
                if (n != NULL) {
                        r = ++n;
                } else {
                        n = pSig->pFileName;
                }
                while(*r != 0) {
                        if (*r == ',') {
                                *r = '/';
                        }
                        r++;
                }
                snprintf(key, sizeof(key), "%s", n);
        } else {
                LinkLogError("picuploader pFileName not exits");
                return NULL;
        }
        
        char uptoken[1024] = {0};
        char upHost[192] = {0};
        LinkUploadParam param;
        memset(&param, 0, sizeof(param));
        param.pTokenBuf = uptoken;
        param.nTokenBufLen = sizeof(uptoken);
        param.pUpHost = upHost;
        param.nUpHostLen = sizeof(upHost);
        
        int ret = 0;
        int isFirst = 0, tryCount = 2;
        while(tryCount-- > 0) {
                ret = pSig->pPicUploader->picUpSettings_.getUploadParamCallback(pSig->pPicUploader->picUpSettings_.pGetUploadParamCallbackOpaque,
                                                                                    &param, LINK_UPLOAD_CB_GETPARAM);
                if (ret != LINK_SUCCESS) {
                        if (ret == LINK_BUFFER_IS_SMALL) {
                                LinkLogError("param buffer is too small. drop file:");
                        } else {
                                LinkLogError("not get param yet:%d", ret);
                        }
                        if (pSig->pPicUploader->nCount_ == 0) {
                                isFirst = 1;
                                pSig->pPicUploader->nCount_++;
                                LinkLogInfo("first pic upload. may wait get uptoken. sleep 3s");
                                sleep(3);
                        } else {
                                LinkPushFunction(pSig);
                                return NULL;
                        }
                } else {
                        if (!isFirst)
                                pSig->pPicUploader->nCount_++;
                        break;
                }
        }

        LinkPutret putret;

        ret = LinkUploadBuffer(pSig->pData, pSig->nDataLen, upHost, uptoken, key, NULL, 0, NULL, &putret);
        
        LinkUploadResult uploadResult = LINK_UPLOAD_RESULT_FAIL;
        
        if (ret != 0) { //http error
                LinkLogError("upload picture:%s errorcode=%d error:%s", key, ret, putret.error);
        } else {
                if (putret.code / 100 == 2) {
                        uploadResult = LINK_UPLOAD_RESULT_OK;
                        LinkLogDebug("upload picture: %s success", key);
                } else {
                        if (putret.body != NULL) {
                                LinkLogError("upload pic:%s httpcode=%d reqid:%s errmsg=%s",
                                             key, putret.code, putret.reqid, putret.body);
                        } else {
                                LinkLogError("upload pic:%s httpcode=%d reqid:%s errmsg={not receive response}",
                                             key, putret.code, putret.reqid);
                        }
                }
        }
        
        LinkFreePutret(&putret);
        
        
        if (pSig->pPicUploader->picUpSettings_.pUploadStatisticCb) {
                pSig->pPicUploader->picUpSettings_.pUploadStatisticCb(pSig->pPicUploader->picUpSettings_.pUploadStatArg, LINK_UPLOAD_PIC, uploadResult);
        }
        
        LinkPushFunction(pSig);
        
        return NULL;
}

void LinkDestroyPictureUploader(PictureUploader **pPicUploader) {
        if (*pPicUploader == NULL)
                return;
        LinkPushFunction(*pPicUploader);
        *pPicUploader = NULL;
        return;
}
