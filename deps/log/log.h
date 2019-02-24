#ifndef __LINK_LOG_H__
#define __LINK_LOG_H__

#include <stdarg.h>
#include <stdio.h>

#ifndef OUT
#define OUT
#endif
#ifndef IN
#define IN
#endif

#define MAX_LOG_LENGTH 512

#define LOG_WITH_TIME

#define _s_l_(x) #x
#define _str_line_(x) _s_l_(x)
#define __STR_LINE__ _str_line_(__LINE__)

#define LINK_LOG_LEVEL_TRACE 1
#define LINK_LOG_LEVEL_DEBUG 2
#define LINK_LOG_LEVEL_INFO 3
#define LINK_LOG_LEVEL_WARN 4
#define LINK_LOG_LEVEL_ERROR 5

#ifndef __PROJECT__
#define __PROJECT__
#endif

#ifndef __FILE_NAME__
#define __FILE_NAME__
#define __FILE_LINE__
#else
#define __FILE_LINE__ ":" __STR_LINE__
#endif

extern int nLogLevel;
typedef void (*LinkLogFunc)(IN int nLevel, IN char * pLog);

void LinkSetLogLevel(IN int nLevel);
void LinkSetLogCallback(IN LinkLogFunc f);
void LinkLog(IN int nLevel, IN char * pFmt, ...);

#define LinkLogTrace(fmt,...) \
        LinkLog(LINK_LOG_LEVEL_TRACE, "[T]" __PROJECT__ __FILE_NAME__ __FILE_LINE__ " " fmt "\n", ##__VA_ARGS__)
#define LinkLogDebug(fmt,...) \
        LinkLog(LINK_LOG_LEVEL_DEBUG, "[D]" __PROJECT__ __FILE_NAME__ __FILE_LINE__ " " fmt "\n", ##__VA_ARGS__)
#define LinkLogInfo(fmt,...) \
        LinkLog(LINK_LOG_LEVEL_INFO,  "[I]" __PROJECT__ __FILE_NAME__ __FILE_LINE__ " " fmt "\n", ##__VA_ARGS__)
#define LinkLogWarn(fmt,...) \
        LinkLog(LINK_LOG_LEVEL_WARN,  "[W]" __PROJECT__ __FILE_NAME__ __FILE_LINE__ " " fmt "\n", ##__VA_ARGS__)
#define LinkLogError(fmt,...) \
        LinkLog(LINK_LOG_LEVEL_ERROR, "[E]" __PROJECT__ __FILE_NAME__ __FILE_LINE__ " " fmt "\n", ##__VA_ARGS__)

#endif //__LINK_LOG_H__

