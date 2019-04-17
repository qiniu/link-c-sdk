// Last Update:2019-03-14 14:24:50
/**
 * @file h264_decode.c
 * @brief 
 * @author qiniu
 * @version 0.1.00
 * @date 2018-12-14
 */

#include <string.h>
#include <arpa/inet.h>
#include "log/log.h"
#include "h264_decode.h"

/*
 * the NAL start prefix code (it can also be 0x00000001, depends on the encoder implementation)
 * */
#define NALU_START_CODE (0x00000001)

static int H264DecodeNalu( int *_pIndex, char *_pData, int _nStartCodeLen, OUT NalUnit *_pNalus, int _nMax )
{
    NalUnit *pNalu = _pNalus + *_pIndex;

    if ( !_pNalus ) {
        return DECODE_PARARM_ERROR;
    }

    pNalu->addr = _pData;
    LinkLogInfo("pNalu->addr = %p\n", pNalu->addr );
    pNalu->type = (*_pData) & 0x1F;
    if ( *_pIndex > 0 ) {
        ( _pNalus + *_pIndex - 1)->size = _pData - ( _pNalus + *_pIndex - 1 )->addr - _nStartCodeLen;

        //LinkLogInfo("( _pNalus + *_pIndex - 1)->size = %d\n", ( _pNalus + *_pIndex - 1)->size);

        if (( _pNalus + *_pIndex - 1)->size < 0 ) {
            LinkLogError("deocde size < 0, size = %d\n", ( _pNalus + *_pIndex - 1)->size);
            return DECODE_FRAME_FAIL;
        }
    }
    (*_pIndex) ++;
    if ( *_pIndex >= _nMax ) {
        LinkLogError("DECODE_BUF_OVERFLOW\n");
        return DECODE_BUF_OVERFLOW;
    }

    return 0;
}

int H264ParseNalUnit( char *_pFrame, int _nLen, OUT NalUnit *_pNalus, int *_pSize )
{
    char *pStart = _pFrame, *pEnd = pStart + _nLen;
    unsigned int *pStartCode = NULL;
    int nIndex = 0;
    int ret = 0;

    if ( !_pFrame || _nLen <= 0 || !_pNalus || !_pSize ) {
        return DECODE_PARARM_ERROR;
    }

    while( pStart < pEnd-4 ) {// if the last 4 byte  has 00000001, but no data, so ignore it
        pStartCode = (unsigned int *)pStart;
        if ( htonl(*pStartCode) == NALU_START_CODE ) {
            pStart += 4;// skip start code
            ret = H264DecodeNalu( &nIndex, pStart, 4, _pNalus, *_pSize );
            if ( ret < 0 ) {
                LinkLogError("DECODE_FRAME_FAIL\n");
                return DECODE_FRAME_FAIL;
            }
        } else if (( htonl(*pStartCode) >> 8 ) == NALU_START_CODE ) {
            pStart += 3;
            ret = H264DecodeNalu( &nIndex, pStart, 3,  _pNalus, *_pSize );
            if ( ret < 0 ) {
                LinkLogError("DECODE_FRAME_FAIL\n");
                return DECODE_FRAME_FAIL;
            }
        } else {
            pStart++;
        }
    }

    /* the last one */
    (_pNalus + nIndex - 1)->size = pEnd - (_pNalus + nIndex - 1) ->addr ;
    if ( (_pNalus + nIndex - 1)->size < 0 ) {
        LinkLogError("index = %d\n", nIndex );
        DUMPBUF( _pFrame,  _nLen );
        return DECODE_FRAME_FAIL;
    }

    *_pSize = nIndex;

    return DECODE_OK;
}

