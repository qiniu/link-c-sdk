/**
 * @file queue.h
 * @author Qiniu.com
 * @copyright 2018(c) Shanghai Qiniu Information Technologies Co., Ltd.
 * @brief link-c-sdk queue header file
 */

#ifndef __LINK_QUEUE_H__
#define __LINK_QUEUE_H__
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#ifndef __APPLE__
#include <stdint.h>
#include "log.h"
#endif

#define QUEUE_MIN_ALLOCATE 256

/** @name 循环队列错误返回码 */
/**@{*/
#define LINK_QUEUE_SUCCESS          0   /**< 成功 */
#define LINK_QUEUE_ERROR           -1   /**< 错误 */
#define LINK_QUEUE_NO_MEMORY    -7000   /**< 队列分配内存失败 */
#define LINK_QUEUE_NO_PUSH      -7001   /**< 队列推入数据失败 */
#define LINK_QUEUE_OVERWRIT     -7002   /**< 队列元素被覆盖 */
#define LINK_QUEUE_TIMEOUT      -7003   /**< 队列操作等待超时 */
#define LINK_QUEUE_MUTEX_ERROR  -7004   /**< 队列互斥锁错误 */
#define LINK_QUEUE_COND_ERROR   -7005   /**< 队列条件变量错误 */
#define LINK_QUEUE_NO_SPACE     -7006   /**< 获取队列元素的 buffer 空间不足 */

/**@}*/

/** @brief 循环队列属性 */
typedef enum _LinkQueueProperty{
        LQP_NONE              = 0x00,   /**< 队列空属性 */
        LQP_VARIABLE_LENGTH   = 0x01,   /**< 队列可变长度 */
        LQP_OVERWRITEABLE     = 0x02,   /**< 对列可以覆盖写入 */
        LQP_BYTE_APPEND       = 0x04,   /**< 字节计数模式
                                               队列以字节为单位存储数据
                                               长度可变，POP时将数据全部弹出 */
        LQP_NO_PUSH           = 0x08,   /**< 队列不可推入数据 */
        LQP_NO_POP            = 0x10,   /**< 队列不可弹出数据 */
} LinkQueueProperty;

/** @brief 循环队列状态 */
typedef enum _LinkQueueState {
        LQS_NONE =      0x00,       /**< 无状态 */
        LQS_EMPTY =     0x01,       /**< 队列空状态 */
        LQS_FULL =      0x02,       /**< 队列满状态 */
        LQS_OVERWIRTE = 0x04,       /**< 有数据覆盖发生*/
        LQS_DROP =      0x08        /**< 有数据丢弃发生 */
} LinkQueueState;

typedef struct _LinkQueueInfo {
        LinkQueueProperty property;     /**< 队列属性 */
        LinkQueueState state;           /**< 队列状态 */
        size_t nMaxItemLen;             /**< 队列存储元素的最大单位大小 */
        size_t nCap;                    /**< 队列存储元素的最大个数,字节计数模式时表示最大字节数 */
        size_t nCount;                  /**< 队列已保存的元素个数,字节计数模式时表示已保存字节数 */
        size_t nPushCnt;                /**< 队列已推送元素统计 */
        size_t nPopCnt;                 /**< 队列已弹出元素统计 */
        size_t nDropCnt;                /**< 队列不满足push条件时丢弃的元素统计 */
        size_t nOverwriteCnt;           /**< 队列元素被覆盖的次数 */
} LinkQueueInfo;

typedef struct _LinkQueue LinkQueue;

/** @brief 队列推送接口函数类型 */
typedef size_t(*LinkQueuePush)(LinkQueue *pQueue, const char *pData, size_t nDataLen);

/** @brief 队列弹出接口函数类型 */
/** nUsec < 0 则阻塞等待;  =0 不等待; >0 等待时间 */
typedef size_t(*LinkQueuePop)(LinkQueue *pQueue, char *pBuf, size_t pBufLen, int64_t nUSec);

/** @brief 获取队列属性接口函数类型 */
typedef int(*LinkQueueGetInfo)(const LinkQueue *pQueue, LinkQueueInfo *pInfo);

/** @brief 设置队列属性接口函数类型 */
typedef int(*LinkQueueSetProperty)(LinkQueue *pQueue, const LinkQueueProperty *pProperty);

/** @brief 队列操作结构体 */
typedef struct _LinkQueue{
        LinkQueuePush Push;                     /**< 队列推送元素接口 */
        LinkQueuePop Pop;                       /**< 队列弹出数据接口 */
        LinkQueueGetInfo GetInfo;               /**< 获取队列信息接口 */
        LinkQueueSetProperty SetProperty;       /**< 设置队列属性接口 */

}LinkQueue;

/**
 * @brief 创建队列
 *
 * @param[out] pQueue
 * @param[in] nMaxItemLen
 * @param[in] nInitItemCount
 * @param[in] pProperty
 * @return LINK_SUCCESS 成功; 其它值 失败
 */
int LinkNewCircleQueue(LinkQueue **pQueue,
                       size_t nMaxItemLen,
                       size_t nInitItemCount,
                       LinkQueueProperty pProperty
                       );

/**
 * @brief 销毁队列
 *
 * @param[in] _pQueue
 * @return
 */
int LinkDestroyQueue(LinkQueue **_pQueue);

#endif /* __LINK_QUEUE_H__ */
