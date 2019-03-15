#ifndef __TS_MUX_UPLOADER_H__
#define __TS_MUX_UPLOADER_H__
#include "base.h"
#include "tsuploader.h"
#include "resource.h"
#include "picuploader.h"

#define LINK_AUDIO_FORMAT_G711A 1
#define LINK_AUDIO_FORMAT_G711U 2
#define LINK_VIDEO_FORMAT_H264 1
#define LINK_VIDEO_FORMAT_H265 2


int LinkNewTsMuxUploader(OUT LinkTsMuxUploader **pTsMuxUploader, IN const LinkMediaArg *pAvArg, IN const LinkUserUploadArg *pUserUploadArg);
int LinkNewTsMuxUploaderWillPicAndSeg(OUT LinkTsMuxUploader **pTsMuxUploader, IN const LinkMediaArg *pAvArg,
                                      IN const LinkUserUploadArg *pUserUploadArg, IN const LinkPicUploadArg *pPicArg);
int LinkTsMuxUploaderStart(IN LinkTsMuxUploader *pTsMuxUploader);
void LinkDestroyTsMuxUploader(IN OUT LinkTsMuxUploader **pTsMuxUploader);
void LinkInitSn();

/**
 * 获取上传实例的缓存 buffer 大小
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @return size > 0 上传实例的缓存 buffer 大小, <= 0 参照错误码
 */
int LinkGetUploadBufferUsedSize(IN LinkTsMuxUploader *pTsMuxUploader);

/**
 * 设置上传实例的缓存 buffer 大小
 *
 * @param[in] pTsMuxUploader 切片上传实例
 * @param[nSize] 切片的初始化缓冲大小
 * @param[isFix] nSize作为切片的固定初始化缓冲大小, 如果为0初始化缓冲大小动态调整，否则不动态调整
 */
int LinkSetTsBufferInitSize(IN LinkTsMuxUploader *pTsMuxUploader, int nSize, int isFix);

/**
 * 创建并且启动一个切片上传实例
 *
 * 此函数不是线程安全函数。
 *
 * @param[out] pTsMuxUploader 切片上传实例
 * @param[in]  pAvArg 上传的 audio/video 的格式参数
 * @param[in]  pUserUploadArg 上传需要的参数，用来设置 token,deviceName
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkCreateAndStartAVUploader(OUT LinkTsMuxUploader **pTsMuxUploader,
                                 IN LinkMediaArg *pAvArg,
                                 IN LinkUserUploadArg *pUserUploadArg
                                 );

#endif
