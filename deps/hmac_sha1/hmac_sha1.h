/**
 *
 * Copyright (C) 2010 Creytiv.com
 */

#ifndef __HMAC_SHA1_H__
#define __HMAC_SHA1_H__

#include <stdint.h>

/*******************************************************************************
 *
 *                           HMAC-SHA1
 *
 ******************************************************************************/

/**
 * Function to compute the digest
 *
 * @param k   Secret key
 * @param lk  Length of the key in bytes
 * @param d   Data
 * @param ld  Length of data in bytes
 * @param out Digest output
 * @param t   Output buffer length
 *
 * @return Size of digest output
 */
size_t hmac_sha1(const uint8_t * k, size_t lk,
               const uint8_t * d, size_t ld,
               uint8_t * out, size_t t
               );


/*******************************************************************************
 *
 *                           SHA-1
 *
 ******************************************************************************/

/*
 * public api for steve reid's public domain SHA-1 implementation
 * this file is in the public domain
 * */

/** SHA-1 Context */
typedef struct {
    uint32_t state[5];   /**< Context state */
    uint32_t count[2];   /**< Counter       */
    uint8_t  buffer[64]; /**< SHA-1 buffer  */
} SHA1_CTX;

/** SHA-1 Context (OpenSSL compat) */
typedef SHA1_CTX SHA_CTX;

/** SHA-1 Digest size in bytes */
#define SHA1_DIGEST_SIZE 20
/** SHA-1 Digest size in bytes (OpenSSL compat) */
#define SHA_DIGEST_LENGTH SHA1_DIGEST_SIZE

void SHA1_Init(SHA1_CTX* context);
void SHA1_Update(SHA1_CTX* context, const void *p, size_t len);
void SHA1_Final(uint8_t digest[SHA1_DIGEST_SIZE], SHA1_CTX* context);

#endif // __HMAC_SHA1_H__
