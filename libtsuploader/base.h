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

typedef struct {
        int64_t nTimestamp90Khz;
        int nOffset;
        int nLength;
}LinkKeyFrameMetaInfo;

typedef void (*LinkSetKeyframeMetaInfo)(void *pUserArg, LinkKeyFrameMetaInfo *pMetaInfo);

typedef struct {
        IN char *pTokenBuf;
        IN int nTokenBufLen;
        IN char *pTypeBuf;
        IN int nTypeBufLen;
        IN char *pUpHost;
        IN int nUpHostLen;
        IN char* pSegUrl;
        IN int nSegUrlLen;
}LinkUploadParam;

typedef int (*LinkGetUploadParamCallback)(IN void *pOpaque, IN OUT LinkUploadParam *pParam);

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

typedef enum _LinkUploadZone{
        LINK_ZONE_UNKNOWN = 0,
        LINK_ZONE_HUADONG = 1,
        LINK_ZONE_HUABEI = 2,
        LINK_ZONE_HUANAN = 3,
        LINK_ZONE_BEIMEI = 4,
        LINK_ZONE_DONGNANYA = 5,
}LinkUploadZone;

typedef enum {
        LINK_VIDEO_H264 = 1,
        LINK_VIDEO_H265 = 2
}LinkVideoFormat;

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
        const char *pDeviceId_;
        int   nDeviceIdLen_;
        const char * pApp;
        size_t nAppLen;
        const char * pConfigRequestUrl;
        size_t nConfigRequestUrlLen;
        const char *pDeviceAk;
        size_t nDeviceAkLen;
        const char *pDeviceSk;
        size_t nDeviceSkLen;
        
        UploadStatisticCallback pUploadStatisticCb;
        void *pUploadStatArg;
}LinkUserUploadArg;


/** @brief 上传参数 */
typedef struct _LinkUploadArg {
        LinkAudioFormat nAudioFormat;           /**< 音频格式 */
        size_t nChannels;                       /**< 音频通道数 */
        size_t nSampleRate;                     /**< 音频通道数 */
        LinkVideoFormat nVideoFormat;           /**< 视频格式 */
        const char * pDeviceId_;               /**< 设备名 */
        size_t nDeviceIdLen_;                  /**< 设备名长度 */
        const char * pApp;                      /**< 命名空间 */
        size_t nAppLen;                         /**< 命名空间长度 */
        const char * pConfigRequestUrl;         /**< 获取业务配置的请求地址 */
        size_t nConfigRequestUrlLen;            /**< 业务配置的请求地址长度 */
        const char *pDeviceAk;                  /**< 设备 APP KEY */
        size_t nDeviceAkLen;                    /**< 设备 APP KEY 长度 */
        const char *pDeviceSk;                  /**< 设备 SECRET KEY */
        size_t nDeviceSkLen;                    /**< 设备 SECRET KEY 长度 */
        void(*getPictureCallback)(void *pUserData, const char *pFilename, int nFilenameLen);
        void *pGetPictureCallbackUserData;
        /**< 图片上传回调函数 */
        void * reserved1;                       /**< 预留1 */
        void * reserved2;                       /**< 预留2 */
}LinkUploadArg;



#define LINK_STREAM_UPLOAD 1

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
#define LINK_PAUSED          -6000
#define LINK_GHTTP_FAIL      -7000
#define LINK_SUCCESS         0


int LinkIsProcStatusQuit();
#if 0
#define strlen mystrlen
#define strcpy mystrcpy
#endif

#define LINK_DEFINE_HANDLE(object) typedef struct object##_T* object;

#endif
