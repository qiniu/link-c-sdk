// Last Update:2019-04-11 18:08:30
/**
 * @file rtmp.c
 * @brief 
 * @author qiniu
 * @version 0.1.00
 * @date 2019-02-26
 */

#include <stdlib.h>
#include <string.h>
#include "fastrtmp.h"
#include "flv.h"
#include "transport.h"
#include "log/log.h"

#define FASTRTMP_DEBUG 0
#if DEBUG
#define ASSERT( cond ) if ( !(cond) ) { \
    LOGE("assert "#cond" error\n"); \
    exit(0);\
}
#else
#define ASSERT(cond)
#endif

static int64_t gStart;

#include <stdint.h>
static inline int64_t getCurMilliSec() {
        struct  timeval    tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec*(int64_t)(1000)+tv.tv_usec/(int64_t)(1000);
}

void DbgTraceTimeStart()
{
	gStart = getCurMilliSec();
}

void DbgTraceTimeElapsed( const char *func, int line )
{
    int64_t end = getCurMilliSec();
    LinkLogDebug("[ %s() %d ] time elapsed : %lld", func, line, end - gStart);
}

#define DBG_TRACE_TIME_START() DbgTraceTimeStart()
#define DBG_TRACE_TIME_ELAPSED() DbgTraceTimeElapsed( __FUNCTION__, __LINE__ )

static int ConvertVideoCodec( VideoCodec codec )
{
    int ret = 0;

    switch( codec ) {
        case VIDEO_CODECID_H263:
            ret = FLV_CODECID_H263;
            break;
        case VIDEO_CODECID_SCREEN:
            ret = FLV_CODECID_H263;
            break;
        case VIDEO_CODECID_VP6:
            ret = FLV_CODECID_H263;
            break;
        case VIDEO_CODECID_VP6A:
            ret = FLV_CODECID_VP6A;
            break;
        case VIDEO_CODECID_SCREEN2:
            ret = FLV_CODECID_SCREEN2;
            break;
        case VIDEO_CODECID_H264:
            ret = FLV_CODECID_H264;
            break;
        default:
            ret = -1;
            break;
    }

    return ret;
}

static int ConvertAudioCodec( AudioCodec codec )
{
    int ret = 0;

    switch( codec ) {
    case AUDIO_CODECID_LINER_PCM:
        ret = FLV_CODECID_SCREEN;
        break;
    case AUDIO_CODECID_PCM:
        ret = FLV_CODECID_PCM;
        break;
    case AUDIO_CODECID_MP3:
        ret = FLV_CODECID_MP3;
        break;
    case AUDIO_CODECID_PCM_LE:
        ret = FLV_CODECID_PCM_LE;
        break;
    case AUDIO_CODECID_NELLYMOSER16:
        ret = FLV_CODECID_NELLYMOSER16;
        break;
    case AUDIO_CODECID_NELLYMOSER8:
        ret = FLV_CODECID_NELLYMOSER8;
        break;
    case AUDIO_CODECID_NELLYMOSER:
        ret = FLV_CODECID_NELLYMOSER;
        break;
    case AUDIO_CODECID_ALAW:
        ret = FLV_CODECID_ALAW;
        break;
    case AUDIO_CODECID_MULAW:
        ret = FLV_CODECID_MULAW;
        break;
    case AUDIO_CODECID_RESERVED:
        ret = FLV_CODECID_RESERVED;
        break;
    case AUDIO_CODECID_AAC:
        ret = FLV_CODECID_AAC;
        break;
    case AUDIO_CODECID_SPEEX:
        ret = FLV_CODECID_SPEEX;
        break;
    case AUDIO_CODECID_MP3_8K:
        ret = FLV_CODECID_MP3_8K;
        break;
    default:
        ret = -1;
        break;
    }

    return ret;
}

