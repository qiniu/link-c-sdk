/**
 * @file tsuploaderapi.h
 * @author Qiniu.com
 * @copyright 2018(c) Shanghai Qiniu Information Technologies Co., Ltd.
 * @brief link-c-sdk api header file
 */

#ifndef __TS_UPLOADER_API__
#define __TS_UPLOADER_API__

#include "tsmuxuploader.h"
#include "log.h"
#include "base.h"

/**
 * 初始化上传 sdk， 此函数必须在任何其他子功能之前调用。
 *
 * 此函数不是线程安全函数。
 *
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkInitUploader();

/**
 * 创建并且启动一个切片上传实例
 *
 * 此函数不是线程安全函数。
 *
 * @param[out] pTsMuxUploader 切片上传实例
 * @param[in]  pAvArg 上传的 audio/video 的格式参数
 * @param[in]  pUserUploadArg 上传需要的参数，用来设置 token,deviceid
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkCreateAndStartAVUploader(OUT LinkTsMuxUploader **pTsMuxUploader,
                                 IN LinkMediaArg *pAvArg,
                                 IN LinkUserUploadArg *pUserUploadArg
                                 );

/**
 * 创建并且启动一个切片上传实例, 带图片上传功能
 *
 * 此函数不是线程安全函数。
 *
 * @param[out] pTsMuxUploader 切片上传实例
 * @param[in]  pAvArg 上传的 audio/video 的格式参数
 * @param[in]  pUserUploadArg 上传需要的参数，用来设置 token,deviceid
 * @param[in]  pPicArg 图片上传参数
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkCreateAndStartAll(OUT LinkTsMuxUploader **pTsMuxUploader,
                          IN LinkMediaArg *pAvArg,
                          IN LinkUserUploadArg *pUserUploadArg,
                          IN LinkPicUploadArg *pPicArg
                          );

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
int LinkSendUploadPictureSingal(IN LinkTsMuxUploader *pTsMuxUploader,
                                const char *pFilename,
                                int nFilenameLen,
                                const char *pBuf,
                                int nBuflen
                                );

/**
 * 片段文件更新间隔时间
 *
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @param[in] nSeconds 片段文件重命名间隔时间
 * @return NULL
 */
void LinkSetSegmentUpdateInterval(IN LinkTsMuxUploader *pTsMuxUploader,
                                  IN int64_t nSeconds
                                  );

/**
 * 通知当前没有可上传数据
 *
 * 此函数用于当上传结束时，将当前已缓存的资源完成进行上传
 * 例如当移动侦测结束时，暂时不再上传资源，调用函数后会将已缓存的资源完成切片上传
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @return NULL
 */
void LinkNotifyNomoreData(IN LinkTsMuxUploader *pTsMuxUploader);

/**
 * 暂停上传
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkPauseUpload(IN LinkTsMuxUploader *pTsMuxUploader);

/**
 * 恢复上传
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkResumeUpload(IN LinkTsMuxUploader *pTsMuxUploader);

/**
 * 临时设置切片上传文件命名的自定义后缀字段
 *
 * 仅当前切片有效
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @param[in] pType 自定义后缀名称
 * @param[in] nTypeLen 自定义后缀名称长度
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkSetTsTypeOneshot(IN LinkTsMuxUploader *pTsMuxUploader,
                         IN const char *pType,
                         IN int nTypeLen
                         );

/**
 * 设置切片上传文件命名的自定义后缀字段
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @param[in] pType 自定义后缀名称
 * @param[in] nTypeLen 自定义后缀名称长度
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkSetTsType(IN LinkTsMuxUploader *pTsMuxUploader,
                  IN const char *pType,
                  IN int nTypeLen
                  );

/**
 * 清空切片上传文件命名的自定义后缀字段
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
void LinkClearTsType(IN LinkTsMuxUploader *pTsMuxUploader);

/**
 * 设置上传实例的缓存 buffer
 *
 * buffer 最小值是 256KB, 此函数不是线程安全函数。
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @param[in] nSize 缓存 buffer 大小
 * @return NULL
 */
void LinkSetUploadBufferSize(IN LinkTsMuxUploader *pTsMuxUploader,
                             IN int nSize
                             );

/**
 * 获取上传实例的缓存 buffer 大小
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @return size > 0 上传实例的缓存 buffer 大小, <= 0 参照错误码
 */
int LinkGetUploadBufferUsedSize(IN LinkTsMuxUploader *pTsMuxUploader);

/**
 * 设置视频片段分段间隔
 *
 * 用于索引视频片段时判断是否为同一视频片段，
 * 小于视频分段间隔的两个视频文件被认为同一个视频片段
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @param[in] nSeconds 视频分段间隔
 * @return NULL
 */
void LinkUpdateNewSegmentInterval(IN LinkTsMuxUploader *pTsMuxUploader,
                                  IN int nIntervalSecond);

/**
 * 推送视频流数据
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @param[in] pData 视频数据
 * @param[in] nDataLen 视频数据大小
 * @param[in] nTimestamp 视频时间戳
 * @param[in] nIsKeyFrame 是否是关键帧
 * @param[in] nIsSegStart 是否是新的片段开始
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkPushVideo(IN LinkTsMuxUploader *pTsMuxUploader,
                  IN char * pData,
                  IN int nDataLen,
                  IN int64_t nTimestamp,
                  IN int nIsKeyFrame,
                  IN int nIsSegStart
                  );

/**
 * 推送音频流数据。
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @param[in] pData 音频数据
 * @param[in] nDataLen 音频数据大小
 * @param[in] nTimestamp 音频时间戳
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkPushAudio(IN LinkTsMuxUploader *pTsMuxUploader,
                  IN char * pData,
                  IN int nDataLen,
                  IN int64_t nTimestamp
                  );

/**
 * 销毁切片上传实例
 *
 * 如果正在上传会停止上传
 *
 * @param[in,out] pTsMuxUploader 切片上传实例
 * @return NULL
 */
void LinkDestroyAVUploader(IN OUT LinkTsMuxUploader **pTsMuxUploader);

/**
 * 销毁释放 sdk 资源。
 *
 * 此函数不是线程安全函数。
 *
 * @return NULL
 */
void LinkUninitUploader();


#endif
