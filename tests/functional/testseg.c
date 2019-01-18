
#include "segmentmgr.h"
#include "servertime.h"
#include <string.h>
#include <time.h>
#include "security.h"
#include <unistd.h>

static char segStoreToken[1024];
static char segStoreFnamePrefix[256];
static char *gpMgrTokenRequestUrl;
static int gnMgrTokenRequestUrlLen;
static int segGetUploadParamCallback(void *pOpaque, IN OUT LinkUploadParam *pParam, LinkUploadCbType cbtype) {
        LinkLogDebug("in segGetTokenCallback");
        memcpy(pParam->pTokenBuf, segStoreToken, strlen(segStoreToken));
        memcpy(pParam->pSegUrl, gpMgrTokenRequestUrl, gnMgrTokenRequestUrlLen);
        return strlen(segStoreToken);
        
}

void JustTestSegmentMgr(const char *pUpToken, const char *pMgrUrl) {
        /*
        int ret = LinkInitTime();
        assert(ret == LINK_SUCCESS);
        int nDeadline;
        ret = LinkGetUploadToken(segStoreToken, sizeof(segStoreToken), &nDeadline,
                                 segStoreFnamePrefix, sizeof(segStoreFnamePrefix), pUpToken, NULL, 0);
        if (ret != LINK_SUCCESS) {
                LinkLogError("LinkGetUploadToken fail:%d", ret);
                return;
        }
        LinkLogDebug("jseg token:%s\n", segStoreToken);
        
        ret = LinkInitSegmentMgr();
        if (ret != LINK_SUCCESS) {
                LinkLogError("LinkInitSegmentMgr fail:%d", ret);
                return;
        }
        
        SegmentHandle segHandle;
        SegmentArg arg;
        arg.getUploadParamCallback = segGetUploadParamCallback;
        arg.pGetUploadParamCallbackArg = NULL;
        gpMgrTokenRequestUrl = (char *)pMgrUrl;
        gnMgrTokenRequestUrlLen = strlen(pMgrUrl);
        arg.pUploadStatisticCb = NULL;
        arg.pUploadStatArg = NULL;
        ret = LinkNewSegmentHandle(&segHandle, &arg);
        if (ret != LINK_SUCCESS) {
                LinkLogError("LinkInitSegmentMgr fail:%d", ret);
                return;
        }
        
        int count = 5;
        struct timespec tp;
        
        clock_gettime(CLOCK_MONOTONIC, &tp);
        LinkSession session;
        memset(&session, 0, sizeof(session));
        session.nSessionStartTime = tp.tv_sec * 1000;
        int64_t basetime = session.nSessionStartTime;
        session.nTsDuration = basetime+5320;
        while(count) {
                LinkUpdateSegment(segHandle, &session);
                session.nTsDuration+=5222;
                count--;
                sleep(8);
        }
        */
}
