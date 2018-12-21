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
                      printf("EncodedSign %s\n", EncodedSign);
              } else if (index == 2) {
                      encodedPutPolicy = p;
                      printf("encodedPutPolicy %s\n", encodedPutPolicy);
              }
              p = strtok(NULL, delim);
              ++index;
        }
        if (EncodedSign == NULL || encodedPutPolicy == NULL) {
              return 0;
        }
	int ret = memcmp(ak, parserDak, strlen(ak));
        if (ret != 0) {
                printf("DAK is not correct\n");
                return 0;
        }
        unsigned char md[20] = {0};
        unsigned int len = 20;
        printf("sign %s date %s \n", sk, encodedPutPolicy);

        unsigned char* digest = HMAC(EVP_sha1(), sk, strlen(sk), (const unsigned char*)encodedPutPolicy, strlen(encodedPutPolicy), md, &len);

        printf("digest %s len %d\n", digest, len);
        char test[100] = {0};
        int testlen = 100;
        int realsize = urlsafe_b64_encode(md, len, test, testlen);
        printf("test %s len %d\n", test, realsize);
        ret = memcmp(test, EncodedSign, realsize);
        if (ret != 0) {
                printf("token is not correct\n");
                return 0;
        }
        return 1;
}

#if 0
int testLinkVerify() {
        char key[] = "eNFrLXKG3R8TJ-DJA9YiMjLwuEfQnw8krrDuZzoy";
        char Pwd[1000] = "Ves3WTXC8XnEHT0I_vacEQQz-9jrJZxNExcmarzQ:nbnRXShp85g6N3kU3LgJC7Ewpw4=:eyJzY29wZSI6ImlwY2FtZXJhIiwiZGVhZGxpbmUiOjE1NDQxMDA1NjAsImNhbGxiYWNrVXJsIjoiaHR0cDovL2lvdC1saW5rLmRldi5xaW5pdS5pby9xaW5pdS91cGxvYWQvY2FsbGJhY2siLCJjYWxsYmFja0JvZHkiOiJ7XCJrZXlcIjpcIiQoa2V5KVwiLFwiaGFzaFwiOlwiJChldGFnKVwiLFwiZnNpemVcIjokKGZzaXplKSxcImJ1Y2tldFwiOlwiJChidWNrZXQpXCIsXCJuYW1lXCI6XCIkKHg6bmFtZSlcIiwgXCJkdXJhdGlvblwiOlwiJChhdmluZm8uZm9ybWF0LmR1cmF0aW9uKVwifSIsImNhbGxiYWNrQm9keVR5cGUiOiJhcHBsaWNhdGlvbi9qc29uIiwiZGVsZXRlQWZ0ZXJEYXlzIjo3fQ==";
        int ret = LinkVerify("Ves3WTXC8XnEHT0I_vacEQQz-9jrJZxNExcmarzQ", key, Pwd);
        if (ret == 0) {
                printf("token is not correct\n");
        } else {
                printf("token is correct\n");
        }
	return ret;
}
#endif
