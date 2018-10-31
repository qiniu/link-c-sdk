// Last Update:2018-10-23 14:07:36
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
    int retry_count;
    int logStop;
} socket_status;

typedef struct {
    char *cmd;
    void (*pCmdHandle)(char *param);
} DemoCmd;

extern int log_send( char *message );
extern int log_send( char *message );
extern int report_status( int code, char *_pFileNmae );
extern int GetTimeDiff( struct timeval *_pStartTime, struct timeval *_pEndTime );
extern int GetCurrentTime( char *now_time );
extern void DbgSendFileName( char *logfile );
extern void StartSocketDbgTask();
extern int socket_init();

#endif  /*SOCKET_LOGGING_H*/
