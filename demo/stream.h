// Last Update:2018-10-25 15:08:51
/**
 * @file stream.h
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-06-29
 */

#ifndef STREAM_H
#define STREAM_H

typedef enum {
    TYPE_VIDEO,
    TYPE_AUDIO,
} StreamType;

typedef struct {
    char localfile[512];
    char uploadName[1024];
} JpegInfo;

extern int VideoGetFrameCb( char *_pFrame, 
                   int _nLen, int _nIskey, double _dTimeStamp, 
                   unsigned long _nFrameIndex, unsigned long _nKeyFrameIndex, 
                   int streamno);

extern int AudioGetFrameCb( char *_pFrame, int _nLen, double _dTimeStamp,
                     unsigned long _nFrameIndex, int streamno );

#endif  /*STREAM_H*/
