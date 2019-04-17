// Last Update:2019-03-20 14:26:39
/**
 * @file fastrtmp.h
 * @brief 
 * @author felix
 * @version 0.1.00
 * @date 2019-03-06
 */

#ifndef FASTRTMP_H
#define FASTRTMP_H

#include <stdint.h>
#include <pthread.h>

typedef enum {
    VIDEO_CODECID_H263,
    VIDEO_CODECID_SCREEN,
    VIDEO_CODECID_VP6,
    VIDEO_CODECID_VP6A,
    VIDEO_CODECID_SCREEN2,
    VIDEO_CODECID_H264,
} VideoCodec;

typedef enum {
    AUDIO_CODECID_LINER_PCM,
    AUDIO_CODECID_PCM,
    AUDIO_CODECID_MP3,
    AUDIO_CODECID_PCM_LE,
    AUDIO_CODECID_NELLYMOSER16,
    AUDIO_CODECID_NELLYMOSER8,
    AUDIO_CODECID_NELLYMOSER,
    AUDIO_CODECID_ALAW,
    AUDIO_CODECID_MULAW,
    AUDIO_CODECID_RESERVED,
    AUDIO_CODECID_AAC,
    AUDIO_CODECID_SPEEX,
    AUDIO_CODECID_MP3_8K,
} AudioCodec;

typedef enum {
    AUDIO_SAMPLERATE_SPECIAL,
    AUDIO_SAMPLERATE_11025HZ,
    AUDIO_SAMPLERATE_22050HZ,
    AUDIO_SAMPLERATE_16000HZ,
    AUDIO_SAMPLERATE_44100HZ,
} AudioSampleRate;

typedef enum {
    AUDIO_SAMPLESIZE_8BIT,
    AUDIO_SAMPLESIZE_16BIT,
} AudioSampleSize;

typedef enum {
    AUDIO_MONO,
    AUDIO_STEREO,
} AudioSoundType; 

typedef struct {
    char *url;
    char *cert_file;
    int wss_port;
    int timeout;
    VideoCodec video_codec;
    AudioCodec audio_codec;
    AudioSampleRate samplerate;
    AudioSampleSize samplesize;
    AudioSoundType soundtype;
} FastRtmpParam;

typedef struct {
    void *flv_ctx;
    void *wss_ctx;
    pthread_mutex_t mutex;
} LinkFastRtmpContext;

extern LinkFastRtmpContext *LinkFastRtmpNewContext( FastRtmpParam *param );
extern int LinkFastRtmpSendVideo( LinkFastRtmpContext *ctx, char *data, int len, int iskey, int64_t timestamp );
extern int LinkFastRtmpSendAudio( LinkFastRtmpContext *ctx, char *data, int len, int64_t timestamp );
extern int LinkFastRtmpDestroyContext( LinkFastRtmpContext *ctx );

#endif  /*FASTRTMP_H*/
