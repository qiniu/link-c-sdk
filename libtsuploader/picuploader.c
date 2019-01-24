#include "picuploader.h"
#include "resource.h"
#include "queue.h"
#include "uploader.h"
#include <qupload.h>
#include "httptools.h"
 #include <unistd.h>

#define LINK_PIC_UPLOAD_MAX_FILENAME 256
enum LinkPicUploadSignalType {
        LinkPicUploadSignalIgnore = 0,
        LinkPicUploadSignalStop,
        LinkPicUploadGetPicSignalCallback,
        LinkPicUploadSignalUpload,
        LinkPicSetTsType,
        LinkPicSetTsOneShotType,
        LinkPicClearTsType,
        LinkPicSetPlanTpye,
};

typedef struct {
        LinkAsyncInterface asyncWait_;
        pthread_t workerId_;
        LinkCircleQueue *pSignalQueue_;
        int nQuit_;
        int tsType_; // may control pic upload. 0 notype. -1 oneshot type. 1 persistent type
        LinkPlanType planType_;
        int64_t nCount_;
        LinkPicUploadFullArg picUpSettings_;
}PicUploader;

typedef struct {
        LinkAsyncInterface asyncWait_;
        enum LinkPicUploadSignalType signalType_;
        LinkPlanType planType;
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

        return LINK_SUCCESS;
}

int LinkSendPictureToPictureUploader(PictureUploader *pPicUploader, LinkPicture pic) {
        
        PicUploader *pPicUp = (PicUploader *)pPicUploader;
        
        LinkPicUploadSignal sig;
        memset(&sig, 0, sizeof(LinkPicUploadSignal));
        
        sig.pPicUploader = pPicUp;
        sig.signalType_ = LinkPicUploadSignalUpload;
        
        sig.nDataLen = pic.nBuflen;
        sig.pFileName = (char *)pic.pFilename;
        sig.pData = (char *)pic.pBuf;
        int ret = pPicUp->pSignalQueue_->Push(pPicUp->pSignalQueue_, (char *)&sig, sizeof(LinkPicUploadSignal));
        if (ret <= 0) {
                LinkLogError("pic2 push queue error:%d", ret);
        }
        
        return LINK_SUCCESS;
}

static int linkPicSendTsTypeInfo(PicUploader *pPicUp, enum LinkPicUploadSignalType sigType, LinkPlanType ptype) {
        LinkPicUploadSignal sig;
        memset(&sig, 0, sizeof(LinkPicUploadSignal));
        
        sig.pPicUploader = pPicUp;
        sig.signalType_ = sigType;
        sig.planType = ptype;
        
        int ret = pPicUp->pSignalQueue_->Push(pPicUp->pSignalQueue_, (char *)&sig, sizeof(LinkPicUploadSignal));
        if (ret <= 0) {
                LinkLogError("pic push queue ts type(%d) error:%d",sigType, ret);
                return ret;
        }
        return LINK_SUCCESS;
}

int LinkPicSendTsType(PictureUploader *pPicUploader, int isOneShot) {
        PicUploader *pPicUp = (PicUploader *)pPicUploader;
        enum LinkPicUploadSignalType sigType = LinkPicSetTsType;
        if (isOneShot)
                sigType = LinkPicSetTsOneShotType;
        return linkPicSendTsTypeInfo(pPicUp, sigType, LINK_PLAN_TYPE_NONE);
}

int LinkPicSendClearTsType(PictureUploader *pPicUploader) {
        PicUploader *pPicUp = (PicUploader *)pPicUploader;
        return linkPicSendTsTypeInfo(pPicUp, LinkPicClearTsType, LINK_PLAN_TYPE_NONE);
}

int LinkPicSetPlanType(PictureUploader *pPicUploader, LinkPlanType ptype) {
        PicUploader *pPicUp = (PicUploader *)pPicUploader;
        return linkPicSendTsTypeInfo(pPicUp, LinkPicSetPlanTpye, ptype);
}

