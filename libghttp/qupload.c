#include "qupload.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "ghttp.h"
#include <errno.h>


static int get_fix_random_str(char *buf, int bufLen, int len) {
        int i = 0, val = 0;
        const char *base = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        int base_len = 62;
        
        srand((unsigned int) time(NULL));
        
        len = bufLen - 1 >= len ? len : bufLen - 1;
        for (; i < len; i++) {
                val = 1 + (int) ((float) (base_len - 1) * rand() / (RAND_MAX + 1.0));
                buf[i] = base[val];
        }
        buf[len] = 0;
        return len;
}


/*
 * form body concatenation function
 *
 * @param dst_buffer     a buffer used to make concatenation of strings
 * @param src_buffer     a buffer to be concated to the dst_buffer
 * @param src_buffer_len src_buffer length
 *
 * @return the end pointer of the dst_buffer, used for the next concatenation
 * */
static char *qn_memconcat(char *dst_buffer, const char *src_buffer, size_t src_buffer_len) {
        memcpy(dst_buffer, src_buffer, src_buffer_len);
        char *p_end = dst_buffer + src_buffer_len;
        return p_end;
}


/*
 * assemble the multi-form body
 *
 * @param dst_buffer         a buffer used to make concatenation of strings
 * @param form_boundary      form boundary string
 * @param form_boundary_len  form boundary length
 * @param field_name         form field name
 * @param field_value        form field value
 * @param field_value_len    form field value length
 * @param field_mime_type    form field mime type (can be NULL)
 * @param form_data_len      form data length, used to accumulate the form body length
 *
 * @return the end pointer of the dst_buffer, used for the next concatenation
 * **/
static char *qn_addformfield(char *dst_buffer, char *form_boundary, size_t form_boundary_len,
                             const char *field_name,
                             const char *field_value, size_t field_value_len,
                             const char *field_mime_type, size_t *form_data_len, int isFieldValueFile) {
        size_t delta_len = 0;
        size_t field_name_len = strlen(field_name);
        size_t field_mime_len = 0;
        if (field_mime_type) {
                field_mime_len = strlen(field_mime_type);
        }
        
        char *dst_buffer_p = dst_buffer;
        dst_buffer_p = qn_memconcat(dst_buffer_p, form_boundary, form_boundary_len);
        dst_buffer_p = qn_memconcat(dst_buffer_p, "\r\n", 2);
        delta_len += 2;
        dst_buffer_p = qn_memconcat(dst_buffer_p, "Content-Disposition: form-data; name=\"", 38);
        delta_len += 38;
        dst_buffer_p = qn_memconcat(dst_buffer_p, field_name, field_name_len);
        dst_buffer_p = qn_memconcat(dst_buffer_p, "\"", 1);
        delta_len += 1;
        if (field_mime_type) {
                dst_buffer_p = qn_memconcat(dst_buffer_p, "\r\nContent-Type: ", 16);
                dst_buffer_p = qn_memconcat(dst_buffer_p, field_mime_type, field_mime_len);
                delta_len += 16;
        }
        
        dst_buffer_p = qn_memconcat(dst_buffer_p, "\r\n\r\n", 4);
        delta_len += 4;
        if (isFieldValueFile) {
                
                //add file
                FILE *fp;
                fp = fopen(field_value, "rb+");
                if (fp == NULL) {
                        return NULL;
                }
                
                fseek(fp, 0, SEEK_END);
                long file_len = ftell(fp);
                rewind(fp);
                
                size_t read_num = fread(dst_buffer_p, sizeof(char), file_len, fp);
                if (read_num != file_len) {
                        fclose(fp);
                        return NULL;
                }
                fclose(fp);
                dst_buffer_p += file_len;
                field_value_len = file_len;
                
        } else {
                if (field_value_len > 0)
                        dst_buffer_p = qn_memconcat(dst_buffer_p, field_value, field_value_len);
        }
        dst_buffer_p = qn_memconcat(dst_buffer_p, "\r\n--", 4);
        delta_len += 4;
        
        delta_len += form_boundary_len + field_name_len + field_mime_len + field_value_len;
        *form_data_len += delta_len;
        return dst_buffer_p;
}

