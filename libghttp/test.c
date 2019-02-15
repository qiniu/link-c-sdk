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
    printf("in testhttps: url:%s\n", uri);

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
    printf("in testpost: url:%s\n", uri);
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
    ghttp_request_destroy(request);
    return 0;
}

int testget(char *p) {
    char *uri = "http://www.baidu.com";
    if (p != NULL) {
        uri = p;
    }
    printf("in testget: url:%s\n", uri);

    ghttp_request *request = NULL;
    ghttp_status status;
    char retbuf[1024];

    request = ghttp_request_new();
    if (ghttp_set_uri(request, uri) == -1)
    return -1;

    ghttp_prepare(request);
    status = ghttp_process(request);
    if (status == ghttp_error) {
	if (ghttp_is_timeout(request)) {
		printf("---------->timeout [%s]\n", ghttp_get_error(request));
	} else {
		printf("ghttp_process err:%d\n", status);
	}
    	ghttp_request_destroy(request);
    	return -1;
    }
    char *buf = ghttp_get_body(request);//test
    snprintf(retbuf, sizeof(retbuf), "%s", buf);
    ghttp_request_destroy(request);
    printf("ret:%s\n", retbuf);
    return 0;
}

void testupfile(char * file, char * token) {
	int ret;
	LinkPutret putret={0};
	ret = LinkUploadFile(file, "http://upload.qiniup.com", token, file, NULL, &putret);
	printf("%d %d [%s] [%s]\n", ret, putret.code, putret.error, putret.body);
}

void testupbuf(int file_len, char * token, int times) {
	int ret;
	LinkPutret putret={0};
        
	char * buf = malloc(file_len);
	if (buf == NULL) {
                printf("malloc %d fail\n", file_len);
                return;
	}
        memset(buf, 0x31, file_len);
        fprintf(stderr, "upload times:%d\n", times);
        int i = 0;
        for (i = 0; i < times; i++) {
                char file[128] = {0};
                sprintf(file, "testupbuf_%ld", time(NULL));
                ret = LinkUploadBuffer(buf, file_len, "http://upload.qiniup.com",  token, file, NULL, 0, NULL, 0, NULL, &putret);
                fprintf(stderr, "upload:%s %d %d [%s] [%s]\n",file, ret, putret.code, putret.error, putret.body);
        }
	free(buf);
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
		if (argc != 4 && argc != 5) {
			printf("usage as:%s testupbuf uploadsize token [nums]\n", argv[0]);
			return 2;
		}
                int uploadSize = atoi(argv[2]);
                int times = 0;
                if (argc == 5)
                        times = atoi(argv[4]);
                if (times == 0)
                        times = 1;
                if (uploadSize <= 0 || uploadSize > 2 * 1024*1024) {
                        printf("uploadsize must match: 0 < uploadsize < 2M\n");
                        return 2;
                }
		testupbuf(uploadSize, argv[3], times);
	}
}
