// Last Update:2018-11-20 16:27:45
/**
 * @file socket_logging.c
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-08-16
 */
#include<stdio.h> 
#include<string.h> 
#include<sys/socket.h>
#include<arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "uploader.h"
#include "log2tcp.h"
#include "queue.h"
#include "dbg.h"
#include "main.h"

//#define BASIC() printf("[ %s %s() %d ] ", __FILE__, __FUNCTION__, __LINE__ )
#define ARRSZ(arr) sizeof(arr)/sizeof(arr[0])


static socket_status gStatus;
static Queue *gLogQueue;

void CmdHnadleDump( char *param );
void CmdHnadleLogStop( char *param );
void CmdHnadleLogStart( char *param );
void CmdHnadleOutput( char *param );
void CmdHandleMovingDetection( char *param );
void CmdHnadleUpdateFrom( char *param );
void CmdHnadleHelp( char *param );
void CmdHnadleCache( char *param );
void CmdHandleGetVersion( char *param );
void* LogOverTcpTask( void *arg );

int gsock = 0;
static DemoCmd gCmds[] =
{
    { "dump", CmdHnadleDump },
    { "logstop", CmdHnadleLogStop },
    { "logstart", CmdHnadleLogStart },
    { "output", CmdHnadleOutput },
    { "moving", CmdHandleMovingDetection },
    { "updatefrom", CmdHnadleUpdateFrom },
    { "cache", CmdHnadleCache },
    { "remotehelp", CmdHnadleHelp },
    { "get-version", CmdHandleGetVersion }

};

void DbgSendFileName( char *logfile )
{
    char message[256] = { 0 };
    int ret = 0;
    int flags = 0;

#ifdef __APPLE__
    flags = SO_NOSIGPIPE;
#else
    flags = MSG_NOSIGNAL;
#endif

    sprintf( message, "%s.log", logfile );
    printf("%s %s %d send file name %s\n", __FILE__, __FUNCTION__, __LINE__, message );
    ret = send( gsock , message , strlen(message) , flags );// MSG_NOSIGNAL ignore SIGPIPE signal
    if(  ret < 0 ) {
        printf("%s %s %d Send failed, ret = %d, %s\n", __FILE__, __FUNCTION__, __LINE__,  ret, strerror(errno) );
    }

}

int SendLog( char *message )
{
    if ( gLogger.logQueue && gStatus.connected && !gStatus.logStop ) {
        gLogger.logQueue->enqueue( gLogger.logQueue, message, strlen(message) );
    }

    return 0;
}

void ReportUploadStatistic(void *pUserOpaque, LinkUploadKind uploadKind, LinkUploadResult uploadResult)
{
    static int total = 0, error = 0;
    char message[512] = { 0 };
    char now[200] = { 0 };

    //DBG_LOG("uploadKind = %d\n", uploadKind );
    if ( uploadKind == LINK_UPLOAD_TS ) {
        memset( message, 0, sizeof(message) );
        if ( uploadResult != LINK_UPLOAD_RESULT_OK ) {
            error ++;
        }
        total++;
        memset( now, 0, sizeof(now) );
        GetCurrentTime( now );
        sprintf( message, "[ %s ] [ %s ] [ cur : %s] [ total : %d ] [ error : %d ] [ percent : %%%f ]\n", 
                 now,
                 gIpc.devId,
                 "ts",
                 total, error, error*1.0/total*100 ); 
        DBG_LOG("%s", message );
    }

}

int GetTimeDiff( struct timeval *_pStartTime, struct timeval *_pEndTime )
{
    int time = 0;

    if ( _pEndTime->tv_sec < _pStartTime->tv_sec ) {
        return -1;
    }

    if ( _pEndTime->tv_usec < _pStartTime->tv_usec ) {
        time = (_pEndTime->tv_sec - 1 - _pStartTime->tv_sec) +
            ((1000000-_pStartTime->tv_usec) + _pEndTime->tv_usec)/1000000;
    } else {
        time = (_pEndTime->tv_sec - _pStartTime->tv_sec) +
            (_pEndTime->tv_usec - _pStartTime->tv_usec)/1000000;
    }

    return ( time );

}

int GetTimeDiffMs( struct timeval *_pStartTime, struct timeval *_pEndTime )
{
    int time = 0;

    if ( _pEndTime->tv_sec < _pStartTime->tv_sec ) {
        return -1;
    }

    if ( _pEndTime->tv_usec < _pStartTime->tv_usec ) {
        time = (_pEndTime->tv_sec - 1 - _pStartTime->tv_sec)*1000 +
            ((1000000-_pStartTime->tv_usec) + _pEndTime->tv_usec)/1000;
    } else {
        time = (_pEndTime->tv_sec - _pStartTime->tv_sec)*1000 +
            (_pEndTime->tv_usec - _pStartTime->tv_usec)/1000;
    }

    return ( time );

}

int GetCurrentTime( char *now_time )
{
    time_t now;
    struct tm *tm_now = NULL;

    time(&now);
    tm_now = localtime(&now);
    strftime( now_time, 200, "%Y-%m-%d %H:%M:%S", tm_now);

    return(0);
}

