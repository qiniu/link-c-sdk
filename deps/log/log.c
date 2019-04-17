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
        char log_buf[MAX_LOG_LENGTH + 1] = { 0 };
        if (nLevel >= nLogLevel) {
                va_list ap;
                va_start(ap, pFmt);
#ifdef LOG_WITH_TIME
#include <time.h>
#include <string.h>
                char time_buf[64] = { 0 };
                time_t t = time(NULL);
                struct tm *tm_now = localtime(&t);
                strftime(time_buf, 64, "[%Y-%m-%d %H:%M:%S]", tm_now);
                int time_len = strlen(time_buf);
                strncpy(log_buf, time_buf, strlen(time_buf));
                vsnprintf(log_buf + time_len, (MAX_LOG_LENGTH - time_len), pFmt, ap);
#else
                vsnprintf(log_buf, MAX_LOG_LENGTH, pFmt, ap);
#endif
                va_end(ap);
                if (logFunc == NULL) {
                        printf("%s", log_buf);
                } else {
                        logFunc(nLevel, log_buf);
                }
        }
}

void DbgDumpBuf( int line, const char *func, char *buf, int size )
{
    int i = 0;
    int j = 0;

    if ( size > 192 )
        size = 192;

    printf("[ %s %d ] size : %d the buffer  is : \n[%02d] ",  func, line, size,  j++  );
    for ( i=0; i<size; i++ ) {
        printf("0x%02x, ", (unsigned char)buf[i] );
        if ( (i+1)%16 == 0 ) {
            printf("\n[%02d] ", j++ );
        }
    }
    printf("\n");
}
