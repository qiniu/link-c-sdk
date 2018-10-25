#ifndef __PIC_UPLOADER_API__
#define __PIC_UPLOADER_API__
#include "base.h"

LINK_DEFINE_HANDLE(PictureUploader);

enum LinkPicUploadType {
        LinkPicUploadTypeFile = 1,
        LinkPicUploadTypeBuffer
};

enum LinkGetPictureSyncMode {
        LinkGetPictureModeNone = -1,
        LinkGetPictureModeSync = 1,
        LinkGetPictureModeAsync = 2
};

typedef enum LinkGetPictureSyncMode (*LinkGetPictureCallback)(void *pOpaque, void *pSvaeWhenAsync, OUT char **pBuf, OUT int *pBufSize, OUT enum LinkPicUploadType *pType);
typedef int (*LinkGetPictureFreeCallback)(char *pBuf, int nNameBufSize);
typedef int (*LinkUploadPictureGetTokenCallback)(IN void *pOpaque, OUT char *pBuf, IN int nBuflen);

typedef struct {
        LinkGetPictureCallback getPicCallback;
        void *pGetPicCallbackOpaque;
        LinkGetPictureFreeCallback getPictureFreeCallback;
        LinkUploadPictureGetTokenCallback getTokenCallback;
        void *pGetTokenCallbackOpaque;
}LinkPicUploadFullArg;

int LinkNewPictureUploader(PictureUploader **pPicUploader, LinkPicUploadFullArg *pArg);

int LinkSendGetPictureSingalToPictureUploader(PictureUploader *pPicUploader, const char *pDeviceId, int nDeviceIdLen, int64_t nTimestamp);

//  getPicCallback return , should invoke this function to notify picuploader to upload picture
int LinkSendUploadPictureToPictureUploader(PictureUploader *pPicUploader, void *pOpaque, const char *pBuf, int nBuflen, enum LinkPicUploadType type);


void LinkSetGetPictureCallback(PictureUploader *pPicUploader, LinkGetPictureCallback cb, LinkGetPictureCallback freeCbArg);

void LinkDestroyPictureUploader(PictureUploader **pPicUploader);


#endif
