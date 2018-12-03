#ifndef __TS_LOCAL_TEST__
#define __TS_LOCAL_TEST__

#include "base.h"

int LinkGetUploadToken(OUT char *pBuf, IN int nBufLen, OUT LinkUploadZone *pZone, OUT int *pDeadline, IN const char *pUrl);
void LinkSetAk(IN char *pAk);
void LinkSetSk(IN char *pSk);
void LinkSetBucketName(IN char *_pName);
void LinkSetCallbackUrl(IN char *pUrl);


int HmacSha1(const char* pKey,  int nKeyLen, const char* pInput,  int pInputLen,
             char *pOutput, int *pOutputLen);

#endif
