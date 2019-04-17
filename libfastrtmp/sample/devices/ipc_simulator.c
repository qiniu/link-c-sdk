// Last Update:2019-03-21 14:01:49
/**
 * @file sim_dev.c
 * @brief 
 * @author felix
 * @version 0.1.00
 * @date 2018-10-19
 */

#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <math.h>
#include "log/log.h"
#include "dev_core.h"
#include "h264_decode.h"
#include "flv.h"

#define SIM_IPC

#define MAX_NALUS (50000)
typedef struct {
    int running;
    char *videoFile;
    char *audioFile;
} SimDev;

typedef struct  {
    unsigned short syncword:    12;
    unsigned char id:                       1;
    unsigned char layer:    2;
    unsigned char protection_absent:        1;
    unsigned char profile:                  2;
    unsigned char sampling_frequency_index: 4;
    unsigned char private_bit:              1;
    unsigned char channel_configuration:    3;
    unsigned char original_copy:    1;
    unsigned char home:                     1;
} ADTSFixHeader;

typedef struct  {
    unsigned char copyright_identification_bit:1;
    unsigned char copyright_identification_start:1;
    unsigned short aac_frame_length:13;
    unsigned short adts_buffer_fullness:11;
    unsigned char number_of_raw_data_blocks_in_frame:   2;
} ADTSVarHeader;


static SimDev gSimIpc; 
static int aacfreq[13] = {96000, 88200,64000,48000,44100,32000,24000, 22050 , 16000 ,12000,11025,8000,7350};
extern CaptureDevice gSimDevCaptureDev;

int GetFileSize( char *_pFileName)
{
    FILE *fp = fopen( _pFileName, "r");
    int size = 0;

    if( !fp ) {
        LinkLogError("fopen file %s error\n", _pFileName );
        return -1;
    }

    fseek(fp, 0L, SEEK_END );
    size = ftell(fp);
    fclose(fp);

    return size;
}

void ParseAdtsFixedHeader(const unsigned char *pData, ADTSFixHeader *_pHeader) {
    unsigned long long adts = 0;
    const unsigned char *p = pData;
    adts |= *p ++; adts <<= 8;
    adts |= *p ++; adts <<= 8;
    adts |= *p ++; adts <<= 8;
    adts |= *p ++; adts <<= 8;
    adts |= *p ++; adts <<= 8;
    adts |= *p ++; adts <<= 8;
    adts |= *p ++;


    _pHeader->syncword                 = (adts >> 44);
    _pHeader->id                       = (adts >> 43) & 0x01;
    _pHeader->layer                    = (adts >> 41) & 0x03;
    _pHeader->protection_absent        = (adts >> 40) & 0x01;
    _pHeader->profile                  = (adts >> 38) & 0x03;
    _pHeader->sampling_frequency_index = (adts >> 34) & 0x0f;
    _pHeader->private_bit              = (adts >> 33) & 0x01;
    _pHeader->channel_configuration    = (adts >> 30) & 0x07;
    _pHeader->original_copy            = (adts >> 29) & 0x01;
    _pHeader->home                     = (adts >> 28) & 0x01;
}

void ParseAdtsVarHeader(const unsigned char *pData, ADTSVarHeader *_pHeader) {
    unsigned long long adts = 0;
    adts  = pData[0]; adts <<= 8;
    adts |= pData[1]; adts <<= 8;
    adts |= pData[2]; adts <<= 8;
    adts |= pData[3]; adts <<= 8;
    adts |= pData[4]; adts <<= 8;
    adts |= pData[5]; adts <<= 8;
    adts |= pData[6];

    _pHeader->copyright_identification_bit = (adts >> 27) & 0x01;
    _pHeader->copyright_identification_start = (adts >> 26) & 0x01;
    _pHeader->aac_frame_length = (adts >> 13) & ((int)pow(2, 14) - 1);
    _pHeader->adts_buffer_fullness = (adts >> 2) & ((int)pow(2, 11) - 1);
    _pHeader->number_of_raw_data_blocks_in_frame = adts & 0x03;
}

