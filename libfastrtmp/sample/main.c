// Last Update:2019-03-25 10:07:49
/**
 * @file main.c
 * @brief 
 * @author felix
 * @version 0.1.00
 * @date 2019-03-08
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "fastrtmp.h"
#include "dev_core.h"
#include "dbg.h"

LinkFastRtmpContext *ctx = NULL;

static int VideoFrameCallback ( char *_pFrame, 
                   int _nLen, int _nIskey, double _dTimeStamp, 
                   unsigned long _nFrameIndex, unsigned long _nKeyFrameIndex, 
                   int streamno )
{
    int ret = 0;

    LOGI("timestamp : %f\n", _dTimeStamp );
    if ( ctx ) {
        /* 3. 推送视频帧 */
        ret = LinkFastRtmpSendVideo( ctx, _pFrame, _nLen, _nIskey, (int64_t)_dTimeStamp );
        if ( ret != 0 ) {
            LOGE("LinkFastRtmpSendVideo error\n");
            exit(1);
        }
    }
    return 0;
}

static int AudioFrameCallBack( char *_pFrame, int _nLen, double _dTimeStamp,
                     unsigned long _nFrameIndex, int streamno )
{
    int ret = 0;

    if ( ctx ) {
        /* 4. 推送音频帧 */
        ret = LinkFastRtmpSendAudio( ctx, _pFrame, _nLen, (int64_t)_dTimeStamp );
        if ( ret != 0 ) {
            LOGE("LinkFastRtmpSendAudio error\n");
            exit(1);
        }
    }
    return 0;
}

int main()
{
    CoreDevice *ipc;
    FastRtmpParam param = 
    {
        .timeout = 10,
        .video_codec = VIDEO_CODECID_H264,
        .audio_codec = AUDIO_CODECID_AAC,
        .samplerate = AUDIO_SAMPLERATE_16000HZ,
        .samplesize = AUDIO_SAMPLESIZE_16BIT,
        .soundtype = AUDIO_MONO,
        .cert_file = "./cert.pem",
        .url = "qwss://wss-publish-test.cloudvdn.com/ly-live/lytest",
    };

    /* 1. 创建fastrtmp推流实例 */
    ctx = LinkFastRtmpNewContext( &param );
    if ( !ctx ) {
        LOGE("LinkFastRtmpContext error\n");
        return 0;
    }

    ipc =  NewCoreDevice();
    if ( !ipc ) {
        LOGE("NewCoreDevice error\n");
        return 0;
    }

    /* 2. 初始化ipc，注册音视频回调 */
    ipc->init( VideoFrameCallback, AudioFrameCallBack, "./video.h264", "audio.aac"  );
    ipc->startStream( 0 );


    for (;;) {
        sleep( 2 );
    }

    return 0;
}


