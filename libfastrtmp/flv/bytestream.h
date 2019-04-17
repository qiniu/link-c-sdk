// Last Update:2019-03-04 10:21:05
/**
 * @file bytestream.h
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2019-03-03
 */

#ifndef BYTESTREAM_H
#define BYTESTREAM_H
#include <stdint.h>

typedef struct {
    char *buf;
    char *buf_ptr;
    char *buf_end;
    int buf_size;
    int pos;
    int (*write_packet)( void *opaque, uint8_t *buf, int buf_size );
    void *opaque;
}ByteStream;

ByteStream *bs_new(
                   uint8_t *buf,
                   int buf_size,
                   void *opaque,
                   int (*write_packet)( void *opaque, uint8_t *buf, int buf_size ));

void bs_w8( ByteStream *bs, int val );
void bs_wl16(ByteStream *bs, unsigned int val);
void bs_wb16(ByteStream *bs, unsigned int val);
void bs_wl24(ByteStream *bs, unsigned int val);
void bs_wb24(ByteStream *bs, unsigned int val);
void bs_wl32(ByteStream *bs, unsigned int val);
void bs_wb32(ByteStream *bs, unsigned int val);
void bs_flush( ByteStream *bs );
int64_t bs_seek ( ByteStream *bs, int64_t offset, int whence );
int64_t bs_tell( ByteStream *bs );
int bs_write_buffer( ByteStream *bs, const unsigned char *buf, int size  );

#endif  /*BYTESTREAM_H*/
