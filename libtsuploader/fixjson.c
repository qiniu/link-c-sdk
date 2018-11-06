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

int GetJsonContentByKey(const char *pJson, const char *pKeyWithDoubleQuotation, char *pBuf, int *pBufLen) {

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
