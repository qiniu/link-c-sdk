#include "security.h"
#include <openssl/hmac.h>

/*hmac-sha1 output is 160bit(20 byte)*/
int HmacSha1(const char * pKey, int nKeyLen, const char * pInput, int pInputLen,
        char *pOutput, int *pOutputLen) { //EVP_MAX_MD_SIZE
        
        const EVP_MD * engine = engine = EVP_sha1();
  
        HMAC_CTX ctx;
        HMAC_CTX_init(&ctx);
        HMAC_Init_ex(&ctx, pKey, nKeyLen, engine, NULL);
        HMAC_Update(&ctx, (unsigned char*)pInput, pInputLen);
        
        unsigned int nOutLen = 0;
        HMAC_Final(&ctx, (unsigned char*)pOutput, &nOutLen);
        HMAC_cleanup(&ctx);
        *pOutputLen = (int)nOutLen;
        return 0;
}