static int ConvertAudioSampleRate( int samplerate )
{
    int ret = 0;

    switch( samplerate ) {
    case AUDIO_SAMPLERATE_SPECIAL:
        ret = FLV_SAMPLERATE_SPECIAL;
        break;
    case AUDIO_SAMPLERATE_11025HZ:
        ret = FLV_SAMPLERATE_11025HZ;
        break;
    case AUDIO_SAMPLERATE_16000HZ:
        ret = FLV_SAMPLERATE_16000HZ;
        break;
    case AUDIO_SAMPLERATE_22050HZ:
        ret = FLV_SAMPLERATE_22050HZ;
        break;
    case AUDIO_SAMPLERATE_44100HZ:
        ret = FLV_SAMPLERATE_44100HZ;
        break;
    default:
        ret = -1;
        break;
    }

    return ret;
}

#define WSS_PROTOCOL_SIZE 16

#if FASTRTMP_DEBUG 

static int DbgFLVWriteHeader( FILE *fp )
{
    char buffer[1024] = { 0 }, *buf = buffer;

    sprintf( buf, "%s", "FLV" );
    buf += 3;
    *buf++ = 0x01;//Version
    *buf++ = 0x05;// TypeFlagsAudio = 1, TypeFlagsVideo = 1
    /* DataOffset */
    *buf++ = 0;
    *buf++ = 0;
    *buf++ = 0;
    *buf++ = 0x09;
    // pre tag size
    *buf++ = 0x00;
    *buf++ = 0x00;
    *buf++ = 0x00;
    *buf++ = 0x00;

    fwrite( buffer, 1, 13, fp );

    return 0;
}
static void DbgFastRtmpPushTag( uint8_t *buf, int buf_size )
{
    static FILE *fp = NULL;
    static int i = 0;
    
    DUMPBUF( buf, buf_size );
    DUMPBUF( buf+buf_size-4, 4 );

    if ( !fp ) {
        fp = fopen("./fastrtmp-dump.flv", "w+" );
        if ( !fp ) {
            LinkLogError("open file ./fastrtmp-dump.flv error\n");
            return;
        }
        DbgFLVWriteHeader( fp );
    } 

    fwrite( buf, 1, buf_size, fp );
    i++;
    if ( i == 50000 ) {
        exit(0);
    }
}
#endif

int FastRtmpWritePacket( void *opaque, uint8_t *buf, int buf_size )
{
    int ret = 0;
    LinkFastRtmpContext *ctx = (LinkFastRtmpContext *)opaque;
    char *pbuf = (char *) malloc( buf_size + WSS_PROTOCOL_SIZE );
    static int i = 0;

    ASSERT( buf_size > 0 );
    LinkLogInfo("[ %03d ] pbuf = %p, buf_size = %d\n", i++, pbuf, buf_size );

    if ( !buf || !opaque || !ctx->wss_ctx ) {
        LinkLogError("check param error\n");
        goto err;
    }

    if ( !pbuf ) {
        LinkLogError("malloc error\n");
        goto err;
    }

#if FASTRTMP_DEBUG
    int timestamp = 0;

    timestamp =  buf[4]<<16 | buf[5] <<8 | buf[6];
    LinkLogInfo(" timestamp = %d ms, 0x%x\n", timestamp, timestamp );
    DbgFastRtmpPushTag( buf, buf_size );
    if ( buf[0] == 0x09 ) {
        ASSERT( buf_size > 30 );
    }
#endif

    memset( pbuf, 0, buf_size + WSS_PROTOCOL_SIZE );
    memcpy( pbuf+WSS_PROTOCOL_SIZE, buf, buf_size );

    DBG_TRACE_TIME_START();
    ret = FRtmpPushTag( ctx->wss_ctx, (char *)pbuf, buf_size );
    ASSERT( ret == 0 );
    if ( ret != 0 ) {
        static int i = 0;
        if ( i < 20 ) {
            LinkLogError("FRtmpPushTag error, ret = %d, buf_size = %d\n", ret, buf_size );
        }
        i++;
        return -1;
    }
    DBG_TRACE_TIME_ELAPSED();

    return 0;

err:
    return -1;
}

