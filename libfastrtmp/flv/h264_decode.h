// Last Update:2018-12-14 11:43:46
/**
 * @file h264_decode.h
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-12-14
 */

#ifndef H264_DECODE_H
#define H264_DECODE_H

#define NALU_TYPE_SLICE     1
#define NALU_TYPE_DPA       2
#define NALU_TYPE_DPB       3
#define NALU_TYPE_DPC       4
#define NALU_TYPE_IDR       5
#define NALU_TYPE_SEI       6
#define NALU_TYPE_SPS       7
#define NALU_TYPE_PPS       8
#define NALU_TYPE_AUD       9
#define NALU_TYPE_EOSEQ     10
#define NALU_TYPE_EOSTREAM  11
#define NALU_TYPE_FILL      12

#define IN const
#define OUT

#define DECODE_OK 0
#define DECODE_PARARM_ERROR -1
#define DECODE_BUF_OVERFLOW -2
#define DECODE_FRAME_FAIL   -3

typedef struct {
    char *addr;
    int size;
    int type;
} NalUnit;


extern int H264ParseNalUnit( char *_pFrame, int _nLen, OUT NalUnit *_pNalus, int *_pSize );

#endif  /*H264_DECODE_H*/
