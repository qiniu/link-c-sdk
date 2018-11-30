#ifndef __TS_MUX_UPLOADER_H__
#define __TS_MUX_UPLOADER_H__
#include "base.h"
#include "uploader.h"
#include "resource.h"
#include "picuploader.h"

typedef struct _LinkTsMuxUploader LinkTsMuxUploader;

#define LINK_AUDIO_FORMAT_G711A 1
#define LINK_AUDIO_FORMAT_G711U 2
#define LINK_VIDEO_FORMAT_H264 1
#define LINK_VIDEO_FORMAT_H265 2


typedef struct _LinkTsMuxUploader{
        int(*PushVideo)(IN LinkTsMuxUploader *pTsMuxUploader, IN const char * pData, IN int nDataLen, IN int64_t nTimestamp, IN int nIsKeyFrame, IN int nIsSegStart);
        int(*PushAudio)(IN LinkTsMuxUploader *pTsMuxUploader, IN const char * pData, IN int nDataLen, IN int64_t nTimestamp);
        void (*SetUploaderBufferSize)(IN LinkTsMuxUploader* pTsMuxUploader, int);
        int (*GetUploaderBufferUsedSize)(IN LinkTsMuxUploader* pTsMuxUploader);
        void (*SetUpdateSegmentInterval)(IN LinkTsMuxUploader* pTsMuxUploader, IN int nInterval);
}LinkTsMuxUploader;

int LinkNewTsMuxUploader(OUT LinkTsMuxUploader **pTsMuxUploader, IN const LinkMediaArg *pAvArg, IN const LinkUserUploadArg *pUserUploadArg);
int LinkNewTsMuxUploaderWillPicAndSeg(OUT LinkTsMuxUploader **pTsMuxUploader, IN const LinkMediaArg *pAvArg,
                                      IN const LinkUserUploadArg *pUserUploadArg, IN const LinkPicUploadArg *pPicArg);
int LinkTsMuxUploaderStart(IN LinkTsMuxUploader *pTsMuxUploader);
void LinkDestroyTsMuxUploader(IN OUT LinkTsMuxUploader **pTsMuxUploader);
#endif