LinkFastRtmpContext *LinkFastRtmpNewContext( FastRtmpParam *param )
{
    LinkFastRtmpContext *ctx = NULL;
    int ret = 0;
    FLVParam flv_param;
    int audio_codec = 0; 
    int video_codec = 0;
    int samplerate = 0;
    RtmpSettings settings;
    int url_len = 0;
    char *wss_url = NULL, *rtmp_url = NULL;
    char *wss_suffix = ".wsrtmp";
    char *ws = "qws://", *wss = "qwss://", *rtmp = "rtmp://";

    if ( !param  || !param->url || !param->cert_file ) {
        LinkLogError("check param error\n");
        goto err;
    }

    url_len = strlen( param->url );
    if ( url_len == 0 ) {
        LinkLogError("pass url length error\n");
        goto err;
    }
    wss_url = (char *)malloc( url_len+strlen(wss_suffix)+1 );
    if ( !wss_url ) {
        LinkLogError("malloc error\n");
        goto err;
    }
    memset( wss_url, 0, url_len+strlen(wss_suffix)+1 );
    rtmp_url = (char *)malloc( url_len + 3 );// `wss` : 3bytes `rtmps` : 5bytes 5-3=2, 1 = `\0`, 2+1 = 3
    if ( !rtmp_url ) {
        LinkLogError("malloc error\n");
        goto err;
    }
    LinkLogInfo("param->url = %s\n", param->url );
    memset( rtmp_url, 0, url_len+strlen(wss_suffix)+1 );
    if ( memcmp( ws, param->url, strlen(ws) ) == 0 ) {
        sprintf( wss_url, "ws://%s%s", param->url+strlen(ws), wss_suffix );
        sprintf( rtmp_url, "rtmp://%s", param->url+strlen(ws) );
    } else if ( memcmp( wss, param->url, strlen(wss) ) == 0 ) {
        sprintf( wss_url, "wss://%s%s", param->url+strlen(wss), wss_suffix );
        sprintf( rtmp_url, "rtmp://%s", param->url+strlen(wss) );
    } else if ( memcmp( rtmp, param->url, strlen(rtmp) ) == 0 ) {
        // TODO add rtmp support
        LinkLogError("currnetly don't support rtmp://xxx, will support in the future\n");
        goto err;
    } else {
        LinkLogError("check protocol of url error\n");
        goto err;
    }

    LinkLogInfo("wss_url = %s\n", wss_url );
    LinkLogInfo("rtmp_url = %s\n", rtmp_url );

    memset( &settings, 0, sizeof(settings) );

    ctx = (LinkFastRtmpContext *) malloc( sizeof(LinkFastRtmpContext) );
    if ( !ctx ) {
        LinkLogError("malloc error\n");
        goto err;
    }

    memset( ctx, 0, sizeof(LinkFastRtmpContext) );

    if ( param->samplesize == AUDIO_SAMPLESIZE_8BIT ) {
        flv_param.samplesize = FLV_SAMPLESIZE_8BIT;
    } else if ( param->samplesize == AUDIO_SAMPLESIZE_16BIT ) {
        flv_param.samplesize = FLV_SAMPLESIZE_16BIT;
    } else {
        LinkLogError("check sample size error\n");
        goto err;
    }
    if ( param->soundtype == AUDIO_MONO ) {
        flv_param.soundtype = FLV_MONO;
    } else if ( param->soundtype == AUDIO_STEREO ) {
        flv_param.soundtype = FLV_STEREO;
    } else {
        LinkLogError("check sound type error\n");
        goto err;
    }
    video_codec = ConvertVideoCodec( param->video_codec );
    if ( video_codec < 0 ) {
        LinkLogError("get video codec error\n");
        goto err;
    }
    audio_codec = ConvertAudioCodec( param->audio_codec );
    if ( audio_codec < 0 ) {
        LinkLogError("get audio codec error\n");
        goto err;
    }
    samplerate = ConvertAudioSampleRate( param->samplerate );
    if ( samplerate < 0 ) {
        LinkLogError("get samplerate error\n");
        goto err;
    }
    flv_param.videocodec = video_codec;
    flv_param.audiocodec = audio_codec;
    flv_param.samplerate = samplerate;
    flv_param.opaque     = ctx;
    flv_param.write_packet = FastRtmpWritePacket;
    
    ctx->flv_ctx = FLVNewContext( &flv_param );
    if ( !ctx->flv_ctx ) {
        LinkLogError("FLVNewContext error\n");
        goto err;
    }

    settings.pRtmpUrl = rtmp_url;
    settings.nRtmpUrlLen = strlen( rtmp_url );
    settings.pCertFile = param->cert_file;
    settings.nCertFileLen = strlen( param->cert_file );
    ret = FRtmpWssInit( wss_url,
                        strlen(wss_url),
                        param->timeout, 
                        &settings, 
                        &ctx->wss_ctx );
    if ( ret != 0 ) {
        LinkLogError("FRtmpWssInit error, ret = %d\n", ret );
        goto err;
    }
    pthread_mutex_init( &ctx->mutex, NULL );

    return ctx;

err:
    if ( ctx) {
        if ( ctx->flv_ctx )
            free( ctx->flv_ctx );
        free( ctx );
    }

    if ( wss_url ) {
        free( wss_url );
    }

    if ( rtmp_url ) {
        free( rtmp_url );
    }

    return NULL;
}

