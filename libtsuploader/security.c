#include "security.h"
#include <openssl/hmac.h>
#include "base.h"
#include "wolfssl/wolfcrypt/sha.h"

/*hmac-sha1 output is 160bit(20 byte)*/
int HmacSha1(const char * pKey, int nKeyLen, const char * pInput, int nInputLen,
        char *pOutput, int *pOutputLen) { //EVP_MAX_MD_SIZE
        
        *pOutputLen=20;
        unsigned char * res = HMAC(EVP_sha1(), pKey, nKeyLen, pInput, nInputLen, pOutput, pOutputLen);
        if (res == NULL) {
                return LINK_WOLFSSL_ERR;
        }
        *pOutputLen = 20;
        return LINK_SUCCESS;
        
#if 0
        Hmac hmac;
        memset(&hmac, 0, sizeof(hmac));
        int ret = 0;
        
        ret = wc_HmacSetKey(&hmac, SHA, (byte*)pKey, nKeyLen);
        if (ret != 0) {
                return LINK_WOLFSSL_ERR;
        }

        if( (ret = wc_HmacUpdate(&hmac, (byte*)pInput, nInputLen)) != 0) {
                return LINK_WOLFSSL_ERR;
        }
        
        if ((ret = wc_HmacFinal(&hmac, (byte*)pOutput)) != 0) {
                return LINK_WOLFSSL_ERR;
        }
        *pOutputLen = 20;
        return LINK_SUCCESS;
#endif

#if 0
        int ret = 0;
        Sha sha;
        
        ret = wc_InitSha(&sha);
        if (ret != 0) {
                return LINK_WOLFSSL_ERR;
        }
        INVALID_DEVID;
        wc_HmacInit();
        ret = wc_ShaUpdate(&sha, (unsigned char*)pInput, nInputLen);
        if (ret != 0) {
                wc_ShaFree(&sha);
                return LINK_WOLFSSL_ERR;
        }
        
        ret = wc_ShaFinal(&sha, (byte *)pOutput);
        if (ret != 0) {
                wc_ShaFree(&sha);
                return LINK_WOLFSSL_ERR;
        }
        wc_ShaFree(&sha);
        *pOutputLen = 20;
        
        return LINK_SUCCESS;
#endif

#if 0
        int ret = 0;
        EVP_MD_CTX md_ctx;
        
        EVP_MD_CTX_init(&md_ctx);

        ret = EVP_DigestInit(&md_ctx, EVP_sha1());
        if (ret == 0) {
                 EVP_MD_CTX_cleanup(&md_ctx);
                return LINK_WOLFSSL_ERR;
        }
        
        ret = EVP_DigestUpdate(&md_ctx, (unsigned char*)pInput, nInputLen);
        if (ret == 0) {
                EVP_MD_CTX_cleanup(&md_ctx);
                return LINK_WOLFSSL_ERR;
        }
        
        unsigned int nOutLen = 0;
        ret = EVP_DigestFinal(&md_ctx, (unsigned char*)pOutput, &nOutLen);
        if (ret == 0) {
                 EVP_MD_CTX_cleanup(&md_ctx);
                return LINK_WOLFSSL_ERR;
        }
         EVP_MD_CTX_cleanup(&md_ctx);
        *pOutputLen = (int)nOutLen;
        return LINK_SUCCESS;
#endif
        
#if 0
        const EVP_MD * engine = engine = EVP_sha1();
        int ret = 0;
  
        HMAC_CTX ctx ;
        memset(&ctx, 0, sizeof(ctx));
        ret = HMAC_CTX_init(&ctx);
        if (ret == 0) {
                return LINK_WOLFSSL_ERR;
        }
        ret = HMAC_Init_ex(&ctx, pKey, nKeyLen, engine, NULL);
        if (ret == 0) {
                return LINK_WOLFSSL_ERR;
        }
        ret = HMAC_Update(&ctx, (unsigned char*)pInput, nInputLen);
        if (ret == 0) {
                return LINK_WOLFSSL_ERR;
        }
        
        unsigned int nOutLen = 0;
        ret = HMAC_Final(&ctx, (unsigned char*)pOutput, &nOutLen);
        if (ret == 0) {
                return LINK_WOLFSSL_ERR;
        }
        HMAC_cleanup(&ctx);
        *pOutputLen = (int)nOutLen;
        return LINK_SUCCESS;
#endif
}

#if 0
#ifdef strlen
#undef strlen
size_t mystrlen(const char *s) {
        size_t l = strlen(s);
        if (l > 120) {
                return l;
        }
        return l;
}
#endif

#ifdef strcpy
#undef strcpy
char *mystrcpy(char *dest, const char *src) {
        size_t l = strlen(src);
        if (l>120) {
                return strcpy(dest, src);
        }
        return strcpy(dest, src);
}

#endif

#endif
