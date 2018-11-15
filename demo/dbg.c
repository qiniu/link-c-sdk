// Last Update:2018-11-13 12:27:00
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

#include "dbg.h"
#include "socket_logging.h"
#include "log2file.h"
#include "stream.h"
#include "main.h"

static Logger gLogger;

void SdkLogCallback(int nLogLevel, char *log )
{
    DBG_LOG( log );
}

int LoggerInit( unsigned printTime, int output, char *pLogFile, int logVerbose )
{

    memset( &gLogger, 0, sizeof(gLogger) );

    gLogger.output = output;
    gLogger.logFile = pLogFile;
    gLogger.printTime = printTime;
    gLogger.logVerbose = logVerbose;

    printf("output = %d\n", output );
    switch( output ) {
    case OUTPUT_FILE:
        FileOpen( gLogger.logFile );
        break;
    case OUTPUT_SOCKET:
        StartSocketDbgTask();
        break;
    case OUTPUT_MQTT:
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
        log_send( buffer );
        break;
    case OUTPUT_MQTT:
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
        DBG_LOG( "[ %s ] [ %s ] [ %s ] [ timestamp interval ] [ %f ]\n", 
                 gIpc.devId,
                 pStream,
                 pType,
                 duration );
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
        DBG_LOG( "[ %s ] [ %s ] [ %s ] [ %s ]\n", 
                 pStreamTypeStr,
                 gIpc.devId,
                 pStreamStr,
                 reason
                 );
        start = end;
    }
}

int DbgGetMemUsed( char *memUsed )
{
    pid_t pid = 0;
    char buffer[1024] = { 0 };
    char line[256] = { 0 }, key[32] = { 0 }, value[32] = { 0 };
    FILE *fp = NULL;
    char *ret = NULL;

    pid = getpid();
    sprintf( buffer, "/proc/%d/status", pid );
    fp = fopen( buffer, "r" );
    if ( !fp ) {
        printf("open %s error\n", buffer );
        return -1;
    }

    for (;;) {
        ret = fgets( line, sizeof(line), fp );
        if (ret) {
            sscanf( line, "%s %s", key, value );
//            printf("key : %s, value : %s\n", key, value );
            if (strcmp( key, "VmRSS:" ) == 0 ) {
                memcpy( memUsed, value, strlen(value) );
                return 0;
            }
        }
    }

    return -1;
}

