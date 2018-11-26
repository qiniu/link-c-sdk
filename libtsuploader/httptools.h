#ifndef __HTTP_TOOLS__
#define __HTTP_TOOLS__

#include "base.h"

/**
 * 简单的http get
 *
 * @param[in] pUrl http地址
 * @param[OUT]  pBuf 存放结果的内存
 * @param[in]  nBufLen pBuf的长度
 * @param[OUT] pRespLen 实际长度，可以监测pBuf是否过小
 * @return LINK_SUCCESS 成功;
 *         < 0, errcode
 *         > 0, http error code
 */
int LinkSimpleHttpGet(IN const char * pUrl, OUT char* pBuf, IN int nBufLen, OUT int* pRespLen);


/**
 * 简单的http pos
 *
 * @param[in] pUrl http地址
 * @param[OUT]  pBuf 存放结果的内存
 * @param[in]  nBufLen pBuf的长度
 * @param[OUT] pRespLen 实际长度，可以监测pBuf是否过小
 * @param[in] pReqBody post请求的数据
 * @param[in] nReqBodyLen pReqBody的长度
 * @param[in] pContentType http content-type头内容
 * @return LINK_SUCCESS 成功;
 *         < 0, errcode
 *         > 0, http error code
 */
int LinkSimpleHttpPost(IN const char * pUrl, OUT char* pBuf, IN int nBufLen, OUT int* pRespLen,
                       IN const char *pReqBody, IN int nReqBodyLen, IN const char *pContentType);

const char *LinkGetUploadHost(int nUseHttps, LinkUploadZone zone);

const char *LinkGetRsHost(int nUseHttps);

#endif
