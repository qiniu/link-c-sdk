#ifndef __QPULOAD__H__
#define __QPULOAD__H__

typedef struct {
    int code;
    char reqid[17];
    char error[64];
    char *body;
} LinkPutret;

int LinkMoveFile(const char *pMoveUrl, const char *pMoveToken, LinkPutret *put_ret);
int LinkUploadFile(const char *local_path, const char* upHost, const char *upload_token, const char *file_key, const char *mime_type,
                LinkPutret *put_ret);
int LinkUploadBuffer(const char *buffer, int bufferLen, const char* upHost, const char *upload_token, const char *file_key,
	       	const char **customMeta, int nCustomMetaLen, const char *mime_type, LinkPutret *put_ret);
void LinkFreePutret(LinkPutret *put_ret);


typedef enum {
        LinkStreamCallbackType_Data = 1,
        LinkStreamCallbackType_Meta = 2,
        LinkStreamCallbackType_Magic = 3,
        LinkStreamCallbackType_Key = 4
} LinkStreamCallbackType;
typedef int (*LinkStreamCallback)(void *pOpaque, LinkStreamCallbackType *type, char *pData, int nDataLen);
int LinkUploadStream(const char * upHost, const char *upload_token, const char *mime_type,
                     LinkStreamCallback cb, void *opaque, LinkPutret *put_ret);

#endif
