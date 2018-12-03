#ifndef __TS_LOCAL_TEST__
#define __TS_LOCAL_TEST__

#include "base.h"

int LinkGetUploadToken(OUT char *pBuf, IN int nBufLen, OUT LinkUploadZone *pZone, OUT int *pDeadline, IN const char *pUrl);

int HmacSha1(const char* pKey,  int nKeyLen, const char* pInput,  int pInputLen,
             char *pOutput, int *pOutputLen);

#endif
