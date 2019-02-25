#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "base.h"
#include <time.h>
#include "httptools.h"

#define TRUST_LOCAL_TIME

static struct timespec tmResolution;

#ifdef TRUST_LOCAL_TIME
int64_t LinkGetCurrentNanosecond()
{
        struct timespec tp;
        clock_gettime(CLOCK_REALTIME, &tp);
        return (int64_t)(tp.tv_sec * 1000000000ll + tp.tv_nsec);
}

int64_t LinkGetCurrentMillisecond()
{
        struct timespec tp;
        clock_gettime(CLOCK_REALTIME, &tp);
        return ((int64_t)(tp.tv_sec * 1000000000ll + tp.tv_nsec))/1000000;
}

int LinkInitTime() {
        clock_getres(CLOCK_MONOTONIC, &tmResolution);
        
        return LINK_SUCCESS;
}
#else
static int64_t getUptime()
{
        struct timespec tp;
        clock_gettime(CLOCK_MONOTONIC, &tp);
        return (int64_t)(tp.tv_sec * 1000000000ll + tp.tv_nsec / tmResolution.tv_nsec);
}

static int64_t nServerTimestamp;
static int64_t nLocalupTimestamp;

struct ServerTime{
        char * pData;
        int nDataLen;
        int nCurlRet;
};

static size_t writeTime(void *pTimeStr, size_t size,  size_t nmemb,  void *pUserData) {
        struct ServerTime *pTime = (struct ServerTime *)pUserData;
        
        char *pTokenStart = strstr(pTimeStr, "\"timestamp\"");
        if (pTokenStart == NULL) {
                pTime->nCurlRet = -11;
                return 0;
        }
        pTokenStart += strlen("\"timestamp\"");
        while(!(*pTokenStart >= '1' && *pTokenStart <= '9')) {
                pTokenStart++;
        }
        
        char *pTokenEnd = pTokenStart+1;
        while(*pTokenEnd >= '0' && *pTokenEnd <= '9') {
                pTokenEnd++;
        }
        memcpy(pTime->pData, pTokenStart, pTokenEnd - pTokenStart);
        
        return size * nmemb;
}

static int getTimeFromServer(int64_t *pStime) {
        
        char respBuf[128];
        int nRespBufLen = sizeof(respBuf);
        int nRealRespLen = 0;
        int ret = LinkSimpleHttpGet("http://39.107.247.14:8086/timestamp", respBuf, nRespBufLen, &nRealRespLen);
        if (ret != LINK_SUCCESS){
                return ret;
        }
       
        char timeStr[16] = {0};
        struct ServerTime stime;
        stime.pData = timeStr;
        stime.nDataLen = sizeof(timeStr);
        stime.nCurlRet = 0;
        
        if ( writeTime(respBuf, nRealRespLen, 1, &stime) == 0) {
                LinkLogError("may response format:%s", respBuf);
                return LINK_JSON_FORMAT;
        }
        
        *pStime = (int64_t)atoll(stime.pData);

        return LINK_SUCCESS;
}

int64_t LinkGetCurrentNanosecond()
{
        int64_t nUptime = getUptime();
        return (nUptime - nLocalupTimestamp)+nServerTimestamp;
}

int LinkInitTime() {
        clock_getres(CLOCK_MONOTONIC, &tmResolution);

        int ret = 0;
        ret = getTimeFromServer(&nServerTimestamp);
        nLocalupTimestamp = getUptime();
        return ret;

}
#endif
