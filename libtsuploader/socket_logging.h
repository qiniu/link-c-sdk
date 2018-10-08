// Last Update:2018-08-21 17:15:10
/**
 * @file socket_logging.h
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-08-16
 */

#ifndef SOCKET_LOGGING_H
#define SOCKET_LOGGING_H

typedef struct {
    int connecting;
    int retrying;
    int retry_count;
} socket_status;

extern int log_send( char *message );
extern int log_send( char *message );
extern int report_status( int code );
extern int GetTimeDiff( struct timeval *_pStartTime, struct timeval *_pEndTime );
extern int get_current_time( char *now_time );

#endif  /*SOCKET_LOGGING_H*/
