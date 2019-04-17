// Last Update:2019-03-08 10:35:22
/**
 * @file ipc_dev.c
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-10-19
 */
#include <string.h>
#include "devsdk.h"
#include "dbg.h"
#include "dev_core.h"

extern CaptureDevice gIpcCaptureDev;

static int AjVideoGetFrameCb( int streamno, char *_pFrame,
                   int _nLen, int _nIskey, double _dTimeStamp,
                   unsigned long _nFrameIndex, unsigned long _nKeyFrameIndex,
                   void *_pContext)
{
    int stream = 0;

    if ( &gIpcCaptureDev.subContext == _pContext ) {
        stream = STREAM_SUB;
    } else {
        stream = STREAM_MAIN;
    }
    gIpcCaptureDev.videoCb( _pFrame, _nLen, _nIskey, _dTimeStamp, _nFrameIndex, _nKeyFrameIndex, stream );

    return 0;
}

static int AjAudioGetFrameCb( char *_pFrame, int _nLen, double _dTimeStamp,
                     unsigned long _nFrameIndex, void *_pContext )
{
    int stream = 0;
    static double localTimeStamp = 0, timeStamp = 0; 
    static int first = 1;

    if ( first ) {
        localTimeStamp = _dTimeStamp;
        first = 0;
    } else {
        localTimeStamp += 40;
    }

    if ( gIpcCaptureDev.audioType == AUDIO_AAC ) {
        timeStamp = _dTimeStamp;
    } else {
        timeStamp = localTimeStamp;
    }

    if ( &gIpcCaptureDev.subContext == _pContext ) {
        stream = STREAM_SUB;
    } else {
        stream = STREAM_MAIN;
    }
    if ( gIpcCaptureDev.audioCb )
        gIpcCaptureDev.audioCb( _pFrame, _nLen, timeStamp, _nFrameIndex, stream );

    return 0;
}

static int AjInitIPC( VideoFrameCb videoCb, AudioFrameCb audioCb, char *video, char *audio  )
{
    int s32Ret = 0;
    AudioConfig audioConfig;
    int audioType = AUDIO_AAC;

    printf("%s %s %d AjInitIPC ....\n", __FILE__, __FUNCTION__, __LINE__ );
    s32Ret = dev_sdk_init( DEV_SDK_PROCESS_APP );
    if ( s32Ret < 0 ) {
        printf("dev_sdk_init error, s32Ret = %d\n", s32Ret );
        return -1;
    }
    dev_sdk_get_AudioConfig( &audioConfig );
    gIpcCaptureDev.audioCb = audioCb;
    gIpcCaptureDev.videoCb = videoCb;
    if ( audioConfig.audioEncode.enable ) {
        if ( audioType == AUDIO_AAC ) {
            dev_sdk_start_audio_play( AUDIO_TYPE_AAC );
        } else {
            dev_sdk_start_audio_play( AUDIO_TYPE_G711 );
        }
        gIpcCaptureDev.audioEnable = 1;
        gIpcCaptureDev.audioType = audioType;
    } else {
        printf("not enabled\n");
    }

    return 0;
}

static int AjGetDevId( char *devId )
{
    MediaStreamConfig config;

    GetMediaStreamConfig( &config );
    strncpy( devId, config.rtmpConfig.server, strlen(config.rtmpConfig.server ) );

    return 0;
}

int AjStartStream()
{
    printf("%s %s %d start stream ....\n", __FILE__, __FUNCTION__, __LINE__ );
    dev_sdk_start_video( 0, 0, AjVideoGetFrameCb, &gIpcCaptureDev.mainContext );
    if ( gIpcCaptureDev.audioEnable ) {
        dev_sdk_start_audio( 0, 0, AjAudioGetFrameCb, &gIpcCaptureDev.mainContext );
    }
    if ( gIpcCaptureDev.subStreamEnable ) {
        dev_sdk_start_video( 0, 1, AjVideoGetFrameCb, &gIpcCaptureDev.subContext );
        if ( gIpcCaptureDev.audioEnable ) {
            dev_sdk_start_audio( 0, 1, AjAudioGetFrameCb, &gIpcCaptureDev.subContext );
        }
    }
    return 0;
}

static int AjDeInitIPC()
{
    dev_sdk_stop_video(0, 1);
    dev_sdk_stop_audio(0, 1);
    dev_sdk_stop_audio_play();
    dev_sdk_release();

    return 0;
}

static int AjIsAudioEnable()
{
    return ( gIpcCaptureDev.audioEnable );
}

int AjAlarmCallback(ALARM_ENTRY _alarm, void *pcontext)
{
    int alarm = 0;

    if ( _alarm.code == ALARM_CODE_MOTION_DETECT ) {
        alarm = ALARM_MOTION_DETECT;
    } else if ( _alarm.code == ALARM_CODE_MOTION_DETECT_DISAPPEAR ) {
        alarm = ALARM_MOTION_DETECT_DISAPPEAR;
    } else if ( _alarm.code == ALARM_CODE_JPEG_CAPTURED ) {
        printf("_alarm.flag = %d\n", _alarm.flag );
        printf("_alarm.level = %d\n", _alarm.level );
        printf("_alarm.data = %s\n", _alarm.data );
        alarm = ALARM_JPEG_CAPTURED;
    } else {
        /* do nothing */
    }
    if ( gIpcCaptureDev.alarmCallback )
        gIpcCaptureDev.alarmCallback( alarm, _alarm.data );
    return 0;
}
static int AjRegisterAlarmCb( int (*alarmCallback)(int alarm, void *data ) )
{
    gIpcCaptureDev.alarmCallback = alarmCallback;
    dev_sdk_register_callback( AjAlarmCallback, NULL, NULL, NULL );

    return 0;
}

static int AjCaptureJpeg( int stream, int quality, char *path, char *filename)
{
    int ret = 0;

    ret = dev_sdk_set_SnapJpegFile( stream, quality, path, filename );
    printf("%s %s %d ret = %d\n", __FILE__, __FUNCTION__, __LINE__, ret );
    return 0;
}

static int AjStopStream()
{
    dev_sdk_stop_video(0, 0);
    dev_sdk_stop_audio(0, 0);
    return 0;
}

CaptureDevice gIpcCaptureDev =
{
    0,
    0,
    0,
    0,
    0,
    NULL,
    NULL,
    AjInitIPC,
    AjDeInitIPC,
    AjGetDevId,
    AjStartStream,
    AjIsAudioEnable,
    AjRegisterAlarmCb,
    NULL,
    AjCaptureJpeg,
    AjStopStream
};

void __attribute__((constructor)) IpcDevRegistrerToCore()
{
    printf("%s %s %d IpcDevRegistrerToCore ....\n", __FILE__, __FUNCTION__, __LINE__ );
    CaptureDeviceRegister( &gIpcCaptureDev );
}


