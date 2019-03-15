/**
 * @file uploader.h
 * @author Qiniu.com
 * @copyright 2018(c) Shanghai Qiniu Information Technologies Co., Ltd.
 * @brief link-c-sdk api header file
 */

#include <stdio.h>
#include <stdint.h>

#ifndef __TS_UPLOADER_API__
#define __TS_UPLOADER_API__

#define LINK_TRUE 1
#define LINK_FALSE 0

/** @brief 返回值 */
#define LINK_NO_MEMORY       -1000      /**< 内存申请错误 */
#define LINK_MUTEX_ERROR     -1100      /**< 线程互斥锁错误 */
#define LINK_COND_ERROR      -1101      /**< 线程条件变量错误 */
#define LINK_THREAD_ERROR    -1102      /**< 线程错误 */
#define LINK_TIMEOUT         -2000      /**< 超时 */
#define LINK_NO_PUSH         -2001      /**< 队列不允许推送数据 */
#define LINK_BUFFER_IS_SMALL -2003      /**< 缓存太小 */
#define LINK_ARG_TOO_LONG    -2004      /**< 参数过长 */
#define LINK_ARG_ERROR       -2100      /**< 参数错误 */
#define LINK_JSON_FORMAT     -2200      /**< JSON数据格式错误 */
#define LINK_TIME_ERROR      -2300      /**< 获取时间错误 */
#define LINK_Q_OVERWRIT      -5001      /**< 队列数据被覆盖 */
#define LINK_Q_WRONGSTATE    -5002      /**< 队列状态错误 */
#define LINK_MAX_SEG         -5103      /**< 片段处理到达最大值 */
#define LINK_NOT_INITED      -5104      /**< 操作对象未初始化 */
#define LINK_MAX_CACHE       -5105      /**< 缓存到达最大值 */
#define LINK_Q_OVERFLOW      -5005      /**< 队列数据溢出 */
#define LINK_PAUSED          -6000      /**< 暂停状态 */
#define LINK_GHTTP_FAIL      -7000      /**< http请求失败 */
#define LINK_SUCCESS         0          /**< 返回成功 */
#define LINK_ERROR           -1         /**< 返回失败 */

typedef enum _LinkMediaType {
        LINK_MEDIA_AUDIO = 1,
        LINK_MEDIA_VIDEO = 2,
}LinkMediaType;

typedef enum _LinkVideoFormat {
        LINK_VIDEO_NONE = 0,
        LINK_VIDEO_H264 = 1,
        LINK_VIDEO_H265 = 2
}LinkVideoFormat;

typedef enum _LinkAudioFormat {
        LINK_AUDIO_NONE = 0,
        LINK_AUDIO_PCMU = 1,
        LINK_AUDIO_PCMA = 2,
        LINK_AUDIO_AAC = 3
}LinkAudioFormat;

typedef enum _LinkUploadKind {
        LINK_UPLOAD_TS = 1,
        LINK_UPLOAD_PIC = 2,
        LINK_UPLOAD_SEG = 3,
        LINK_UPLOAD_MOVE_SEG = 4
} LinkUploadKind;

typedef enum _LinkUploadResult {
        LINK_UPLOAD_RESULT_OK = 1,
        LINK_UPLOAD_RESULT_FAIL = 2
} LinkUploadResult;

typedef void (*UploadStatisticCallback)(void *pUserOpaque,
                LinkUploadKind uploadKind,
                LinkUploadResult uploadResult);


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
        int nTsMaxSize;                         /*ts切片的最大大小*/

        void * reserved1;                       /**< 预留1 */
        void * reserved2;                       /**< 预留2 */
}LinkUploadArg;

typedef struct _LinkTsMuxUploader LinkTsMuxUploader;
typedef struct _LinkTsMuxUploader{
        int(*PushVideo)(LinkTsMuxUploader *pTsMuxUploader, const char * pData, int nDataLen, int64_t nTimestamp, int nIsKeyFrame, int nIsSegStart, int64_t nFrameSysTime);
        int(*PushAudio)(LinkTsMuxUploader *pTsMuxUploader, const char * pData, int nDataLen, int64_t nTimestamp, int64_t nFrameSysTime);
        int (*GetUploaderBufferUsedSize)(LinkTsMuxUploader* pTsMuxUploader);
}LinkTsMuxUploader;


/**
 * 初始化上传 sdk， 此函数必须在任何其他子功能之前调用。
 *
 * 此函数不是线程安全函数。
 *
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkInit();

/**
 * 创建并且启动一个切片上传实例, 带图片上传功能
 *
 * 此函数不是线程安全函数。
 *
 * @param[out] pTsMuxUploader 切片上传实例
 * @param[in]  pUserUploadArg 上传需要的参数，用来设置 token,deviceName
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkNewUploader(LinkTsMuxUploader **pTsMuxUploader,
                    LinkUploadArg *pUserUploadArg
                    );

typedef struct _LinkSessionMeta{
        const char **keys;
        int *keylens;
        const char **values;
        int *valuelens;
        int len;
        int isOneShot;
}LinkSessionMeta;


typedef struct _LinkMediaArg{
        LinkAudioFormat nAudioFormat;
        int nChannels;
        int nSamplerate;
        LinkVideoFormat nVideoFormat;
} LinkMediaArg;


typedef struct _LinkMediaInfo {
        int64_t startTime;
        int64_t endTime;
        LinkMediaArg media[2];
        LinkMediaType mediaType[2];
        int nCount;
        char sessionId[21];
        const LinkSessionMeta* pSessionMeta;
} LinkMediaInfo;

typedef int (*LinkTsOutput)(const char *buffer, int size, void *userCtx, LinkMediaInfo info);

/**
 * 设置ts切片数据回调
 *
 * 此函数不是线程安全函数。
 *
 * @param[out] pTsMuxUploader 切片上传实例
 * @param[in]  pTsDataCb 回调函数
 * @param[in]  pUserArg 作为pTsDataCb函数的userCtx参数，返回给用户
 */
