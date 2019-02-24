#ifndef __PIC_UPLOADER_API__
#define __PIC_UPLOADER_API__
#include "base.h"

LINK_DEFINE_HANDLE(PictureUploader);



typedef void (*LinkGetPictureCallback)(void *pOpaque, const char *pFileName, int nFileNameLen);

typedef struct {
        LinkGetPictureCallback getPicCallback;
        void *pGetPicCallbackOpaque;
}LinkPicUploadArg;

typedef struct {
        LinkGetPictureCallback getPicCallback;
        void *pGetPicCallbackOpaque;
        LinkUploadParamCallback getUploadParamCallback;
        void *pGetUploadParamCallbackOpaque;
        UploadStatisticCallback pUploadStatisticCb;
        void *pUploadStatArg;
        int useHttps;
}LinkPicUploadFullArg;

typedef struct _LinkPicUploadParam {
        LinkUploadParamCallback getUploadParamCallback;
        UploadStatisticCallback pUploadStatisticCb;
        void *pParamOpaque;
        void *pStatOpauqe;
        int nRetCode;
}LinkPicUploadParam;

int LinkNewPictureUploader(PictureUploader **pPicUploader, LinkPicUploadFullArg *pArg);

int LinkSendItIsTimeToCaptureSignal(PictureUploader *pPicUploader, int64_t nSysTimestamp);

//  getPicCallback return , should invoke this function to notify picuploader to upload picture
int LinkSendUploadPictureToPictureUploader(PictureUploader *pPicUploader, const char *pFileName, int nFileNameLen, const char *pBuf, int nBuflen);
int LinkSendPictureToPictureUploader(PictureUploader *pPicUploader, LinkPicture pic);
int LinkUploadPicture(LinkPicture *pic, LinkPicUploadParam *upParam);
void LinkDestroyPictureUploader(PictureUploader **pPicUploader);


#endif
