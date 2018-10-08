#ifndef __TS_MUX_UPLOADER_H__
#define __TS_MUX_UPLOADER_H__
#include "base.h"
#include "uploader.h"
#include "resource.h"

typedef struct _LinkTsMuxUploader LinkTsMuxUploader;

#define LINK_AUDIO_FORMAT_G711A 1
#define LINK_AUDIO_FORMAT_G711U 2
#define LINK_VIDEO_FORMAT_H264 1
#define LINK_VIDEO_FORMAT_H265 2


typedef struct _LinkTsMuxUploader{
        int(*PushVideo)(LinkTsMuxUploader *pTsMuxUploader, char * pData, int nDataLen, int64_t nTimestamp, int nIsKeyFrame, int nIsSegStart);
        int(*PushAudio)(LinkTsMuxUploader *pTsMuxUploader, char * pData, int nDataLen, int64_t nTimestamp);
        int (*SetToken)(LinkTsMuxUploader*, char *, int);
        void (*SetUploaderBufferSize)(LinkTsMuxUploader*, int);
        int (*GetUploaderBufferUsedSize)(LinkTsMuxUploader*);
        void (*SetNewSegmentInterval)(LinkTsMuxUploader*, int);
}LinkTsMuxUploader;

int LinkNewTsMuxUploader(LinkTsMuxUploader **pTsMuxUploader, LinkMediaArg *pAvArg, LinkUserUploadArg *pUserUploadArg);
int LinkTsMuxUploaderStart(LinkTsMuxUploader *pTsMuxUploader);
void LinkDestroyTsMuxUploader(LinkTsMuxUploader **pTsMuxUploader);
#endif
