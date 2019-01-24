#ifndef __TS_UPLOADER_H__
#define __TS_UPLOADER_H__

#include <pthread.h>
#include <errno.h>
#include "queue.h"
#include "base.h"

typedef void (*LinkTsUploadArgUpadater)(void *pOpaque, LinkSession *pSession,int64_t nNow, int64_t nEnd, int64_t nCurTsDuration);
typedef void (*LinkEndUploadCallback)(void *pOpaque, int64_t nTimestamp);

typedef struct _LinkTsUploadArg {
        LinkUploadParamCallback uploadParamCallback;
        void *pGetUploadParamCallbackArg;
        void    *pUploadArgKeeper_;
        int64_t nSegmentId_;
        LinkTsUploadArgUpadater UploadUpdateSegmentId;
        UploadStatisticCallback pUploadStatisticCb;
        void *pUploadStatArg;
        int64_t nSegSeqNum;
        int64_t nLastCheckTime;
}LinkTsUploadArg;

typedef struct _LinkReportTimeInfo {
        int64_t nAudioDuration;
        int64_t nVideoDuration;
        int64_t nMediaDuation;
        int64_t nSystimestamp;
}LinkReportTimeInfo;

typedef struct _LinkTsUploader LinkTsUploader;

enum LinkUploaderTimeInfoType {
        LINK_SEG_TIMESTAMP = 3,
        LINK_TS_START = 4,
        LINK_TS_END = 5,
};

typedef struct _LinkTsUploader{
        LinkUploadState (*GetUploaderState)(IN LinkTsUploader *pTsUploader);
        int(*Push)(IN LinkTsUploader *pTsUploader, IN const char * pData, int nDataLen);
        void (*GetStatInfo)(IN LinkTsUploader *pTsUploader, IN LinkUploaderStatInfo *pStatInfo);
        void (*ReportTimeInfo)(IN LinkTsUploader *pTsUploader, IN LinkReportTimeInfo *pTinfo, IN enum LinkUploaderTimeInfoType tmtype);
}LinkTsUploader;

int LinkNewTsUploader(OUT LinkTsUploader ** _pUploader, IN const LinkTsUploadArg *pArg, IN CircleQueuePolicy _policy, IN int _nMaxItemLen, IN int _nInitItemCount);
void LinkTsUploaderSetTsEndUploadCallback(IN LinkTsUploader * _pUploader, IN LinkEndUploadCallback cb, IN void *pOpaque);
void LinkDestroyTsUploader(IN OUT LinkTsUploader ** _pUploader);
void LinkAppendKeyframeMetaInfo(void *pOpaque, LinkKeyFrameMetaInfo *pMediaInfo);
void LinkSetSessionMeta(IN LinkTsUploader * _pUploader, LinkSessionMeta *pSessionMeta);
void LinkClearSessionMeta(IN LinkTsUploader * _pUploader);
int LinkTsUploaderPushPic(IN LinkTsUploader * _pUploader, LinkPicture pic);
void LinkTsUploaderSetPlanType(IN LinkTsUploader * _pUploader, LinkPlanType planType);

void LinkTsUploaderSetTsCallback(IN LinkTsUploader * _pUploader, IN LinkTsOutput, IN void * pUserArg, IN LinkMediaArg mediaArg);

#endif
