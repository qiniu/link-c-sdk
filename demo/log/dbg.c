// Last Update:2019-02-18 17:00:15
/**
 * @file dbg.c
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-09-03
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "dbg.h"
#include "log2tcp.h"
#include "log2file.h"
#include "log2mqtt.h"
#include "stream.h"
#include "main.h"

Logger gLogger;

void SdkLogCallback(int nLogLevel, char *log )
{
//    dbg2( "%s", log );
}

int LoggerInit( unsigned printTime, int output, char *pLogFile, int logVerbose, void *arg )
{
    memset( &gLogger, 0, sizeof(gLogger) );

    gLogger.output = output;
    gLogger.logFile = pLogFile;
    gLogger.printTime = printTime;
    gLogger.logVerbose = logVerbose;
    gLogger.logQueue = NULL;

    LOGI("output = %d\n", output );
    switch( output ) {
    case OUTPUT_FILE:
        FileOpen( gLogger.logFile );
        break;
    case OUTPUT_SOCKET:
        gLogger.logQueue = NewQueue();
        StartSocketDbgTask();
        break;
    case OUTPUT_MQTT:
        if ( !arg ) {
            printf("%s check param error\n", __FUNCTION__ );
            return -1;
        }
        MqttParam *param = (MqttParam*)arg;
        MqttInit( param->pClientId, param->qos, param->user,
                  param->passwd, param->topic, param->server,param->port);
        break;
    case OUTPUT_CONSOLE:
    default:
        break;
    }
    
    LinkSetLogCallback( SdkLogCallback );
    return 0;
}


int dbg( unsigned logLevel, const char *file, const char *function, int line, const char *format, ...  )
{
    char buffer[BUFFER_SIZE] = { 0 };
    va_list arg;
    int len = 0;
    char now[200] = { 0 };

    if ( gLogger.printTime ) {
        memset( now, 0, sizeof(now) );
        GetCurrentTime( now );
        len = sprintf( buffer, "[ %s ] ", now );
    }

    if ( gLogger.logVerbose ) {
        len = sprintf( buffer+len, "[ %s %s +%d ] ", file, function, line );
    }

    va_start( arg, format );
    vsprintf( buffer+strlen(buffer), format, arg );
    va_end( arg );

    switch( gLogger.output ) {
    case OUTPUT_FILE:
        WriteLog( buffer ); 
        break;
    case OUTPUT_SOCKET:
        SendLog( buffer );
        break;
    case OUTPUT_MQTT:
        LogOverMQTT( buffer );
        break;
    case OUTPUT_CONSOLE:
        if ( logLevel == DBG_LEVEL_FATAL ) {
            printf( RED"%s"NONE, buffer );
        } else {
            printf( GRAY"%s"NONE, buffer );
        }
        break;
    default:
        break;
    }

    return 0;

}

void DbgTraceTimeStamp( int type, double _dTimeStamp, int stream )
{
    double duration = 0;
    char *pType = NULL;
    static double lastTimeStamp = 0, interval = 0;
    static struct timeval start = { 0, 0 }, end = { 0, 0 };
    char *pStream = NULL;

    if ( type == TYPE_VIDEO ) {
        pType = "video";
    } else {
        pType = "audio";
    }

    if ( stream  == STREAM_MAIN ) {
        pStream = "MAIN STREAM";
    } else {
        pStream = "SUB STREAM";
    }

    duration = _dTimeStamp - lastTimeStamp;
    gettimeofday( &end, NULL );
    interval = GetTimeDiff( &start, &end );
    if ( interval >= gIpc.config.timeStampPrintInterval ) {
#if 0
        DBG_LOG( "[ %s ] [ %s ] [ %s ] [ timestamp interval ] [ %f ]\n", 
                 gIpc.devId,
                 pStream,
                 pType,
                 duration );
#endif
        start = end;
    }
    lastTimeStamp = _dTimeStamp;
}

void DbgReportLog( int stream, int streamType, char *reason )
{
    static struct timeval start = { 0, 0 }, end = { 0, 0 };
    double interval = 0;
    char *pStreamStr = NULL;
    char *pStreamTypeStr = NULL;

    if ( stream == STREAM_MAIN ) {
       pStreamStr = "main stream"; 
    } else {
       pStreamStr = "sub stream"; 
    }

    if ( streamType == TYPE_VIDEO ) {
        pStreamTypeStr = "VIDEO";
    } else {
        pStreamTypeStr = "AUDIO";
    }

    gettimeofday( &end, NULL );
    interval = GetTimeDiff( &start, &end );
    if ( interval >= gIpc.config.timeStampPrintInterval ) {
#if 0
        DBG_LOG( "[ %s ] [ %s ] [ %s ] [ %s ]\n", 
                 pStreamTypeStr,
                 gIpc.devId,
                 pStreamStr,
                 reason
                 );
#endif
        start = end;
    }
}

int DbgGetMemUsed( char *memUsed )
{
    char line[256] = { 0 }, key[32] = { 0 }, value[32] = { 0 };
    FILE *fp = NULL;
    char *ret = NULL;

    fp = fopen( "/proc/self/status", "r" );
    if ( !fp ) {
        printf("open /proc/self/status error\n" );
        return -1;
    }

    for (;;) {
        memset( line, 0, sizeof(line) );
        ret = fgets( line, sizeof(line), fp );
        if (ret) {
            sscanf( line, "%s %s", key, value );
//            printf("key : %s, value : %s\n", key, value );
            if (strcmp( key, "VmRSS:" ) == 0 ) {
                memcpy( memUsed, value, strlen(value) );
                fclose( fp );
                return 0;
            }
        }
    }

    fclose( fp );
    return -1;
}

int *parser_result(const char *buf, int size){
    static int ret[10];
    int i, j = 0, start = 0;

    for(i=0; i<size; i++){
        char c = buf[i];
        if(c >= '0' && c <= '9'){
            if(!start){
                start = 1;
                ret[j] = c-'0';
            } else {
                ret[j] *= 10;
                ret[j] += c-'0';
            }
        } else if(c == '\n'){
            break;
        } else {
            if(start){
                j++;
                start = 0;
            }
        }
    }

    return ret;
}

float DbgGetCpuUsage()
{
    char buf[256];
    int size, *nums, prev_idle = 0, prev_total = 0, idle, total, i;
    static int fd = 0;

    if ( !fd )
        fd = open("/proc/stat", O_RDONLY);

    size = read(fd, buf, sizeof(buf));
    if(size <= 0)
        return 0;

    nums = parser_result(buf, size);

    idle=nums[3];

    for(i=0, total=0; i<10; i++){
        total += nums[i];
    }


    int diff_idle = idle-prev_idle;
    int diff_total = total-prev_total;
    float usage = (float)(((float)(1000*(diff_total-diff_idle))/(float)diff_total+5)/(float)10);
    fflush(stdout);

    prev_total = total;
    prev_idle = idle;

    sleep(3);
    lseek(fd, 0, SEEK_SET);

    return usage;
}

int dbg2( const char *format, ...  )
{
    char buffer[BUFFER_SIZE] = { 0 };
    va_list arg;
    int len = 0;
    char now[200] = { 0 };

    if ( gLogger.printTime ) {
        memset( now, 0, sizeof(now) );
        GetCurrentTime( now );
        len = sprintf( buffer, "[ %s ] ", now );
    }

    va_start( arg, format );
    vsprintf( buffer+strlen(buffer), format, arg );
    va_end( arg );

    switch( gLogger.output ) {
    case OUTPUT_FILE:
        WriteLog( buffer ); 
        break;
    case OUTPUT_SOCKET:
        SendLog( buffer );
        break;
    case OUTPUT_MQTT:
        LogOverMQTT( buffer );
        break;
    case OUTPUT_CONSOLE:
        printf( GRAY"%s"NONE, buffer );
        break;
    default:
        break;
    }

    return 0;

}

