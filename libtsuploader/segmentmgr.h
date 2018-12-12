#ifndef __SEGMENT_MGR__
#define __SEGMENT_MGR__
#include "base.h"

#define LINK_INVALIE_SEGMENT_HANDLE -1
typedef int SegmentHandle;


typedef struct {
        LinkUploadParamCallback getUploadParamCallback;
        void *pGetUploadParamCallbackArg;
        UploadStatisticCallback pUploadStatisticCb;
        void *pUploadStatArg;
}SegmentArg;

int LinkInitSegmentMgr();

int LinkNewSegmentHandle(OUT SegmentHandle *pSeg, IN const SegmentArg *pArg);
void LinkReleaseSegmentHandle(IN OUT SegmentHandle *pSeg);
int LinkUpdateSegment(IN SegmentHandle seg, IN const LinkSession *pSession);

void LinkUninitSegmentMgr();

#endif