void LinkUploaderSetTsOutputCallback(LinkTsMuxUploader *pTsMuxUploader,
                                     LinkTsOutput pTsDataCb,
                                     void * pUserArg
                                     );


typedef enum {
        LinkCloudStorageStateNone = 0,
        LinkCloudStorageStateOn = 1,
        LinkCloudStorageStateOff = 2
}LinkCloudStorageState;

typedef void(*LinkCloudStorageStateCallback)(void *pOpaue, LinkCloudStorageState state);

/**
 * 云存储状态回调函数
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @param[in] cb 回调函数
 * @param[in] pOpaque 用户参数，会在cb回调函数的第一个参数中返回
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
void LinkSetCloudStorageStateCallback(LinkTsMuxUploader *pTsMuxUploader,
	LinkCloudStorageStateCallback cb, void *pOpaue);

/**
 * 发送图片上传信号
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @param[in] pOpaque
 * @param[in] pBuf 上传类型是文件时为图片本地文件名，上传类型是缓存时为缓存指针
 * @param[in] nBuflen 缓存长度或者文件名长度
 * @param[in] type 上传类型，文件上传 或者 缓存上传
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkPushPicture(LinkTsMuxUploader *pTsMuxUploader,
                    const char *pFilename,
                    int nFilenameLen,
                    const char *pBuf,
                    int nBuflen
                    );

/**
 * 刷新缓存数据
 *
 * @brief 通知当前没有可上传数据,通常使用场景为摄像头检查到移动侦测后消失调用该接口，以通知上传缓冲的数据
 *
 * 此函数用于当上传结束时，将当前已缓存的资源完成进行上传
 * 例如当移动侦测结束时，暂时不再上传资源，调用函数后会将已缓存的资源完成切片上传
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @return NULL
 */
void LinkFlushUploader(LinkTsMuxUploader *pTsMuxUploader);

/**
 * 暂停上传
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkPauseUpload(LinkTsMuxUploader *pTsMuxUploader);

/**
 * 恢复上传
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkResumeUpload(LinkTsMuxUploader *pTsMuxUploader);

/**
 * 设置片段上报的元数据
 *
 * @brief 设置片段上报的元数据,通常使用场景为摄像头检查到移动侦测后调用该接口
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @param[in] metas 自定义的元数据，key->value结构
 *                metas->isOneShot 非0，仅上报一次后便不在上报
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkSetTsType(LinkTsMuxUploader *pTsMuxUploader,
                  LinkSessionMeta *metas
                  );

/**
 * 清空段上报的元数据
 *
 * @brief 清空段上报的元数据，通常使用场景为摄像头检查到移动侦测消失后调用该接口
 *
 * @param[in] pTsMuxUploader 切片上传实例
 */
void LinkClearTsType(LinkTsMuxUploader *pTsMuxUploader);

/**
 * 推送视频流数据
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @param[in] pData 视频数据
 * @param[in] nDataLen 视频数据大小
 * @param[in] nTimestamp 视频时间戳, 如果存在音频，和音频时间戳一定要对应同一个基点
 * @param[in] nIsKeyFrame 是否是关键帧
 * @param[in] nIsSegStart 是否是新的片段开始
 * @param[in] nFrameSysTime 帧对应的系统时间,单位为m毫秒。通常的使用场景是：开启运动侦测时候，送入预录数据关键帧时候填写该预录视频关键帧对应的系统时间,其它情况可以填0
 *                          就是说，如果这个值大于1548064836000，则使用传入的时间，否则取系统时间
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkPushVideo(LinkTsMuxUploader *pTsMuxUploader,
                  char * pData,
                  int nDataLen,
                  int64_t nTimestamp,
                  int nIsKeyFrame,
                  int nIsSegStart,
                  int64_t nFrameSysTime
                  );

/**
 * 推送音频流数据
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @param[in] pData 音频数据
 * @param[in] nDataLen 音频数据大小
 * @param[in] nTimestamp 音频时间戳，必须和视频时间戳对应同一个基点
 * @param[in] nFrameSysTime 帧对应的系统时间,单位为m毫秒。目前值填固定的0
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkPushAudio(LinkTsMuxUploader *pTsMuxUploader,
                  char * pData,
                  int nDataLen,
                  int64_t nTimestamp,
                  int64_t nFrameSysTime
                  );

/**
 * 销毁切片上传实例
 *
 * 如果正在上传会停止上传
 *
 * @param[in,out] pTsMuxUploader 切片上传实例
 * @return NULL
 */
void LinkFreeUploader(LinkTsMuxUploader **pTsMuxUploader);

/**
 * 销毁释放 sdk 资源
 *
 * 此函数不是线程安全函数
 *
 * @return NULL
 */
void LinkCleanup();

/**
 * 验证七牛凭证合法性。
 *
 * @param[in] pAk 设备端的 accessKey
 * @param[in] nAkLen accessKey 长度，最大长度 512 字节
 * @param[in] pSk 设备端的 secretKey
 * @param[in] nSkLen secretKey 长度，最大长度 512 字节
 * @param[in] pToken 访问凭证， 格式为 "ak + ':' + encodedSign + ':' + encodedPutPolicy"
 * @param[in] nTokenLen Token 长度，最大长度 4096 字节
 * @return LINK_TRUE: 验证成功; LINK_FALSE: 验证失败; LINK_ERROR: 参数错误
 */
int LinkVerify(const char *pAk,
               size_t nAkLen,
               const char *pSk,
               size_t nSkLen,
               const char* pToken,
               size_t nTokenLen
               );

#endif
