#ifndef __SEGMENT_MGR__
#define __SEGMENT_MGR__
#include "base.h"

typedef int SegmentHandle;

typedef int (*LinkSegmentGetTokenCallback)(IN void *pOpaque, OUT char *pBuf, IN int nBuflen);

typedef struct {
        char *pDeviceId;
        int nDeviceIdLen;
        LinkSegmentGetTokenCallback getTokenCallback;
        void *pGetTokenCallbackArg;
        char *pMgrTokenRequestUrl;
        int nMgrTokenRequestUrlLen;
        UploadStatisticCallback pUploadStatisticCb;
        void *pUploadStatArg;
        int useHttps;
        int64_t nUpdateIntervalSeconds;
}SegmentArg;

int LinkInitSegmentMgr();

int LinkNewSegmentHandle(SegmentHandle *pSeg, SegmentArg *pArg);
void LinkSetSegmentUpdateInt(SegmentHandle seg, int64_t nSeconds);
void LinkReleaseSegmentHandle(SegmentHandle *pSeg);
int LinkUpdateSegment(SegmentHandle seg, int64_t nStart, int64_t nEnd, int isRestart);

void LinkUninitSegmentMgr();

#endif
