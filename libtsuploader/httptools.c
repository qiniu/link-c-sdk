#include "httptools.h"
#include "libghttp/ghttp.h"
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


static int linkSimpleHttpRequest(IN int isPost,
                                 IN const char * pUrl, OUT char* pBuf, IN int nBufLen, OUT int* pRespLen,
                                 IN const char *pReqBody, IN int nReqBodyLen, IN const char *pContentType,
                                 IN const char *pToken, IN int nTokenLen) {
        
        
        ghttp_status status;
        char tokenbuf[40+40+18+2+1+1+10]; //40 for sha1, 40 for ak, 18 for QiniuLinkingDevice, 2for space, 1for :, 1for null terminator
                                          //10 for backup
        
        ghttp_request * pRequest = ghttp_request_new();
        if (pRequest == NULL) {
                return LINK_NO_MEMORY;
        }
        if (ghttp_set_uri(pRequest, pUrl) == -1) {
                ghttp_request_destroy(pRequest);
                return LINK_ARG_ERROR;
        }
        
        if (isPost) {
                ghttp_set_type(pRequest, ghttp_type_post);
                if (pContentType == NULL) {
                        ghttp_set_header(pRequest, http_hdr_Content_Type, "application/x-www-form-urlencoded");
                }
                if (nReqBodyLen > 0 && pReqBody != NULL)
                        ghttp_set_body(pRequest, pReqBody, nReqBodyLen);
        }
        if (pContentType != NULL) {
                ghttp_set_header(pRequest, http_hdr_Content_Type, pContentType);
        }

        if (pToken) {
                const char * prefix = "QiniuLinkingDevice ";
                memcpy(tokenbuf, prefix, strlen(prefix));
                memcpy(tokenbuf+strlen(prefix), pToken, nTokenLen);
                tokenbuf[strlen(prefix) + nTokenLen] = 0;
                
                ghttp_set_header(pRequest, "Authorization", tokenbuf);
        }
        
        status = ghttp_prepare(pRequest);
        if (status != 0) {
                LinkLogError("ghttp_prepare:%s", ghttp_get_error(pRequest));
                ghttp_request_destroy(pRequest);
                return LINK_GHTTP_FAIL;
        }
        
        status = ghttp_process(pRequest);
        if (status == ghttp_error) {
                if (ghttp_is_timeout(pRequest)) {
                        LinkLogError("ghttp_process timeout[%s] url[%s]", ghttp_get_error(pRequest), pUrl);
                        ghttp_request_destroy(pRequest);
                        return LINK_TIMEOUT;
                } else {
                        char *pBody = ghttp_get_body(pRequest);
                        if (pBody == NULL)
                                LinkLogError("ghttp_process fail[%s] url[%s]", ghttp_get_error(pRequest), pUrl);
                        else
                                LinkLogError("ghttp_process fail[%s] resp[%s]", ghttp_get_error(pRequest), ghttp_get_body(pRequest));
                        ghttp_request_destroy(pRequest);
                        return LINK_GHTTP_FAIL;
                }
        }
        
        int httpCode = ghttp_status_code(pRequest);
        if (httpCode / 100 != 2) {
                LinkLogError("%s error httpcode:%d [%s]", isPost ?  "LinkSimpleHttpPost" : "LinkSimpleHttpGet",
                             httpCode, ghttp_get_body(pRequest));
                ghttp_request_destroy(pRequest);
                return httpCode;
        }


        int nBodyLen = ghttp_get_body_len(pRequest);
        *pRespLen = nBodyLen;
        char *buf = ghttp_get_body(pRequest);//test
        
        int nCopyLen = nBodyLen > nBufLen - 1 ? nBufLen - 1 : nBodyLen;
        memcpy(pBuf, buf, nCopyLen);
        ghttp_request_destroy(pRequest);
        pBuf[nCopyLen] = 0;
        
        if (nCopyLen <= nBufLen - 1)
                return LINK_SUCCESS;
        return LINK_BUFFER_IS_SMALL;
}

int LinkSimpleHttpGet(IN const char * pUrl, OUT char* pBuf, IN int nBufLen, OUT int* pRespLen) {
        
        return linkSimpleHttpRequest(0, pUrl, pBuf, nBufLen, pRespLen, NULL, 0, NULL, NULL, 0);
}

int LinkSimpleHttpGetWithToken(IN const char * pUrl, OUT char* pBuf, IN int nBufLen, OUT int* pRespLen,
                               IN const char *pToken, IN int nTokenLen) {
        
        return linkSimpleHttpRequest(0, pUrl, pBuf, nBufLen, pRespLen, NULL, 0, NULL, pToken, nTokenLen);
}

int LinkSimpleHttpPost(IN const char * pUrl, OUT char* pBuf, IN int nBufLen, OUT int* pRespLen,
                       IN const char *pReqBody, IN int nReqBodyLen, IN const char *pContentType) {
        
        return linkSimpleHttpRequest(1, pUrl, pBuf, nBufLen, pRespLen, pReqBody, nReqBodyLen, pContentType, NULL, 0);
}

int LinkSimpleHttpPostWithToken(IN const char * pUrl, OUT char* pBuf, IN int nBufLen, OUT int* pRespLen,
                       IN const char *pReqBody, IN int nReqBodyLen, IN const char *pContentType,
                                IN const char *pToken, IN int nTokenLen) {
        
        return linkSimpleHttpRequest(1, pUrl, pBuf, nBufLen, pRespLen, pReqBody, nReqBodyLen, pContentType, pToken, nTokenLen);
}

//https://developer.qiniu.com/kodo/manual/1201/access-token
int LinkGetUserConfig(IN const char * pUrl, OUT char* pBuf, IN int nBufLen, OUT int* pRespLen,
                              IN const char *pToken, IN int nTokenLen) {
        
        return linkSimpleHttpRequest(0, pUrl, pBuf, nBufLen, pRespLen, NULL, 0, NULL, pToken, nTokenLen);
}
