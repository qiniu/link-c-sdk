//
// Created by chenh on 2019/1/23.
//
#include <stdarg.h>
#include "wolfssl/options.h"
#include "wolfssl/wolfcrypt/hmac.h"
#include "urlsafe_b64.h"
#include "common.h"

void DevPrint(int level, const char *fmt, ...)
{
    if (level <= DEV_LOG_DBG) {
	va_list args;
	char argsBuf[ARGSBUF_LEN] = {0};
	if (argsBuf == NULL) {
	    return;
	}
	va_start(args, fmt);
	vsprintf(argsBuf, fmt, args);
	printf("%s\n", argsBuf);
	va_end(args);

    }
}

void *DevMalloc(int size)
{
    return malloc(size);
}

void DevFree(void *ptr)
{
    if (ptr) {
        free(ptr);
    }
}

static int HmacSha1(const char * pKey, int nKeyLen, const char * pInput, int nInputLen,
	     char *pOutput, char *dsk)
{

    Hmac hmac;
    memset(&hmac, 0, sizeof(hmac));
    int ret = 0;

    ret = wc_HmacSetKey(&hmac, SHA, (byte*)pKey, nKeyLen);
    if (ret != 0) {
	return -1;
    }

    if( (ret = wc_HmacUpdate(&hmac, (byte*)pInput, nInputLen)) != 0) {
	return -1;
    }

    if ((ret = wc_HmacFinal(&hmac, (byte*)pOutput)) != 0) {
	return -1;
    }
    return 0;
}

void GenerateUserName(char *username, int *len, char *dak)
{
    long timestamp = 0.0;
    char *query = (char *)DevMalloc(256);

    timestamp = (long)time(NULL);
    sprintf(query, "dak=%s&timestamp=%ld&version=v1", dak, timestamp);
    *len = strlen(query);
    memcpy(username, query, *len);
    DevFree(query);
}

int GeneratePassword(char *username, int unlen, char *password, int *passlen, char *dsk)
{
    int ret = 0;
    int sha1len = 20;
    char sha1[256] = {};
printf("GeneratePassword..............#\n");

    ret = HmacSha1(dsk, strlen(dsk), username, unlen, sha1, &sha1len);
    if (ret != DEV_CODE_SUCCESS) {
	DevPrint(DEV_LOG_ERR, "Hmacsha1 failed.\n");
	return DEV_CODE_ERROR;
    }
    int len = urlsafe_b64_encode(sha1, 20, password, passlen);
    *passlen = len;
    return DEV_CODE_SUCCESS;
}

void GetTimestamp(char *_pTimestamp)
{
    struct tm *tblock = NULL;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    tblock = localtime(&tv.tv_sec);
    sprintf(_pTimestamp, "%d/%d/%d %d:%d:%d", 1900 + tblock->tm_year, 1 + tblock->tm_mon,
	    tblock->tm_mday, tblock->tm_hour, tblock->tm_min, tblock->tm_sec);
}
