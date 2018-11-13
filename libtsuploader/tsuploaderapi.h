#ifndef __TS_UPLOADER_API__
#define __TS_UPLOADER_API__

#include "tsmuxuploader.h"
#include "log.h"
#include "base.h"

int LinkInitUploader();

int LinkCreateAndStartAVUploader(OUT LinkTsMuxUploader **pTsMuxUploader, IN LinkMediaArg *pAvArg, IN LinkUserUploadArg *pUserUploadArg);
int LinkCreateAndStartAll(OUT LinkTsMuxUploader **pTsMuxUploader, IN LinkMediaArg *pAvArg,
                          IN LinkUserUploadArg *pUserUploadArg, IN LinkPicUploadArg *pPicArg);
int LinkSendUploadPictureSingal(IN LinkTsMuxUploader *pTsMuxUploader, void *pOpaque, const char *pBuf, int nBuflen, enum LinkPicUploadType type);
void LinkSetSegmentUpdateInterval(IN LinkTsMuxUploader *pTsMuxUploader, int64_t nSeconds);
void LinkNotifyNomoreData(IN LinkTsMuxUploader *pTsMuxUploader);
int LinkPauseUpload(IN LinkTsMuxUploader *pTsMuxUploader);
int LinkResumeUpload(IN LinkTsMuxUploader *pTsMuxUploader);

int LinkSetTsTypeOneshot(IN LinkTsMuxUploader *pTsMuxUploader, IN const char *pType, IN int nTypeLen);
int LinkSetTsType(IN LinkTsMuxUploader *pTsMuxUploader, IN const char *pType, IN int nTypeLen);
void LinkClearTsType(IN LinkTsMuxUploader *pTsMuxUploader);

void LinkSetuploadZone(IN LinkTsMuxUploader *pTsMuxUploader, LinkUploadZone zone);

int LinkUpdateToken(IN LinkTsMuxUploader *pTsMuxUploader, IN const char * pToken, IN int nTokenLen);
void LinkSetUploadBufferSize(IN LinkTsMuxUploader *pTsMuxUploader, IN int nSize);
int LinkGetUploadBufferUsedSize(IN LinkTsMuxUploader *pTsMuxUploader);
void LinkSetNewSegmentInterval(IN LinkTsMuxUploader *pTsMuxUploader, IN int nIntervalSecond);
int LinkPushVideo(IN LinkTsMuxUploader *pTsMuxUploader, IN char * pData, IN int nDataLen, IN int64_t nTimestamp, IN int nIsKeyFrame, IN int nIsSegStart);
int LinkPushAudio(IN LinkTsMuxUploader *pTsMuxUploader, IN char * pData, IN int nDataLen, IN int64_t nTimestamp);
void LinkDestroyAVUploader(IN OUT LinkTsMuxUploader **pTsMuxUploader);
void LinkUninitUploader();


#endif
