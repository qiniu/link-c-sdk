#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <qiniu/io.h>
#include <qiniu/rs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "tsuploaderapi.h"
#include "localkey.h"
#include "main.h"
#include "log2file.h"
#include "dbg.h"
#include "cfg.h"
#include "socket_logging.h"
#include "queue.h"
#include "mymalloc.h"
#include "dev_core.h"
#include "stream.h"
#include "picuploader.h"
#include "ota.h"

/* global variable */
App gIpc;

int AlarmCallback( int alarm, void *data )
{
    static char lastPicName[256] = { 0 };

    if ( alarm == ALARM_MOTION_DETECT ) {
        //DBG_LOG("get event ALARM_MOTION_DETECT\n");
        gIpc.detectMoving = alarm;
    } else if ( alarm == ALARM_MOTION_DETECT_DISAPPEAR ) {
        //DBG_LOG("get event ALARM_MOTION_DETECT_DISAPPEAR\n");
        gIpc.detectMoving = alarm;
        if ( gIpc.stream[STREAM_MAIN].uploader )
            LinkNotifyNomoreData( gIpc.stream[STREAM_MAIN].uploader );
        if ( gIpc.stream[STREAM_SUB].uploader ) {
            LinkNotifyNomoreData( gIpc.stream[STREAM_SUB].uploader );
        }
    } else if ( alarm == ALARM_JPEG_CAPTURED ) {
        char *file = (char *) malloc(  strlen((char *)data)+1 );
        //DBG_LOG( "data = %s\n", (char *)data );
        /*
         * sometimes the ipc will notify twice of one picture
         * at this moment, demo will notify sdk twice
         * inside LinkSendUploadPictureSingal will free the pointer pOpaque
         * so it will cause double free
         * here we will check the notify
         * if the notify is same as the last
         * ignore it
         * */
        if ( strncmp( (char *)data, lastPicName, strlen((char *)data) ) == 0 ) {
            DBG_LOG("this is the bug of ipc, we will ignore this notify, pic name is : %s\n", data );
            return 0;
        }

        memset( file, 0, strlen((char *)data)+1 );
        memcpy( file, (char *)data, strlen((char *)data) );
        memset( lastPicName, 0, sizeof(lastPicName) );
        memcpy( lastPicName, (char *)data, sizeof(lastPicName) );
        DBG_LOG("gIpc.stream[STREAM_MAIN].pOpaque = %p, file : %s \n", gIpc.stream[STREAM_MAIN].pOpaque, file );
        LinkSendUploadPictureSingal( gIpc.stream[STREAM_MAIN].uploader, gIpc.stream[STREAM_MAIN].pOpaque,
                                     file, strlen( (char *)data)+1, LinkPicUploadTypeFile );
    } else {
        /* do nothing */
    }

    return 0;
}

static int CaptureDevInit( )
{
    gIpc.dev = NewCoreDevice();
    gIpc.audioType = AUDIO_AAC;

    DBG_LOG("start to init ipc...\n");
    gIpc.dev->init( gIpc.audioType, gIpc.config.multiChannel, VideoGetFrameCb, AudioGetFrameCb );
    gIpc.dev->getDevId( gIpc.devId );
//    DbgSendFileName( gIpc.devId );
    gIpc.stream[STREAM_MAIN].videoCache = NewQueue();
    gIpc.stream[STREAM_MAIN].jpegQ = NewQueue();
    if ( gIpc.config.multiChannel ) {
        gIpc.stream[STREAM_SUB].videoCache = NewQueue();
    }
    if ( gIpc.dev->isAudioEnable() ) {
        gIpc.stream[STREAM_MAIN].audioCache = NewQueue();
        if ( gIpc.config.multiChannel ) {
            gIpc.stream[STREAM_SUB].audioCache = NewQueue();
        }
    } else {
        DBG_ERROR("audio not enabled\n");
    }

    if ( gIpc.dev->registerAlarmCb ) {
        gIpc.dev->registerAlarmCb( AlarmCallback );
    }

    return 0;
}

static int CaptureDevDeinit()
{
    gIpc.dev->deInit();

    return 0;
}

