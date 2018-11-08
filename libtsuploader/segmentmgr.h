#ifndef __SEGMENT_MGR__
#define __SEGMENT_MGR__
#include "base.h"

#define LINK_INVALIE_SEGMENT_HANDLE -1
typedef int SegmentHandle;


typedef struct {
        char *pDeviceId;
        int nDeviceIdLen;
        LinkGetUploadParamCallback getUploadParamCallback;
        void *pGetUploadParamCallbackArg;
        char *pMgrTokenRequestUrl;
        int nMgrTokenRequestUrlLen;
        UploadStatisticCallback pUploadStatisticCb;
        void *pUploadStatArg;
        int useHttps;
        int64_t nUpdateIntervalSeconds;
        LinkUploadZone uploadZone;
}SegmentArg;

int LinkInitSegmentMgr();

int LinkNewSegmentHandle(SegmentHandle *pSeg, SegmentArg *pArg);
void LinkSetSegmentUpdateInt(SegmentHandle seg, int64_t nSeconds);
void LinkSetSegmentUploadZone(SegmentHandle seg, LinkUploadZone upzone);
void LinkReleaseSegmentHandle(SegmentHandle *pSeg);
int LinkUpdateSegment(SegmentHandle seg, int64_t nStart, int64_t nEnd, int isRestart);

void LinkUninitSegmentMgr();

#endif