void *AudioCaptureTask( void *param )
{
    if ( !gSimIpc.audioFile ) {
        LinkLogError("check audio file error\n");
    }

    int len = GetFileSize( gSimIpc.audioFile );
    if ( len <= 0 ) {
        LinkLogError("GetFileSize error\n");
        exit(1);
    }

    FILE *fp = fopen( gSimIpc.audioFile, "r" );
    if( !fp ) {
        LinkLogError("open file %s error\n", gSimIpc.audioFile );
        return NULL;
    }

    char *buf_ptr = (char *) malloc ( len );
    if ( !buf_ptr ) {
        LinkLogError("malloc error\n");
        exit(1);
    }
    memset( buf_ptr, 0, len );
    fread( buf_ptr, 1, len, fp );
    
    int offset = 0;
    int64_t timeStamp = 0;
    int64_t interval = 0;
    int count = 0;

    while( gSimIpc.running ) {
        ADTSFixHeader fix;
        ADTSVarHeader var;

        if ( offset + 7 <= len ) {
            ParseAdtsFixedHeader( (unsigned char *)( buf_ptr + offset), &fix );
            int size = fix.protection_absent == 1 ? 7 : 9;
            LinkLogInfo("size = %d\n", size );
            ParseAdtsVarHeader( (unsigned char *)( buf_ptr + offset), &var );
            if ( offset + size + var.aac_frame_length <= len ) {
                if ( gSimDevCaptureDev.audioCb ) {
                    count++;
                    LinkLogInfo("fix.sampling_frequency_index = %d\n", fix.sampling_frequency_index );
                    interval = ((1024*1000.0)/aacfreq[fix.sampling_frequency_index]);
                    LinkLogInfo("interval = %d\n", interval );
                    timeStamp = interval * count;
                    gSimDevCaptureDev.audioCb( buf_ptr + offset, var.aac_frame_length, timeStamp, 0, 0 );
                }
            }
            offset += var.aac_frame_length;
            LinkLogInfo("var.aac_frame_length = %d\n", var.aac_frame_length );
            //LinkLogInfo("interval = %ld\n", interval );
            usleep( interval*1000 );
        }
    }
    
    return NULL;
}

void *VideoCaptureTask( void *param )
{
    double timeStamp = 0;

    if ( !gSimIpc.videoFile ) {
        LinkLogError("check video file error\n");
        return NULL;
    }

    int len = GetFileSize( gSimIpc.videoFile );
    if ( len <= 0 ) {
        LinkLogError("GetFileSize error\n");
        exit(1);
    }

    FILE *fp = fopen( gSimIpc.videoFile, "r" );
    if( !fp ) {
        LinkLogError("open file %s error\n", gSimIpc.videoFile );
        return NULL;
    }

    char *buf_ptr = (char *) malloc ( len );
    if ( !buf_ptr ) {
        LinkLogError("malloc error\n");
        return NULL;
    }

    memset( buf_ptr, 0, len );
    fread( buf_ptr, 1, len, fp );

    NalUnit nalus[MAX_NALUS], *pnalu = nalus;
    int size = MAX_NALUS;
    int ret = H264ParseNalUnit( buf_ptr, len, nalus, &size );
    if ( ret != DECODE_OK ) {
        LinkLogError("H264ParseNalUnit error\n");
        return NULL;
    }
    LinkLogTrace("size = %d\n", size );
    if ( size < 10 ) {
        LinkLogError("parse nalu fail\n");
        exit(1);
    }
    int keyframe_size = 0;
    while( gSimIpc.running ) {
        switch( pnalu->type ) {
        case NALU_TYPE_SPS:
            if ( pnalu->addr[-4] != 0x00 ) {
                LinkLogError("check startcode of sps error\n");
                return NULL;
            }
            char *pstart = pnalu->addr;
            if ( !pstart ) {
                LinkLogError("check pstart error\n");
                return NULL;
            }
            pnalu++;
            if ( pnalu->type != NALU_TYPE_PPS ) {
                LinkLogError("the next nalu after sps is not pps\n");
                return NULL;
            }

            if ( (pnalu+1)->type == NALU_TYPE_SEI 
                 || ( pnalu+1)->type == NALU_TYPE_IDR ) {
                pnalu++;
                LinkLogInfo("key frame has nalu type : %d\n", pnalu->type );
                if ( (pnalu+1)->type == NALU_TYPE_SEI 
                     || ( pnalu+1)->type == NALU_TYPE_IDR ) {
                    pnalu++;
                    LinkLogInfo("key frame has nalu type : %d\n", pnalu->type );
                }
            }
            keyframe_size = pnalu->addr - pstart + 4 + pnalu->size;
            LinkLogInfo("keyframe_size = %d\n", keyframe_size );
            if ( keyframe_size <= 0 ) {
                LinkLogError("keyframe_size error\n");
                return NULL;
            }
            if ( gSimDevCaptureDev.videoCb ) {
                gSimDevCaptureDev.videoCb( pstart-4, keyframe_size, 1, timeStamp,
                                           0, 0, 0 );
            } else {
                LinkLogError("gSimDevCaptureDev.videoCb is NULL\n");
                return NULL;
            }
            break;
        case NALU_TYPE_SEI:
        case NALU_TYPE_IDR:
        case NALU_TYPE_SLICE:
        case NALU_TYPE_AUD:
            if ( gSimDevCaptureDev.videoCb ) {
                gSimDevCaptureDev.videoCb( pnalu->addr-4, pnalu->size+4, 0, timeStamp,
                                           0, 0, 0 );
            } else {
                LinkLogError("gSimDevCaptureDev.videoCb is NULL\n");
                return NULL;
            }
            break;
        }
        pnalu++;
        if ( pnalu - nalus >= size  ) {
            pnalu = nalus;
        }
        timeStamp += 40;
        usleep( 40000 );
    }
    return NULL;
}