static enum LinkGetPictureSyncMode GetPicCallback ( void *pOpaque, void *pSvaeWhenAsync, 
                                                   OUT char **pBuf, OUT int *pBufSize,
                                                   OUT enum LinkPicUploadType *pType) 
{
    static unsigned int count = 0;
    char file[128] = { 0 };
    char *path = "/tmp";
    static struct timeval start = { 0, 0 }, end = { 0, 0 };
    int interval = 0;

    gettimeofday( &end, NULL );
    interval = GetTimeDiffMs( &start, &end );

    sprintf( file, "capture%d.jpeg", count++ );
    *pType = LinkPicUploadTypeFile;
    gIpc.stream[STREAM_MAIN].pOpaque = pSvaeWhenAsync;
    DBG_LOG("pSvaeWhenAsync = %p, file : %s interval = %d \n", pSvaeWhenAsync, file, interval  );
    gIpc.dev->captureJpeg( 0, 0, path, file );

    start = end;

    return LinkGetPictureModeAsync;
}

static int getPictureFreeCallback (char *pBuf, int nNameBufSize) 
{
    int ret = 0;
    char okfile[512] = { 0 };

    //DBG_LOG("pBuf = %s\n", pBuf );
    //DBG_LOG("pBuf address = %p\n", pBuf );
    if ( access( pBuf, R_OK ) == 0  ) {
        ret = remove( pBuf );
        if ( ret != 0 ) {
            DBG_ERROR("remove file %s error, ret = %d\n", pBuf, ret );
        }
    } else {
        DBG_ERROR("file %s not exist \n", pBuf );
    }

    sprintf( okfile, "%s.ok", pBuf );
    if ( access( okfile, R_OK ) == 0  ) {
        ret = remove( okfile );
        if ( ret != 0 ) {
            DBG_ERROR("remove file %s error, ret = %d\n", okfile, ret );
        }
    } else {
        DBG_ERROR("file %s not exist \n", okfile );
    }
    free( pBuf );
    return 0;
}

