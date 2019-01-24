#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "uploader.h"
#include "security.h"
#include "main.h"
#include "log2file.h"
#include "dbg.h"
#include "cfg.h"
#include "log2tcp.h"
#include "queue.h"
#include "mymalloc.h"
#include "dev_core.h"
#include "stream.h"
#include "picuploader.h"
#include "httptools.h"
#include "ota.h"

/* global variable */
App gIpc;

int GetFileSize( char *_pFileName)
{
    FILE *fp = fopen( _pFileName, "r"); 
    int size = 0;
    
    if( !fp ) {
        DBG_ERROR("fopen file %s error\n", _pFileName );
        return -1;
    }

    fseek(fp, 0L, SEEK_END ); 
    size = ftell(fp); 
    fclose(fp);

    return size;
}

int LoadFile( char *_pFileName, int size, void *_pBuf )
{
    FILE *fp = fopen( _pFileName, "r"); 
    size_t ret = 0;

    if( !fp ) {
        DBG_ERROR("fopen file %s error\n", _pFileName );
        return -1;
    }

    ret = fread( _pBuf, size, 1, fp );
    if ( ret != 1 ) {
        DBG_ERROR("fread error\n");
        fclose( fp );
        return -1;
    }
    fclose( fp );

    return 0;
}

int RemoveFile( char *_pFileName )
{
    int ret = 0;
    char okfile[512] = { 0 };

    if ( access( _pFileName, R_OK ) == 0  ) {
        ret = remove( _pFileName );
        if ( ret != 0 ) {
            DBG_ERROR("remove file %s error, ret = %d\n", _pFileName, ret );
        }
    } else {
        DBG_ERROR("file %s not exist \n", _pFileName );
    }

    sprintf( okfile, "%s.ok", _pFileName );
    if ( access( okfile, R_OK ) == 0  ) {
        ret = remove( okfile );
        if ( ret != 0 ) {
            DBG_ERROR("remove file %s error, ret = %d\n", okfile, ret );
        }
    } else {
        DBG_ERROR("file %s not exist \n", okfile );
    }

    return 0;
}

int AlarmCallback( int alarm, void *data )
{
    static char lastPicName[256] = { 0 };
        
    LinkSessionMeta metas = {0};
    metas.len = 1;
    char *keys[1] = {"type"};
    metas.keys = (const char **)keys;
    int keylens[1] = {4};
    metas.keylens = keylens;
        
    char *values[1] = {"move"};
    metas.values = (const char **)values;
    int valuelens[1] = {4};
    metas.valuelens = valuelens;

    if ( alarm == ALARM_MOTION_DETECT ) {
        //DBG_LOG("get event ALARM_MOTION_DETECT\n");
        LinkSessionMeta metas = {0};
        metas.len = 1;
        char *keys[1] = {"type"};
        metas.keys = (const char **)keys;
        int keylens[1] = {4};
        metas.keylens = keylens;
        char *values[1] = {"move"};
        metas.values = (const char **)values;
        int valuelens[1] = {4};
        metas.valuelens = valuelens;

        if ( gIpc.stream[STREAM_MAIN].uploader ) {
            LinkSetTsType(gIpc.stream[STREAM_MAIN].uploader, &metas);
        }if ( gIpc.stream[STREAM_SUB].uploader ) {
            LinkSetTsType(gIpc.stream[STREAM_SUB].uploader, &metas);
        }
        gIpc.detectMoving = alarm;
    } else if ( alarm == ALARM_MOTION_DETECT_DISAPPEAR ) {
        //DBG_LOG("get event ALARM_MOTION_DETECT_DISAPPEAR\n");
        gIpc.detectMoving = alarm;
        if ( gIpc.stream[STREAM_MAIN].uploader ) {
            LinkClearTsType(gIpc.stream[STREAM_MAIN].uploader);
            LinkFlushUploader( gIpc.stream[STREAM_MAIN].uploader );
        }if ( gIpc.stream[STREAM_SUB].uploader ) {
            LinkClearTsType(gIpc.stream[STREAM_SUB].uploader);
            LinkFlushUploader( gIpc.stream[STREAM_SUB].uploader );
        }
    } else if ( alarm == ALARM_JPEG_CAPTURED ) {
        void *pBuf = NULL;
        int size = 0, ret = 0;

        //DBG_LOG( "data = %s\n", (char *)data );
        /*
         * sometimes the ipc will notify twice of one picture
         * at this moment, demo will notify sdk twice
         * inside LinkPushPicture will free the pointer pOpaque
         * so it will cause double free
         * here we will check the notify
         * if the notify is same as the last
         * ignore it
         * */
        if ( strncmp( (char *)data, lastPicName, strlen((char *)data) ) == 0 ) {
            DBG_LOG("this is the bug of ipc, we will ignore this notify, pic name is : %s\n", data );
            return 0;
        }
        
        size = GetFileSize( (char*)data );
        if ( size < 0 ) {
            DBG_ERROR("GetFileSize error\n");
            return 0;
        }
        pBuf = malloc( size ); 
        if ( !pBuf ) {
            DBG_LOG("malloc error\n");
            return 0;
        }

        ret = LoadFile( (char *)data, size, pBuf );
        if ( ret < 0 ) {
            DBG_ERROR("LoadFile error\n");
            free( pBuf );
            return 0;
        }
        memset( lastPicName, 0, sizeof(lastPicName) );
        memcpy( lastPicName, (char *)data, sizeof(lastPicName) );

        LinkPushPicture( gIpc.stream[STREAM_MAIN].uploader, (char *)data,
                                strlen((char *)data), pBuf, size ); 
        RemoveFile( (char*)data );
        free( pBuf );
    } else {
        /* do nothing */
    }

    return 0;
}

