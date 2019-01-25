#ifndef __LINK_BASE_H__
#define __LINK_BASE_H__

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <errno.h>
#include <inttypes.h>
#include "log.h"
#ifndef __APPLE__
#include <stdint.h>
#endif

#ifndef OUT
#define OUT
#endif
#ifndef IN
#define IN
#endif

#define LINK_MQTT_SERVER "mqtt.qnlinking.com"
#define LINK_MQTT_PORT 1883
#define LINK_MQTT_KEEPALIVE 15

#define LINK_MAX_APP_LEN 200
#define LINK_MAX_BUCKET_LEN 63
#define LINK_MAX_DEVICE_NAME_LEN 200
#define LINK_MAX_SESSION_ID_LEN 20

typedef struct {
	void *pOpaque;
        const char *pFilename;
        int nFilenameLen;
        const char *pBuf;
        int nBuflen;
}LinkPicture;
 
typedef struct {
        const char **keys;
        int *keylens;
        const char **values;
        int *valuelens;
        int len;
        int isOneShot;
}LinkSessionMeta;

typedef struct {
        int64_t nTimestamp90Khz;
        int nOffset;
        int nLength;
}LinkKeyFrameMetaInfo;

typedef void (*LinkSetKeyframeMetaInfo)(void *pUserArg, LinkKeyFrameMetaInfo *pMetaInfo);

typedef struct {
        IN char *pTokenBuf;
        IN OUT int nTokenBufLen;
        IN char *pUpHost;
        IN OUT int nUpHostLen;
        IN char* pSegUrl;
        IN OUT int nSegUrlLen;

        char sessionId[LINK_MAX_SESSION_ID_LEN+1];
        char *pAk;
        int nAkLen;
        char *pSk;
        int nSkLen;
        char *pFilePrefix;
        int nFilePrefix;
}LinkUploadParam;

typedef enum {
        LINK_UPLOAD_CB_GETPARAM = 1,
        LINK_UPLOAD_CB_UPREMOTECONFIG = 2,
        LINK_UPLOAD_CB_GETFRAMEPARAM = 3,
        LINK_UPLOAD_CB_GETTSPARAM = 4,
} LinkUploadCbType;

typedef enum {
        LINK_PLAN_TYPE_NONE = 0,
        LINK_PLAN_TYPE_24   = 1,
        LINK_PLAN_TYPE_MOVE = 2,
} LinkPlanType;

typedef int (*LinkUploadParamCallback)(IN void *pOpaque, IN OUT LinkUploadParam *pParam, IN LinkUploadCbType cbtype);

typedef enum {
        LINK_UPLOAD_TS = 1,
        LINK_UPLOAD_PIC = 2,
        LINK_UPLOAD_SEG = 3,
        LINK_UPLOAD_MOVE_SEG = 4
} LinkUploadKind;

typedef enum {
        LINK_UPLOAD_RESULT_OK = 1,
        LINK_UPLOAD_RESULT_FAIL = 2
} LinkUploadResult;
typedef void (*UploadStatisticCallback)(void *pUserOpaque, LinkUploadKind uploadKind, LinkUploadResult uploadResult);

typedef enum {
        LINK_VIDEO_NONE = 0,
        LINK_VIDEO_H264 = 1,
        LINK_VIDEO_H265 = 2
}LinkVideoFormat;

typedef enum {
        LINK_MEDIA_AUDIO = 1,
        LINK_MEDIA_VIDEO = 2,
}LinkMediaType;

typedef enum {
        LINK_AUDIO_NONE = 0,
        LINK_AUDIO_PCMU = 1,
        LINK_AUDIO_PCMA = 2,
        LINK_AUDIO_AAC = 3
}LinkAudioFormat;

typedef enum {
        LINK_UPLOAD_INIT,
        LINK_UPLOAD_FAIL,
        LINK_UPLOAD_OK
}LinkUploadState;

typedef struct _LinkMediaArg{
        LinkAudioFormat nAudioFormat;
        int nChannels;
        int nSamplerate;
        LinkVideoFormat nVideoFormat;
} LinkMediaArg;


typedef struct _LinkUserUploadArg{
        const char *pDeviceName;
        int   nDeviceNameLen;
        const char * pConfigRequestUrl;
        size_t nConfigRequestUrlLen;
        const char *pDeviceAk;
        size_t nDeviceAkLen;
        const char *pDeviceSk;
        size_t nDeviceSkLen;
        size_t nMaxUploadThreadNum;
        UploadStatisticCallback pUploadStatisticCb;
        void *pUploadStatArg;
}LinkUserUploadArg;


