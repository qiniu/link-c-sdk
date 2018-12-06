#ifndef __TS_UPLOADER_H__
#define __TS_UPLOADER_H__

#include <pthread.h>
#include <errno.h>
#include "queue.h"
#include "base.h"

typedef void (*LinkTsUploadArgUpadater)(void *pOpaque, void* pUploadArg, int64_t nNow, int64_t nEnd);

typedef struct _LinkTsUploadArg {
        LinkGetUploadParamCallback getUploadParamCallback;
        void *pGetUploadParamCallbackArg;
        void    *pUploadArgKeeper_;
        int64_t nSegmentId_;
        int64_t nLastUploadTsTime_;
        LinkTsUploadArgUpadater UploadSegmentIdUpadate;
        UploadStatisticCallback pUploadStatisticCb;
        void *pUploadStatArg;
        int64_t nSegSeqNum;
        int64_t nLastEndTsTime;
}LinkTsUploadArg;

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
        void (*RecordTimestamp)(IN LinkTsUploader *pTsUploader, IN int64_t nTimestamp, IN int64_t nSysNanotime);
}LinkTsUploader;

typedef void (*LinkTsStartUploadCallback)(void *pOpaque, int64_t nTimestamp);

int LinkNewTsUploader(OUT LinkTsUploader ** _pUploader, IN const LinkTsUploadArg *pArg, IN enum CircleQueuePolicy _policy, IN int _nMaxItemLen, IN int _nInitItemCount);
void LinkTsUploaderSetTsStartUploadCallback(IN LinkTsUploader * _pUploader, IN LinkTsStartUploadCallback cb, IN void *pOpaque);
void LinkDestroyTsUploader(IN OUT LinkTsUploader ** _pUploader);
void LinkAppendKeyframeMetaInfo(void *pOpaque, LinkKeyFrameMetaInfo *pMediaInfo);

#endif
