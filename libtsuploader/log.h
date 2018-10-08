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


#define _s_l_(x) #x
#define _str_line_(x) _s_l_(x)
#define __STR_LINE__ _str_line_(__LINE__)

#define LINK_LOG_LEVEL_TRACE 1
#define LINK_LOG_LEVEL_DEBUG 2
#define LINK_LOG_LEVEL_INFO 3
#define LINK_LOG_LEVEL_WARN 4
#define LINK_LOG_LEVEL_ERROR 5

extern int nLogLevel;
typedef void (*LinkLogFunc)(IN int nLevel, IN char * pLog);

void LinkSetLogLevel(IN int nLevel);
void LinkSetLogCallback(IN LinkLogFunc f);
void LinkLog(IN int nLevel, IN char * pFmt, ...);

#define LinkLogTrace(fmt,...) \
        LinkLog(LINK_LOG_LEVEL_TRACE, __FILE__ ":" __STR_LINE__ "[T]: " fmt "\n", ##__VA_ARGS__)
#define LinkLogDebug(fmt,...) \
        LinkLog(LINK_LOG_LEVEL_DEBUG, __FILE__ ":" __STR_LINE__ "[D]: " fmt "\n", ##__VA_ARGS__)
#define LinkLogInfo(fmt,...) \
        LinkLog(LINK_LOG_LEVEL_INFO,  __FILE__ ":" __STR_LINE__ "[I]: " fmt "\n", ##__VA_ARGS__)
#define LinkLogWarn(fmt,...) \
        LinkLog(LINK_LOG_LEVEL_WARN,  __FILE__ ":" __STR_LINE__ "[W]: " fmt "\n", ##__VA_ARGS__)
#define LinkLogError(fmt,...) \
        LinkLog(LINK_LOG_LEVEL_ERROR, __FILE__ ":" __STR_LINE__ "[E]: " fmt "\n", ##__VA_ARGS__)


#endif
