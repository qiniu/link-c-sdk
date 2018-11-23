// Last Update:2018-11-19 18:02:11
/**
 * @file md5.h
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-11-19
 */

#ifndef MD5_H
#define MD5_H

#undef BIG_ENDIAN_HOST
typedef unsigned int u32;


/****************
 * Rotate a 32 bit integer by n bytes
 */
#if defined(__GNUC__) && defined(__i386__)
static inline u32
rol( u32 x, int n)
{
	__asm__("roll %%cl,%0"
		:"=r" (x)
		:"0" (x),"c" (n));
	return x;
}
#else
#define rol(x,n) ( ((x) << (n)) | ((x) >> (32-(n))) )
#endif


typedef struct {
    u32 A,B,C,D;	  /* chaining variables */
    u32  nblocks;
    unsigned char buf[64];
    int  count;
} MD5_CONTEXT;

extern void
md5_init( MD5_CONTEXT *ctx );
extern void
md5_write( MD5_CONTEXT *hd, unsigned char *inbuf, size_t inlen);
extern void
md5_final( MD5_CONTEXT *hd );


#endif  /*MD5_H*/
