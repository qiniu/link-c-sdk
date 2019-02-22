#ifndef __QPULOAD__H__
#define __QPULOAD__H__

typedef struct {
    int code;
    unsigned char isTimeout;
    char reqid[17];
    char xreqid[17];
    char error[64];
    char *body;
} LinkPutret;

int LinkMoveFile(const char *pMoveUrl, const char *pMoveToken, LinkPutret *put_ret);
int LinkUploadFile(const char *local_path, const char* upHost, const char *upload_token, const char *file_key, const char *mime_type,
                LinkPutret *put_ret);
int LinkUploadBuffer(const char *buffer, int bufferLen, const char* upHost, const char *upload_token, const char *file_key,
	       	const char **customMeta, int nCustomMetaLen,
                const char **customMagic, int nCustomMagicLen, const char *mime_type, LinkPutret *put_ret);
void LinkFreePutret(LinkPutret *put_ret);

typedef void (*LinkGhttpLog)(const char *);
void LinkGhttpSetLog(LinkGhttpLog log);
void LinkGhttpLogger(const char *);

#endif
