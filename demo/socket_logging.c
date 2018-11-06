// Last Update:2018-11-06 17:10:56
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
#include "tsuploaderapi.h"
#include "socket_logging.h"
#include "queue.h"
#include "dbg.h"
#include "main.h"

#define BASIC() printf("[ %s %s() %d ] ", __FILE__, __FUNCTION__, __LINE__ )
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

int socket_init()
{
    struct sockaddr_in server;
    int ret = 0;
    static int first = 1;

    if ( first ) {
        gLogQueue = NewQueue();
        if ( !gLogQueue ) {
            printf("new queue error\n");
            return -1;
        }
        first = 0;
    }

    if ( gsock != -1 ) {
        close( gsock );
    }

    gsock = socket(AF_INET , SOCK_STREAM , 0);
    if (gsock == -1) {
        DBG_LOG("Could not create socket\b");
        gStatus.connecting = 0;
        return -1;
    }

    if ( !gIpc.config.serverIp ) {
        return -1;
    }

    server.sin_addr.s_addr = inet_addr( gIpc.config.serverIp );
    server.sin_family = AF_INET;
    DBG_LOG("gIpc.config.serverPort = %d\n", gIpc.config.serverPort );
    server.sin_port = htons( gIpc.config.serverPort );

    ret = connect(gsock , (struct sockaddr *)&server , sizeof(server));
    if ( ret < 0) {
        gStatus.connecting = 0;
        return -1;
    }

    gStatus.connecting = 1;
    gStatus.logStop = 0;
    printf("connet to %s:%d sucdefully, gsock = %d\n", gIpc.config.serverIp, gIpc.config.serverPort, gsock  );
    return 0;
}

void DbgSendFileName( char *logfile )
{
    char message[256] = { 0 };
    int ret = 0;

    sprintf( message, "%s.log", logfile );
    printf("%s %s %d send file name %s\n", __FILE__, __FUNCTION__, __LINE__, message );
    ret = send(gsock , message , strlen(message) , MSG_NOSIGNAL );// MSG_NOSIGNAL ignore SIGPIPE signal
    if(  ret < 0 ) {
        printf("%s %s %d Send failed, ret = %d, %s\n", __FILE__, __FUNCTION__, __LINE__,  ret, strerror(errno) );
    }

}

int log_send( char *message )
{
    if ( gLogQueue && gStatus.connecting && !gStatus.logStop ) {
        gLogQueue->enqueue( gLogQueue, message, strlen(message) );
    }

    return 0;
}

