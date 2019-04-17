// Last Update:2019-03-13 14:05:05
/**
 * @file flv.c
 * @brief flv enc
 * @author qiniu
 * @version 0.1.00
 * @date 2019-02-26
 */

#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "flv.h"
#include "log/log.h"
#include "h264_decode.h"
#include "adts.h"
#include "bytestream.h"

#define MEMSET( p ) memset( p, 0, sizeof(*p) )
#define LOG_PARAM_FAIL() LinkLogError("check param fail!!!\n")
#define BUF_SIZE (200*1024)
#define MAX_NALUS_PER_FRAME (10)
#define AUDIO_TAG_HEADER_LEN (1)
#define AAC_PACKET_TYPE_LEN (1)
#define AAC_SEQUENCE_HEADER_LEN (2)
#define VIDEO_TAG_HEADER_LEN (1)
#define AVC_PACKET_TYPE_LEN (1)
#define COMPISITION_TIME_LEN (3)
#define TAG_TIMESTAMP_LEN (3)
#define TAG_TIMESTAMPEXTENDED_LEN (1)
#define STREAM_ID_LEN (3)
#define TAG_DATASIZE_LEN (3)
#define TAG_TYPE_LEN (1)
#define FLV_TAG_LEN ( TAG_TYPE_LEN + \
                      TAG_DATASIZE_LEN + \
                      TAG_TIMESTAMP_LEN + \
                      TAG_TIMESTAMPEXTENDED_LEN +\
                      STREAM_ID_LEN )

#define MAX_ADTS_PER_FRAME 128

enum {
    PACKET_TYPE_SEQUENCE_HEADER,
    PACKET_TYPE_NALU,
};

enum {
    PACKET_TYPE_AAC_SEQUENC_HEADER,
    PACKET_TYPE_AAW_RAW
};

#if FLV_DEBUG
static int gCompositionTime;

void DbgFLVSetCompositionTime( int compisition_time )
{
    gCompositionTime = compisition_time;
}
#endif

FLVContext *FLVNewContext( FLVParam *param )
{
    FLVContext *ctx = (FLVContext *) malloc ( sizeof(FLVContext) );

    if ( !param ) {
        LOG_PARAM_FAIL();
        goto err;
    }

    if ( !ctx ) {
        LinkLogError("malloc FLVContext error!!!\n");
        goto err;
    }
    MEMSET( ctx );
    ctx->buf = (char *) malloc ( BUF_SIZE );
    if ( !ctx->buf ) {
        LinkLogError("malloc ctx->buf error!!!\n");
        goto err;
    }
    MEMSET( ctx->buf );
    ctx->bs = bs_new( (uint8_t *)ctx->buf, BUF_SIZE, param->opaque, param->write_packet );
    if ( !ctx->bs ) {
        LinkLogError("bs_new error\n");
        goto err;
    }
    ctx->videocodec = param->videocodec;
    ctx->audiocodec = param->audiocodec;
    ctx->samplerate = param->samplerate;
    ctx->samplesize = param->samplesize;
    ctx->soundtype  = param->soundtype;
    ctx->videocfgsend = 1;
    ctx->audiocfgsend = 1;
    ctx->last_audio_timestamp = -1;
    ctx->last_video_timestamp = -1;
    ctx->tag_pos = 0;

    return ctx;

err:
    if ( ctx ) {
        if ( ctx->buf ) {
            free( ctx->buf );
        }
        free( ctx );
    }
    return NULL;
}

