#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "tsuploader/uploader.h"
#include "ipc.h"
#include "dbg.h"

typedef struct {
    int running;
    int ts_index;
    int total;
    int error;
    LinkTsMuxUploader *uploader;
} app_t;

static app_t app;

#define REQUEST_URL "http://linking-device.qiniuapi.com/v1/device/config"

/*截图的回调函数, 主要用于将一个切片截一个图出来，上传到云存储，用于预览 */
void GetPictureCb(void *pUserData, const char *pFilename, int nFilenameLen )
{
    ipc_capture_picture( (char *)pFilename );
}

/* 统计切片上传的成功率 */
void UploadStatisticCb(void *pUserOpaque,
                LinkUploadKind uploadKind,
                LinkUploadResult uploadResult)
{
    if ( uploadKind == LINK_UPLOAD_TS ) {
        if ( uploadResult == LINK_UPLOAD_RESULT_FAIL ) {
            app.error++;
        }
        app.total++;
        LOGI("total upload : %d err : %d\n", app.total, app.error );
    }
}

/* TS切片写入到SD卡 */
int TsFileSave2SDCardCb( const char *buffer, int size, void *userCtx, LinkMediaInfo info )
{
    char ts_name[16] = { 0 };
    FILE *fp = NULL;

    sprintf( ts_name, "./output/%03d.ts", app.ts_index++ );
    LOGI("save ts : %s\n", ts_name );
    fp = fopen( ts_name, "w+" );
    if ( !fp ) {
        LOGE("open file %s error\n", ts_name );
        return -1;
    }
    fwrite( buffer, 1, size, fp );
    fclose( fp );
    return 0;
}

int VideoFrameCallBack ( uint8_t *frame, int len, int iskey, int64_t timestamp )
{
    /* 6. 摄像头编码一帧视频数据，调用此回调函数, 调用LinkPushVideo推送视频流 */
    LinkPushVideo( app.uploader, (char *)frame, len, timestamp, iskey, 0, 0 );

    return 0;
}

int AudioFrameCallBack ( uint8_t *frame, int len, int64_t timestamp )
{
    /* 7. 摄像头编码一帧音频数据，调用此函数，调用LinkPushAudio推送音频流 */
    LinkPushAudio( app.uploader, (char *)frame, len, timestamp, 0 );
    return 0;
}

int IpcEventCallBack( int event, void *data )
{
    switch( event ) {
    case EVENT_MOTION_DETECTION:
        {
            LinkSessionMeta metas;
            metas.len = 3;
            char *keys[3] = {"key1", "key23", "key345"};
            metas.keys = (const char **)keys;
            int keylens[3] = {4, 5, 6};
            metas.keylens = keylens;

            char *values[3] = {"value1", "value23", "value345"};
            metas.values = (const char **)values;
            int valuelens[3] = {6, 7, 8};
            metas.valuelens = valuelens;

            metas.isOneShot = 0;
            LinkSetTsType( app.uploader, &metas );
        }
        break;
    case EVENT_MOTION_DETECTION_DISAPEER:
        LinkClearTsType( app.uploader );
        break;
    case EVENT_CAPTURE_PICTURE_SUCCESS:
        {
            char *file = (char *)data;
            FILE *fp = NULL;
            int size = 0;
            char *buf_ptr = NULL;

            if ( !file ) {
                LOGE("check file name error\n");
                return -1;
            }

            fp = fopen( file, "r" );
            if ( !fp ) {
                LOGE("open file %s error\n", file );
                return -1;
            }
            fseek( fp, 0L, SEEK_END );
            size = ftell( fp );
            LOGI("pic file size is %d\n", size );
            fseek( fp, 0L, SEEK_SET );
            buf_ptr = (char *)malloc( size );
            if ( !buf_ptr ) {
                LOGE("malloc error\n");
                return -1;
            }
            fread( buf_ptr, 1, size, fp );
            LinkPushPicture( app.uploader, file, strlen(file), buf_ptr, size );
            free( buf_ptr );

        }
        break;
    default:
        break;
    }
    return 0;
}

void LogCb( int level,  char *log )
{
    if ( log ) {
        LOGI("%s", log );
    }
}

int main()
{
    char * DAK = NULL;
    char * DSK = NULL;
    //获取 dak dsk
    DAK = getenv("LINK_TEST_DAK");
    if (!DAK) {
        printf("No DAK specified.\n");
        exit(EXIT_FAILURE);
    }
    DSK = getenv("LINK_TEST_DSK");
    if (!DSK) {
        printf("No DSK specified.\n");
        exit(EXIT_FAILURE);
    }

    LinkUploadArg arg =
    {
        .nAudioFormat = LINK_AUDIO_AAC,
        .nChannels = 1,
        .nSampleRate = 16000,
        .nVideoFormat = LINK_VIDEO_H264,
        .pConfigRequestUrl = REQUEST_URL,
        .nConfigRequestUrlLen = strlen( REQUEST_URL ),
        .pDeviceAk = DAK,
        .nDeviceAkLen = strlen( DAK ),
        .pDeviceSk = DSK,
        .nDeviceSkLen = strlen( DSK ),
        /* 如果需要截图功能，需要注册截图回调函数，截图的时机掌握在sdk，每个切片截一张图
           摄像头在回调函数中要根据sdk传过来的文件名去截图，截图成功后调用LinkPushPicture上传到云存储
           */
        .getPictureCallback = GetPictureCb, 
        /*
         * 此回调函数主要用于调试，统计上传切片的成功率
         * */
        .pUpStatCb = UploadStatisticCb,
    };
    ipc_param_t param = 
    {
        .audio_type = AUDIO_AAC,
        .video_file = "./material/video.h264",
        .audio_file = "./material/audio.aac",
        .pic_file = "./material/picture.jpg",
        .video_cb = VideoFrameCallBack,
        .audio_cb = AudioFrameCallBack,
        /* 摄像头事件回调函数，当摄像头发生如下事件时，通知app
         * 1) 检测到移动
         * 2) 移动侦测消失
         * 3) 抓图成功或者失败
         * sample当中这些事件都是用软件模拟,真实的摄像头需要使用摄像头底层的sdk
         * */
        .event_cb = IpcEventCallBack,
    };
    int ret = 0;

    app.running = 1;

    /* 1.初始化sdk */
    LinkInit();

    if ( arg.nDeviceAkLen == 0 || arg.nDeviceSkLen == 0 ) {
        LOGE("check dak/dsk fail, please get dak/dsk first.\
             see https://developer.qiniu.io/linking/manual/5323/linking-quick-start for more info\n");
        return 0;
    }

    /* 2.创建上传实例 */
    ret = LinkNewUploader( &app.uploader, &arg );
    if ( ret != LINK_SUCCESS ) {
        LOGE("new uploader error\n");
        return 0;
    }
    /* 3.设置切片回调函数，每一个切片上传云存的同时，
     * 会调用此回调函数，将一个切片的完整数据，传给该回调函数 */
    LinkUploaderSetTsOutputCallback( app.uploader, TsFileSave2SDCardCb, NULL );

    /* 4.初始化模拟摄像头，注册音视频、事件回调函数 */
    ipc_init( &param );
    /* 5.启动模拟摄像头音视频流，开始读取文件，模拟ipc，
     * 每隔一段时间调用一次视音频回调函数 */
    ipc_run();

    /* 设置sdk日志回调函数 */
    LinkSetLogCallback( LogCb );

    while( app.running ) {
        sleep( 2 );
    }

    /* 销毁上传实例 */
    LinkFreeUploader( &app.uploader );
    /* 释放sdk资源 */
    LinkCleanup();

    return 0;
}