void ReportUploadStatistic(void *pUserOpaque, LinkUploadKind uploadKind, LinkUploadResult uploadResult)
{
    static int total = 0, error = 0;
    char message[512] = { 0 };
    char now[200] = { 0 };

    DBG_LOG("uploadKind = %d\n", uploadKind );
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

int GetCurrentTime( char *now_time )
{
    time_t now;
    struct tm *tm_now = NULL;

    time(&now);
    tm_now = localtime(&now);
    strftime( now_time, 200, "%Y-%m-%d %H:%M:%S", tm_now);

    return(0);
}

void *SocketLoggingTask( void *param )
{
    char log[1024] = { 0 };
    int ret = 0;

    for (;;) {
        if ( !gStatus.logStop && ( gIpc.config.logOutput == OUTPUT_SOCKET) ) {
            if ( gStatus.connecting ) {
                if ( gLogQueue ) {
                    memset( log, 0, sizeof(log) );
                    gLogQueue->dequeue( gLogQueue, log, NULL );
                } else {
                    printf("error, gLogQueue is NULL\n");
                    return NULL;
                }
                //printf("log = %s", log);
                ret = send(gsock , log , strlen(log) , MSG_NOSIGNAL );// MSG_NOSIGNAL ignore SIGPIPE signal
                if(  ret < 0 ) {
                    printf("Send failed, ret = %d, %s\n", ret, strerror(errno) );
                    gStatus.connecting = 0;
                    shutdown( gsock, SHUT_RDWR );
                    close( gsock );
                    gsock = -1;
                }
            } else {
                ret = socket_init();
                if ( ret < 0 ) {
                    sleep(5);
                    gStatus.retry_count ++;
                    printf("%s %s %d reconnect retry count %d\n", __FILE__, __FUNCTION__, __LINE__, gStatus.retry_count );
                    continue;
                }
                printf("%s %s %d reconnect to %s ok\n", __FILE__, __FUNCTION__, __LINE__,  gIpc.config.serverIp );
                gStatus.connecting = 1;
                gStatus.retry_count = 0;
                DbgSendFileName( gIpc.devId );
                printf("%s %s %d queue size = %d\n", __FILE__, __FUNCTION__, __LINE__, gLogQueue->getSize( gLogQueue )) ;
            }
        } else {
            sleep( 3 );
        }
    }
    return NULL;
}

void *SimpleSshTask( void *param )
{
    ssize_t ret = 0;
    char buffer[1024] = { 0 };
    int i = 0;

    for (;;) {
        if ( gStatus.connecting ) {
            memset( buffer, 0, sizeof(buffer) );
            //printf("%s %s %d gsock = %d\n", __FILE__, __FUNCTION__, __LINE__, gsock );
            ret = recv( gsock, buffer, 1024, 0 );
            if ( ret < 0 ) {
                if ( errno != 107 ) {
                    printf("recv error, errno = %d\n", errno );
                }
                sleep(5);
                gStatus.connecting = 0;
                printf("errno = %s\n", strerror(errno) );
                continue;
            } else if ( ret == 0 ){
                sleep(5);
                gStatus.connecting = 0;
                continue;
            }
            printf("buffer = %s", buffer );
            for ( i=0; i<ARRSZ(gCmds); i++ ) {
                char *res = NULL;
                //printf("buffer = %s\n", buffer );
                //printf("gCmds[i].cmd = %s\n", gCmds[i].cmd );
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
            ret = socket_init();
            if ( ret < 0 ) {
                gStatus.retry_count ++;
                //printf("%s %s %d reconnect retry count %d\n", __FILE__, __FUNCTION__, __LINE__, gStatus.retry_count );
                sleep(5);
                continue;
            }
            printf("%s %s %d reconnect to %s ok\n", __FILE__, __FUNCTION__, __LINE__,  gIpc.config.serverIp );
            gStatus.connecting = 1;
            gStatus.retry_count = 0;
            DbgSendFileName( gIpc.devId );
            printf("%s %s %d queue size = %d\n", __FILE__, __FUNCTION__, __LINE__, gLogQueue->getSize( gLogQueue )) ;
            sleep( 3 );
        }
    }
    return NULL;
}

void StartSocketDbgTask()
{
    static pthread_t log = 0, cmd;

    if ( !log ) {
        printf("%s %s %d start socket logging thread\n", __FILE__, __FUNCTION__, __LINE__);
        if ( gIpc.config.logOutput == OUTPUT_SOCKET)
            pthread_create( &log, NULL, SocketLoggingTask, NULL );
        if ( gIpc.config.simpleSshEnable )
            pthread_create( &cmd, NULL, SimpleSshTask, NULL );
    }
}

void CmdHnadleDump( char *param )
{
    char buffer[1024] = { 0 } ;
    int ret = 0;
    Config *pConfig = &gIpc.config;

    printf("%s %s %d get command dump\n", __FILE__, __FUNCTION__, __LINE__ );
    sprintf( buffer, "\n%s", "Config :\n" );
    sprintf( buffer+strlen(buffer), "logOutput = %d\n", pConfig->logOutput );
    sprintf( buffer+strlen(buffer), "logFile = %s\n", pConfig->logFile );
    sprintf( buffer+strlen(buffer), "movingDetection = %d\n", pConfig->movingDetection );
    sprintf( buffer+strlen(buffer), "gMovingDetect = %d\n", gIpc.detectMoving );
    sprintf( buffer+strlen(buffer), "gAudioType = %d\n", gIpc.audioType );
    sprintf( buffer+strlen(buffer), "queue = %d\n", gLogQueue->getSize( gLogQueue ) );
    sprintf( buffer+strlen(buffer), "logStop = %d\n", gStatus.logStop );
    ret = send(gsock , buffer , strlen(buffer) , MSG_NOSIGNAL );// MSG_NOSIGNAL ignore SIGPIPE signal
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

    sprintf( buffer, "version : %s, compile time : %s %s\n", gIpc.version,  __DATE__, __TIME__ );
    ret = send(gsock , buffer , strlen(buffer) , MSG_NOSIGNAL );// MSG_NOSIGNAL ignore SIGPIPE signal
    if(  ret < 0 ) {
        printf("Send failed, ret = %d, %s\n", ret, strerror(errno) );
    }
}