/* AVCDecoderConfigurationRecord */
static int FLVWriteH264Config( FLVContext *ctx, FrameInfo *frame )
{
    if ( !ctx || !ctx->buf || !frame || !frame->data || !ctx->bs ) {
        LOG_PARAM_FAIL();
        goto err;
    }

    LinkLogDebug("write h264 config\n");
    ByteStream *bs = ctx->bs;
    NalUnit nalus[MAX_NALUS_PER_FRAME];
    NalUnit *pnalu = nalus;
    int size = MAX_NALUS_PER_FRAME;
    int ret = 0;

    bs_w8( bs, 0x01 );// configurationVersion

    MEMSET( nalus );
    ret = H264ParseNalUnit( frame->data, frame->len, nalus, &size );
    if ( ret != DECODE_OK ) {
        LinkLogError("H264ParseNalUnit error, ret = %d\n", ret );
        goto err;
    }

    LinkLogTrace("nalu size : %d\n", size );
    if ( size ) {
        int i = 0;

        for ( i=0; i<size; i++ ) {
            switch( pnalu->type ) {
            case NALU_TYPE_SPS:
                LinkLogInfo("write sps\n");
                if ( pnalu->addr && pnalu->size ) {
                    char *pdata = pnalu->addr + 1;// the first byte is nalu type

                    bs_w8( bs, *pdata++ );// AVCProfileIndication
                    bs_w8( bs, *pdata++ );// profile_compatibility
                    bs_w8( bs, *pdata++ );// AVCProfileIndication
                    bs_w8( bs, 0xff );// lengthSizeMinusOne
                    /* sps nums */
                    bs_w8( bs, 0xE1 );
                    /* sps data length */
                    bs_wb16( bs, pnalu->size );
                    /* sps data */
                    bs_write_buffer( bs, (uint8_t *)pnalu->addr, pnalu->size );
                } else {
                    LinkLogError("get sps data error\n");
                    goto err;
                }
                break;
            case NALU_TYPE_PPS:
                LinkLogInfo("write pps\n");
                if ( pnalu->addr && pnalu->size ) {
                    /* pps nums */
                    bs_w8( bs, 0x01 );
                    /* pps data length */
                    bs_wb16( bs, pnalu->size );
                    /* pps data */
                    bs_write_buffer( bs, (uint8_t *)pnalu->addr, pnalu->size );
                } else {
                    LinkLogError("get pps data error\n");
                    goto err;
                }
                break;
            default:
                break;
            }
            pnalu++;
        }
    } else {
        LinkLogError("decode h264 nalu fail, check size error, size = %d\n", size );
        goto err;
    }

    return 0;
err:
    return -1;
}

static int FLVUpdateTagDataSize( FLVContext *ctx )
{
    if ( !ctx || !ctx->buf|| !ctx->bs ) {
        LOG_PARAM_FAIL();
        goto err;
    }
    ByteStream *bs = ctx->bs;
    int64_t pos = bs_tell( bs );
    int pre_tag_size = pos - ctx->tag_pos;
    int datasize = pre_tag_size - FLV_TAG_LEN;

    LinkLogTrace("datasize = %d\n", datasize );
    /* update tag data size */
    bs_seek( bs, ctx->tag_pos+TAG_TYPE_LEN, SEEK_SET );
    bs_wb24( bs, datasize );
    bs_seek( bs, pos, SEEK_SET );

    /* PreviousTagSize : 4bytes */
    bs_wb32( bs, pre_tag_size );

    return 0;
err:
    return -1;
}

static int FLVWriteTagHeader( FLVContext *ctx, int type, int _len, int _timestamp, int _streamid  )
{
    if ( !ctx || !ctx->buf || !ctx->bs ) {
        LOG_PARAM_FAIL();
        goto err;
    }

    if ( _len > 0x0FFFFFF ) {
        LinkLogError("data too long\n");
        goto err;
    }

    ByteStream *bs = ctx->bs;
    int64_t pos = bs_tell( bs );

    
    /* save new tag address */
    ctx->tag_pos = pos;

    /* Tag type : 1bytes */
    bs_w8( bs, type );
    /* Tag data length : 3 bytes */
    bs_wb24( bs, 0 );// will be updated later
    /* timestamp : 3bytes */
    bs_wb24( bs, _timestamp );
    /* TimestampExtended : 1bytes */
    bs_w8( bs, _timestamp>>24 );
    /* StreamId : 3bytes */
    bs_wb24( bs, _streamid );

    return 0;

err:
    return -1;
}

