#include "fixjson.h"
#include "b64/b64.h"

int LinkGetJsonStringByKey(const char *pJson, const char *pKeyWithDoubleQuotation, char *pBuf, int *pBufLen) {

        char *pKeyStart = strstr(pJson, pKeyWithDoubleQuotation);
        if (pKeyStart == NULL) {
                return LINK_JSON_FORMAT;
        }
        pKeyStart += strlen(pKeyWithDoubleQuotation);
        while(*pKeyStart++ != '\"') {
        }
        
        char *pKeyEnd = strchr(pKeyStart, '\"');
        if (pKeyEnd == NULL) {
                return LINK_JSON_FORMAT;
        }
        int len = pKeyEnd - pKeyStart;
        if (len >= *pBufLen) {
                return LINK_BUFFER_IS_SMALL;
        }
        memcpy(pBuf, pKeyStart, len);
        
        *pBufLen = len;
        return LINK_SUCCESS;
}

int LinkGetJsonIntByKey(const char *pJson, const char *pKeyWithDoubleQuotation) {
        
        char *pExpireStart = strstr(pJson, pKeyWithDoubleQuotation);
        if (pExpireStart == NULL) {
                return 0;
        }
        pExpireStart += strlen(pKeyWithDoubleQuotation);
        
        char days[10] = {0};
        int nStartFlag = 0;
        int nDaysLen = 0;
        char *pDaysStrat = NULL;
        while(1) {
                if (*pExpireStart >= 0x30 && *pExpireStart <= 0x39) {
                        if (nStartFlag == 0) {
                                pDaysStrat = pExpireStart;
                                nStartFlag = 1;
                        }
                        nDaysLen++;
                }else {
                        if (nStartFlag)
                                break;
                }
                pExpireStart++;
        }
        memcpy(days, pDaysStrat, nDaysLen);
        return atoi(days);
}

static int getTokenPolicy(const char *pToken, char **ppPolicy) {
        char * pPolicy = strchr(pToken, ':');
        if (pPolicy == NULL) {
                return LINK_ARG_ERROR;
        }
        pPolicy++;
        pPolicy = strchr(pPolicy, ':');
        if (pPolicy == NULL) {
                return LINK_ARG_ERROR;
        }
        
        pPolicy++;
        *ppPolicy = pPolicy;
        return LINK_SUCCESS;
}

int getTokenPlainPolicy(const char *pToken, char **pJson) {
        
        char * pPolicy = NULL;
        int ret = getTokenPolicy(pToken, &pPolicy);
        if (ret != LINK_SUCCESS) {
                return ret;
        }
        
        int nPolicyLen = strlen(pPolicy);
        int len = (nPolicyLen + 2) * 3 / 4 + 1;
        char *pPlain = malloc(len);
        
        ret = b64_decode( pPolicy, nPolicyLen, pPlain, len);
        if (ret == 0) {
                free(pPlain);
                return LINK_ARG_ERROR;
        }
        
        *pJson = pPlain;
        pPlain[len - 1] = 0;
        
        return LINK_SUCCESS;
}

int LinkGetBucketFromUptoken(const char *pToken, char *pBuf, int *pBufLen) {
        
        char *pJson = NULL;;
        int ret = getTokenPlainPolicy(pToken, &pJson);
        if (ret != LINK_SUCCESS) {
                return ret;
        }
        
        char *pKeyWithDoubleQuotation = "\"scope\"";
        char *pBucketStart = strstr(pJson, pKeyWithDoubleQuotation);
        if (pBucketStart == NULL) {
                free(pJson);
                return LINK_JSON_FORMAT;
        }
        pBucketStart += strlen(pKeyWithDoubleQuotation);
        while(*pBucketStart++ != '\"') {
        }
        
        char *pBucketEnd = strchr(pBucketStart, '\"');
        if (pBucketEnd == NULL) {
                free(pJson);
                return LINK_JSON_FORMAT;
        }
        
        char *pBucketTmpEnd = strchr(pBucketStart, ':');
        if (pBucketTmpEnd != NULL) {
                if (pBucketTmpEnd < pBucketEnd)
                        pBucketEnd = pBucketTmpEnd;
        }
        
        int len = pBucketEnd - pBucketStart;
        if (len >= *pBufLen) {
                free(pJson);
                return LINK_BUFFER_IS_SMALL;
        }
        if (len == 0) {
                free(pJson);
                return LINK_JSON_FORMAT;
        }
        memcpy(pBuf, pBucketStart, len);
        pBuf[len] = 0;
        
        *pBufLen = len;
        free(pJson);
        return LINK_SUCCESS;
}


int LinkGetDeleteAfterDaysFromUptoken(char * pToken, int *pDeleteAfterDays) {
        
        char * pJson = NULL;
        int ret = getTokenPlainPolicy(pToken, &pJson);
        if (ret != LINK_SUCCESS) {
                return ret;
        }
        
        ret = LinkGetJsonIntByKey(pJson, "\"deleteAfterDays\"");
        free(pJson);
        *pDeleteAfterDays = ret;
        return LINK_SUCCESS;
}
