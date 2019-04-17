// Last Update:2019-03-02 19:39:29
/**
 * @file dev_core.h
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-07-05
 */

#ifndef DEV_CORE_H
#define DEV_CORE_H

typedef enum {
    ALARM_MOTION_DETECT,
    ALARM_MOTION_DETECT_DISAPPEAR,
    ALARM_JPEG_CAPTURED
} AlarmCode;

typedef enum {
    STREAM_MAIN,
    STREAM_SUB,
    STREAM_MAX,
} StreamChannel;

typedef enum {
    AUDIO_AAC,
    AUDIO_G711
} AudioType;

typedef int (*VideoFrameCb) ( char *_pFrame, 
                   int _nLen, int _nIskey, double _dTimeStamp, 
                   unsigned long _nFrameIndex, unsigned long _nKeyFrameIndex, 
                   int streamno );
typedef int ( *AudioFrameCb)( char *_pFrame, int _nLen, double _dTimeStamp,
                     unsigned long _nFrameIndex, int streamno );

typedef struct {
    int audioType;
    int audioEnable;
    int subStreamEnable;
    int mainContext;
    int subContext;
    AudioFrameCb audioCb;
    VideoFrameCb videoCb;
    int (*init)( VideoFrameCb videoCb, AudioFrameCb audioCb, char *videoFile, char *audioFile );
    int (*deInit)();
    int (*getDevId)( char *devId );
    int (*startStream)();
    int (*isAudioEnable)();
    int (*registerAlarmCb)( int (*alarmCb)( int alarm, void *data ) );
    int (*alarmCallback)(int alarm, void *data );
    int (*captureJpeg)( int stream, int quality, char *path, char *filename);
    int (*getAudioEncodeType)();
} CaptureDevice;

typedef struct {
    CaptureDevice *pCaptureDevice;
    int (*init)(  VideoFrameCb videoCb, AudioFrameCb audioCb, char *videoFile, char *audioFile );
    int (*deInit)();
    int (*getDevId)( char *devId );
    int (*startStream)( int streamType );
    int (*isAudioEnable)();
    int (*registerAlarmCb)( int (*alarmCb)( int alarm, void *data ) );
    int (*captureJpeg)( int stream, int quality, char *path, char *filename);
    int (*getAudioEncodeType)();
} CoreDevice;

extern CoreDevice * NewCoreDevice();
extern int CaptureDeviceRegister( CaptureDevice *pDev );

#endif  /*DEV_CORE_H*/