int TsUploaderSdkInit()
{
    int ret = 0, i=0;
    char url[1024] = { 0 }; 
    char *pUrl = NULL;
    LinkMediaArg mediaArg;
    LinkPicUploadArg arg;
    LinkUserUploadArg userUploadArg;

    memset(&userUploadArg, 0, sizeof(userUploadArg));

    arg.getPicCallback = GetPicCallback;
    arg.getPictureFreeCallback = getPictureFreeCallback;


    DBG_LOG("start to init ts uploader sdk \n");
    gIpc.version = "v00.00.08";
    if ( gIpc.audioType == AUDIO_AAC ) {
        mediaArg.nAudioFormat = LINK_AUDIO_AAC;
        mediaArg.nChannels = 1;
        mediaArg.nSamplerate = 16000;
    } else {
        mediaArg.nAudioFormat = LINK_AUDIO_PCMU;
        mediaArg.nChannels = 1;
        mediaArg.nSamplerate = 8000;
    }
    mediaArg.nVideoFormat = LINK_VIDEO_H264;

    LinkSetLogLevel(LINK_LOG_LEVEL_DEBUG);
    DBG_LOG("gIpc.config.ak = %s\n", gIpc.config.ak);
    DBG_LOG("gIpc.config.sk = %s\n", gIpc.config.sk);
    DBG_LOG("gIpc.config.bucketName = %s\n", gIpc.config.bucketName);

    if ( !gIpc.config.useLocalToken && !gIpc.config.tokenUrl ) {
        DBG_ERROR("token url not set, please modify /tmp/oem/app/ipc.conf and add token url\n");
        return -1;
    }

    printf("%s %s %d tokenUrl = %s\n", __FILE__, __FUNCTION__, __LINE__, gIpc.config.tokenUrl );
    if ( gIpc.config.tokenUrl )
        sprintf( url, "%s/%sa/token/upload?callback=false", gIpc.config.tokenUrl, gIpc.devId );
    DBG_LOG("url = %s\n", url );

    for ( i=0; i<gIpc.config.tokenRetryCount; i++ ) {
        if ( !gIpc.config.useLocalToken ) {
            pUrl = url;
        }
        DBG_LOG("pUrl = %s, i = %d\n", pUrl, i );
        ret = LinkGetUploadToken( gIpc.stream[STREAM_MAIN].token, sizeof(gIpc.stream[STREAM_MAIN].token), &userUploadArg.uploadZone_, pUrl );
        if ( ret != 0 ) {
            DBG_ERROR("%d GetUploadToken error, ret = %d, retry = %d\n", __LINE__, ret, i );
            sleep(2);
            continue;
        } else {
            break;
        }
    }

    if ( i == gIpc.config.tokenRetryCount ) {
        DBG_LOG( "GetUploadToken error, ret = %d\n", ret );
    }

    DBG_LOG("gIpc.devId= %s\n", gIpc.devId);

    ret = LinkInitUploader();
    if (ret != 0) {
        DBG_LOG("InitUploader error, ret = %d\n", ret );
        return ret;
    }

    sprintf( gIpc.stream[STREAM_MAIN].devId, "%s%s", gIpc.devId, "a" );
    sprintf( gIpc.stream[STREAM_SUB].devId, "%s%s", gIpc.devId, "b" );

    userUploadArg.pToken_ = gIpc.stream[STREAM_MAIN].token;
    userUploadArg.nTokenLen_ = strlen(gIpc.stream[STREAM_MAIN].token);
    userUploadArg.pDeviceId_ = gIpc.stream[STREAM_MAIN].devId;
    userUploadArg.nDeviceIdLen_ = strlen(gIpc.stream[STREAM_MAIN].devId);
    userUploadArg.nUploaderBufferSize = 512;
    userUploadArg.pUploadStatisticCb = ReportUploadStatistic;

    userUploadArg.pMgrTokenRequestUrl = gIpc.config.renameTokenUrl;
    if ( gIpc.config.renameTokenUrl )
        userUploadArg.pMgrTokenRequestUrl = gIpc.config.renameTokenUrl;
    else
        userUploadArg.nMgrTokenRequestUrlLen = 0;

    userUploadArg.useHttps = 0;

    ret = LinkCreateAndStartAll(&gIpc.stream[STREAM_MAIN].uploader, &mediaArg, &userUploadArg, &arg );
    if (ret != 0) {
        DBG_LOG("CreateAndStartAVUploader error, ret = %d\n", ret );
        return ret;
    }

    ret = LinkUpdateToken( gIpc.stream[STREAM_MAIN].uploader, gIpc.stream[STREAM_MAIN].token,
                          strlen(gIpc.stream[STREAM_MAIN].token));
    if (ret != 0) {
        DBG_ERROR("UpdateToken error, ret = %d\n", ret );
    }
    /* sub stream */
    if ( gIpc.config.multiChannel ) {
        memset( url, 0, sizeof(url) );
        sprintf( url, "%s/%sb/token/upload?callback=false", gIpc.config.tokenUrl, gIpc.devId );
        DBG_LOG("url = %s\n", url );

        if ( !gIpc.config.useLocalToken ) {
            pUrl = url;
        }

        for ( i=0; i<gIpc.config.tokenRetryCount; i++ ) {
            DBG_LOG("i = %d\n", i );
            ret = LinkGetUploadToken( gIpc.stream[STREAM_SUB].token, sizeof(gIpc.stream[STREAM_SUB].token), &userUploadArg.uploadZone_, pUrl );
            if ( ret != 0 ) {
                DBG_ERROR("%d GetUploadToken error, ret = %d, retry = %d\n", __LINE__, ret, i );
                continue;
            } else {
                break;
            }
        }

        userUploadArg.pDeviceId_ = gIpc.stream[STREAM_SUB].devId;
        userUploadArg.pToken_ = gIpc.stream[STREAM_SUB].token;
        userUploadArg.nTokenLen_ = strlen(gIpc.stream[STREAM_SUB].token);
        arg.getPicCallback = NULL;
        arg.getPictureFreeCallback = NULL;
        ret = LinkCreateAndStartAll(&gIpc.stream[STREAM_SUB].uploader, &mediaArg, &userUploadArg, &arg );
        if (ret != 0) {
            DBG_LOG("CreateAndStartAVUploader error, ret = %d\n", ret );
            return ret;
        }

        ret = LinkUpdateToken( gIpc.stream[STREAM_SUB].uploader, gIpc.stream[STREAM_SUB].token,
                               strlen(gIpc.stream[STREAM_SUB].token));
        if (ret != 0) {
            DBG_ERROR("%d UpdateToken error, __LINE__, ret = %d\n", ret );
        }
    }

    DBG_LOG("[ %s ] link ts uploader sdk init ok\n", gIpc.devId );
    return 0;
}

