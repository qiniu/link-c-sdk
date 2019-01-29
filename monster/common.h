//
// Created by chenh on 2019/1/23.
//

#ifndef LINK_C_SDK_COMMON_H
#define LINK_C_SDK_COMMON_H

#define __compiler_offsetof(a,b) __builtin_offsetof(a,b)
#undef offsetof
#ifdef __compiler_offsetof
#define offsetof(TYPE,MEMBER) __compiler_offsetof(TYPE,MEMBER)
#else
#define offsetof(TYPE,MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#define container_of(ptr, type, member) ({\
    const typeof(((type*)0)->member) *__mptr = (ptr);\
    (type *)((char *)__mptr - offsetof(type,member));})


#define UNIT_LEN 256

#define ARGSBUF_LEN 256
#define DEV_LOG_ERR 1
#define DEV_LOG_WARN 2
#define DEV_LOG_DBG 4

enum DEVICE_STATUS {
    DEV_CODE_ERROR = -1,
    DEV_CODE_SUCCESS = 0
};

void DevPrint(int level, const char *fmt, ...);
void *DevMalloc(int size);
void DevFree(void *ptr);
void GenerateUserName(char *username, int *len, char *dak);
int GeneratePassword(char *username, int unlen, char *password, int *passlen, char *dak);


#endif //LINK_C_SDK_COMMON_H