static void getReqidAndResponse(ghttp_request * pRequest, LinkPutret *put_ret) {
        
        int resp_body_len = ghttp_get_body_len(pRequest);
        if (resp_body_len > 0) {
                char *resp_body = (char *)malloc(resp_body_len+1);
                char *resp_body_end = qn_memconcat(resp_body, ghttp_get_body(pRequest), resp_body_len);
                *resp_body_end = 0; //end it
                put_ret->body = resp_body;
        }
        
        const char *reqid = ghttp_get_header(pRequest, "X-Reqid");
        //get reqid
        if (reqid) {
                strncpy(put_ret->reqid, reqid, sizeof(put_ret->reqid) - 1);
        }
}

/**
 * form upload file to qiniu storage
 *
 * @param filepathOrBufer    local file path or buffer pointer
 * @param bufferLen          if filepathOrBufer stand for buffer, this is the buffer length
 * @param upload_token       upload token get from remote server
 * @param file_key           the file name in the storage bucket, must be unique for each file
 * @param mime_type          the optional mime type to specify when upload the file, (can be NULL)
 * @param put_ret            the file upload response from qiniu storage server
 *
 * @return 0 on success, -1 on failure
 * */
static int linkUpload(const char *filepathOrBufer, int bufferLen, const char * upHost, const char *upload_token, const char *file_key,
                      const char **customMeta, int nCustomMetaLen, const char *mime_type, LinkPutret *put_ret, int isTypeFile) {
        
        if (file_key == NULL || upload_token == NULL || filepathOrBufer == NULL) {
                return -1;
        }
        memset(put_ret, 0, sizeof(LinkPutret));
        
        //form boundary
        const char *form_prefix = "________LINKFormBoundary"; //24
        size_t form_prefix_len = strlen(form_prefix);
        char form_boundary[24+16+1];
        memcpy(form_boundary, form_prefix, form_prefix_len);
        get_fix_random_str(form_boundary + form_prefix_len, sizeof(form_boundary) - form_prefix_len, 16);
        size_t form_boundary_len = strlen(form_boundary);
        
        char *form_data = NULL;
        char *form_data_p = NULL;
        int form_buf_buf_len  = 0;
        char static_form_data[1536];
        int staticPrevFormDataLen = 0;
        //alloc body buffer
        struct stat st;
        if (isTypeFile) {
                if (stat(filepathOrBufer, &st) != 0) {
                        snprintf(put_ret->error, sizeof(put_ret->error), "%s", strerror(errno));
                        return -2;
                }
                form_buf_buf_len = st.st_size + 1024*5;
                form_data = (char *) malloc(form_buf_buf_len);
        } else {
                form_data = static_form_data;
        }
        form_data_p = form_data;
        
        //add init tag
        size_t form_data_len = 0;
        form_data_p = qn_memconcat(form_data_p, "--", 2);
        form_data_len += 2;
        
        //add upload_token
        form_data_p = qn_addformfield(form_data_p, form_boundary, form_boundary_len, "token", (char *) upload_token,
                                      strlen(upload_token), NULL, &form_data_len, 0);
        
        //add file key
        form_data_p = qn_addformfield(form_data_p, form_boundary, form_boundary_len, "key", (char *) file_key,
                                      strlen(file_key),
                                      NULL, &form_data_len, 0);
        
        if (nCustomMetaLen > 0) {
                int i = 0;
                char mkey[32] = "x-qn-meta-";
                int nKeyPreLen = 10;
                for (i = 0; i < nCustomMetaLen; i+=2) {
                        snprintf(mkey + nKeyPreLen, sizeof(mkey) - nKeyPreLen, "%s", customMeta[i]);
                        form_data_p = qn_addformfield(form_data_p, form_boundary, form_boundary_len, mkey,
                                                      customMeta[i+1], strlen(customMeta[i+1]), NULL, &form_data_len, 0);
                }
        }
        
        if (!isTypeFile) {
                form_data_p = qn_addformfield(form_data_p, form_boundary, form_boundary_len, "file",  NULL, 0,// filepathOrBufer, bufferLen,
                                              mime_type, &form_data_len, 0);
                staticPrevFormDataLen = form_data_len - 4;
        } else {
                form_data_p = qn_addformfield(form_data_p, form_boundary, form_boundary_len, "file",  filepathOrBufer, -1,
                                              mime_type, &form_data_len, 1);
        }
        if (form_data_p == NULL) {
                snprintf(put_ret->error, sizeof(put_ret->error), "read file error");
                if (form_data != static_form_data)
                        free(form_data);
                return -3;
        }
        
        form_data_p = qn_memconcat(form_data_p, form_boundary, form_boundary_len);
        qn_memconcat(form_data_p, "--\r\n", 4);
        form_data_len += form_boundary_len + 4;
        
        
        //send the request
        size_t form_content_type_len = 31 + form_boundary_len;
        char form_content_type[31+64] = {0};
        snprintf(form_content_type, form_content_type_len, "multipart/form-data; boundary=%s", form_boundary);
        
        ghttp_request *request = NULL;
        request = ghttp_request_new();
        if (request == NULL) {
                snprintf(put_ret->error, sizeof(put_ret->error), "ghttp_request_new return null");
                if (form_data != static_form_data)
                        free(form_data);
                return -4;
        }
        
        ghttp_set_uri(request, upHost);
        ghttp_set_header(request, "Content-Type", form_content_type);
        ghttp_set_type(request, ghttp_type_post);
        if (!isTypeFile)
                ghttp_set_body3(request, form_data, staticPrevFormDataLen, filepathOrBufer, bufferLen,
                                &form_data[staticPrevFormDataLen], form_data_len - staticPrevFormDataLen);
        else
                ghttp_set_body(request, form_data, form_data_len);
        ghttp_prepare(request);
        ghttp_status status = ghttp_process(request);
        if (status == ghttp_error) {
                if (ghttp_is_timeout(request)) {
                        snprintf(put_ret->error, sizeof(put_ret->error), "ghttp_process timeout[%s]", ghttp_get_error(request));
                } else {
                        snprintf(put_ret->error, sizeof(put_ret->error), "%s", ghttp_get_error(request));
                        getReqidAndResponse(request, put_ret);
                }
                ghttp_request_destroy(request);
                if (form_data != static_form_data)
                        free(form_data);
                return -5;
        }
        
        if (form_data != static_form_data)
                free(form_data);
        
        put_ret->code = ghttp_status_code(request);
        if (put_ret->code / 100 == 2) {
                ghttp_request_destroy(request);
                return 0;
        }
        
        getReqidAndResponse(request, put_ret);
        ghttp_request_destroy(request);
        
        return 0; //http request ok(maybe return 404, but http is ok)
}

