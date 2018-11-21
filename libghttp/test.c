#include <stdio.h>
#include <string.h>
#include <ghttp.h>
#include "qupload.h"
int testhttps(char * p)
{
    char *uri = "https://www.baidu.com";
    if (p != NULL) {
	uri = p;
    }
    ghttp_request *request = NULL;
    ghttp_status status;
    char *buf;
    int bytes_read;
    int size, ret;
 
 
    request = ghttp_request_new();
    if((ret = ghttp_set_uri(request, uri)) == -1) {
       fprintf(stderr, "ghttp_set_uri fail:%d\n", ret);
       return -1;
    }
    if((ret = ghttp_set_type(request, ghttp_type_get)) == -1){
       fprintf(stderr, "ghttp_set_type fail:%d\n", ret);
       return -1;
    }
    ghttp_prepare(request);
    status = ghttp_process(request);
    if(status == ghttp_error) {
     fprintf(stderr, "status fail:%d\n", status);
     return -1;
    }
    printf("Status code -> %d\n", ghttp_status_code(request));
    buf = ghttp_get_body(request);
 
    bytes_read = ghttp_get_body_len(request);
    size = strlen(buf);//size == bytes_read
    fprintf(stderr, "%d:%s\n",size, buf);
    return 0;
}

int testpost(char *p) {
    char *uri = "http://www.baidu.com";
    if (p != NULL) {
	uri = p;
    }
    char szXML[2048];
    char szVal[256];
     
    ghttp_request *request = NULL;
    ghttp_status status;
    char *buf;
    char retbuf[128];
    int len;
     
    strcpy(szXML, "POSTDATA=");
    sprintf(szVal, "%d", 15);
    strcat(szXML, szVal);
     
    printf("%s\n", szXML);//test
     
    request = ghttp_request_new();
    if (ghttp_set_uri(request, uri) == -1)
    return -1;
    if (ghttp_set_type(request, ghttp_type_post) == -1)//post
    return -1;
     
    ghttp_set_header(request, http_hdr_Content_Type, "application/x-www-form-urlencoded");
    //ghttp_set_sync(request, ghttp_sync); //set sync
    len = strlen(szXML);
    ghttp_set_body(request, szXML, len);//
    ghttp_prepare(request);
    status = ghttp_process(request);
    if (status == ghttp_error)
    return -1;
    buf = ghttp_get_body(request);//test
    sprintf(retbuf, "%s", buf);
    ghttp_clean(request);
    return 0;
}

int testget(char *p) {
    char *uri = "http://www.baidu.com";
    if (p != NULL) {
        uri = p;
    }

    ghttp_request *request = NULL;
    ghttp_status status;
    char retbuf[1024];
    int len;

    request = ghttp_request_new();
    if (ghttp_set_uri(request, uri) == -1)
    return -1;

    ghttp_prepare(request);
    status = ghttp_process(request);
    if (status == ghttp_error) {
    	ghttp_clean(request);
	if (ghttp_is_timeout(request)) {
		printf("---------->timeout\n");
	}
    	return -1;
    }
    char *buf = ghttp_get_body(request);//test
    snprintf(retbuf, sizeof(retbuf), "%s", buf);
    ghttp_clean(request);
    printf("ret:%s\n", retbuf);
    return 0;
}

void testupfile(char * file, char * token) {
	int ret;
	LinkPutret putret={0};
	ret = LinkUploadFile(file, "http://upload.qiniup.com", token, file, NULL, &putret);
	printf("%d %d [%s] [%s]\n", ret, putret.code, putret.error, putret.body);
}

void testupbuf(char * file, char * token) {
	int ret;
	LinkPutret putret={0};

        FILE *fp;
        fp = fopen(file, "rb+");
        if (fp == NULL) {
            printf("test: fopen error\n");
            return;
        }

        fseek(fp, 0, SEEK_END);
        long file_len = ftell(fp);
        rewind(fp);

	char buf[1024 * 1024 * 2];
        size_t read_num = fread(buf, sizeof(char), file_len, fp);
        if (read_num != file_len) {
            fclose(fp);
	    printf("read less than expect\n");
            return NULL;
        }
        fclose(fp);

	ret = LinkUploadBuffer(buf, file_len, "http://upload.qiniup.com",  token, file, NULL, &putret);
	printf("%d %d [%s] [%s]\n", ret, putret.code, putret.error, putret.body);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("usage as:%s testmethod [arg...]\n", argv[0]);
		return 1;
	}

	if (memcmp("testhttps", argv[1], 9) == 0) {
		if (argc == 2)
			testhttps(NULL);
		else
			testhttps(argv[2]);
	} else if (memcmp("testpost", argv[1], 8) == 0) {
		if (argc == 2)
			testpost(NULL);
		else
			testpost(argv[2]);
	} else if (memcmp("testget", argv[1], 8) == 0) {
		if (argc == 2)
			testget(NULL);
		else
			testget(argv[2]);
	} else if (memcmp("testupfile", argv[1], 8) == 0) {
		if (argc != 4) {
			printf("usage as:%s testupfile filepath token\n", argv[0]);
			return 2;
		}
		testupfile(argv[2], argv[3]);
	} else if (memcmp("testupbuf", argv[1], 8) == 0) {
		if (argc != 4) {
			printf("usage as:%s testupbuf filepath token\n", argv[0]);
			return 2;
		}
		testupfile(argv[2], argv[3]);
	}
}
