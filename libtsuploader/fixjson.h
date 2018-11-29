#ifndef __FIX_JSON_H__
#define __FIX_JSON_H__
#include "base.h"

int LinkGetJsonStringByKey(IN const char *pJson, IN const char *pKeyWithDoubleQuotation, OUT char *pBuf,IN OUT int *pBufLen);

int LinkGetDeleteAfterDaysFromUptoken(char * pToken, int *pDeleteAfterDays);

int LinkGetBucketFromUptoken(const char *pToken, char *pBuf, int *pBufLen);

#endif
