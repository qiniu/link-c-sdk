// Last Update:2019-03-20 19:14:56
/**
 * @file testfastrtmp.c
 * @brief 
 * @author felix
 * @version 0.1.00
 * @date 2019-03-06
 */

#include <stdio.h>
#include <unistd.h>
#include "fastrtmp.h"
#include "unit_test.h"
#include "dev_core.h"
#include "log/log.h"

#define VIDEO_FILE "../src/sample/material/video.h264"
#define AUDIO_FILE "../src/sample/material/audio.aac"

LinkFastRtmpContext *ctx = NULL;

static int VideoFrameCallback ( char *_pFrame, 
                   int _nLen, int _nIskey, double _dTimeStamp, 
                   unsigned long _nFrameIndex, unsigned long _nKeyFrameIndex, 
                   int streamno )
{
    int ret = 0;

    if ( ctx ) {
        ret = LinkFastRtmpSendVideo( ctx, _pFrame, _nLen, _nIskey, (int64_t)_dTimeStamp );
        if ( ret != 0 ) {
            LinkLogError("LinkFastRtmpSendVideo error\n");
            exit(1);
            return -1;
        }
    }
    return 0;
}

static int AudioFrameCallBack( char *_pFrame, int _nLen, double _dTimeStamp,
                     unsigned long _nFrameIndex, int streamno )
{
    int ret = 0;

    if ( ctx ) {
        ret = LinkFastRtmpSendAudio( ctx, _pFrame, _nLen, (int64_t)_dTimeStamp );
        if ( ret != 0 ) {
            LinkLogError("LinkFastRtmpSendAudio error\n");
            return -1;
        }
    }
    return 0;
}

const char *getCmdlineOpt(int argc, const char **argv, const char *val) {
        int n = (int)strlen(val), c = argc;
        while (--c > 0) {
                if (!strncmp(argv[c], val, n)) {
                        if (!*(argv[c] + n) && c < argc - 1) {
                                if (!argv[c + 1] || strlen(argv[c + 1]) > 1024)
                                        return NULL;
                                return argv[c + 1];
                        }
                        return argv[c] + n;
                }
        }
        return NULL;
}

int main(int argc, const char **argv)
{
    CoreDevice *ipc;
    const char *pOpt = NULL;
    FastRtmpParam param = 
    {
        .timeout = 10,
        .video_codec = VIDEO_CODECID_H264,
        .audio_codec = AUDIO_CODECID_AAC,
        .samplerate = AUDIO_SAMPLERATE_44100HZ,
        .samplesize = AUDIO_SAMPLESIZE_16BIT,
        .soundtype = AUDIO_STEREO,
        .cert_file = "../src/sample/material/cert.pem",
        .url = "qwss://wss-publish-test.cloudvdn.com/ly-live/lytest",
    };
    if ((pOpt = getCmdlineOpt(argc, argv, "-ca")))
        param.cert_file = (char *)pOpt;
    if ((pOpt = getCmdlineOpt(argc, argv, "-rtmp")))
        param.url = (char *)pOpt;
 
        
    ctx = LinkFastRtmpNewContext( &param );

    ASSERT_NOT_EQUAL( ctx, NULL );

    ipc =  NewCoreDevice();
    ASSERT_NOT_EQUAL( ipc, NULL );

    ipc->init( VideoFrameCallback, AudioFrameCallBack, VIDEO_FILE, AUDIO_FILE  );
    ipc->startStream( 0 );

    for (;;) {
        sleep( 2 );
    }

    return 0;
}

