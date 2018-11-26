// Last Update:2018-11-26 11:38:09
/**
 * @file ota.c
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-11-19
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include "md5.h"
#include "cfg.h"
#include "dbg.h"
#include "main.h"
#include "ota.h"
#include "httptools.h"

#define OTA_FILE_MAX_SIZE 3096*1024 // 3M

int Download( const char *url, char *filename )
{
    char *buffer = ( char * ) malloc ( OTA_FILE_MAX_SIZE );// 3M
    int len = 0, ret = 0;
    FILE *fp = NULL;
    
    if ( !buffer ) {
        LOGE("malloc buffer error\n");
        return -1;
    }
    ret = LinkSimpleHttpGet( url, buffer, OTA_FILE_MAX_SIZE, &len ); 
    if ( LINK_SUCCESS != ret ) {
        LOGE("LinkSimpleHttpGet() error, ret = %d\n", ret );
        return -1;
    }

    if ( len <= 0 ) {
        LOGE("check length error, len = %d\n", len );
        return -1;
    }

    LOGI("len = %d\n", len );
    fp = fopen( filename, "w+" );
    if ( !fp ) {
        LOGE("open file %s error\n", filename );
        return -1;
    }

    fwrite( buffer, len, 1, fp );
    fclose( fp );
    free( buffer );

    return 0;
}


int CheckUpdate( char *versionFile )
{
    int ret = 0;
    char *version = NULL;
    struct cfg_struct *cfg = NULL;
    char *version_key = "version_number";
    char versionFileUrl[512] = { 0 };

    sprintf( versionFileUrl, "%s/version.txt", gIpc.config.ota_url );
    LOGI("start to download %s\n", versionFileUrl );
    ret = Download( versionFileUrl, versionFile );
    if ( ret != 0 ) {
        LOGE("get %s error, url : %s\n", versionFile, versionFileUrl );
        return -1;
    }

    LOGI("get versionFile %s success,load it\n", versionFile );
    cfg = cfg_init();
    LOGI("cfg = %p\n", cfg );
    if ( !cfg ) {
        LOGE("cfg is null\n");
        return -1;
    }
    if (cfg_load( cfg, versionFile ) < 0) {
        LOGE("Unable to load %s\n", versionFile );
        goto err;
    }

    LOGI("start to parse the version number\n");
    version = cfg_get( cfg, version_key );
    if ( !version ) {
        LOGE("get version error\n");
        goto err;
    }

    LOGI("the new version of remote is %s, current version is %s\n", version, gIpc.version );
    if ( strncmp( version, gIpc.version, strlen(version) ) == 0 ) {
        cfg_free( cfg );
        return 0;
    }

    cfg_free( cfg );

    return 1;
err:
    cfg_free( cfg );
    return -1;
}

void dump_buf( char *buf, int len, char *name )
{
    int i = 0;
    LOGI("dump %s :\n", name);

    for ( i=0; i<len; i++ ) {
        printf("0x%02x ", buf[i] );
    }
    printf("\n");
}

int CheckMd5sum( char *versionFile, char *binFile )
{
    FILE *fp;
    unsigned char buffer[4096];
    size_t n;
    MD5_CONTEXT ctx;
    int i;
    char *remoteMd5 = NULL;
    struct cfg_struct *cfg;
    char *md5_key = "md5";
    char str_md5[16] = { 0 };

    cfg = cfg_init();
    if ( !cfg ) {
        LOGE("cfg init error\n");
        return -1;
    }
    if (cfg_load( cfg, versionFile ) < 0) {
        LOGE("Unable to load %s\n", versionFile );
        goto err;
    }

    remoteMd5 = cfg_get( cfg, md5_key );
    if ( !remoteMd5 ) {
        LOGE("get remoteMd5 error\n");
        goto err;
    }

    LOGI("the md5 of remote is %s\n", remoteMd5 );
    fp = fopen ( binFile, "rb");
    if (!fp) {
        LOGE( "can't open `%s': %s\n", binFile, strerror (errno));
        goto err;
    }
    md5_init (&ctx);
    while ( (n = fread (buffer, 1, sizeof buffer, fp)))
        md5_write (&ctx, buffer, n);
    if (ferror (fp))
    {
        LOGE( "error reading `%s': %s\n", binFile, strerror (errno));
        goto err;
    }
    md5_final (&ctx);
    fclose (fp);

    dump_buf( ctx.buf, 16, "ctx.buf" );
    for ( i=0; i<16; i++ ) {
        sprintf( str_md5 + strlen(str_md5), "%02x", ctx.buf[i] );
    }

    LOGI("str_md5 = %s\, remoteMd5 = %s\n", str_md5, remoteMd5 );
    if ( memcmp( remoteMd5, str_md5, 32 ) == 0 ) {
        free( cfg );
        return 1;
    }

    free( cfg );
    return 0;
err:
    free( cfg );
    return -1;
}

int StartUpgradeProcess()
{
    int ret = 0;
    char *binFile = "/tmp/AlarmProxy";
    char *versionFile = "/tmp/version.txt";
    int msgid = 0;
    msg_t msg;
    key_t key;
    char *keyFile = "/bin";
    char *target = "/tmp/oem/app/AlarmProxy";
    char cmdBuf[256] = { 0 };
    char binUrl[1024] = { 0 };


    for (;;) {
        if ( !gIpc.config.ota_url ) {
            LOGE("OTA_URL not set, please modify /tmp/oem/app/ip.conf and add OTA_URL\n");
            sleep( 5 );
            continue;
        }

        sprintf( binUrl, "%s/AlarmProxy", gIpc.config.ota_url );
        LOGI("start upgrade process\n");
        ret = CheckUpdate( versionFile );
        if ( ret <= 0 ) {
            LOGI("there is no new version in server\n");
            sleep( 10 );
            continue;
        } 

        LOGI("start to download %s\n", binUrl );
        ret = Download( binUrl, binFile );
        if ( ret < 0 ) {
            LOGE("download file %s, url : %s error\n", binFile, binUrl );
            sleep( 5 );
            continue;
        }

        ret = CheckMd5sum( versionFile, binFile );
        if ( ret <= 0 ) {
            LOGE("check md5 error\n");
            sleep( 5 );
            continue;
        }

        LOGI("check the md5 of file %s ok\n", binFile);
        if ( access( target, R_OK ) == 0 ) {
            ret = remove( target );
            if ( ret != 0 ) {
                LOGE("remove file %s error\n", target );
                sleep( 5 );
                continue;
            }
            break;
        }
    }

    LOGI("notify main process to exit\n");
    key = ftok( keyFile ,'6');
    msgid = msgget( key, O_RDONLY );
    if ( msgid < 0 ) {
        printf("msgid < 0 ");
        return 0;
    }
    /* notify main process to exit */
    msg.type = 2;
    msg.event = OTA_START_UPGRADE_EVENT;
    msgsnd( msgid, &msg, sizeof(msg)-sizeof(msg.type), 0 );

    LOGI("copy %s to %s\n", binFile, target );
    sprintf( cmdBuf, "cp %s %s", binFile, target );
    system( cmdBuf );

    LOGI("chmod +x %s\n", target );
    memset( cmdBuf, 0, sizeof(cmdBuf) );
    sprintf( cmdBuf, "chmod +x %s", target );
    system( cmdBuf );

    msgctl( msgid, IPC_RMID, NULL );

    LOGI("the ota update success!!!!\n");

    exit( 0 );
    return 0;
}

