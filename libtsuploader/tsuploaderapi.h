#ifndef __TS_UPLOADER_API__
#define __TS_UPLOADER_API__

#include "tsmuxuploader.h"
#include "log.h"
#include "base.h"

int LinkInitUploader();

int LinkCreateAndStartAVUploader(OUT LinkTsMuxUploader **pTsMuxUploader, IN LinkMediaArg *pAvArg, IN LinkUserUploadArg *pUserUploadArg);
int LinkCreateAndStartAVUploaderWithPictureUploader(OUT LinkTsMuxUploader **pTsMuxUploader, IN LinkMediaArg *pAvArg,
                                                    IN LinkUserUploadArg *pUserUploadArg, IN LinkPicUploadArg *pPicArg);
int LinkSendUploadPictureSingal(IN LinkTsMuxUploader *pTsMuxUploader, void *pOpaque, const char *pBuf, int nBuflen, enum LinkPicUploadType type);

void LinkSetuploadZone(LinkUploadZone zone);
LinkUploadZone LinkGetuploadZone();
int LinkUpdateToken(IN LinkTsMuxUploader *pTsMuxUploader, IN char * pToken, IN int nTokenLen);
void LinkSetUploadBufferSize(IN LinkTsMuxUploader *pTsMuxUploader, IN int nSize);
int LinkGetUploadBufferUsedSize(IN LinkTsMuxUploader *pTsMuxUploader);
void LinkSetNewSegmentInterval(IN LinkTsMuxUploader *pTsMuxUploader, IN int nIntervalSecond);
int LinkPushVideo(IN LinkTsMuxUploader *pTsMuxUploader, IN char * pData, IN int nDataLen, IN int64_t nTimestamp, IN int nIsKeyFrame, IN int nIsSegStart);
int LinkPushAudio(IN LinkTsMuxUploader *pTsMuxUploader, IN char * pData, IN int nDataLen, IN int64_t nTimestamp);
void LinkDestroyAVUploader(IN OUT LinkTsMuxUploader **pTsMuxUploader);
void LinkUninitUploader();


#endif