void *SimpleSshTask( void *param )
{
    ssize_t ret = 0;
    char buffer[1024] = { 0 };
    int i = 0;

    for (;;) {
        if ( gStatus.connected ) {
            memset( buffer, 0, sizeof(buffer) );
            ret = recv( gsock, buffer, 1024, 0 );
            if ( ret < 0 ) {
                if ( errno != 107 ) {
                    printf("recv error, errno = %d\n", errno );
                }
                sleep(5);
                printf("errno = %s\n", strerror(errno) );
                continue;
            } else if ( ret == 0 ){
                sleep(5);
                continue;
            }
            printf("buffer = %s", buffer );
            for ( i=0; i<ARRSZ(gCmds); i++ ) {
                char *res = NULL;
                res = strstr( buffer, gCmds[i].cmd );
                if ( res ) {
                    gCmds[i].pCmdHandle( buffer );
                    break;
                }
            }
            if ( i == ARRSZ(gCmds) ) {
                printf("unknow command %s", buffer );
            }

        } else {
            sleep( 3 );
        }
    }
    return NULL;
}

void StartSocketDbgTask()
{
    static pthread_t log = 0;

    if ( !log ) {
        DBG_LOG("%s %s %d start socket logging thread\n", __FILE__, __FUNCTION__, __LINE__);
        if ( gIpc.config.logOutput == OUTPUT_SOCKET)
            pthread_create( &log, NULL, LogOverTcpTask, NULL );
    }
}

void StartSimpleSshTask()
{
    static pthread_t thread = 0;

    if ( !thread ) {
        if ( gIpc.config.simpleSshEnable )
            pthread_create( &thread, NULL, SimpleSshTask, NULL );
    }
}

void CmdHnadleDump( char *param )
{
    char buffer[1024] = { 0 } ;
    int ret = 0;
    Config *pConfig = &gIpc.config;
    int flags = 0;

#ifdef __APPLE__
    flags = SO_NOSIGPIPE;
#else
    flags = MSG_NOSIGNAL;
#endif

    printf("%s %s %d get command dump\n", __FILE__, __FUNCTION__, __LINE__ );
    sprintf( buffer, "\n%s", "Config :\n" );
    sprintf( buffer+strlen(buffer), "logOutput = %d\n", pConfig->logOutput );
    sprintf( buffer+strlen(buffer), "logFile = %s\n", pConfig->logFile );
    sprintf( buffer+strlen(buffer), "movingDetection = %d\n", pConfig->movingDetection );
    sprintf( buffer+strlen(buffer), "gMovingDetect = %d\n", gIpc.detectMoving );
    sprintf( buffer+strlen(buffer), "gAudioType = %d\n", gIpc.audioType );
    sprintf( buffer+strlen(buffer), "queue = %d\n", gLogQueue->getSize( gLogQueue ) );
    sprintf( buffer+strlen(buffer), "tokenUrl = %s\n", pConfig->tokenUrl );
    sprintf( buffer+strlen(buffer), "renameTokenUrl = %s\n", pConfig->renameTokenUrl );
    sprintf( buffer+strlen(buffer), "cache = %d\n", pConfig->openCache );
    sprintf( buffer+strlen(buffer), "logStop = %d\n", gStatus.logStop );
    ret = send(gsock , buffer , strlen(buffer) , flags );// MSG_NOSIGNAL ignore SIGPIPE signal
    if(  ret < 0 ) {
        printf("Send failed, ret = %d, %s\n", ret, strerror(errno) );
    }

}

void CmdHnadleLogStop( char *param )
{
    printf("get command log stop\n");
    gStatus.logStop = 1;
}

void CmdHnadleLogStart( char *param )
{
    printf("get command log start\n");
    gStatus.logStop = 0;
}

void CmdHnadleOutput( char *param )
{
    char *p = NULL;
    int output = 0;
    static int last = 0;

    p = strchr( (char *)param, ' ');
    if ( !p ) {
        printf("error, p is NULL\n");
        return;
    }

    p++;
    if ( strcmp( p, "socket") == 0 ) {
        output = OUTPUT_SOCKET;
    } else if ( strcmp (p, "console") == 0 ) {
        output = OUTPUT_CONSOLE;
    } else if ( strcmp(p, "file") == 0 ) {
        output = OUTPUT_FILE;
    } else if ( strcmp(p, "mqtt") == 0 ) {
        output = OUTPUT_MQTT;
    } else {
        output = OUTPUT_SOCKET;
    }

    last = gIpc.config.logOutput;
    if ( last != output ) {
        printf("%s %s %d set the log output : %d\n", __FILE__, __FUNCTION__, __LINE__, output );
        gIpc.config.logOutput = output;
    }
}

