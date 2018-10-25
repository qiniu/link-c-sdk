#include "fixjson.h"

char * GetErrorMsg(IN const char *_pJson, OUT char *_pBuf, IN int _nBufLen)
{
        const char * pStart = _pJson;
        pStart = strstr(pStart, "\\\"error\\\"");
        printf("\\\"error\\\"");
        if (pStart == NULL)
                return NULL;
        pStart += strlen("\\\"error\\\"");
        
        while(*pStart != '"') {
                pStart++;
        }
        pStart++;
        
        const char * pEnd = strchr(pStart+1, '\\');
        if (pEnd == NULL)
                return NULL;
        int nLen = pEnd - pStart;
        if(nLen > _nBufLen - 1) {
                nLen = _nBufLen - 1;
        }
        memcpy(_pBuf, pStart, nLen);
        _pBuf[nLen] = 0;
        return _pBuf;
}
