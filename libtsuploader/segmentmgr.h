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
        const char *pMgrTokenRequestUrl;
        int nMgrTokenRequestUrlLen;
        UploadStatisticCallback pUploadStatisticCb;
        void *pUploadStatArg;
        int useHttps;
        int64_t nUpdateIntervalSeconds;
        LinkUploadZone uploadZone;
}SegmentArg;

int LinkInitSegmentMgr();

int LinkNewSegmentHandle(OUT SegmentHandle *pSeg, IN const SegmentArg *pArg);
void LinkSetSegmentUpdateInt(IN SegmentHandle seg, IN int64_t nSeconds);
void LinkSetSegmentUploadZone(IN SegmentHandle seg, IN LinkUploadZone upzone);
void LinkReleaseSegmentHandle(IN OUT SegmentHandle *pSeg);
int LinkUpdateSegment(IN SegmentHandle seg, IN int64_t nStart, IN int64_t nEnd, IN int isRestart);

void LinkUninitSegmentMgr();

#endif
