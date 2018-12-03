#include "httptools.h"
#include <ghttp.h>
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
                } else {
                        ghttp_set_header(pRequest, http_hdr_Content_Type, pContentType);
                }
                if (nReqBodyLen > 0 && pReqBody != NULL)
                        ghttp_set_body(pRequest, pReqBody, nReqBodyLen);
        }
        if (pToken) {
                const char * prefix = " QiniuLinkingDevice ";
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
                        LinkLogError("ghttp_process timeout [%s]", ghttp_get_error(pRequest));
                        ghttp_request_destroy(pRequest);
                        return LINK_TIMEOUT;
                } else {
                        LinkLogError("ghttp_process fail [%s] resp[%s]", ghttp_get_error(pRequest), ghttp_get_body(pRequest));
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


int LinkSimpleHttpPost(IN const char * pUrl, OUT char* pBuf, IN int nBufLen, OUT int* pRespLen,
                       IN const char *pReqBody, IN int nReqBodyLen, IN const char *pContentType) {
        
        return linkSimpleHttpRequest(1, pUrl, pBuf, nBufLen, pRespLen, pReqBody, nReqBodyLen, pContentType, NULL, 0);
}

const char *LinkGetUploadHost(int nUseHttps, LinkUploadZone zone) {
        if (nUseHttps) {
                switch(zone) {
                        case LINK_ZONE_HUABEI:
                                return "https://up-z1.qiniup.com";
                        case LINK_ZONE_HUANAN:
                                return "https://up-z2.qiniup.com";
                        case LINK_ZONE_BEIMEI:
                                return "https://up-na0.qiniup.com";
                        case LINK_ZONE_DONGNANYA:
                                return "https://up-as0.qiniup.com";
                        default:
                                return "https://up.qiniup.com";
                }
        } else {
                switch(zone) {
                        case LINK_ZONE_HUABEI:
                                return "http://upload-z1.qiniup.com";
                        case LINK_ZONE_HUANAN:
                                return "http://upload-z2.qiniup.com";
                        case LINK_ZONE_BEIMEI:
                                return "http://upload-na0.qiniup.com";
                        case LINK_ZONE_DONGNANYA:
                                return "http://upload-as0.qiniup.com";
                        default:
                                return "http://upload.qiniup.com";
                }
        }
}

const char *LinkGetRsHost(int nUseHttps) {
        if (nUseHttps) {
                return "https://rs.qiniu.com";
        } else {
                return "http://rs.qiniu.com";
        }
}


//https://developer.qiniu.com/kodo/manual/1201/access-token
int LinkGetUserConfig(IN const char * pUrl, OUT char* pBuf, IN int nBufLen, OUT int* pRespLen,
                              IN const char *pToken, IN int nTokenLen) {
        
        return linkSimpleHttpRequest(0, pUrl, pBuf, nBufLen, pRespLen, NULL, 0, NULL, pToken, nTokenLen);
}