/** @brief 上传参数 */
typedef struct _LinkUploadArg {
        LinkAudioFormat nAudioFormat;           /**< 音频格式 */
        size_t nChannels;                       /**< 音频通道数 */
        size_t nSampleRate;                     /**< 音频通道数 */
        LinkVideoFormat nVideoFormat;           /**< 视频格式 */

        const char * pConfigRequestUrl;         /**< 获取业务配置的请求地址 */
        size_t nConfigRequestUrlLen;            /**< 业务配置的请求地址长度 */
        const char *pDeviceAk;                  /**< 设备 APP KEY */
        size_t nDeviceAkLen;                    /**< 设备 APP KEY 长度 */
        const char *pDeviceSk;                  /**< 设备 SECRET KEY */
        size_t nDeviceSkLen;                    /**< 设备 SECRET KEY 长度 */
        void(*getPictureCallback)(void *pUserData, const char *pFilename, int nFilenameLen);
        void *pGetPictureCallbackUserData;      /**< 图片上传回调函数 */
        UploadStatisticCallback *pUpStatCb;     /*上传结果回调*/
        void *pUpStatCbUserArg;                 /*作为上传结果回调的第一个参数*/

        void * reserved1;                       /**< 预留1 */
        void * reserved2;                       /**< 预留2 */
}LinkUploadArg;


typedef struct _LinkSession { // seg report info
        // session scope
        char sessionId[LINK_MAX_SESSION_ID_LEN+1];
        int64_t nSessionStartTime;
        int64_t nTsSequenceNumber;
        int64_t nSessionEndTime;
        int64_t nLastTsEndTime;
        int nSessionEndResonCode;
        int isNewSessionStarted;
        
        int64_t nAccSessionDuration;
        int64_t nAccSessionAudioDuration; // tad
        int64_t nAccSessionVideoDuration; // tvd
        
        // report scope
        int64_t nAudioGapFromLastReport; // ad
        int64_t nVideoGapFromLastReport; // vd
        int64_t nLastReportAccSessionAudioDuration;
        int64_t nLastReportAccSessionVideoDuration;
        
        // current ts scope
        int64_t nTsStartTime;
        int64_t nTsDuration;
} LinkSession;

void LinkUpdateSessionId(LinkSession* pSession, int64_t nTsStartSystime);

/************for ts output**************/
typedef struct {
        int64_t startTime;
        int64_t endTime;
        LinkMediaArg media[2];
        LinkMediaType mediaType[2];
        int nCount;
        char sessionId[21];
        const LinkSessionMeta* pSessionMeta;
} LinkMediaInfo;
typedef int (*LinkTsOutput)(const char *buffer, int size, void *userCtx, LinkMediaInfo info);
/**************************/

#define LINK_STREAM_UPLOAD 1

#define LINK_TRUE 1
#define LINK_FALSE 0

#define LINK_NO_MEMORY       -1000
#define LINK_MUTEX_ERROR     -1100
#define LINK_COND_ERROR      -1101
#define LINK_THREAD_ERROR    -1102
#define LINK_TIMEOUT         -2000
#define LINK_NO_PUSH         -2001
#define LINK_BUFFER_IS_SMALL -2003
#define LINK_ARG_TOO_LONG    -2004
#define LINK_ARG_ERROR       -2100
#define LINK_JSON_FORMAT     -2200
#define LINK_HTTP_TIME       -2300
#define LINK_OPEN_TS_ERR     -2400
#define LINK_WRITE_TS_ERR    -2401
#define LINK_WOLFSSL_ERR     -2500
#define LINK_Q_OVERWRIT      -5001
#define LINK_Q_WRONGSTATE    -5002
#define LINK_MAX_SEG         -5103
#define LINK_NOT_INITED      -5104
#define LINK_Q_OVERFLOW      -5005
#define LINK_PAUSED          -6000
#define LINK_GHTTP_FAIL      -7000
#define LINK_SUCCESS         0
#define LINK_ERROR           -1


int LinkIsProcStatusQuit();
#if 0
#define strlen(a, b) mystrlen(a, b)
inline size_t mystrlen(const char *s, int max) {
        size_t l = strlen(s);
        assert(l <= max);
        return l;
}
#endif

#define LINK_DEFINE_HANDLE(object) typedef struct object##_T* object;

#endif
