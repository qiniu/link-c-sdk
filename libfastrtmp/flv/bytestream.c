// Last Update:2019-03-08 16:27:54
/**
 * @file bytestream.c
 * @brief 
 * @author felix
 * @version 0.1.00
 * @date 2019-03-03
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "bytestream.h"
#include "log/log.h"

ByteStream *bs_new(
                   uint8_t *buf,
                   int buf_size,
                   void *opaque,
                   int (*write_packet)( void *opaque, uint8_t *buf, int buf_size ))
{
    ByteStream *bs = (ByteStream *)malloc( sizeof(ByteStream) );
    if ( !bs || !buf )
        return NULL;
    memset( bs, 0, sizeof(ByteStream) );

    bs->buf = (char *)buf;
    bs->buf_ptr = (char *)buf;
    bs->buf_end = (char *)(buf + buf_size);
    bs->buf_size = buf_size;
    bs->opaque = opaque;
    bs->write_packet = write_packet;

    return bs;
}

void bs_w8( ByteStream *bs, int val )
{
//    LinkLogInfo("ptr : %p, start : %p, end : %p\n", bs->buf_ptr, bs->buf, bs->buf_end );
    *bs->buf_ptr++ = val;
    if ( bs->buf_ptr >= bs->buf_end )
        bs_flush( bs );
}

void bs_wl16(ByteStream *bs, unsigned int val)
{
    bs_w8(bs, (uint8_t)val);
    bs_w8(bs, (int)val >> 8);
}

void bs_wb16(ByteStream *bs, unsigned int val)
{
    bs_w8(bs, (int)val >> 8);
    bs_w8(bs, (uint8_t)val);
}

void bs_wl24(ByteStream *bs, unsigned int val)
{
    bs_wl16(bs, val & 0xffff);
    bs_w8(bs, (int)val >> 16);
}

void bs_wb24(ByteStream *bs, unsigned int val)
{
    bs_wb16(bs, (int)val >> 8);
    bs_w8(bs, (uint8_t)val);
}

void bs_wl32(ByteStream *bs, unsigned int val)
{
    bs_w8(bs, (uint8_t) val       );
    bs_w8(bs, (uint8_t)(val >> 8 ));
    bs_w8(bs, (uint8_t)(val >> 16));
    bs_w8(bs,           val >> 24 );
}

void bs_wb32(ByteStream *bs, unsigned int val)
{
    bs_w8(bs,           val >> 24 );
    bs_w8(bs, (uint8_t)(val >> 16));
    bs_w8(bs, (uint8_t)(val >> 8 ));
    bs_w8(bs, (uint8_t) val       );
}

void bs_flush( ByteStream *bs )
{
    LinkLogInfo("====> flush\n");
    if ( bs->write_packet ) {
        bs->write_packet( bs->opaque, (uint8_t *)bs->buf, bs->buf_ptr - bs->buf );
    }
    bs->buf_ptr = bs->buf;
}

int64_t bs_seek ( ByteStream *bs, int64_t offset, int whence )
{
    int64_t pos = 0;

    if ( !bs )
        return -1;

    if ( whence != SEEK_SET && whence != SEEK_CUR )
        return -1;

    if ( whence == SEEK_CUR ) {
        pos = bs->buf_ptr - bs->buf;
        pos += offset;
        if ( pos < 0 ) {
            return -1;
        }
        if ( pos > bs->buf_size ) {
            return -1;
        }
        bs->buf_ptr += pos;
    } else {
        if ( offset < 0 ) {
            LinkLogError("check offset error\n");
            return -1;
        }
        bs->buf_ptr = bs->buf + offset;
    }

    return 0;
}

int bs_write_buffer( ByteStream *bs, const unsigned char *buf, int size )
{
    int left = bs->buf_end - bs->buf_ptr;

    if ( !buf ) {
        return -1;
    }

    if ( size > left ) {
        return -1;
    }

//    LinkLogInfo("size = %d\n", size );
    memcpy( bs->buf_ptr, buf, size );
    bs->buf_ptr += size;
    if ( bs->buf_ptr >= bs->buf_end ) {
        bs_flush( bs );
    }

    return 0;
}

int64_t bs_tell( ByteStream *bs )
{
    return ( bs->buf_ptr - bs->buf );
}