static int CaptureDevInit( )
{
    int audioType = AUDIO_AAC;

    gIpc.dev = NewCoreDevice();

    DBG_LOG("start to init ipc...\n");

    DBG_LOG("gIpc.config.audioType = %s\n", gIpc.config.audioType );
    if ( gIpc.config.audioType ) {
        if ( strcmp( gIpc.config.audioType, "aac" ) == 0 ) {
            audioType = AUDIO_AAC;
        } else {
            audioType = AUDIO_G711;
        }
    }
    gIpc.dev->init( audioType, gIpc.config.multiChannel, VideoGetFrameCb, AudioGetFrameCb );
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

static void GetPicCallback (void *pOpaque,  const char *pFileName, int nFilenameLen )
{
    char *path = "/tmp";
    static struct timeval start = { 0, 0 }, end = { 0, 0 };
    int interval = 0;

    gettimeofday( &end, NULL );
    interval = GetTimeDiffMs( &start, &end );
    DBG_LOG("capture jpeg %s interval %d\n", pFileName, interval );

    gIpc.dev->captureJpeg( 0, 0, path, (char *)pFileName );

    start = end;
}

int _TsUploaderSdkInit( StreamChannel ch )
{
    int ret = 0;
    LinkUploadArg userUploadArg;

    memset( &userUploadArg, 0, sizeof(userUploadArg) );

    if ( ch == STREAM_MAIN ) {
        userUploadArg.getPictureCallback = GetPicCallback;
    }
    if ( gIpc.audioType == AUDIO_AAC ) {
        userUploadArg.nAudioFormat = LINK_AUDIO_AAC;
        userUploadArg.nSampleRate = 16000;
    } else {
        userUploadArg.nAudioFormat = LINK_AUDIO_PCMU;
        userUploadArg.nSampleRate = 8000;
    }
    if ( STREAM_MAIN == ch ) {
        sprintf( gIpc.stream[ch].devId, "%s%s", gIpc.devId, "a" );
    } else {
        sprintf( gIpc.stream[ch].devId, "%s%s", gIpc.devId, "b" );
    }
    userUploadArg.nChannels = 1;
    userUploadArg.nVideoFormat = LINK_VIDEO_H264;
    if ( gIpc.config.tokenUrl ) {
        userUploadArg.pConfigRequestUrl = gIpc.config.tokenUrl;
        userUploadArg.nConfigRequestUrlLen = strlen(gIpc.config.tokenUrl);
    }
    if ( gIpc.config.ak ) {
        userUploadArg.pDeviceAk = gIpc.config.ak;
        userUploadArg.nDeviceAkLen = strlen( gIpc.config.ak );
    }
    if ( gIpc.config.sk ) {
        userUploadArg.pDeviceSk = gIpc.config.sk;
        userUploadArg.nDeviceSkLen = strlen( gIpc.config.sk );
    }
        
    ret = LinkNewUploader( &gIpc.stream[ch].uploader, &userUploadArg);
    if (ret != 0) {
        DBG_LOG("CreateAndStartAVUploader error, ret = %d\n", ret );
        return ret;
    }

    return 0;
}

int TsUploaderSdkInit()
{
    int ret = 0;

    DBG_LOG("start to init ts uploader sdk \n");
    DBG_LOG("gIpc.devId= %s\n", gIpc.devId);

    LinkSetLogLevel(LINK_LOG_LEVEL_DEBUG);
    ret = LinkInit();
    if (ret != 0) {
        DBG_LOG("InitUploader error, ret = %d\n", ret );
        return ret;
    }

    ret = _TsUploaderSdkInit( STREAM_MAIN );
    if ( ret < 0 ) {
        return -1;
    }
    if ( gIpc.config.multiChannel ) {
        _TsUploaderSdkInit( STREAM_SUB );
    }

    DBG_LOG("[ %s ] link ts uploader sdk init ok\n", gIpc.devId );
    printf("[ %s ] link ts uploader sdk init ok\n", gIpc.devId );
    return 0;
}

int WaitForNetworkOk()
{
    char *url = NULL;
    if ( gIpc.config.url ) {
        url = gIpc.config.url;
    } else {
        url = gIpc.config.defaultUrl;
    }

    char buf[256];
    int nRespLen = 0;
    int i = 0;
        
    printf("start to check network, url = %s ....\n", url );
    for ( i=0; i<gIpc.config.tokenRetryCount; i++ ) {
        int ret = LinkSimpleHttpGet(url, buf, sizeof(buf), &nRespLen);
        if (ret == LINK_SUCCESS || ret == LINK_BUFFER_IS_SMALL || ret > 0) {
            return 0;
        } else {
            sleep(1);
        }
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
    LinkFreeUploader(&gIpc.stream[STREAM_MAIN].uploader);
    LinkFreeUploader(&gIpc.stream[STREAM_SUB].uploader);
    LinkCleanup();

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

    gIpc.version = "v00.00.07";
    gIpc.running = 1;

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
    CaptureDevStartStream();
    StartUpgradeTask();

    DBG_LOG("compile time : %s %s \n", __DATE__, __TIME__ );
    DBG_LOG("gIpc.version : %s\n", gIpc.version );
    DBG_LOG("commit id : %s dev_id : %s \n", CODE_VERSION, gIpc.devId );
    DBG_LOG("gIpc.config.heartBeatInterval = %d\n", gIpc.config.heartBeatInterval);

    while ( gIpc.running ) {
        float cpu_usage = 0;

        sleep( gIpc.config.heartBeatInterval );
        DbgGetMemUsed( used );
        cpu_usage = DbgGetCpuUsage();
        DBG_LOG("[ %s ] [ HEART BEAT] move_detect : %d cache : %d multi_ch : %d memeory used : %skB cpu_usage : %%%6.2f\n",
                gIpc.devId, gIpc.config.movingDetection, gIpc.config.openCache, gIpc.config.multiChannel,used, cpu_usage );
        DBG_LOG(" toekn url : %s\n", gIpc.config.tokenUrl );
    }

    CaptureDevDeinit();
    TsUploaderSdkDeInit();
    LOGI("main process exit\n");

    return 0;
}


