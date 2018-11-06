// Last Update:2018-11-06 16:42:26
/**
 * @file log2file.c
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-09-03
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>

static FILE *gFd = NULL;

int FileOpen( char *_pLogFile )
{
    gFd = fopen( _pLogFile, "w+" );
    if ( !gFd ) {
        printf("open file %s error\n", _pLogFile );
        return -1;
    } else {
        printf("open file %s ok\n", _pLogFile);
    }


    return 0;
}

int WriteLog( char *log )
{
    size_t ret = 0;

    ret = fwrite( log, strlen(log), 1, gFd );
    if ( ret != 1 ) {
        printf("error, ret != 1\n");
    } else {
//        printf("log = %s\n", log );
    }
    fflush( gFd );

    return 0;
}

