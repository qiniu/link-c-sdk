// Last Update:2019-02-20 14:39:41
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
#include <pthread.h>
#include "md5.h"
#include "cfg.h"
#include "dbg.h"
#include "main.h"
#include "ota.h"
#include "httptools.h"
#include "queue.h"
#include "mqtt.h"

typedef struct {
    void *pMqttInstance;
    Queue *q;
    char ackTopic[32];
    int binSize;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} OtaInfo;

enum {
    OTA_EVENT_NONE,
    OTA_EVENT_ACK_TOPIC,
    OTA_EVENT_START,
    OTA_EVENT_PACKET,
    OTA_EVENT_END,
};

static OtaInfo gOtaInfo;

#define OTA_FILE_MAX_SIZE 3096*1024 // 3M
#define EXE_OTA_NAME "/tmp/gsbr-tsupload-ota"
#define EXE_FILE_NAME "/tmp/gsbr-tsupload"


int Download( const char *url, char *filename )
{
    char *buffer = ( char * ) malloc ( OTA_FILE_MAX_SIZE );// 3M
    int len = 0, ret = 0;
    FILE *fp = NULL;
    
    if ( !buffer ) {
        DBG_ERROR("malloc buffer error\n");
        return -1;
    }
    ret = LinkSimpleHttpGet( url, buffer, OTA_FILE_MAX_SIZE, &len ); 
    if ( LINK_SUCCESS != ret ) {
        DBG_ERROR("LinkSimpleHttpGet() error, ret = %d\n", ret );
        return -1;
    }

    if ( len <= 0 ) {
        DBG_ERROR("check length error, len = %d\n", len );
        return -1;
    }

    DBG_LOG("len = %d\n", len );
    fp = fopen( filename, "w+" );
    if ( !fp ) {
        DBG_ERROR("open file %s error\n", filename );
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
    DBG_LOG("start to download %s\n", versionFileUrl );
    ret = Download( versionFileUrl, versionFile );
    if ( ret != 0 ) {
        DBG_ERROR("get %s error, url : %s\n", versionFile, versionFileUrl );
        return -1;
    }

    DBG_LOG("get versionFile %s success,load it\n", versionFile );
    cfg = cfg_init();
    DBG_LOG("cfg = %p\n", cfg );
    if ( !cfg ) {
        DBG_ERROR("cfg is null\n");
        return -1;
    }
    if (cfg_load( cfg, versionFile ) < 0) {
        DBG_ERROR("Unable to load %s\n", versionFile );
        goto err;
    }

    DBG_LOG("start to parse the version number\n");
    version = cfg_get( cfg, version_key );
    if ( !version ) {
        DBG_ERROR("get version error\n");
        goto err;
    }

    DBG_LOG("the new version of remote is %s, current version is %s\n", version, gIpc.version );
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
    DBG_LOG("dump %s :\n", name);

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
        DBG_ERROR("cfg init error\n");
        return -1;
    }
    if (cfg_load( cfg, versionFile ) < 0) {
        DBG_ERROR("Unable to load %s\n", versionFile );
        goto err;
    }

    remoteMd5 = cfg_get( cfg, md5_key );
    if ( !remoteMd5 ) {
        DBG_ERROR("get remoteMd5 error\n");
        goto err;
    }

    DBG_LOG("the md5 of remote is %s\n", remoteMd5 );
    fp = fopen ( binFile, "rb");
    if (!fp) {
        DBG_ERROR( "can't open `%s': %s\n", binFile, strerror (errno));
        goto err;
    }
    md5_init (&ctx);
    while ( (n = fread (buffer, 1, sizeof buffer, fp)))
        md5_write (&ctx, buffer, n);
    if (ferror (fp))
    {
        DBG_ERROR( "error reading `%s': %s\n", binFile, strerror (errno));
        goto err;
    }
    md5_final (&ctx);
    fclose (fp);

    dump_buf( (char *)ctx.buf, 16, "ctx.buf" );
    for ( i=0; i<16; i++ ) {
        sprintf( str_md5 + strlen(str_md5), "%02x", ctx.buf[i] );
    }

    DBG_LOG("str_md5 = %s, remoteMd5 = %s\n", str_md5, remoteMd5 );
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

void * UpgradeTask( void *arg )
{
    int ret = 0;
    char *binFile = "/tmp/AlarmProxy";
    char *versionFile = "/tmp/version.txt";
    char *target = "/tmp/oem/app/AlarmProxy";
    char cmdBuf[256] = { 0 };
    char binUrl[1024] = { 0 };


    for (;;) {
        if ( !gIpc.config.ota_enable ) {
            DBG_LOG("ota function not enable\n");
            sleep( gIpc.config.ota_check_interval );
        }

        if ( !gIpc.config.ota_url ) {
            DBG_ERROR("OTA_URL not set, please modify /tmp/oem/app/ip.conf and add OTA_URL\n");
            sleep( gIpc.config.ota_check_interval );
            continue;
        }

        sprintf( binUrl, "%s/AlarmProxy", gIpc.config.ota_url );
        DBG_LOG("start upgrade process\n");
        ret = CheckUpdate( versionFile );
        if ( ret <= 0 ) {
            DBG_LOG("there is no new version in server\n");
            sleep( gIpc.config.ota_check_interval );
            continue;
        } 

        DBG_LOG("start to download %s\n", binUrl );
        ret = Download( binUrl, binFile );
        if ( ret < 0 ) {
            DBG_ERROR("download file %s, url : %s error\n", binFile, binUrl );
            sleep( 5 );
            continue;
        }

        ret = CheckMd5sum( versionFile, binFile );
        if ( ret <= 0 ) {
            DBG_ERROR("check md5 error\n");
            sleep( 5 );
            continue;
        }

        DBG_LOG("check the md5 of file %s ok\n", binFile);
        if ( access( target, R_OK ) == 0 ) {
            ret = remove( target );
            if ( ret != 0 ) {
                DBG_ERROR("remove file %s error\n", target );
                sleep( 5 );
                continue;
            }
            break;
        } else {
            break;
        }
    }

    DBG_LOG("copy %s to %s\n", binFile, target );
    sprintf( cmdBuf, "cp %s %s", binFile, target );
    system( cmdBuf );

    DBG_LOG("chmod +x %s\n", target );
    memset( cmdBuf, 0, sizeof(cmdBuf) );
    sprintf( cmdBuf, "chmod +x %s", target );
    system( cmdBuf );

    DBG_LOG("the ota update success!!!!\n");

    /* notify main thread to exit */
    gIpc.running = 0;

    return NULL;
}

static void OnMessage( const void* _pInstance, int _nAccountId, const char* _pTopic,
                const char* _pMessage, size_t nLength )
{
//    LOGI("[ thread id : %d ] get message topic %s message %s, nLength = %d\n",
//        (int)pthread_self(), _pTopic, _pMessage, (int)nLength );
//    printf("msg : %02d len : %03d\t", _pMessage[0], nLength );
    if ( _pInstance && 
         gOtaInfo.pMqttInstance &&
         _pInstance == gOtaInfo.pMqttInstance && 
         _pMessage &&
         nLength &&
         gOtaInfo.q) {
        gOtaInfo.q->enqueue( gOtaInfo.q, (void *)_pMessage, nLength );
    }
}

static void OnEvent(const void* _pInstance, int _nAccountId, int _nId,  const char* _pReason )
{
    if ( !_pInstance ) {
        LOGE("check param error\n");
        return;
    }

    LOGI("[ thread id : %d] id %d reason %s \n", (int)pthread_self(), _nId, _pReason );
    if ( _pInstance &&
         gOtaInfo.pMqttInstance &&
         _pInstance == gOtaInfo.pMqttInstance && 
         _nId == MQTT_SUCCESS ) {
        LOGI("subscribe topic %s\n", gIpc.config.mqttOtaTopic );
        LinkMqttSubscribe( _pInstance, gIpc.config.mqttOtaTopic );
    }

}

int Md5Sum( char *md5 )
{
    if ( !md5 ) {
        return -1;
    }

    FILE *fp = fopen ( EXE_OTA_NAME, "rb");
    if (!fp) {
        DBG_ERROR( "can't open `%s': %s\n", EXE_FILE_NAME, strerror (errno));
        goto err;
    }

    MD5_CONTEXT ctx;
    md5_init (&ctx);
    char buffer[1024] = { 0 };
    size_t n = 0;
    while ( (n = fread (buffer, 1, sizeof buffer, fp)))
        md5_write (&ctx, buffer, n);
    if (ferror (fp))
    {
        DBG_ERROR( "error reading `%s': %s\n", EXE_OTA_NAME, strerror (errno));
        goto err;
    }
    md5_final (&ctx);
    fclose (fp);

    char str_md5[40] = { 0 };
    int i = 0;
    dump_buf( (char *)ctx.buf, 16, "ctx.buf" );
    for ( i=0; i<16; i++ ) {
        sprintf( str_md5 + strlen(str_md5), "%02x", ctx.buf[i] );
    }

    DBG_LOG("str_md5 = %s, remoteMd5 = %s\n", str_md5, md5 );
    if ( memcmp( md5, str_md5, 32 ) == 0 ) {
        return 1;
    }

    return 0;
err:
    return -1;
}

int do_upgrade()
{

    char buf[256] = { 0 };

    gIpc.running = 0;
    LOGI("notify main process to exit\n");
    sprintf( buf,  "rm %s ; mv %s %s; chmod +x %s; %s ", EXE_FILE_NAME, EXE_OTA_NAME, EXE_FILE_NAME, EXE_FILE_NAME, EXE_FILE_NAME );
    LOGI("buf = %s\n", buf );
    system( buf );

    return 0;
}

void *OtaOverMqttTask( void *arg )
{
    char md5[40] = { 0 }; 
    FILE *fp = NULL;

    LinkMqttLibInit();

    gOtaInfo.q = NewQueue();
    if ( !gOtaInfo.q ) {
        LOGE("NewQueue() error\n");
        return NULL;
    }

    struct MqttOptions options, *ops = &options;
    char clientId[16] = { 0 };

    sprintf( clientId, "%s-ota", gIpc.config.client_id );

    memset( ops, 0, sizeof(struct MqttOptions) );
    ops->pId = clientId;
    ops->bCleanSession = false;
    ops->userInfo.nAuthenicatinMode = MQTT_AUTHENTICATION_NULL;
    ops->userInfo.pHostname = gIpc.config.mqtt_server;
    ops->userInfo.nPort = gIpc.config.mqtt_port;
    ops->userInfo.pCafile = NULL;
    ops->userInfo.pCertfile = NULL;
    ops->userInfo.pKeyfile = NULL;
    ops->nKeepalive = 15;
    ops->nQos = 1;
    ops->bRetain = false;
    ops->callbacks.OnMessage = &OnMessage;
    ops->callbacks.OnEvent = &OnEvent;
    gOtaInfo.pMqttInstance = LinkMqttCreateInstance( ops );
    if ( !gOtaInfo.pMqttInstance ) {
        LOGE("LinkMqttCreateInstance error\n");
        goto err;
    } else {
        LOGE("[ thread id : %d ] create mqtt instance success, client : %s, broker : %s port : %d topic : %s\n",
             (int)pthread_self(), clientId, gIpc.config.mqtt_server, gIpc.config.mqtt_port, gIpc.config.mqttOtaTopic );
    }

    for (;;) {
        char buf[1024] = { 0 };
        int size = 0;

        gOtaInfo.q->dequeue( gOtaInfo.q, buf, &size );

        char event = buf[0];
        switch( event ) {
        case OTA_EVENT_ACK_TOPIC:
            strncpy( gOtaInfo.ackTopic, &buf[2], sizeof(gOtaInfo.ackTopic) );
            LOGI("get OTA_EVENT_ACK_TOPIC, topic = %s\n", gOtaInfo.ackTopic );
            memset( buf, 0, sizeof(buf) );
            char *resp = "get topic ok";
            buf[0] = OTA_EVENT_ACK_TOPIC;
            buf[1] = strlen(resp);
            strncpy( &buf[2], resp, strlen(resp)  );
            LinkMqttPublish( gOtaInfo.pMqttInstance, gOtaInfo.ackTopic, 2+strlen(buf), buf );
            LOGI("send OTA_EVENT_ACK_TOPIC %s success\n", resp );
            break;
        case OTA_EVENT_START:
            {
                LOGI("get OTA_EVENT_START\n");
                LOGI("get OTA_EVENT_ACK_TOPIC, topic = %s\n", gOtaInfo.ackTopic );
                int size = 0;
                char *pBinSie = (char *)&size;
                int i = 0;
                for ( i=0; i<4; i++ ) {
                    *pBinSie++ = buf[i+1];
                }
                gOtaInfo.binSize = size;
                LOGI("get OTA_EVENT_ACK_TOPIC, topic = %s\n", gOtaInfo.ackTopic );
                LOGI("gOtaInfo.binSize = %d\n", gOtaInfo.binSize );
                strncpy( md5, &buf[6], 32 );
                LOGI("md5 = %s\n", md5 );
                fp = fopen( EXE_OTA_NAME, "w+" );
                if ( !fp ) {
                    LOGE("open file %s error\n", EXE_FILE_NAME );
                    break;
                }
                memset( buf, 0, sizeof(buf) );
                char *resp = "ota start ok";
                buf[0] = OTA_EVENT_ACK_TOPIC;
                buf[1] = strlen( resp );
                strncpy( &buf[2], resp, strlen(resp) );
                LOGI("send resp %s, topic : %s\n", resp, gOtaInfo.ackTopic );
                LinkMqttPublish( gOtaInfo.pMqttInstance, gOtaInfo.ackTopic, strlen(buf)+2, buf );
            }
            break;
        case OTA_EVENT_PACKET:
            if ( fp ) {
                int nPacketSize = 0, i = 0;
                char *pPacketSize = (char *)&nPacketSize;
                static int count = 0;

                count ++;
     //           printf("pkt : %03d\t", count );
                for ( i=0; i<4; i++ ) {
                   *pPacketSize++ = buf[i+1]; 
                }
     //           LOGI("nPacketSize = %d\n", nPacketSize );
                fwrite( &buf[5], 1, nPacketSize, fp );
            }
            break;
        case OTA_EVENT_END:
            LOGI("### OTA_EVENT_END\n");
            if ( fp ) {
                fclose( fp );
                fp = NULL;
            }

            if ( Md5Sum( md5 ) != 1 ) {
                LOGI("md5sum error\n");
                char *ack = "md5 check fail";
                buf[0] = OTA_EVENT_ACK_TOPIC;
                buf[1] = strlen(ack);
                strncpy( &buf[2], ack, strlen(ack) );
                LinkMqttPublish( gOtaInfo.pMqttInstance, gOtaInfo.ackTopic, strlen(ack)+2, buf );
                return NULL;
            }
            char *ack = "ota success";
            buf[0] = OTA_EVENT_ACK_TOPIC;
            buf[1] = strlen(ack);
            strncpy( &buf[2], ack, strlen(ack) );
            LinkMqttPublish( gOtaInfo.pMqttInstance, gOtaInfo.ackTopic, strlen(ack)+2, buf );
            do_upgrade();
            break;
        }
    }
    return NULL;
err:
    return NULL;
}

void StartUpgradeTask()
{
    pthread_t thread = 0;

    if ( gIpc.config.ota_enable && gIpc.config.otaMode ) {
        if ( strcmp( gIpc.config.otaMode, "ota-over-mqtt" ) == 0 ) {
            pthread_create( &thread, NULL, OtaOverMqttTask, NULL );
        } else {
            pthread_create( &thread, NULL, UpgradeTask, NULL );
        }
    }
}