void CmdHandleMovingDetection( char *param )
{
    char *p = NULL;

    p = strchr( (char *)param, ' ');
    if ( !p ) {
        printf("error, p is NULL\n");
        return;
    }

    p++;
    if ( strcmp( p, "1") == 0 ) {
        if ( gIpc.config.movingDetection != 1 ) {
            gIpc.config.movingDetection = 1;
            DBG_LOG("set moving detection enable\n");
        }
    } else {
        if ( gIpc.config.movingDetection != 0 ) {
            gIpc.config.movingDetection = 0;
            DBG_LOG("set moving detection disalbe\n");
        }
    }
}

void CmdHnadleUpdateFrom( char *param )
{
    char *p = NULL;

    p = strchr( (char *)param, ' ');
    if ( !p ) {
        printf("error, p is NULL\n");
        return;
    }

    p++;
    if ( strcmp( p, "socket") == 0 ) {
        gIpc.config.updateFrom = UPDATE_FROM_SOCKET;
    } else {
        gIpc.config.updateFrom = UPDATE_FROM_FILE;
    }
}

void CmdHnadleHelp( char *param )
{
    DBG_LOG("command list :\n"
            " dump       - dump the global variable\n"
            " logstop    - stop the log\n"
            " logstart   - stat the log\n"
            " output     - set the output type (socket/file/mqtt/console)\n"
            " moving     - moving detection open or close (0/1)\n"
            " updatefrom - gIpcConfig update from socket or file (socket/file)\n"
            " cache      - set open the cache (0/1)\n"
            " get-version- get version\n"
            " remotehelp - this help\n");
}

void CmdHnadleCache( char *param )
{
    char *p = NULL;

    p = strchr( (char *)param, ' ');
    if ( !p ) {
        printf("error, p is NULL\n");
        return;
    }

    p++;
    if ( strcmp( p, "1") == 0 ) {
        gIpc.config.openCache = 1;
    } else {
        gIpc.config.openCache = 0;
    }
}

void CmdHandleGetVersion( char *param )
{
    char buffer[1024] = { 0 };
    int ret = 0;
    int flags = 0;

    sprintf( buffer, "version : %s, compile time : %s %s\n", gIpc.version,  __DATE__, __TIME__ );
#ifdef __APPLE__
    flags = SO_NOSIGPIPE;
#else
    flags = MSG_NOSIGNAL;
#endif
    ret = send(gsock , buffer , strlen(buffer) , flags );// MSG_NOSIGNAL ignore SIGPIPE signal
    if(  ret < 0 ) {
        printf("Send failed, ret = %d, %s\n", ret, strerror(errno) );
    }
}

void* LogOverTcpTask( void *arg )
{
    struct sockaddr_in server;
    char log[1024] = { 0 };
    int ret = 0;
    int flags = 0, retry = 0;

#ifdef __APPLE__
        flags = SO_NOSIGPIPE;
#else
        flags = MSG_NOSIGNAL;
#endif

    for (;;) {
        if ( !gIpc.config.serverIp ) {
            LOGE("check config of server ip error\n");        
            return NULL;
        }
        if ( !gIpc.config.serverPort ) {
            LOGE("check server port error\n");
            return NULL;
        }

        if ( !gLogger.logQueue ) {
            LOGE("check logQueue error\n");
            return NULL;
        }

        gsock = socket( AF_INET , SOCK_STREAM , 0 );
        if ( gsock == -1 ) {
            LOGE("Could not create socket\b");
            return NULL;
        }

        server.sin_addr.s_addr = inet_addr( gIpc.config.serverIp );
        server.sin_family = AF_INET;
        server.sin_port = htons( gIpc.config.serverPort );

        ret = connect(gsock , (struct sockaddr *)&server , sizeof(server));
        if ( ret < 0 ) {
            LOGE("connect log server %s : %d error, retry\n", gIpc.config.serverIp, gIpc.config.serverPort );
            close( gsock );
            sleep( 3 );
            continue;
        }
        LOGI("connect log server %s : %d success\n", gIpc.config.serverIp, gIpc.config.serverPort );
        while( strlen(gIpc.devId) == 0 ) {
            LOGI("log over tcp wait for ipc init finished to get dev id...\n");
            usleep( 500 );
        }
        LOGI("get the dev id sucess, dev id is %s\n", gIpc.devId );
        DbgSendFileName( gIpc.devId );
        gStatus.connected = 1;

        for (;;) {
            if ( gLogger.logQueue ) {
                memset( log, 0, sizeof(log) );
                gLogger.logQueue->dequeue( gLogger.logQueue, log, NULL );
            } else {
                LOGE("error, gLogQueue is NULL\n");
                return NULL;
            }
            ret = send(gsock , log , strlen(log) , flags );// MSG_NOSIGNAL ignore SIGPIPE signal
            if(  ret < 0 ) {
                LOGE("Send failed, ret = %d, %s\n", ret, strerror(errno) );
                shutdown( gsock, SHUT_RDWR );
                close( gsock );
                retry = 0;
                gStatus.connected = 0;
                break;
            }
        }
        sleep( 5 );
        retry++;
        LOGI("connect to log tcp server retry = %d\n", retry );
    }

    return NULL;
}

