/**
 * @file uploader.h
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
int LinkInit();

/**
 * 创建并且启动一个切片上传实例, 带图片上传功能
 *
 * 此函数不是线程安全函数。
 *
 * @param[out] pTsMuxUploader 切片上传实例
 * @param[in]  pAvArg 上传的 audio/video 的格式参数
 * @param[in]  pUserUploadArg 上传需要的参数，用来设置 token,deviceName
 * @param[in]  pPicArg 图片上传参数
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkNewUploader(OUT LinkTsMuxUploader **pTsMuxUploader,
                          IN LinkUploadArg *pUserUploadArg
                          );

/**
 * 设置日志回调函数
 *
 * 此函数不是线程安全函数。
 *
 * @param[in]  pLogCb 回调函数
 */
void LinkSetLogCallback(IN LinkLogFunc pLogCb);

/**
 * 设置ts切片数据回调
 *
 * 此函数不是线程安全函数。
 *
 * @param[out] pTsMuxUploader 切片上传实例
 * @param[in]  pTsDataCb 回调函数
 * @param[in]  pUserArg 作为pTsDataCb函数的userCtx参数，返回给用户
 */
void LinkUploaderSetTsOutputCallback(IN LinkTsMuxUploader *pTsMuxUploader,
                               IN LinkTsOutput pTsDataCb, IN void * pUserArg
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
int LinkPushPicture(IN LinkTsMuxUploader *pTsMuxUploader,
                                const char *pFilename,
                                int nFilenameLen,
                                const char *pBuf,
                                int nBuflen
                                );

/**
 * @brief 通知当前没有可上传数据,通常使用场景为摄像头检查到移动侦测后消失调用该接口，以通知上传缓冲的数据
 *
 * 此函数用于当上传结束时，将当前已缓存的资源完成进行上传
 * 例如当移动侦测结束时，暂时不再上传资源，调用函数后会将已缓存的资源完成切片上传
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @return NULL
 */
void LinkFlushUploader(IN LinkTsMuxUploader *pTsMuxUploader);

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
 * @brief 设置片段上报的元数据,通常使用场景为摄像头检查到移动侦测后调用该接口
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @param[in] metas 自定义的元数据，key->value结构
 *                metas->isOneShot 非0，仅上报一次后便不在上报
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkSetTsType(IN LinkTsMuxUploader *pTsMuxUploader,IN LinkSessionMeta *metas);

/**
 * @brief 清空段上报的元数据，通常使用场景为摄像头检查到移动侦测消失后调用该接口
 *
 * @param[in] pTsMuxUploader 切片上传实例
 */
void LinkClearTsType(IN LinkTsMuxUploader *pTsMuxUploader);

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
 *
 */
int LinkPushVideo(IN LinkTsMuxUploader *pTsMuxUploader,
                  IN char * pData,
                  IN int nDataLen,
                  IN int64_t nTimestamp,
                  IN int nIsKeyFrame,
                  IN int nIsSegStart,
                  IN int64_t nFrameSysTime
                  );

/**
 * 推送音频流数据。
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @param[in] pData 音频数据
 * @param[in] nDataLen 音频数据大小
 * @param[in] nTimestamp 音频时间戳，必须和视频时间戳对应同一个基点
 * @param[in] nFrameSysTime 帧对应的系统时间,单位为m毫秒。目前值填固定的0
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkPushAudio(IN LinkTsMuxUploader *pTsMuxUploader,
                  IN char * pData,
                  IN int nDataLen,
                  IN int64_t nTimestamp,
                  IN int64_t nFrameSysTime
                  );

/**
 * 销毁切片上传实例
 *
 * 如果正在上传会停止上传
 *
 * @param[in,out] pTsMuxUploader 切片上传实例
 * @return NULL
 */
void LinkFreeUploader(IN OUT LinkTsMuxUploader **pTsMuxUploader);

/**
 * 销毁释放 sdk 资源。
 *
 * 此函数不是线程安全函数。
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
int LinkVerify(const char *pAk, size_t nAkLen, char *pSk, size_t nSkLen, char* pToken, size_t nTokenLen);

#endif