int LinkFastRtmpSendVideo( LinkFastRtmpContext *ctx, char *data, int len, int iskey, int64_t timestamp )
{
    FLVContext *flv_ctx = (FLVContext *)ctx->flv_ctx;
    int ret = 0;
    FrameInfo frame;

    if ( !ctx || !data || !ctx->flv_ctx || !len ) {
        LinkLogError("check param error, ctx = %p, data = %p, ctx->flv_ctx = %p, len = %d\n",
             ctx, data, ctx->flv_ctx, len );
        goto err;
    }

    LinkLogTrace("timestamp = %lld\n", timestamp );
    DBG_TRACE_TIME_START();
    pthread_mutex_lock( &ctx->mutex );
    frame.data = data;
    frame.len = len;
    frame.timestamp = timestamp;
    frame.type = TAG_TYPE_VIDEO;
    frame.iskey = iskey;
//    LinkLogTrace("frame.len = %d\n", frame.len );
    ret = FLVWriteAVData( flv_ctx, &frame );
    if ( ret != 0 ) {
        LinkLogError("FLVWriteAVData() error\n");
        goto err;
    }
//    LinkLogTrace("------> FLUSH\n");
    FLVFlush( flv_ctx );
    pthread_mutex_unlock( &ctx->mutex );
    DBG_TRACE_TIME_ELAPSED();

    return 0;

err:
    pthread_mutex_unlock( &ctx->mutex );
    return -1;
}

int LinkFastRtmpSendAudio( LinkFastRtmpContext *ctx, char *data, int len, int64_t timestamp )
{
    FLVContext *flv_ctx = (FLVContext *)ctx->flv_ctx;
    int ret = 0;
    FrameInfo frame;

    if ( !ctx || !data || !ctx->flv_ctx || !len  ) {
        LinkLogError("check param error, ctx = %p, data = %p, ctx->flv_ctx = %p, len = %d\n",
             ctx, data, ctx->flv_ctx, len );
        goto err;
    }

    DBG_TRACE_TIME_START();
    pthread_mutex_lock( &ctx->mutex );
    frame.data = data;
    frame.len = len;
    frame.timestamp = timestamp;
    frame.type = TAG_TYPE_AUDIO;
    frame.iskey = 0;
    LinkLogTrace("frame.len = %d\n", frame.len );
    ret = FLVWriteAVData( flv_ctx, &frame );
    if ( ret != 0 ) {
        LinkLogError("FLVWriteAVData error\n");
        goto err;
    }
    LinkLogTrace("------> FLUSH\n");
    FLVFlush( flv_ctx );
    pthread_mutex_unlock( &ctx->mutex );
    DBG_TRACE_TIME_ELAPSED();

    return 0;

err:
    pthread_mutex_unlock( &ctx->mutex );
    return -1;
}

int LinkFastRtmpDestroyContext( LinkFastRtmpContext *ctx )
{
    if ( !ctx )
        goto err;

    if ( ctx->flv_ctx )
        FLVDestroyContext( ctx->flv_ctx );

    if ( ctx->wss_ctx )
        FRtmpWssDestroy( ctx->wss_ctx );
        
    free( ctx );

    return 0;

err:
    return -1;
}

