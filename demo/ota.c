// Last Update:2018-11-22 18:41:30
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
#include <curl/curl.h>
#include <errno.h>
#include "md5.h"
#include "cfg.h"
#include "dbg.h"
#include "main.h"
#include "ota.h"


static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);

  return written;
}

int Download( const char *url, char *filename )
{
  CURL *curl_handle;
  static char *pagefilename = NULL;
  FILE *pagefile;
  CURLcode ret = 0;
  long retcode = 0;

  pagefilename = filename;
  curl_global_init(CURL_GLOBAL_ALL);

  /* init the curl session */
  curl_handle = curl_easy_init();
  if ( !curl_handle ) {
      LOGE("curl_easy_init error\n");
      return -1;
  }

  /* set URL to get here */
  curl_easy_setopt(curl_handle, CURLOPT_URL, url );

  /* Switch on full protocol/debug output while testing */
//  curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);

  /* disable progress meter, set to 0L to enable and disable debug output */
  curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);

  /* send all data to this function  */
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);

  /* open the file */
  pagefile = fopen(pagefilename, "wb");
  if (pagefile) {

    /* write the page body to this file handle */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pagefile);

    /* get it! */
    ret = curl_easy_perform(curl_handle);

    /* close the header file */
    fclose(pagefile);
  }

  /* cleanup curl stuff */
  curl_easy_cleanup(curl_handle);
  LOGI("ret = %d\n", ret );
  if ( ret != CURLE_OK ) {
      LOGE("ret = %d\n", ret );
      return -1;
  }
  ret = curl_easy_getinfo( curl_handle, CURLINFO_RESPONSE_CODE, &retcode );
  if ( ret == CURLE_OK ) {
      LOGI("retcode = %d\n", retcode );
      if ( retcode == 404 ) {
          LOGE("get the url return 404 error\n");
          return -1;
      }
  }

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
        return -1;
    }

    LOGI("start to parse the version number\n");
    version = cfg_get( cfg, version_key );
    if ( !version ) {
        LOGE("get version error\n");
        return -1;
    }

    LOGI("the new version of remote is %s\n", version);
    if ( strncmp( version, gIpc.version, strlen(version) ) == 0 ) {
        return 0;
    }

    cfg_free( cfg );

    return 1;
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
        return -1;
    }

    remoteMd5 = cfg_get( cfg, md5_key );
    if ( !remoteMd5 ) {
        LOGE("get remoteMd5 error\n");
        return -1;
    }

    LOGI("the md5 of remote is %s\n", remoteMd5 );
    fp = fopen ( binFile, "rb");
    if (!fp) {
        LOGE( "can't open `%s': %s\n", binFile, strerror (errno));
    }
    md5_init (&ctx);
    while ( (n = fread (buffer, 1, sizeof buffer, fp)))
        md5_write (&ctx, buffer, n);
    if (ferror (fp))
    {
        LOGE( "error reading `%s': %s\n", binFile, strerror (errno));
        exit (1);
    }
    md5_final (&ctx);
    fclose (fp);

    dump_buf( ctx.buf, 16, "ctx.buf" );
    for ( i=0; i<16; i++ ) {
        sprintf( str_md5 + strlen(str_md5), "%02x", ctx.buf[i] );
    }

    LOGI("str_md5 = %s\n", str_md5 );
    if ( memcmp( remoteMd5, str_md5, 32 ) == 0 ) {
        return 1;
    }

    return 0;
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

    LOGI("the ota update success!!!!\n");

    exit( 0 );
    return 0;
}

