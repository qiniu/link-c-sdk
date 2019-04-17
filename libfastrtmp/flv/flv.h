// Last Update:2019-03-11 23:21:08
/**
 * @file flv.h
 * @brief 
 * @author qiniu
 * @version 0.1.00
 * @date 2019-02-26
 */

#ifndef FLV_H
#define FLV_H

#include <stdint.h>
#include <pthread.h>
#include "bytestream.h"

typedef enum {
    FLV_CODECID_H263 = 2,
    FLV_CODECID_SCREEN,
    FLV_CODECID_VP6,
    FLV_CODECID_VP6A,
    FLV_CODECID_SCREEN2,
    FLV_CODECID_H264,
} FLVVideoCodec;

typedef enum {
    FLV_CODECID_LINER_PCM,
    FLV_CODECID_PCM,
    FLV_CODECID_MP3,
    FLV_CODECID_PCM_LE,
    FLV_CODECID_NELLYMOSER16,
    FLV_CODECID_NELLYMOSER8,
    FLV_CODECID_NELLYMOSER,
    FLV_CODECID_ALAW,
    FLV_CODECID_MULAW,
    FLV_CODECID_RESERVED,
    FLV_CODECID_AAC,
    FLV_CODECID_SPEEX,
    FLV_CODECID_MP3_8K = 14,
} FLVAudioCodec;

typedef enum {
    FLV_SCRIPT_TYPE_NUMBER,
    FLV_SCRIPT_TYPE_BOLLEAN,
    FLV_SCRIPT_TYPE_STRING,
    FLV_SCRIPT_TYPE_OBJECT,
    FLV_SCRIPT_TYPE_MOVIECLIP,
    FLV_SCRIPT_TYPE_NULL,
    FLV_SCRIPT_TYPE_UNDEFIND,
    FLV_SCRIPT_TYPE_REFRENCE,
    FLV_SCRIPT_TYPE_ECMAARRAY,
    FLV_SCRIPT_TYPE_OBJECTANDMAKER,
    FLV_SCRIPT_TYPE_STRICTARRAY,
    FLV_SCRIPT_TYPE_DATE,
    FLV_SCRIPT_TYPE_LONGSTRING,
} FLVScriptDataType;

typedef enum {
    FLV_SAMPLERATE_SPECIAL,
    FLV_SAMPLERATE_11025HZ = 11025,
    FLV_SAMPLERATE_22050HZ = 22050,
    FLV_SAMPLERATE_16000HZ = 16000,
    FLV_SAMPLERATE_44100HZ = 44100,
} FLVAudioSampleRate;

typedef enum {
    FLV_SAMPLESIZE_8BIT,
    FLV_SAMPLESIZE_16BIT,
} FLVAudioSampleSize;

typedef enum {
    FLV_MONO,
    FLV_STEREO,
} FLVAudioSoundType;

typedef struct {
    char *key;
    char *value;
} Map;

typedef struct {
    ByteStream *bs;
    char *buf;
    int last_audio_timestamp;
    int last_video_timestamp;
    int tag_pos;
    FLVVideoCodec videocodec;
    FLVAudioCodec audiocodec;
    FLVAudioSampleRate samplerate;
    FLVAudioSampleSize samplesize;
    FLVAudioSoundType soundtype;
    int videocfgsend;// whether AVCDecoderConfigurationRecord has been sent
    int audiocfgsend;// whether AudioSpecificConfig has been sent
} FLVContext;

typedef struct {
    char *data;
    int len;
    int64_t timestamp;
    int type;
    int iskey;
} FrameInfo;

typedef struct {
    FLVVideoCodec videocodec;
    FLVAudioCodec audiocodec;
    FLVAudioSampleRate samplerate;
    FLVAudioSampleSize samplesize;
    FLVAudioSoundType soundtype;
    void *opaque;
    int (*write_packet)( void *opaque, uint8_t *buf, int buf_size );
} FLVParam;

enum {
    TAG_TYPE_AUDIO = 8,
    TAG_TYPE_VIDEO = 9,
    TAG_TYPE_SCRIPT = 18,
    TAG_TYPE_PRIVATE = 0x67,
};

enum {
    FLV_KEY_FRAME = 1,
    FLV_INTER_FRAME,
};

#define FLV_DEBUG 0
#if FLV_DEBUG
extern void DbgFLVSetCompositionTime( int compisition_time );
#else
#define DbgFLVSetCompositionTime( compisition_time ) 
#endif

extern FLVContext *FLVNewContext( FLVParam *param );
extern int FLVWriteAVData( FLVContext *ctx, FrameInfo *frame );
extern int FLVWriteScriptData( FLVContext *ctx, Map *maps, int _size );
extern int FLVWritePrivateData( FLVContext *ctx, char *realip );
extern int FLVDestroyContext( FLVContext *ctx );
extern int FLVFlush( FLVContext *ctx );

#endif  /*FLV_H*/