int LinkUploadBuffer(const char *buffer, int bufferLen, const char * upHost, const char *upload_token, const char *file_key,
                     const char **customMeta, int nCustomMetaLen, const char *mime_type, LinkPutret *put_ret) {
        return linkUpload(buffer, bufferLen,upHost, upload_token, file_key, customMeta, nCustomMetaLen, mime_type, put_ret, 0);
}

int LinkUploadFile(const char *local_path, const char * upHost, const char *upload_token, const char *file_key, const char *mime_type,
                   LinkPutret *put_ret) {
        return linkUpload(local_path, 0 ,upHost, upload_token, file_key, NULL, 0,  mime_type, put_ret, 1);
}

void LinkFreePutret(LinkPutret *put_ret) {
        if(put_ret->body) {
                free(put_ret->body);
        }
        put_ret->body = NULL;
}

int LinkMoveFile(const char *pMoveUrl, const char *pMoveToken, LinkPutret *put_ret) {
        
        ghttp_status status;
        memset(put_ret, 0, sizeof(LinkPutret));
        ghttp_request * pRequest = ghttp_request_new();
        if (pRequest == NULL) {
                snprintf(put_ret->error, sizeof(put_ret->error), "ghttp_request_new return null");
                return -1;
        }
        
        if (ghttp_set_uri(pRequest, pMoveUrl) == -1) {
                ghttp_request_destroy(pRequest);
                return -7;
        }
        
        ghttp_set_type(pRequest, ghttp_type_post);
        
        char tokenBuf[512];
        snprintf(tokenBuf, sizeof(tokenBuf), "QBox %s", pMoveToken);
        ghttp_set_header(pRequest, "Authorization", tokenBuf);
        ghttp_set_header(pRequest, "Content-Type", "application/x-www-form-urlencoded");
        
        status = ghttp_prepare(pRequest);
        if (status != 0) {
                ghttp_request_destroy(pRequest);
                snprintf(put_ret->error, sizeof(put_ret->error), "%s", ghttp_get_error(pRequest));
                return -2;
        }
        
        status = ghttp_process(pRequest);
        if (status == ghttp_error) {
                if (ghttp_is_timeout(pRequest)) {
                        snprintf(put_ret->error, sizeof(put_ret->error), "%s", ghttp_get_error(pRequest));
                        ghttp_request_destroy(pRequest);
                        return -4;
                } else {
                        //fprintf(stderr, "pMoveUrl:%s token:%\n", pMoveUrl, pMoveToken);
                        snprintf(put_ret->error, sizeof(put_ret->error), "%d %s",errno, ghttp_get_error(pRequest));
                        ghttp_request_destroy(pRequest);
                        return -5;
                }
                ghttp_request_destroy(pRequest);
                return -6;
        }
        
        put_ret->code = ghttp_status_code(pRequest);
        if (put_ret->code / 100 == 2) {
                ghttp_request_destroy(pRequest);
                return 0;
        }
        
        getReqidAndResponse(pRequest, put_ret);
        
        ghttp_request_destroy(pRequest);
        return 0;
}

