#ifndef __TS_LOCAL_TEST__
#define __TS_LOCAL_TEST__

#include "base.h"

int LinkGetUploadToken(OUT void ** pJsonRoot, IN const char *pUrl,
                       IN const char *pToken, IN int nTokenLen);

int HmacSha1(const char* pKey,  int nKeyLen, const char* pInput,  int pInputLen,
             char *pOutput, int *pOutputLen);

int GetHttpRequestSign(const char * pKey, int nKeyLen, char *method, char *pUrlWithPathAndQuery, char *pContentType,
                              char *pData, int nDataLen, char *pOutput, int *pOutputLen);

#endif
