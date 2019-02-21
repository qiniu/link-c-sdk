// Last Update:2019-02-21 19:36:42
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

int CacheHandle( Queue *pQueue, LinkTsMuxUploader *pUploader,
                 int _nStreamType, char *_pFrame,
                 int _nLen, int _nIskey, double _dTimeStamp
  )
{
    Frame frame;
    static __thread int count = STREAM_CACHE_SIZE;

    if ( !pQueue || !pUploader  ) {
        return -1;
    }

    memset( &frame, 0, sizeof(frame) );
    frame.data = (char *) malloc ( _nLen );
    if ( !frame.data ) {
        printf("%s %s %d malloc error\n", __FILE__, __FUNCTION__, __LINE__);
        return -1;
    }
    memcpy( frame.data, _pFrame, _nLen );
    frame.len = _nLen;
    frame.timeStamp = _dTimeStamp;
    frame.isKey = _nIskey;
    if (_nIskey) {
        frame.nCurSysTime = LinkGetCurrentNanosecond();	    
    }
    pQueue->enqueue( pQueue, (void *)&frame, sizeof(frame) );

    if (  pQueue->getSize( pQueue ) == gIpc.config.cacheSize ) {
        memset( &frame, 0, sizeof(frame) );
        pQueue->dequeue( pQueue, (void *)&frame, NULL );
        if ( !frame.data ) {
            printf("%s %s %d data is NULL\n", __FILE__, __FUNCTION__, __LINE__ );
            return -1;
        }

        if (  gIpc.detectMoving == ALARM_MOTION_DETECT  ) {
            count = STREAM_CACHE_SIZE;
            if ( _nStreamType == TYPE_VIDEO ) {
                LinkPushVideo( pUploader, frame.data, frame.len, (int64_t)frame.timeStamp, frame.isKey, 0 , frame.nCurSysTime);
            } else {
                LinkPushAudio( pUploader, frame.data, frame.len, (int64_t)frame.timeStamp , 0);
            }
        } else if ( gIpc.detectMoving == ALARM_MOTION_DETECT_DISAPPEAR ) {
            if ( count-- > 0 ) {
                if ( _nStreamType == TYPE_VIDEO ) {
                    LinkPushVideo( pUploader, frame.data, frame.len, (int64_t)frame.timeStamp, frame.isKey, 0 , frame.nCurSysTime);
                } else {
                    LinkPushAudio( pUploader, frame.data, frame.len, (int64_t)frame.timeStamp, 0);
                }
            } else {
                DbgReportLog( STREAM_MAIN, TYPE_VIDEO, "use cache, not detect moving" );
            }
        } else {
            DbgReportLog( STREAM_MAIN, TYPE_VIDEO, "use cache, not detect moving" );
        }
        free( frame.data );
    }

    return 0;
}

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

