#include "tsuploader.h"
#include "cJSON/cJSON.h"
#include "b64/urlsafe_b64.h"
#include <openssl/hmac.h>

int LinkVerify(char *ak, char *sk, char* token)
{
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

        unsigned char* digest = HMAC(EVP_sha1(), sk, strlen(sk), (const unsigned char*)encodedPutPolicy, strlen(encodedPutPolicy), md, &len);

        char test[100] = {0};
        int testlen = 100;
        int realsize = urlsafe_b64_encode(md, len, test, testlen);
        ret = memcmp(test, EncodedSign, realsize);
        if (ret != 0) {
                printf("token is not correct\n");
                return LINK_ERROR;
        }
        return LINK_SUCCESS;
}

#if 0
int testLinkVerify() {
        char key[] = "";
        char Pwd[1000] = "";
        int ret = LinkVerify("", key, Pwd);
        if (ret == 0) {
                printf("token is not correct\n");
        } else {
                printf("token is correct\n");
        }
	return ret;
}
#endif