static int FLVWriteNALU( FLVContext *ctx, char *data, int len )
{
    if ( !data || !ctx || !ctx->bs ) {
        goto err;
    }

    ByteStream *bs = ctx->bs;
    /* nalu size */
    bs_wb32( bs, len );
    /* nalu data */
    bs_write_buffer( bs, (uint8_t *)data, len );

    return 0;
err:
    return -1;
}

static int FLVWriteH264Frame( FLVContext *ctx, FrameInfo *frame )
{
    if ( !ctx || !ctx->buf || !frame || !frame->data ) {
        LOG_PARAM_FAIL();
        goto err;
    }

    LinkLogDebug("write h264 frame\n");
    NalUnit nalus[MAX_NALUS_PER_FRAME];
    NalUnit *pnalu = nalus;
    int size = MAX_NALUS_PER_FRAME;
    int ret = H264ParseNalUnit( frame->data, frame->len, pnalu, &size );

    if ( ret != DECODE_OK ) {
        LinkLogError("H264ParseNalUnit error, ret = %d\n", ret );
        goto err;
    }

    int i = 0;
    LinkLogTrace("nalu count = %d\n", size );
    for ( i=0; i<size; i++ ) {
        if ( pnalu ) {
            switch( pnalu->type ) {
            case NALU_TYPE_SPS:
            case NALU_TYPE_PPS:
            case NALU_TYPE_IDR:
            case NALU_TYPE_SLICE: 
            case NALU_TYPE_SEI:
                if ( pnalu->size < 0 ) {
                    LinkLogError("check nalu size error,pnalu->type = %d, pnalu->size = %d\n", pnalu->type, pnalu->size );
                    goto err;
                }
                if ( pnalu->addr && pnalu->size ) {
                    LinkLogInfo("nalutype : %d, size : %d\n", pnalu->type, pnalu->size );
                    ret = FLVWriteNALU( ctx, pnalu->addr, pnalu->size ); 
                    if ( ret != 0 ) {
                        LinkLogError("FLVWriteNALU error\n");
                        goto err;
                    }
                } else {
                    LinkLogWarn("h264 key fream no sps\n");
                }
                break;
                break;
            default:
                break;
            }
        }
        pnalu++;
    }

    return 0;

err:
    return -1;
}

static int FLVWriteVideoTag( FLVContext *ctx, FrameInfo *frame, int isconfig )
{
    if ( !ctx || !ctx->buf || !frame || !ctx->bs ) {
        LOG_PARAM_FAIL();
        goto err;
    }

    ByteStream *bs = ctx->bs;
    int ret = 0;
    int64_t timestamp = 0;
    int size = 0;

    /*
     * Time in milliseconds at which the data in this tag applies.
     * This value is relative to the first tag in the FLV file,
     * which always has a timestamp of 0.
     * */
    if ( ctx->last_video_timestamp == -1 ) {
        ctx->last_video_timestamp = frame->timestamp;
    } else {
        timestamp = frame->timestamp - ctx->last_video_timestamp;
    }
    LinkLogTrace( "timestamp = %lld\n", timestamp );

    /*
     * Length of the message.
     * Number of bytes after StreamID to end of tag (Equal to length of the tag – 11)
     * */
    size = VIDEO_TAG_HEADER_LEN;
    if ( ctx->videocodec == FLV_CODECID_H264 ) {
        size += AVC_PACKET_TYPE_LEN + COMPISITION_TIME_LEN;
        /*
         * we don't know the length of pps and sps
         * so the length of h264 config will update later
         * */
        if ( !isconfig ) {
            size += frame->len;
        }
    } else {
        size += frame->len;
    }

    ret = FLVWriteTagHeader( ctx, TAG_TYPE_VIDEO, size, timestamp, 0 );
    if ( ret != 0 ) {
        LinkLogError("FLVAppendTagHeader error\n");
        goto err;
    }
    /* Frame Type : 4bit */
    char frametype = 0;
    if ( frame->iskey ) {
        frametype = FLV_KEY_FRAME<<4;
    } else {
        frametype = FLV_INTER_FRAME<<4;
    }
    /* CodecID : 4bit */
    bs_w8( bs, frametype|(ctx->videocodec & 0xff) );
    /* AVCPacketType : 8bit */
    if ( ctx->videocodec == FLV_CODECID_H264 ) {
        if ( frame->iskey && isconfig) {
            bs_w8( bs, PACKET_TYPE_SEQUENCE_HEADER );
        } else {
            bs_w8( bs, PACKET_TYPE_NALU );
        }
        /* CompositionTime : 3bytes  */
#if FLV_DEBUG
        bs_wb24( bs, gCompositionTime );
#else
        bs_wb24( bs, 0 );// for ipc, dts = pts
#endif
        /* VideoTagBody */
        if ( frame->iskey && ctx->videocfgsend && isconfig ) {
            int ret = FLVWriteH264Config( ctx, frame );
            if ( ret != 0 ) {
                LinkLogError("FLVWriteH264Config error\n");
                goto err;
            }
            ctx->videocfgsend = 0;
        } else {
            if ( FLVWriteH264Frame( ctx, frame ) != 0 ) {
                LinkLogError("FLVAppendKeyframe error\n");
                goto err;
            }
        }
    } else {
        bs_write_buffer( bs, (uint8_t *)frame->data, frame->len );
    }
    FLVUpdateTagDataSize( ctx );
    return 0;

err:
    return -1;
}