static void * listenPicUpload(void *_pOpaque)
{
        PicUploader *pPicUploader = (PicUploader *)_pOpaque;
        LinkUploaderStatInfo info;
        while(!pPicUploader->nQuit_ || info.nLen_ != 0) {
                LinkPicUploadSignal sig;
                int ret = pPicUploader->pSignalQueue_->PopWithTimeout(pPicUploader->pSignalQueue_, (char *)(&sig),
                                                                       sizeof(LinkPicUploadSignal), 24 * 60 * 60 * 1000000LL);
                memset(&info, 0, sizeof(info));
                pPicUploader->pSignalQueue_->GetStatInfo(pPicUploader->pSignalQueue_, &info);
                LinkLogDebug("----->pu receive a signal:%d %d qlen:%d cmd:%d", sig.signalType_, ret, info.nLen_, sig.signalType_);
                if (ret <= 0) {
                        if (ret != LINK_TIMEOUT) {
                                LinkLogError("pic queue error. pop:%d", ret);
                        }
                        continue;
                }
                
                if (ret == sizeof(LinkPicUploadSignal)) {

                        switch (sig.signalType_) {
                                case LinkPicUploadSignalIgnore:
                                        LinkLogWarn("ignore signal");
                                        continue;
                                case LinkPicSetTsType:
                                        pPicUploader->tsType_ = 1;
                                        continue;
                                case LinkPicSetTsOneShotType:
                                        pPicUploader->tsType_ = -1;
                                         continue;
                                case LinkPicClearTsType:
                                        pPicUploader->tsType_ = -1;
                                         continue;
                                case LinkPicUploadSignalStop:
                                        return NULL;
                                case LinkPicUploadSignalUpload:
                                        uploadPicture(&sig);
                                        break;
                                case LinkPicSetPlanTpye:
                                        pPicUploader->planType_ = sig.planType;
                                case LinkPicUploadGetPicSignalCallback:
                                        if (pPicUploader->picUpSettings_.getPicCallback) {
                                                char key[64];
                                                memset(key, 0, sizeof(key));
                                                int keyLen = snprintf(key, sizeof(key), "%"PRId64".jpg", sig.nTimestamp);

                                                pPicUploader->picUpSettings_.getPicCallback(
                                                                                            pPicUploader->picUpSettings_.pGetPicCallbackOpaque,
                                                                                            key, keyLen);
                                               
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
        
        int ret = LinkNewCircleQueue(&pPicUploader->pSignalQueue_, 1, TSQ_FIX_LENGTH, sizeof(LinkPicUploadSignal) + sizeof(int), 64);
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

static void * uploadPicture(void *_pOpaque) {
        LinkPicUploadSignal *pSig = (LinkPicUploadSignal*)_pOpaque;
        
        if (pSig->pFileName == NULL) {
                LinkLogError("picuploader pFileName not exits");
                return NULL;
        }
        
        char key[160+LINK_MAX_DEVICE_NAME_LEN+LINK_MAX_APP_LEN] = {0};
        char *realKey = key;
        memset(key, 0, sizeof(key));
        
        char uptoken[1024] = {0};
        char upHost[192] = {0};
        LinkUploadParam param;
        memset(&param, 0, sizeof(param));
        param.pTokenBuf = uptoken;
        param.nTokenBufLen = sizeof(uptoken);
        param.pUpHost = upHost;
        param.nUpHostLen = sizeof(upHost);
        
        param.pFilePrefix = key;
        param.nFilePrefix = sizeof(key);
        
        int ret = 0;
        int isFirst = 0, tryCount = 2;
        while(tryCount-- > 0) {
                ret = pSig->pPicUploader->picUpSettings_.getUploadParamCallback(pSig->pPicUploader->picUpSettings_.pGetUploadParamCallbackOpaque,
                                                                                    &param, LINK_UPLOAD_CB_GETFRAMEPARAM);
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
                                goto END;
                        }
                } else {
                        if (!isFirst)
                                pSig->pPicUploader->nCount_++;
                        break;
                }
        }
        char *pFile = strrchr(pSig->pFileName, '/');
        if (pFile == NULL)
                pFile = pSig->pFileName;
        else
                pFile++;
        int keyLen = snprintf(key+param.nFilePrefix, sizeof(key) - param.nFilePrefix, "/frame/%s", pFile);
        keyLen += param.nFilePrefix;
        assert(keyLen < sizeof(key));

        LinkPutret putret;

        const char *cusMagics[4];
        int nCusMagics = 4;
        cusMagics[0]="x:start";
        cusMagics[1]= pFile;
        while (*pFile != '-' && *pFile != 0) {
                pFile++;
        }
        if (*pFile == 0) {
                LinkLogError("wrong picture name:%s", key);
                goto END;
        }
        
        *pFile++ = 0;
        cusMagics[2]="x:session";
        cusMagics[3]=pFile;
        while (*pFile != '.' && *pFile != 0) {
                pFile++;
        }
        if (*pFile == 0) {
                LinkLogError("wrong picture name:%s", key);
                goto END;
        }
        *pFile++ = 0;
        if(param.nFilePrefix == 0)
                realKey = NULL;
        else
                nCusMagics = 0;
        if (pSig->pPicUploader->planType_ == LINK_PLAN_TYPE_24 || pSig->pPicUploader->tsType_ != 0) {
                ret = LinkUploadBuffer(pSig->pData, pSig->nDataLen, upHost, uptoken, realKey, NULL, 0, cusMagics, nCusMagics, NULL, &putret);
                
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
                
                if (pSig->pPicUploader->tsType_ < 0)
                        pSig->pPicUploader->tsType_ = 0;
        } else {
                LinkLogDebug("not upload pic:%d %d",pSig->pPicUploader->planType_ == LINK_PLAN_TYPE_24, pSig->pPicUploader->tsType_);
        }
END:
        if (pSig->pFileName) {
                free(pSig->pFileName);
        }
 
        return NULL;
}

void LinkDestroyPictureUploader(PictureUploader **pPicUploader) {
        if (*pPicUploader == NULL)
                return;
        LinkPushFunction(*pPicUploader);
        *pPicUploader = NULL;
        return;
}
