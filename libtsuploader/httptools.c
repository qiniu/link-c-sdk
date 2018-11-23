#include "httptools.h"
#include <ghttp.h>

int LinkSimpleHttpGet(IN const char * pUrl, OUT char* pBuf, IN int nBufLen, OUT int* pRespLen) {
        
        ghttp_status status;
        
        ghttp_request * pRequest = ghttp_request_new();
        if (pRequest == NULL) {
                return LINK_NO_MEMORY;
        }
        if (ghttp_set_uri(pRequest, pUrl) == -1) {
                ghttp_request_destroy(pRequest);
                return LINK_ARG_ERROR;
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
                        LinkLogError("ghttp_process fail [%s]", ghttp_get_error(pRequest));
                        ghttp_request_destroy(pRequest);
                        return LINK_GHTTP_FAIL;
                }
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


int LinkSimpleHttpPost(IN const char * pUrl, OUT char* pBuf, IN int nBufLen, OUT int* pRespLen,
                       IN const char *pReqBody, IN int nReqBodyLen, IN const char *pContentType) {
        
        
        ghttp_status status;
        
        ghttp_request * pRequest = ghttp_request_new();
        if (pRequest == NULL) {
                return LINK_NO_MEMORY;
        }
        if (ghttp_set_uri(pRequest, pUrl) == -1) {
                ghttp_request_destroy(pRequest);
                return LINK_ARG_ERROR;
        }
        
        ghttp_set_type(pRequest, ghttp_type_post);
        
        if (pContentType == NULL) {
                ghttp_set_header(pRequest, http_hdr_Content_Type, "application/x-www-form-urlencoded");
        } else {
                ghttp_set_header(pRequest, http_hdr_Content_Type, pContentType);
        }
        
        ghttp_set_body(pRequest, pReqBody, nReqBodyLen);
        
        status = ghttp_prepare(pRequest);
        if (status != 0) {
                ghttp_request_destroy(pRequest);
                LinkLogError("ghttp_prepare:%s", ghttp_get_error(pRequest));
                return LINK_GHTTP_FAIL;
        }
        
        status = ghttp_process(pRequest);
        if (status == ghttp_error) {
                if (ghttp_is_timeout(pRequest)) {
                        ghttp_request_destroy(pRequest);
                        LinkLogError("ghttp_process timeout [%s]", ghttp_get_error(pRequest));
                        return LINK_TIMEOUT;
                } else {
                        ghttp_request_destroy(pRequest);
                        LinkLogError("ghttp_process fail [%s]", ghttp_get_error(pRequest));
                        return LINK_GHTTP_FAIL;
                }
                ghttp_request_destroy(pRequest);
                return LINK_GHTTP_FAIL;
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