static void * UpadateToken() {
    int ret = 0;
    char url[1024] = { 0 };
    char *pUrl = NULL;

    if ( gIpc.config.ak )
        LinkSetAk( gIpc.config.ak );
    if ( gIpc.config.sk )
        LinkSetSk( gIpc.config.sk );

    if ( gIpc.config.bucketName )
        LinkSetBucketName( gIpc.config.bucketName );

    DBG_LOG("gIpc.config.ak = %s\n", gIpc.config.ak);
    DBG_LOG("gIpc.config.sk = %s\n", gIpc.config.sk);
    DBG_LOG("gIpc.config.bucketName = %s\n", gIpc.config.bucketName);

    while( 1 ) {
        memset( url, 0, sizeof(url) );
        if ( !gIpc.config.useLocalToken && !gIpc.config.tokenUrl ) {
            DBG_ERROR("token url net set, please modify /tmp/oem/app/ipc.conf and add token url\n");
            return NULL;
        }
        sprintf( url, "%s/%sa/token/upload?callback=false", gIpc.config.tokenUrl, gIpc.devId );
        DBG_LOG("url = %s\n", url );
        memset(gIpc.stream[STREAM_MAIN].token, 0, sizeof(gIpc.stream[STREAM_MAIN].token));
        if ( !gIpc.config.useLocalToken ) {
            pUrl = url;
        }
        ret = LinkGetUploadToken(gIpc.stream[STREAM_MAIN].token, sizeof(gIpc.stream[STREAM_MAIN].token), NULL, pUrl );
        if ( ret != 0 ) {
            DBG_ERROR("GetUploadToken error, ret = %d\n", ret );
            sleep(2);
            continue;
        }
        ret = LinkUpdateToken(gIpc.stream[STREAM_MAIN].uploader, gIpc.stream[STREAM_MAIN].token,
                              strlen(gIpc.stream[STREAM_MAIN].token));
        if (ret != 0) {
            DBG_ERROR("UpdateToken error, ret = %d\n", ret );
            sleep(2);
            continue;
        }

        if ( gIpc.config.multiChannel ) {
            memset( url, 0, sizeof(url) );
            sprintf( url, "%s/%sb/token/upload?callback=false", gIpc.config.tokenUrl, gIpc.devId );
            DBG_LOG("url = %s\n", url );
            memset( gIpc.stream[STREAM_SUB].token, 0, sizeof(gIpc.stream[STREAM_SUB].token));
            if ( !gIpc.config.useLocalToken ) {
                pUrl = url;
            }
            ret = LinkGetUploadToken( gIpc.stream[STREAM_SUB].token, sizeof(gIpc.stream[STREAM_SUB].token), NULL, pUrl );
            if ( ret != 0 ) {
                DBG_ERROR("GetUploadToken error, ret = %d\n", ret );
                sleep(2);
                continue;
            }
            ret = LinkUpdateToken(gIpc.stream[STREAM_SUB].uploader, gIpc.stream[STREAM_SUB].token,
                                  strlen(gIpc.stream[STREAM_SUB].token));
            if (ret != 0) {
                DBG_ERROR("UpdateToken error, ret = %d\n", ret );
                DBG_ERROR("gIpc.stream[STREAM_SUB].uploader = 0x%x\n", gIpc.stream[STREAM_SUB].uploader);
                DBG_ERROR("gIpc.stream[STREAM_SUB].token = %s\n", gIpc.stream[STREAM_SUB].token );
                sleep(2);
                continue;
            }
        }
        sleep( gIpc.config.tokenUploadInterval );// 59 minutes
    }
    return NULL;
}

int StartTokenUpdateTask()
{
    pthread_t updateTokenThread;
    pthread_attr_t attr;
    int ret = 0;

    pthread_attr_init ( &attr );
    pthread_attr_setdetachstate ( &attr, PTHREAD_CREATE_DETACHED );
    ret = pthread_create( &updateTokenThread, &attr, UpadateToken, NULL );
    if (ret != 0 ) {
        DBG_ERROR("create update token thread fail\n");
        return ret;
    }
    pthread_attr_destroy (&attr);

    return 0;
}

