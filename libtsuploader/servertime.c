#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "base.h"
#include <time.h>
#include "httptools.h"

#define USE_CLOCK 1

#ifdef USE_CLOCK
static struct timespec tmResolution;
#endif

int uptimefd = -1;

static int64_t getUptime()
{
#ifdef USE_CLOCK
        struct timespec tp;
        clock_gettime(CLOCK_MONOTONIC, &tp);
        return (int64_t)(tp.tv_sec * 1000000000ll + tp.tv_nsec / tmResolution.tv_nsec);
#else
        char str[33];
        if(uptimefd < 0) {
                uptimefd = open("/proc/uptime", O_RDONLY);
        }
        int nReadLen = 0;
        if (uptimefd >= 0) {
                lseek(uptimefd, 0, SEEK_SET);
                nReadLen = read(uptimefd, str, sizeof(str));
        } else {
                return -1;
        }
        str[nReadLen - 1] = 0;
        char *pSpace = strchr(str, ' ');
        *pSpace = 0;
        return (int64_t)(atof(str) * 1000000000);
#endif
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
#ifdef USE_CLOCK
        clock_getres(CLOCK_MONOTONIC, &tmResolution);
#endif
        int ret = 0;
        ret = getTimeFromServer(&nServerTimestamp);
        nLocalupTimestamp = getUptime();
        return ret;
}
