#include "tsuploader.h"
#include "cJSON/cJSON.h"
#include "b64/urlsafe_b64.h"
#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/hmac.h>


int LinkVerify(const char *_ak, size_t _akLen, const char *_sk, size_t _skLen, const char* _token, size_t _tokenLen)
{
        if (_ak == NULL || _sk == NULL || _token == NULL) {
                return LINK_ERROR;
        }
        if (_akLen > 512 || _skLen > 512 || _tokenLen > 4096) {
                return LINK_ERROR;
        }
        char ak[512] = {0};
        strncpy(ak, _ak, _akLen);
        char sk[512] = {0};
        strncpy(sk, _sk, _skLen);
        char token[4096] = {0};
        strncpy(token, _token, _tokenLen);


        char* EncodedSign = NULL;
        char* encodedPutPolicy = NULL;
        char *delim = ":";
        char *p = strtok(token, delim);
        int index = 0;
        char * parserDak;
        while(p != NULL) {
              printf("%s \n", p);
              if (index == 0) {
                      parserDak = p;
              } else if (index == 1) {
                      EncodedSign = p;
              } else if (index == 2) {
                      encodedPutPolicy = p;
              }
              p = strtok(NULL, delim);
              ++index;
        }
        if (EncodedSign == NULL || encodedPutPolicy == NULL) {
              return LINK_ERROR;
        }
	int ret = memcmp(ak, parserDak, strlen(ak));
        if (ret != 0) {
                printf("DAK is not correct\n");
                return LINK_ERROR;
        }

        unsigned char md[20] = {0};
        unsigned int len = 20;
        Hmac hmac;
        memset(&hmac, 0, sizeof(hmac));

        ret = wc_HmacSetKey(&hmac, SHA, (byte*)sk, strlen(sk));
        if (ret != 0) {
                return LINK_WOLFSSL_ERR;
        }

        if( (ret = wc_HmacUpdate(&hmac, (byte*)encodedPutPolicy, strlen(encodedPutPolicy))) != 0) {
                return LINK_WOLFSSL_ERR;
        }

        if ((ret = wc_HmacFinal(&hmac, (byte*)md)) != 0) {
                return LINK_WOLFSSL_ERR;
        }

        char test[100] = {0};
        int testlen = 100;
        int realsize = urlsafe_b64_encode(md, len, test, testlen);
        ret = memcmp(test, EncodedSign, realsize);
        if (ret != 0) {
                printf("token is not correct\n");
                return LINK_FALSE;
        }
        return LINK_TRUE;
}