int WaitForNetworkOk()
{
    CURL *curl;
    CURLcode res;
    int i = 0;
    char *url = NULL;

    if ( gIpc.config.url ) {
        url = gIpc.config.url;
    } else {
        url = gIpc.config.defaultUrl;
    }

    printf("start to check network, url = %s ....\n", gIpc.config.url );
    curl = curl_easy_init();
    if ( curl != NULL ) {
        for ( i=0; i<gIpc.config.tokenRetryCount; i++ ) {
            curl_easy_setopt( curl, CURLOPT_URL, url );
            res = curl_easy_perform( curl );
            if ( res == CURLE_OK ) {
                return 0;
            } else {
                sleep(1);
            }
        }
        curl_easy_cleanup( curl );
    }
    printf("finished to check network \n");
    return 0;
}

void *ConfigUpdateTask( void *param )
{
    for (;;) {
        if ( gIpc.config.updateFrom == UPDATE_FROM_FILE ) {
            UpdateConfig();
        }
        sleep( gIpc.config.configUpdateInterval );
    }
}

void StartConfigUpdateTask()
{
    pthread_t thread;

    pthread_create( &thread, NULL, ConfigUpdateTask, NULL );
}

int CaptureDevStartStream()
{
    DBG_LOG("start the stream...\n");
    gIpc.dev->startStream( STREAM_MAIN );
    if ( gIpc.config.multiChannel ) {
        gIpc.dev->startStream( STREAM_SUB );
    }

    return 0;
}

int TsUploaderSdkDeInit()
{
    LinkDestroyAVUploader(&gIpc.stream[STREAM_MAIN].uploader);
    LinkDestroyAVUploader(&gIpc.stream[STREAM_SUB].uploader);
    LinkUninitUploader();

    return 0;
}

char *GetH264File()
{
    return gIpc.config.h264_file;
}

char *GetAacFile()
{
    return gIpc.config.aac_file;
}


int main()
{
    char *logFile = NULL;
    char used[1024] = { 0 };
    pid_t pid = 0;
    char *keyFile = "/bin";
    int msgid = 0;
    int event = 0;
    msg_t msg;
    key_t key;

    InitConfig();
    UpdateConfig();
    
    WaitForNetworkOk();
    if ( gIpc.config.logFile ) {
        logFile = gIpc.config.logFile;
    } else {
        logFile = gIpc.config.defaultLogFile;
    }
    LoggerInit( gIpc.config.logPrintTime, gIpc.config.logOutput,
                logFile, gIpc.config.logVerbose );
    CaptureDevInit();
    StartConfigUpdateTask();
    /* 
     * ipc need to receive server command
     * so socket logging task must been started
     * */
    StartSimpleSshTask();
    StartSocketDbgTask();
    TsUploaderSdkInit();
    StartTokenUpdateTask();
    CaptureDevStartStream();

    DBG_LOG("compile time : %s %s \n", __DATE__, __TIME__ );
    DBG_LOG("gIpc.version : %s\n", gIpc.version );
    DBG_LOG("commit id : %s dev_id : %s \n", CODE_VERSION, gIpc.devId );
    DBG_LOG("gIpc.config.heartBeatInterval = %d\n", gIpc.config.heartBeatInterval);

    if ( (pid = fork()) == 0 ) {
        StartUpgradeProcess();
    }

    key = ftok( keyFile , '6' );
    msgid = msgget( key, IPC_CREAT|O_WRONLY|0777 );
    if ( msgid < 0 ) {
        printf("msgid < 0 ");
        return 0;
    }

    for (;; ) {
        float cpu_usage = 0;

        sleep( gIpc.config.heartBeatInterval );
        DbgGetMemUsed( used );
        cpu_usage = DbgGetCpuUsage();
        DBG_LOG("[ %s ] [ HEART BEAT] move_detect : %d cache : %d multi_ch : %d memeory used : %skB cpu_usage : %%%6.2f  token_url : %s\n reanme_url : %s\n",
                gIpc.devId, gIpc.config.movingDetection, gIpc.config.openCache, gIpc.config.multiChannel,used, cpu_usage,
                gIpc.config.tokenUrl, gIpc.config.renameTokenUrl );
        msgrcv( msgid, &msg, sizeof(msg)-sizeof(msg.type), 2, IPC_NOWAIT );
        printf("main process get event = %d\n", msg.event );
        if ( msg.event == OTA_START_UPGRADE_EVENT ) {
            printf("get the value OTA_START_UPGRADE_EVENT\n");
            break;
        }
    }

    CaptureDevDeinit();
    TsUploaderSdkDeInit();
    LOGI("main process exit\n");

    return 0;
}

