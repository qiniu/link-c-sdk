#ifndef __PIC_UPLOADER_API__
#define __PIC_UPLOADER_API__
#include "base.h"

LINK_DEFINE_HANDLE(PictureUploader);

enum LinkPicUploadType {
        LinkPicUploadTypeFile = 1,
        LinkPicUploadTypeBuffer
};

typedef int (*LinkGetPictureCallback)(void *pOpaque, OUT char **pBuf, OUT int *pBufSize, OUT enum LinkPicUploadType *pType);
typedef int (*LinkGetPictureFreeCallback)(char *pBuf, int nNameBufSize);
typedef int (*LinkUploadPictureGetTokenCallback)(IN void *pOpaque, OUT char *pBuf, IN int nBuflen);

typedef struct {
        LinkGetPictureCallback getPicCallback;
        void *pGetPicCallbackOpaque;
        LinkGetPictureFreeCallback getPictureFreeCallback;
        LinkUploadPictureGetTokenCallback getTokenCallback;
        void *pGetTokenCallbackOpaque;
}LinkPicUploadArg;

int LinkNewPictureUploader(PictureUploader **pPicUploader, LinkPicUploadArg *pArg);

int LinkSendGetPictureSingalToPictureUploader(PictureUploader *pPicUploader, const char *pDeviceId, int nDeviceIdLen, int64_t nTimestamp);

void LinkSetGetPictureCallback(PictureUploader *pPicUploader, LinkGetPictureCallback cb, LinkGetPictureCallback freeCbArg);

void LinkDestroyPictureUploader(PictureUploader **pPicUploader);


#endif
