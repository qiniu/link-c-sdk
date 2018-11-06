#ifndef __FIX_JSON_H__
#define __FIX_JSON_H__
#include "base.h"
/*
 *
 */
char * GetErrorMsg(IN const char *_pJson, OUT char *_pBuf, IN int _nBufLen);

int GetJsonContentByKey(IN const char *pJson, IN const char *pKeyWithDoubleQuotation, OUT char *pBuf,IN OUT int *pBufLen);

#endif