static int aacfreq[13] = {96000, 88200,64000,48000,44100,32000,24000, 22050 , 16000 ,12000,11025,8000,7350};
static int findFrequencyIndex(int freq) {
    int i = 0;
    for (i = 0; i < 13; i++) {
        if (aacfreq[i] == freq)
            return i;
    }
    return -1;
}

static int FLVWriteAudioConfig( FLVContext *ctx, FrameInfo *frame )
{
    if ( !ctx || !ctx->buf || !frame || !ctx->bs ) {
        LOG_PARAM_FAIL();
        goto err;
    }

    ByteStream *bs = ctx->bs;

    unsigned char c[2] = {0, 0};
    int idx = findFrequencyIndex(ctx->samplerate);
    if (idx < 0)
	    return -1;
    c[0] |= 0x10;
    c[0] |= (idx>>1);
    c[1] |= (idx<<7);
    if (ctx->soundtype == FLV_MONO) {
        c[1] |= 0x08;;
    } else {
        c[1] |= 0x01;;
    }
    bs_w8( bs, c[0] );
    bs_w8( bs, c[1] );

    return 0;

err:
    return -1;
}

static int FLVWriteAudioTag( FLVContext *ctx, FrameInfo *frame, int isconfig )
{
    if ( !ctx || !ctx->buf || !frame || !ctx->bs ) {
        LOG_PARAM_FAIL();
        goto err;
    }

    ByteStream *bs = ctx->bs;
    int ret = 0;
    int timestamp = 0;
    int size = 0;

    /*
     * Time in milliseconds at which the data in this tag applies.
     * This value is relative to the first tag in the FLV file,
     * which always has a timestamp of 0.
     * */
    if ( ctx->last_audio_timestamp  == -1 ) {
        ctx->last_audio_timestamp = frame->timestamp;
    } else {
        timestamp = frame->timestamp - ctx->last_audio_timestamp;
    }

    /*
     * Length of the message.
     * Number of bytes after StreamID to end of tag (Equal to length of the tag – 11)
     * */
    size = AUDIO_TAG_HEADER_LEN;
    if ( ctx->audiocodec == FLV_CODECID_AAC ) {
        size += AAC_PACKET_TYPE_LEN;
        if ( isconfig ) {
            size += AAC_SEQUENCE_HEADER_LEN;
        } else {
            size += frame->len;
        }
    } else {
        size += frame->len;
    }

    ret = FLVWriteTagHeader( ctx, TAG_TYPE_AUDIO, size, timestamp, 0 );
    if ( ret != 0 ) {
        LinkLogError("FLVAppendTagHeader error\n");
        goto err;
    }
    bs_w8( bs, (ctx->audiocodec)<<4 |
              ctx->samplerate << 2 |
              ctx->samplesize << 1 |
              ctx->soundtype );
    if ( ctx->audiocodec == FLV_CODECID_AAC ) {
        if ( isconfig ) {
            bs_w8( bs, PACKET_TYPE_AAC_SEQUENC_HEADER);
            FLVWriteAudioConfig( ctx, frame );
        } else {
            Adts adts[ MAX_ADTS_PER_FRAME ], *padts = adts;
            char *pbuf = (char *) malloc ( frame->len ),  *psave = NULL;;
            int i = 0;

            if ( !pbuf ) {
                goto err;
            }
            size = MAX_ADTS_PER_FRAME;
            memset( adts, 0, sizeof(adts) );
            psave = pbuf;
            ret = AacDecodeAdts( frame->data, frame->len, adts, &size );
            if ( ret != ADTS_DECODE_OK ) {
                LinkLogInfo("adts decode fail, ret = %d\n", ret );
                free( psave );
                goto err;
            }

            if ( size <= 0 || size >= MAX_ADTS_PER_FRAME ) {
                LinkLogError("check nSize error, nSize = %d\n", size );
                free( psave );
                goto err;
            }

            memset( pbuf, 0, frame->len );
            for ( i=0; i<size; i++ ) {
                if ( padts->addr && padts->size > 0 ) {
                    memcpy( pbuf, padts->addr, padts->size );
                    pbuf += padts->size;
                } else {
                    LinkLogError("found invalid adts!!!\n");
                }
                padts++;
            }


            bs_w8( bs, PACKET_TYPE_AAW_RAW );
            bs_write_buffer( bs, (uint8_t*)psave, pbuf - psave );
            free( psave );
        }
    } else {
        bs_write_buffer( bs, (uint8_t*)frame->data, frame->len );
    }

    FLVUpdateTagDataSize( ctx );
    return 0;
err:
    return -1;
}



