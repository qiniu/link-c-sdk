#ifndef __FIX_JSON_H__
#define __FIX_JSON_H__
#include "base.h"

int LinkGetJsonStringByKey(IN const char *pJson, IN const char *pKeyWithDoubleQuotation, OUT char *pBuf,IN OUT int *pBufLen);

int LinkGetJsonIntByKey(const char *pJson, const char *pKeyWithDoubleQuotation);

int LinkGetDeleteAfterDaysFromUptoken(char * pToken, int *pDeleteAfterDays);

int LinkGetPolicyFromUptoken(char * pToken, int *pDeleteAfterDays, int *pDeadline);

int LinkGetBucketFromUptoken(const char *pToken, char *pBuf, int *pBufLen);

int LinkGetBucketFromUptoken(const char *pToken, char *pBuf, int *pBufLen);

#endif
