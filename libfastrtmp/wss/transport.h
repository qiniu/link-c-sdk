
#define FRTMP_AUDIO_AAC 1
#define FRTMP_VIDEO_H264 1
#define FRTMP_VIDEO_H265 2

#define LWS_HEADER 16

#define FRtmpStatusConnectOk   1
#define FRtmpStatusConnectFail 2
#define FRtmpStatusError 3
#define FRtmpStatusClosed 4
#define FRtmpStatusTimeout 5
#define FRtmpStatusQuit 5

typedef void* FRTMPHANDLER;
typedef void(*FRtmpFreeCallback)(void *pUser, char *pPushTag, int nTagLen);

typedef struct _RtmpSettings {
    char *pRtmpUrl;
    int nRtmpUrlLen;
    char *pCertFile;
    int nCertFileLen;
    FRtmpFreeCallback freeCb;
    void *pUser;
    int nMaxFrameCache;
} RtmpSettings;

void FRtmpWssDestroy(FRTMPHANDLER *pHandler);
int FRtmpWssInit(const char *pWsUrl, int nWsUrlLen, int nTimeoutInSecs, const RtmpSettings *pSettings, FRTMPHANDLER *pHandler);
int FRtmpPushTag(FRTMPHANDLER pHandler, char *pTag, int nTagLen);

typedef void (*LinkGhttpLog)(const char *);
void LinkGhttpSetLog(LinkGhttpLog log);
void LinkGhttpLogger(const char *);