typedef struct {
        void                   *pUserOpaque;
        LinkStreamCallback userCb;
        const char * boundary;
        const char * mimeType;
        int nBoundaryLen;
        char *pTmpBuf;
        int nTmpBuf;
        int nCurTmpBufLen;
        uint8_t sendFirstDataState; // 0init, 1 sending, 2 sendover
        uint8_t isTailWrited;
}UploadStreamParam;

static int fillChunkData(const char *pSrc, int *pSrclen, char *pBuf, int nBufLen, int isEnd) {
        int len = nBufLen - 8; // 2 for tail \r\n
        if (*pSrclen < len) {
                len = *pSrclen;
        }
        
        int prefixLen = snprintf(pBuf, nBufLen, "%X\r\n", len);

        memmove(pBuf + prefixLen, pSrc, len);
        *pSrclen -= len;
        len = len+prefixLen;
        pBuf[len++] = '\r';
        pBuf[len++] = '\n';
        if (isEnd) {
                pBuf[len++] = '0';
                pBuf[len++] = '\r';
                pBuf[len++] = '\n';
                pBuf[len++] = '\r';
                pBuf[len++] = '\n';
        }
        return len;
}
#include <assert.h>
// [chunk size][\r\n][chunk data][\r\n][chunk size][\r\n][chunk data][\r\n]
int static linkStreamCallback(void *pOpaque, char *pData, int nDataLen) {
        UploadStreamParam* pParam = (UploadStreamParam *)pOpaque;
        
        int i = 0, ret = 0, nCustomMeatLen = 0, prevLen = 0;;
        LinkStreamCallbackType type;
        const char **customMeta = NULL;
        char * form_data_p = NULL;
        char tmpBuf[512];
        
        if (pParam->isTailWrited) {
                return 0;
        }
SendData:
        if (pParam->nCurTmpBufLen > 0) {
                return fillChunkData(pParam->pTmpBuf, &pParam->nCurTmpBufLen, pData, nDataLen, 0);
                /*
                if (pParam->sendFirstDataState != 1) {
                        return fillChunkData(pParam->pTmpBuf, &pParam->nCurTmpBufLen, pData, nDataLen);
                } else {
                        int len = pParam->nCurTmpBufLen;
                        if (nDataLen < len)
                                len = nDataLen;
                        memmove(pData, pParam->pTmpBuf, len);
                        pParam->nCurTmpBufLen -= len;
                        return len;
                }
                 */
        }
        
        // nCurTmpBufLen must be zero
        assert(pParam->nCurTmpBufLen == 0);
        ret = pParam->userCb(pParam->pUserOpaque, &type, pParam->pTmpBuf, pParam->nTmpBuf);
        if (ret < 0) {
                return ret;
        } else if (ret > 0){
                pParam->nCurTmpBufLen = ret;
                switch(type) {
                        case LinkStreamCallbackType_Data:
                                if (pParam->sendFirstDataState == 0) {
                                        i = 0;
                                        pParam->sendFirstDataState = 1;
                                        qn_addformfield(tmpBuf, (char *)pParam->boundary, pParam->nBoundaryLen, "file",  NULL, 0,
                                                                      pParam->mimeType, &i, 0);

                                        i = i-4; // trucated form filed tail
                                        return fillChunkData(tmpBuf, &i, pData, nDataLen, 0);
                                } else if (pParam->sendFirstDataState == 1) {
                                        goto SendData;
                                }
                        case LinkStreamCallbackType_Meta:
                                prevLen = 0;
                                if (pParam->sendFirstDataState == 1) {
                                        pParam->sendFirstDataState = 2;
                                        i = 4;
                                        memcpy(tmpBuf, "\r\n--" , 4);
                                        prevLen = fillChunkData(tmpBuf, &i, pData, nDataLen, 0);
                                }
                                
                                customMeta = (const char **)pParam->pTmpBuf;
                                form_data_p = tmpBuf;
                                
                                int fdpLen = 0;
                                char mkey[32] = "x-qn-meta-";
                                int nKeyPreLen = 10;
                                for (i = 0; i < 8; i+=2) {
                                        snprintf(mkey + nKeyPreLen, sizeof(mkey) - nKeyPreLen, "%s", customMeta[i]);
                                        form_data_p = qn_addformfield(form_data_p, pParam->boundary, pParam->nBoundaryLen, mkey,
                                                                      customMeta[i+1], strlen(customMeta[i+1]), NULL, &fdpLen, 0);
                                }
                                pParam->nCurTmpBufLen = 0;
                                return fillChunkData(tmpBuf, &fdpLen, pData, nDataLen, 0);
                                
                        case LinkStreamCallbackType_Magic:
                                prevLen = 0;
                                if (pParam->sendFirstDataState == 1) {
                                        pParam->sendFirstDataState = 2;
                                        i = 4;
                                        memcpy(tmpBuf, "\r\n--" , 4);
                                        prevLen = fillChunkData(tmpBuf, &i, pData, nDataLen, 0);
                                }
                                
                                
                        case LinkStreamCallbackType_Key:
                                prevLen = 0;
                                if (pParam->sendFirstDataState == 1) {
                                        pParam->sendFirstDataState = 2;
                                        i = 4;
                                        memcpy(tmpBuf, "\r\n--" , 4);
                                        prevLen = fillChunkData(tmpBuf, &i, pData, nDataLen, 0);
                                }
                                
                                //add file key //TODO key add in the last is ok? or must use magic variable?
                                i = 0;
                                qn_addformfield(tmpBuf, pParam->boundary, pParam->nBoundaryLen, "key", (char *)pParam->pTmpBuf,
                                                              ret,
                                                              NULL, &i, 0);
                                pParam->nCurTmpBufLen = 0;
                                return fillChunkData(tmpBuf, &i, pData + prevLen, nDataLen - prevLen, 0) + prevLen;

                }
        } else {
                if (pParam->isTailWrited == 0) {
                        // tail
                        pParam->isTailWrited = 1;
                        memcpy(tmpBuf, pParam->boundary, pParam->nBoundaryLen);
                        memcpy(tmpBuf + pParam->nBoundaryLen, "--\r\n", 4);
                        i = pParam->nBoundaryLen + 4;
                        return fillChunkData(tmpBuf, &i, pData, nDataLen, 1);
                }
        }

        return 0;
}