int FLVWriteScriptData( FLVContext *ctx, Map *maps, int size )
{
    if ( !ctx || !ctx->buf || !maps || !ctx->bs ) {
        LOG_PARAM_FAIL();
        goto err;
    }

    ByteStream *bs = ctx->bs;
    char *name = "onMetaData";
    int len = strlen( name );
    int i = 0;
    int ret = 0;

    LinkLogInfo("len = 0x%x\n", len );
    ret = FLVWriteTagHeader( ctx, TAG_TYPE_SCRIPT, 0, 0, 0 );
    if ( ret != 0 ) {
        LinkLogError("FLVAppendTagHeader error\n");
        goto err;
    }
    /* type */
    bs_w8( bs, FLV_SCRIPT_TYPE_STRING );
    /* StringLength : 2bytes */
    bs_wb16( bs, len );
    /* StringData */
    bs_write_buffer( bs, (uint8_t *)name, len );

    /* type */
    bs_w8( bs, FLV_SCRIPT_TYPE_ECMAARRAY );
    /* ECMAArrayLength : 4bytes */
    LinkLogTrace("size = %d\n", size );
    bs_wb32( bs, size );
    for ( i=0; i<size; i++ ) {
        /* PropertyName */
        if ( maps->key ) {
            len = strlen(maps->key );
            /* StringLength : 2bytes */
            bs_wb16( bs, len );
            /* StringData */
            bs_write_buffer( bs, (uint8_t *)maps->key, strlen(maps->key) );
        } else {
            LinkLogError("check map key  error\n");
            goto err;
        }
        /* PropertyData */
        if ( maps->value ) {

            /* type */
            bs_w8( bs, FLV_SCRIPT_TYPE_STRING );
            len = strlen(maps->value );
            /* StringLength : 2bytes */
            bs_wb16( bs, len );
            /* StringData */
            bs_write_buffer( bs, (uint8_t *)maps->value, strlen(maps->value) );
        } else {
            LinkLogError("check map value error\n");
            goto err;
        }
        maps++;
    }
    /* SCRIPTDATAOBJECTEND  : 3bytes */
    bs_w8( bs, 0x00 );
    bs_w8( bs, 0x00 );
    bs_w8( bs, 0x09 );

    FLVUpdateTagDataSize( ctx );
    return 0;

err:
    return -1;
}


