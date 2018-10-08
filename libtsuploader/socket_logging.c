// Last Update:2018-08-21 18:16:45
/**
 * @file socket_logging.c
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-08-16
 */
#include<stdio.h> //DBG_LOG
#include<string.h>    //strlen
#include<sys/socket.h>    //socket
#include<arpa/inet.h> //inet_addr
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include "socket_logging.h"
#include "devsdk.h"

#define BASIC() printf("[ %s %s() %d ] ", __FILE__, __FUNCTION__, __LINE__ )
#define DBG_ERROR(args...) BASIC();printf(args)
#define DBG_LOG(args...) BASIC();printf(args)

extern MediaStreamConfig gAjMediaStreamConfig;
static socket_status gStatus;

char *host = "39.107.247.14";
int port = 8089;
int gsock = 0;

int socket_init()
{
    struct sockaddr_in server;
    char message[1000] = { 0 } ;
    int ret = 0;

    if ( gsock != -1 ) {
        DBG_LOG("close last sock\n");
        close( gsock );
    }
    gsock = socket(AF_INET , SOCK_STREAM , 0);
    if (gsock == -1) {
        DBG_LOG("Could not create socket\b");
        gStatus.connecting = 0;
        return -1;
    }
    DBG_LOG("Socket created\n");

    server.sin_addr.s_addr = inet_addr( host );
    server.sin_family = AF_INET;
    server.sin_port = htons( port );

    ret = connect(gsock , (struct sockaddr *)&server , sizeof(server));
    if ( ret < 0) {
        DBG_ERROR("connect failed. Error, ret = %d, %s \n", ret , strerror(errno) );
        gStatus.connecting = 0;
        return -1;
    }


    gStatus.connecting = 1;
    sprintf( message, "tsupload_%s_%s.log", gAjMediaStreamConfig.rtmpConfig.streamid, gAjMediaStreamConfig.rtmpConfig.server );
    log_send( message );
    DBG_LOG("connet to %s:%d sucdefully\n", host, port  );
    return 0;
}

void *socket_reconnect_task( void * arg )
{
    int ret = 0;

    gStatus.retry_count = 0;

    while ( gStatus.retrying && !gStatus.connecting ) {

        ret = socket_init();
        if ( ret < 0 ) {
            sleep(5);
            gStatus.retry_count ++;
            DBG_LOG("reconnect retry count %d\n", gStatus.retry_count );
            continue;
        }
        gStatus.connecting = 1;
        gStatus.retrying = 0;
        DBG_LOG("reconnect to %s ok\n", host );
        return NULL;
    }
}

int log_send( char *message )
{
    int ret = 0;

    if ( gStatus.connecting ) {
        ret = send(gsock , message , strlen(message) , MSG_NOSIGNAL );// MSG_NOSIGNAL ignore SIGPIPE signal
        if(  ret < 0 ) {
            DBG_LOG("Send failed, ret = %d, %s\n", ret, strerror(errno) );
            gStatus.connecting = 0;
            return -1;
        }
    } else if ( !gStatus.retrying ) {
        pthread_t thread;

        gStatus.retrying = 1;
        DBG_LOG("start socket_reconnect_task\n");
        pthread_create( &thread, NULL, socket_reconnect_task, NULL );
    }

    return 0;
}

int report_status( int code )
{
    static int total = 0, error = 0;
    char message[512] = { 0 };
    char now[200] = { 0 };

    memset( message, 0, sizeof(message) );
    if ( code != 200 ) {
        error ++;
    }
    total++;
    memset( now, 0, sizeof(now) );
    get_current_time( now );
    sprintf( message, "[ %s ] [ %s ] [ %s ] upload ts [ total : %d ] [ error : %d ] [ percent : %%%f ]\n", 
             now,
             gAjMediaStreamConfig.rtmpConfig.streamid,
             gAjMediaStreamConfig.rtmpConfig.server,
             total, error, error*1.0/total*100 ); 
    log_send( message );

    return 0;
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

int get_current_time( char *now_time )
{
    time_t now;
    struct tm *tm_now = NULL;
    char mytime[200];

    time(&now);
    tm_now = localtime(&now);
    strftime( now_time, 200, "%Y-%m-%d %H:%M:%S", tm_now);

    return(0);
}