int LinkUploadStream(const char * upHost, const char *upload_token, const char *mime_type,
                     LinkStreamCallback cb, void *opaque, LinkPutret *put_ret) {
        
        
        if (upload_token == NULL || cb == NULL || put_ret == NULL) {
                return -1;
        }
        memset(put_ret, 0, sizeof(LinkPutret));
        
        //form boundary
        const char *form_prefix = "________LINKFormBoundary"; //24
        size_t form_prefix_len = strlen(form_prefix);
        char form_boundary[24+16+1];
        memcpy(form_boundary, form_prefix, form_prefix_len);
        get_fix_random_str(form_boundary + form_prefix_len, sizeof(form_boundary) - form_prefix_len, 16);
        size_t form_boundary_len = strlen(form_boundary);
        
        
        char static_form_data[188*7];
        char *form_data = static_form_data;
        char *form_data_p = static_form_data;
        
        //add init tag
        size_t form_data_len = 0;
        form_data_p = qn_memconcat(form_data_p, "--", 2);
        form_data_len += 2;
        
        //add upload_token
        form_data_p = qn_addformfield(form_data_p, form_boundary, form_boundary_len, "token", (char *) upload_token,
                                      strlen(upload_token), NULL, &form_data_len, 0);
        
        ghttp_request *request = NULL;
        request = ghttp_request_new();
        if (request == NULL) {
                snprintf(put_ret->error, sizeof(put_ret->error), "ghttp_request_new return null");
                if (form_data != static_form_data)
                        free(form_data);
                return -4;
        }
        
        ghttp_set_uri(request, upHost);
        
        //construct content-type header
        size_t form_content_type_len = 31 + form_boundary_len;
        char form_content_type[31+64] = {0};
        snprintf(form_content_type, form_content_type_len, "multipart/form-data; boundary=%s", form_boundary);
        ghttp_set_header(request, "Content-Type", form_content_type);
        
        ghttp_set_header(request, "Transfer-Encoding", "chunked");
        ghttp_set_type(request, ghttp_type_post);
        
        UploadStreamParam upsParam;
        upsParam.boundary = form_boundary;
        upsParam.nBoundaryLen = form_boundary_len;
        upsParam.userCb = cb;
        upsParam.pUserOpaque = opaque;
        upsParam.pTmpBuf = form_data;
        upsParam.nTmpBuf = sizeof(static_form_data);
        upsParam.nCurTmpBufLen = form_data_len;
        upsParam.sendFirstDataState = 0;
        upsParam.mimeType = mime_type;
        upsParam.isTailWrited = 0;
        ghttp_set_data_callback(request, linkStreamCallback, &upsParam);
        
        ghttp_prepare(request);
        ghttp_status status = ghttp_process(request);
        if (status == ghttp_error) {
                if (ghttp_is_timeout(request)) {
                        snprintf(put_ret->error, sizeof(put_ret->error), "ghttp_process timeout[%s]", ghttp_get_error(request));
                } else {
                        snprintf(put_ret->error, sizeof(put_ret->error), "%s", ghttp_get_error(request));
                        getReqidAndResponse(request, put_ret);
                }
                ghttp_request_destroy(request);
                if (form_data != static_form_data)
                        free(form_data);
                return -5;
        }
        
        put_ret->code = ghttp_status_code(request);
        if (put_ret->code / 100 == 2) {
                ghttp_request_destroy(request);
                return 0;
        }
        
        getReqidAndResponse(request, put_ret);
        ghttp_request_destroy(request);
        
        return 0; //http request ok(maybe return 404, but http is ok)
}