void *SimFlvTask( void *arg )
{
    FILE *fp = NULL;
    static int audio = 0, video = 0;

    LinkLogInfo("filename : %s\n", gSimIpc.videoFile );
    if ( gSimIpc.videoFile ) {
        fp = fopen( gSimIpc.videoFile, "r" );
        if ( !fp ) {
            printf("open file %s fail\n", gSimIpc.videoFile  );
            exit(1);
        }
    }

    if ( !fp ) {
        LinkLogError("check file pointer fail, file  : %s\n", gSimIpc.videoFile );
        exit( 1 );
    }

    while( !feof(fp) ) {
        int size = 0;
        char type = 0;
        char iskey = 0;
        int timestamp = 0, comoposition_time = 0;


        LinkLogInfo("pos = %ld\n", ftell(fp) );
        fread( (char *)&type, 1, 1, fp );
        if ( feof(fp) ) {
            LinkLogInfo("end of file\n");
            exit(1);
        }
        LinkLogInfo("type = %d\n", type );
        if ( type == 1 ) {
            fread( (char *)&iskey, 1, 1, fp );
            LinkLogInfo("iskey = %d\n", iskey );
            if ( iskey != 0 && iskey != 1  && iskey != 2 ) {
                printf("iskey error\n");
                exit(1);
            }
        } else if ( type != 0 ) {
            printf("type error\n");
            exit(1);
        }

        fread( (char *)&timestamp, 1, 4, fp ) ;
        LinkLogInfo("timestamp = %d\n", timestamp );
        if ( timestamp < 0 ) {
            printf("timeStamp error\n");
            exit(1);
        }
        fread( (char *)&comoposition_time, 1, 4, fp );
        if ( comoposition_time < 0 ) {
            printf("comoposition_time error\n");
            exit(1);
        }
        LinkLogInfo("comoposition_time = %d\n", comoposition_time );
        DbgFLVSetCompositionTime( comoposition_time );

        fread( (char *)&size, 1, 4, fp );
        LinkLogInfo("size = %d\n", size );
        if ( size <= 0 || size > 200000 ) {
            LinkLogError("error size = %d\n", size );
            exit(1);
        }

        char *buf_ptr = (char *)malloc( size );
        if ( !buf_ptr ) {
            printf("malloc fail\n");
            exit(1);
        }

        memset( buf_ptr, 0, size );
        fread( buf_ptr, 1, size, fp );
        DUMPBUF( buf_ptr+size-8, 8 );
        if ( type == 1 && gSimDevCaptureDev.videoCb ) {
            video++;
            gSimDevCaptureDev.videoCb( buf_ptr, size, iskey, timestamp, 0, 0, 0  );
        } else if ( type == 0 && gSimDevCaptureDev.audioCb) {
            gSimDevCaptureDev.audioCb( buf_ptr, size, timestamp, 0, 0 );
            audio++;
        } else {
            printf("error\n");
            exit(1);
        }
        free( buf_ptr );
    }

    printf("totoal %d audios %d videos\n", video, audio );

    return NULL;
}

static int SimDevInitIPC( VideoFrameCb videoCb, AudioFrameCb audioCb, char *videoFile, char *audioFile )
{
    gSimDevCaptureDev.audioCb = audioCb;
    gSimDevCaptureDev.videoCb = videoCb;
    gSimDevCaptureDev.audioEnable = 1;
    gSimIpc.videoFile = videoFile;
    gSimIpc.audioFile = audioFile;

    pthread_t thread;
#ifdef SIM_IPC
    pthread_create( &thread, NULL, VideoCaptureTask, NULL );
    pthread_create( &thread, NULL, AudioCaptureTask, NULL );
#else
    pthread_create( &thread, NULL, SimFlvTask, NULL );
#endif
    return 0;
}

int SimDevStartStream()
{
    gSimIpc.running = 1;
    return 0;
}


int SimDevStopStream()
{
    gSimIpc.running = 0;
    return 0;
}


CaptureDevice gSimDevCaptureDev =
{
    0,
    0,
    0,
    0,
    0,
    NULL,
    NULL,
    SimDevInitIPC,
    NULL,
    NULL,
    SimDevStartStream,
    NULL,
    NULL,
    NULL,
    NULL,
    SimDevStopStream
};

void __attribute__((constructor)) SimDevRegistrerToCore()
{
    LinkLogInfo("sim dev register\n");
    CaptureDeviceRegister( &gSimDevCaptureDev );
}


