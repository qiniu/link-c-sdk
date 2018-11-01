#ifndef __TS_UPLOADER_H__
#define __TS_UPLOADER_H__

#include <pthread.h>
#include <errno.h>
#include "queue.h"
#include "base.h"

typedef void (*LinkUploadArgUpadater)(void *pOpaque, void* pUploadArg, int64_t nNow, int64_t nEnd);

typedef struct _UploadArg {
        char    *pToken_;
        LinkUploadZone uploadZone;
        char    *pDeviceId_;
        void    *pUploadArgKeeper_;
        int64_t nSegmentId_;
        int64_t nLastUploadTsTime_;
        LinkUploadArgUpadater UploadSegmentIdUpadate;
}LinkUploadArg;

typedef struct _LinkTsUploader LinkTsUploader;
typedef int (*StreamUploadStart)(LinkTsUploader* pUploader);
typedef void (*StreamUploadStop)(LinkTsUploader*);

typedef struct _LinkTsUploader{
        StreamUploadStart UploadStart;
        StreamUploadStop UploadStop;
        LinkUploadState (*GetUploaderState)(LinkTsUploader *pTsUploader);
        void (*NotifyDataPrapared)(LinkTsUploader *pTsUploader);
        int(*Push)(LinkTsUploader *pTsUploader, char * pData, int nDataLen);
        void (*GetStatInfo)(LinkTsUploader *pTsUploader, LinkUploaderStatInfo *pStatInfo);
        void (*RecordTimestamp)(LinkTsUploader *pTsUploader, int64_t nTimestamp);
}LinkTsUploader;

typedef void (*LinkTsStartUploadCallback)(void *pOpaque, int64_t nTimestamp);

int LinkNewUploader(LinkTsUploader ** _pUploader, LinkUploadArg *pArg, enum CircleQueuePolicy _policy, int _nMaxItemLen, int _nInitItemCount);
void LinkUploaderSetTsStartUploadCallback(LinkTsUploader * _pUploader, LinkTsStartUploadCallback cb, void *pOpaque);
void LinkDestroyUploader(LinkTsUploader ** _pUploader);

#endif
