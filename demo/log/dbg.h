// Last Update:2019-02-13 14:49:15
/**
 * @file dbg.h
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-09-03
 */

#ifndef DBG_H
#define DBG_H

#include <stdio.h>
#include "../tools/queue.h"

typedef enum {
    OUTPUT_NONE,
    OUTPUT_FILE,
    OUTPUT_SOCKET,
    OUTPUT_MQTT,
    OUTPUT_CONSOLE,
} OutputType;

enum {
    DBG_LEVEL_DEBUG,
    DBG_LEVEL_FATAL,
};


typedef struct {
    int output;
    char *logFile;
    int printTime;
    unsigned logLevel;
    int logVerbose;
    Queue *logQueue;
} Logger;

typedef struct {
    char *server;
    int port;
    char *user;
    char *passwd;
    char *topic;
    char *pClientId;
    int qos;
} MqttParam;

#define NONE                 "\e[0m"
#define BLACK                "\e[0;30m"
#define L_BLACK              "\e[1;30m"
#define RED                  "\e[0;31m"
#define L_RED                "\e[1;31m"
#define GREEN                "\e[0;32m"
#define L_GREEN              "\e[1;32m"
#define BROWN                "\e[0;33m"
#define YELLOW               "\e[1;33m"
#define BLUE                 "\e[0;34m"
#define L_BLUE               "\e[1;34m"
#define PURPLE               "\e[0;35m"
#define L_PURPLE             "\e[1;35m"
#define CYAN                 "\e[0;36m"
#define L_CYAN               "\e[1;36m"
#define GRAY                 "\e[0;37m"
#define WHITE                "\e[1;37m"

#define BOLD                 "\e[1m"
#define UNDERLINE            "\e[4m"
#define BLINK                "\e[5m"
#define REVERSE              "\e[7m"
#define HIDE                 "\e[8m"
#define CLEAR                "\e[2J"
#define CLRLINE              "\r\e[K" //or "\e[1K\r"

#define BUFFER_SIZE 1024

#define LOG(args...) dbg( DBG_LEVEL_DEBUG, __FILE_NAME__, __FUNCTION__, __LINE__, args )
#define DBG_ERROR(args...) dbg( DBG_LEVEL_FATAL, __FILE_NAME__, __FUNCTION__, __LINE__, args )
#define DBG_LOG(args...) LOG(args)
#define BASIC() printf("[ %s %s() +%d] ", __FILE_NAME__, __FUNCTION__, __LINE__ )
#define LOGE(args...) BASIC();printf(args)
#define LOGI(args...) BASIC();printf(args)

extern int LoggerInit( unsigned printTime, int output, char *pLogFile, int logVerbose, void *arg );
extern int dbg( unsigned logLevel, const char *file, const char *function, int line, const char *format, ...  );
extern int dbg2( const char *format, ...  );
extern void DbgReportLog( int stream, int streamType, char *reason );
extern void DbgTraceTimeStamp( int type, double _dTimeStamp, int stream );
extern void DbgReportLog( int stream, int streamType, char *reason );
extern int DbgGetMemUsed();
extern float DbgGetCpuUsage();
extern Logger gLogger;


#endif  /*DBG_H*/
