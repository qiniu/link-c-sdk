// Last Update:2018-11-20 12:48:14
/**
 * @file socket_logging.h
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-08-16
 */

#ifndef SOCKET_LOGGING_H
#define SOCKET_LOGGING_H

#include "uploader.h"
typedef struct {
    int connected;
    int logStop;
} socket_status;

typedef struct {
    char *cmd;
    void (*pCmdHandle)(char *param);
} DemoCmd;

extern int SendLog( char *message );
extern int report_status( int code, char *_pFileNmae );
extern int GetTimeDiff( struct timeval *_pStartTime, struct timeval *_pEndTime );
extern int GetCurrentTime( char *now_time );
extern void DbgSendFileName( char *logfile );
extern void StartSocketDbgTask();
extern void ReportUploadStatistic(void *pUserOpaque, LinkUploadKind uploadKind, LinkUploadResult uploadResult);
extern void StartSimpleSshTask();
extern int GetTimeDiffMs( struct timeval *_pStartTime, struct timeval *_pEndTime );

#endif  /*SOCKET_LOGGING_H*/
