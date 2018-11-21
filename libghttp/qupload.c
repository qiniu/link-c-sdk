#include "qupload.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "ghttp.h"


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
                      char *field_value, size_t field_value_len,
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
        dst_buffer_p = qn_memconcat(dst_buffer_p, field_value, field_value_len);
    }
    dst_buffer_p = qn_memconcat(dst_buffer_p, "\r\n--", 4);
    delta_len += 4;

    delta_len += form_boundary_len + field_name_len + field_mime_len + field_value_len;
    *form_data_len += delta_len;
    return dst_buffer_p;
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
static int linkUpload(const char *filepathOrBufer, int bufferLen, const char * upHost, const char *upload_token,
	       	const char *file_key, const char *mime_type, LinkPutret *put_ret) {

    if (file_key == NULL || upload_token == NULL || filepathOrBufer == NULL) {
	    return -1;
    }

    //form boundary
    const char *form_prefix = "--------LINKFormBoundary"; //24
    size_t form_prefix_len = strlen(form_prefix);
    char form_boundary[24+16+1];
    memcpy(form_boundary, form_prefix, form_prefix_len);
    get_fix_random_str(form_boundary + form_prefix_len, sizeof(form_boundary) - form_prefix_len, 16);
    size_t form_boundary_len = strlen(form_boundary);


    //alloc body buffer
    struct stat st;
    if (stat(filepathOrBufer, &st) != 0) {
        return -2;
    }
    int form_buf_buf_len = st.st_size + 1024*5;
    char *form_data = (char *) malloc(form_buf_buf_len);
    char *form_data_p = form_data;

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

    if (bufferLen) {
        form_data_p = qn_addformfield(form_data_p, form_boundary, form_boundary_len, "file",  filepathOrBufer, bufferLen,
                                  mime_type, &form_data_len, 1);
    } else {
        form_data_p = qn_addformfield(form_data_p, form_boundary, form_boundary_len, "file",  filepathOrBufer, -1,
                                  mime_type, &form_data_len, 1);
    }
    if (form_data_p == NULL) {
        snprintf(put_ret->error, sizeof(put_ret->error), "read file error");
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
        free(form_data);
        return -4;
    }

    ghttp_set_uri(request, upHost);
    ghttp_set_header(request, "Content-Type", form_content_type);
    ghttp_set_type(request, ghttp_type_post);
    ghttp_set_body(request, form_data, form_data_len);
    ghttp_prepare(request);
    ghttp_status status = ghttp_process(request);
    if (status == ghttp_error) {
        snprintf(put_ret->error, sizeof(put_ret->error), "%s", ghttp_get_error(request));
        ghttp_request_destroy(request);
        free(form_data);
        return -5;
    }

    //free
    free(form_data);

    put_ret->code = ghttp_status_code(request);
    if (put_ret->code / 200 == 2) {
	return 0;
    }

    int resp_body_len = ghttp_get_body_len(request);
    char *resp_body = (char *)malloc(resp_body_len+1);
    char *resp_body_end = qn_memconcat(resp_body, ghttp_get_body(request), resp_body_len);
    *resp_body_end = 0; //end it

    put_ret->body = resp_body;
    //get reqid
    strcpy(put_ret->reqid, ghttp_get_header(request, "X-Reqid"));

    return 0;
}

int LinkUploadBuffer(const char *buffer, int bufferLen, const char * upHost, const char *upload_token, const char *file_key, const char *mime_type,
                LinkPutret *put_ret) {
	return linkUpload(buffer, bufferLen,upHost, upload_token, file_key, mime_type, put_ret);
}

int LinkUploadFile(const char *local_path, const char * upHost, const char *upload_token, const char *file_key, const char *mime_type,
		LinkPutret *put_ret) {
	return linkUpload(local_path, 0,upHost, upload_token, file_key, mime_type, put_ret);
}
