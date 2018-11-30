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
        LinkGetUploadParamCallback getUploadParamCallback;
        void *pGetUploadParamCallbackOpaque;
        UploadStatisticCallback pUploadStatisticCb;
        void *pUploadStatArg;
        LinkUploadZone uploadZone;
        int useHttps;
}LinkPicUploadFullArg;

int LinkNewPictureUploader(PictureUploader **pPicUploader, LinkPicUploadFullArg *pArg);

int LinkSendGetPictureSingalToPictureUploader(PictureUploader *pPicUploader, const char *pDeviceId, int nDeviceIdLen, int64_t nTimestamp);

//  getPicCallback return , should invoke this function to notify picuploader to upload picture
int LinkSendUploadPictureToPictureUploader(PictureUploader *pPicUploader, const char *pFileName, int nFileNameLen, const char *pBuf, int nBuflen);

void LinkPicUploaderSetUploadZone(PictureUploader *pPicUploader, LinkUploadZone upzone);

void LinkDestroyPictureUploader(PictureUploader **pPicUploader);


#endif
