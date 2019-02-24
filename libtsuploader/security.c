#include "security.h"
#include "base.h"
#include "hmac_sha1/hmac_sha1.h"
#include "b64/urlsafe_b64.h"

int GetHttpRequestSign(const char * pKey, int nKeyLen, char *method, char *pUrlWithPathAndQuery, char *pContentType,
                              char *pData, int nDataLen, char *pOutput, int *pOutputLen) {
        int isHttps = 0;
        if (memcmp(pUrlWithPathAndQuery, "http", 4) == 0) {
                if (pUrlWithPathAndQuery[4] == 's')
                        isHttps = 1;
        }
        char *hostStart = pUrlWithPathAndQuery+7+isHttps;
        char *hostEnd = strchr(hostStart, '/');
        if (hostEnd == NULL) {
                LinkLogError("no path in url:", pUrlWithPathAndQuery);
                return LINK_ARG_ERROR;
        }
        
        int nBufLen = strlen(pUrlWithPathAndQuery);
        assert(nBufLen < 192); //TODO
        nBufLen = nBufLen + nDataLen + 256;
        char *buf = malloc(nBufLen);
        if (buf == NULL) {
                return LINK_NO_MEMORY;
        }
        
        int nOffset = snprintf(buf, nBufLen, "%s ", method);

        nOffset+= snprintf(buf+nOffset, nBufLen - nOffset, "%s", hostEnd);
        
        int nHostLen = hostEnd - hostStart;
        nOffset+= snprintf(buf+nOffset, nBufLen - nOffset, "\nHost: ");
        memcpy(buf + nOffset, hostStart, nHostLen);
        nOffset += nHostLen;

        if (pContentType) {
                nOffset+= snprintf(buf+nOffset, nBufLen - nOffset, "\nContent-Type: %s", pContentType);
        }
        
        buf[nOffset++] = '\n';
        buf[nOffset++] = '\n';
        if (nDataLen > 0) {
                memcpy(buf + nOffset, pData, nDataLen);
                nOffset += nDataLen;
        }
        
        char hsha1[20];
        int ret = hmac_sha1(pKey, nKeyLen, buf, nOffset, hsha1, sizeof(hsha1));
        free(buf);
        if (ret != 20) {
                return LINK_ERROR;
        }
        int outlen = urlsafe_b64_encode(hsha1, 20, pOutput, *pOutputLen);
        *pOutputLen = outlen;
        return LINK_SUCCESS;
}

