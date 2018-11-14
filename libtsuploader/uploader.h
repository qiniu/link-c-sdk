#ifndef __TS_UPLOADER_H__
#define __TS_UPLOADER_H__

#include <pthread.h>
#include <errno.h>
#include "queue.h"
#include "base.h"

typedef void (*LinkUploadArgUpadater)(void *pOpaque, void* pUploadArg, int64_t nNow, int64_t nEnd);

typedef struct _UploadArg {
        LinkGetUploadParamCallback getUploadParamCallback;
        void *pGetUploadParamCallbackArg;
        LinkUploadZone uploadZone;
        char    *pDeviceId_;
        void    *pUploadArgKeeper_;
        int64_t nSegmentId_;
        int64_t nLastUploadTsTime_;
        LinkUploadArgUpadater UploadSegmentIdUpadate;
        UploadStatisticCallback pUploadStatisticCb;
        void *pUploadStatArg;
}LinkUploadArg;

typedef struct _LinkTsUploader LinkTsUploader;
typedef int (*StreamUploadStart)(LinkTsUploader* pUploader);
typedef void (*StreamUploadStop)(LinkTsUploader*);

typedef struct _LinkTsUploader{
        StreamUploadStart UploadStart;
        StreamUploadStop UploadStop;
        LinkUploadState (*GetUploaderState)(IN LinkTsUploader *pTsUploader);
        void (*NotifyDataPrapared)(IN LinkTsUploader *pTsUploader);
        int(*Push)(IN LinkTsUploader *pTsUploader, IN const char * pData, int nDataLen);
        void (*GetStatInfo)(IN LinkTsUploader *pTsUploader, IN LinkUploaderStatInfo *pStatInfo);
        void (*RecordTimestamp)(IN LinkTsUploader *pTsUploader, IN int64_t nTimestamp);
}LinkTsUploader;

typedef void (*LinkTsStartUploadCallback)(void *pOpaque, int64_t nTimestamp);

int LinkNewUploader(OUT LinkTsUploader ** _pUploader, IN const LinkUploadArg *pArg, IN enum CircleQueuePolicy _policy, IN int _nMaxItemLen, IN int _nInitItemCount);
void LinkUploaderSetTsStartUploadCallback(IN LinkTsUploader * _pUploader, IN LinkTsStartUploadCallback cb, IN void *pOpaque);
void LinkDestroyUploader(IN OUT LinkTsUploader ** _pUploader);
void LinkAppendKeyframeMetaInfo(void *pOpaque, LinkKeyFrameMetaInfo *pMediaInfo);

#endif