int FLVWritePrivateData( FLVContext *ctx, char *realip )
{
    if ( !ctx || !ctx->buf || !realip || !ctx->bs ) {
        LOG_PARAM_FAIL();
        goto err;
    }

    ByteStream *bs = ctx->bs;
    FLVWriteTagHeader( ctx, TAG_TYPE_PRIVATE, 0, 0, 0x2333  );
    bs_write_buffer( bs, (uint8_t *)realip, strlen(realip) );
    FLVUpdateTagDataSize( ctx );
    
    return 0;

err:
    return -1;
}

int FLVWriteAVData( FLVContext *ctx, FrameInfo *frame )
{
    if ( !ctx || !ctx->buf || !frame | !frame->len ) {
        LOG_PARAM_FAIL();
        goto err;
    }

    int len = FLV_TAG_LEN;
    int ret = 0;

    if ( frame->type == TAG_TYPE_VIDEO ) {
        if ( ctx->videocodec == FLV_CODECID_H264 
              && frame->iskey 
              && ctx->videocfgsend ) {
            ret = FLVWriteVideoTag( ctx, frame, 1 );
            if ( ret != 0 ) {
                LinkLogError("FLVAppendVideoTag error\n");
                goto err;
            }
            LinkLogTrace("------> FLUSH\n");
            FLVFlush( ctx );
            ret = FLVWriteVideoTag( ctx, frame, 0 );
            if ( ret != 0 ) {
                LinkLogError("FLVAppendVideoTag error\n");
                goto err;
            }
            ctx->videocfgsend = 0;
        } else {
            len += frame->len;
            if ( ctx->videocodec == FLV_CODECID_H264 ) {
                len += AVC_PACKET_TYPE_LEN + COMPISITION_TIME_LEN;
            }

            ret = FLVWriteVideoTag( ctx, frame, 0 );
            if ( ret != 0 ) {
                LinkLogError("FLVAppendVideoTag error\n");
                goto err;
            }
        }
    } else if ( frame->type == TAG_TYPE_AUDIO ) {
        if ( ctx->audiocodec == FLV_CODECID_AAC &&
             ctx->audiocfgsend ) {
            len += AAC_SEQUENCE_HEADER_LEN + AUDIO_TAG_HEADER_LEN; 
            ret = FLVWriteAudioTag( ctx, frame, 1 );
            if ( ret != 0 ) {
                LinkLogError("FLVAppendAudioTag error\n");
                goto err;
            }
            ctx->audiocfgsend = 0;
            LinkLogTrace("------> FLUSH\n");
            FLVFlush( ctx );
            ret = FLVWriteAudioTag( ctx, frame, 0 );
            if ( ret != 0 ) {
                LinkLogError("FLVWriteAudioTag error\n");
                goto err;
            }
        } else { 
            ret = FLVWriteAudioTag( ctx, frame, 0 );
            if ( ret != 0 ) {
                LinkLogError("FLVAppendAudioTag error\n");
                goto err;
            }
        }
    } else {
        LinkLogError("check frame type error\n");
        goto err;
    }

    return 0;

err:
    return -1;
}

int FLVFlush( FLVContext *ctx )
{
    if ( !ctx || !ctx->bs )
        return -1;

    ByteStream *bs = ctx->bs;

    bs_flush( bs );
    ctx->tag_pos = 0;

    return 0;
}

int FLVDestroyContext( FLVContext *ctx )
{
    if ( ctx ) {
        if ( ctx->buf ) {
            free( ctx->buf );
        }
        free( ctx );
    }
    
    return 0;
}

