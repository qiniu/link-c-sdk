#include "log.h"

int nLogLevel = LINK_LOG_LEVEL_INFO;
LinkLogFunc logFunc = NULL;

void LinkSetLogCallback(LinkLogFunc f)
{
        logFunc = f;
}

void LinkSetLogLevel(int level)
{
        if (level >= LINK_LOG_LEVEL_TRACE && level <= LINK_LOG_LEVEL_ERROR)
                nLogLevel = level;
}


void LinkLog(int nLevel, char * pFmt, ...)
{
        if (nLevel >= nLogLevel){
                va_list ap;
                va_start(ap, pFmt);
                if (logFunc == NULL) {
                        vprintf(pFmt, ap);
                } else {
                        char logStr[513] = {0};
                        vsnprintf(logStr, sizeof(logStr), pFmt, ap);
                        logFunc(nLevel, logStr);
                }
                va_end(ap);
        }
}
