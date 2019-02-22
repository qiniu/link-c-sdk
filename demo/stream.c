// Last Update:2019-02-22 20:29:19
/**
 * @file stream.c
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-10-22
 */
#include <stdio.h>
#include "stream.h"
#include "queue.h"
#include "uploader.h"
#include "dev_core.h"
#include "main.h"
#include "dbg.h"
#include "mymalloc.h"

int StreamCommonHandle( int _nStreamType, int _nStream, char *_pFrame, int _nLen, int _nIskey,
                        double _dTimeStamp )
{
    LinkTsMuxUploader *pUploader = NULL;

    DbgTraceTimeStamp( _nStreamType, _dTimeStamp, _nStream );
    pUploader = gIpc.stream[_nStream].uploader;
    if ( _nStreamType == TYPE_VIDEO ) {
        LinkPushVideo( pUploader,  _pFrame, _nLen, (int64_t)_dTimeStamp, _nIskey, 0, 0);
    } else {
        LinkPushAudio( pUploader,  _pFrame, _nLen, (int64_t)_dTimeStamp, 0);
    }

    return 0;
}


int VideoGetFrameCb( char *_pFrame,
                   int _nLen, int _nIskey, double _dTimeStamp,
                   unsigned long _nFrameIndex, unsigned long _nKeyFrameIndex,
                   int streamno )
{
    StreamCommonHandle( TYPE_VIDEO, streamno, _pFrame, _nLen, _nIskey, _dTimeStamp );

    return 0;
}

int AudioGetFrameCb( char *_pFrame, int _nLen, double _dTimeStamp,
                     unsigned long _nFrameIndex, int streamno )
{
    StreamCommonHandle( TYPE_AUDIO, streamno, _pFrame, _nLen, 0, _dTimeStamp );

    return 0;
}

