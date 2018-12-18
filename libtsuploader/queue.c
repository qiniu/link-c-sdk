/**
 * @file queue.c
 * @author Qiniu.com
 * @copyright 2018(c) Shanghai Qiniu Information Technologies Co., Ltd.
 * @brief link-c-sdk queue library file
 */

#include "queue.h"

/** @brief 队列实现结构体 */
typedef struct _LinkQueueImp{
        LinkQueue linkQueue_;           /**< 队列操作接口 */
        LinkQueueProperty property_;    /**< 队列属性 */
        char *pData_;                   /**< 队列数据指针 */
        size_t nMaxItemLen_;            /**< 队列存储元素的最大单位大小 */
        size_t nCap_;                   /**< 队列存储元素的最大个数,字节计数模式时表示最大字节数 */
        size_t nStart_;                 /**< 队列头部游标 */
        size_t nEnd_;                   /**< 队列尾部游标 */
        size_t nCount_;                 /**< 队列已保存的元素个数,字节计数模式时表示已保存字节数 */
        size_t nPushCnt_;               /**< 队列已推送元素统计 */
        size_t nPopCnt_;                /**< 队列已弹出元素统计 */
        size_t nDropCnt_;               /**< 队列不满足push条件时丢弃的元素统计 */
        size_t nOverwriteCnt_;          /**< 队列元素被覆盖的次数 */
        LinkQueueState state_;          /**< 队列当前状态 */
        pthread_mutex_t mutex_;         /**< 线程互斥锁 */
        pthread_cond_t condition_;      /**< 线程条件变量 */
} LinkQueueImp;


static void updateQueueState_(LinkQueue *_pQueue)
{
        LinkQueueImp *pQueueImp = (LinkQueueImp *)_pQueue;
        pQueueImp->state_ = LQS_NONE;
        if (pQueueImp->nCount_  ==  pQueueImp->nCap_)
                pQueueImp->state_ |= LQS_FULL;
        if (pQueueImp->nCount_  == 0)
                pQueueImp->state_ |= LQS_EMPTY;
        if(pQueueImp->nOverwriteCnt_ > 0)
                pQueueImp->state_ |= LQS_OVERWIRTE;
        if(pQueueImp->nDropCnt_ > 0)
                pQueueImp->state_ |= LQS_DROP;
        return;
}


static size_t pushQueueNormal(LinkQueue *_pQueue, const char *_pData, size_t _nDataLen)
{
        if (NULL == _pData || NULL == _pQueue) {
                return LINK_QUEUE_ERROR;
        }
        LinkQueueImp *pQueueImp = (LinkQueueImp *)_pQueue;
        pthread_mutex_lock(&pQueueImp->mutex_);

        if (pQueueImp->property_ & LQP_NO_PUSH) {
                pQueueImp->nDropCnt_++;
                LinkLogWarn("Queue is no push now.");
                updateQueueState_(_pQueue);
                pthread_mutex_unlock(&pQueueImp->mutex_);
                return LINK_QUEUE_NO_PUSH;
        }

        assert(pQueueImp->nMaxItemLen_ - sizeof(size_t) >= _nDataLen);
        if (pQueueImp->nCount_ < pQueueImp->nCap_) {
                /* | start              |        |          start     | */
                /* |---========---------|   or   |=====-------========| */
                /* |         end        |        |   end              | */
                memcpy(pQueueImp->pData_ + pQueueImp->nEnd_ * pQueueImp->nMaxItemLen_, &_nDataLen, sizeof(size_t));
                memcpy(pQueueImp->pData_ + pQueueImp->nEnd_ * pQueueImp->nMaxItemLen_ + sizeof(size_t), _pData, _nDataLen);
                if(pQueueImp->nEnd_ + 1 == pQueueImp->nCap_){
                        pQueueImp->nEnd_ = 0;
                } else {
                        pQueueImp->nEnd_++;
                }
                pQueueImp->nCount_++;
                pQueueImp->nPushCnt_++;
                updateQueueState_(_pQueue);
                LinkLogTrace("[QUEUE](!full) push buflen:%d queueLen:%d, queueCap:%d", _nDataLen, pQueueImp->nCount_, pQueueImp->nCap_);
                pthread_mutex_unlock(&pQueueImp->mutex_);
                pthread_cond_signal(&pQueueImp->condition_);
                return _nDataLen;
        } else {
                /* |   queue is full    | */
                /* |====================| */
                /* |                    | */
                if (!(pQueueImp->property_ & LQP_VARIABLE_LENGTH)) {
                        if (!(pQueueImp->property_ & LQP_OVERWRITEABLE)) {
                                pQueueImp->nDropCnt_++;
                                LinkLogWarn("[QUEUE] full and no overwrite, dropped:%p", _pQueue);
                                updateQueueState_(_pQueue);
                                pthread_mutex_unlock(&pQueueImp->mutex_);
                                return 0;
                        }
                        /* If fix length, overwrite */
                        memcpy(pQueueImp->pData_ + pQueueImp->nEnd_ * pQueueImp->nMaxItemLen_, &_nDataLen, sizeof(size_t));
                        memcpy(pQueueImp->pData_ + pQueueImp->nEnd_ * pQueueImp->nMaxItemLen_  + sizeof(size_t), _pData, _nDataLen);
                        if(pQueueImp->nEnd_ + 1 == pQueueImp->nCap_){
                                pQueueImp->nEnd_ = 0;
                                pQueueImp->nStart_ = 0;
                        } else {
                                pQueueImp->nEnd_++;
                                pQueueImp->nStart_++;
                        }
                        pQueueImp->nPushCnt_++;
                        pQueueImp->nOverwriteCnt_++;
                        updateQueueState_(_pQueue);
                        LinkLogTrace("[QUEUE](full fix length) push buflen:%d queueLen:%d, queueCap:%d", _nDataLen, pQueueImp->nCount_, pQueueImp->nCap_);
                        pthread_mutex_unlock(&pQueueImp->mutex_);
                        pthread_cond_signal(&pQueueImp->condition_);
                        return LINK_QUEUE_OVERWRIT;
                } else {
                        /* If variable length, queue capacity expand 1.5 times when it is full */
                        size_t nNewCap = (pQueueImp->nCap_ + 1) * 3 / 2;
                        if (pQueueImp->nMaxItemLen_ * nNewCap > QUEUE_MIN_ALLOCATE) {
                                char *pNewSpace = (char *)realloc(pQueueImp->pData_, pQueueImp->nMaxItemLen_ * nNewCap);
                                if (NULL == pNewSpace) {
                                        pthread_mutex_unlock(&pQueueImp->mutex_);
                                        return LINK_QUEUE_NO_MEMORY;
                                }
                                pQueueImp->pData_ = pNewSpace;
                        }

                        if (pQueueImp->nStart_ == 0) {
                                /* |start         |       |start               | */
                                /* ||=============|  ==>  |==============------| */
                                /* |end           |       |             end    | */
                                pQueueImp->nEnd_ = pQueueImp->nCap_;
                        } else {
                                /* |     start    |       |            start   | */
                                /* |=======|======|  ==>  |=======-------======| */
                                /* |      end     |       |     end            | */
                                memmove(pQueueImp->pData_ + pQueueImp->nMaxItemLen_ * (nNewCap - (pQueueImp->nCap_ - pQueueImp->nStart_)),
                                                pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nMaxItemLen_,
                                                (pQueueImp->nCap_ - pQueueImp->nStart_) * pQueueImp->nMaxItemLen_);
                                pQueueImp->nStart_ = nNewCap - (pQueueImp->nCap_ - pQueueImp->nStart_);
                        }
                        pQueueImp->nCap_ = nNewCap;
                        /* push new iterm into queue */
                        memcpy(pQueueImp->pData_ + pQueueImp->nEnd_ * pQueueImp->nMaxItemLen_, &_nDataLen, sizeof(size_t));
                        memcpy(pQueueImp->pData_ + pQueueImp->nEnd_ * pQueueImp->nMaxItemLen_ + sizeof(size_t), _pData, _nDataLen);
                        if(pQueueImp->nEnd_ + 1 == pQueueImp->nCap_){
                                pQueueImp->nEnd_ = 0;
                        } else {
                                pQueueImp->nEnd_++;
                        }
                        pQueueImp->nCount_++;
                        pQueueImp->nPushCnt_++;
                        updateQueueState_(_pQueue);
                        LinkLogTrace("[QUEUE](full variable length) push buflen:%d queueLen:%d, queueCap:%d", _nDataLen, pQueueImp->nCount_, pQueueImp->nCap_);
                        pthread_mutex_unlock(&pQueueImp->mutex_);
                        pthread_cond_signal(&pQueueImp->condition_);
                        return _nDataLen;
                }
        }
        updateQueueState_(_pQueue);
        pthread_mutex_unlock(&pQueueImp->mutex_);
        return LINK_QUEUE_ERROR;
}

static size_t popQueueNormal(LinkQueue *_pQueue, char *_pBuf, size_t _nBufLen, int64_t _nUSec)
{
        if (NULL == _pQueue || NULL == _pBuf) {
                return LINK_QUEUE_ERROR;
        }
        LinkQueueImp *pQueueImp = (LinkQueueImp *)_pQueue;

        pthread_mutex_lock(&pQueueImp->mutex_);

        if (pQueueImp->property_ & LQP_NO_POP) {
                pthread_mutex_unlock(&pQueueImp->mutex_);
                return 0;
        }
        if ((pQueueImp->property_ & LQP_NO_PUSH) && (pQueueImp->nCount_ == 0)) {
                pthread_mutex_unlock(&pQueueImp->mutex_);
                return 0;
        }

        int ret = 0;
        while (pQueueImp->nCount_ == 0) {
                if (_nUSec < 0 ) {
                        ret = pthread_cond_wait(&pQueueImp->condition_, &pQueueImp->mutex_);
                } else {
                        struct timeval now;
                        gettimeofday(&now, NULL);
                        struct timespec timeout;
                        timeout.tv_sec = now.tv_sec + _nUSec / 1000000;
                        timeout.tv_nsec = (now.tv_usec + _nUSec % 1000000) * 1000;
                        ret = pthread_cond_timedwait(&pQueueImp->condition_, &pQueueImp->mutex_, &timeout);
                        if (ret == ETIMEDOUT) {
                                pthread_mutex_unlock(&pQueueImp->mutex_);
                                return LINK_QUEUE_TIMEOUT;
                        }
                }

                if (pQueueImp->nCount_ == 0) {
                        pthread_mutex_unlock(&pQueueImp->mutex_);
                        return 0;
                }
        }
        assert (pQueueImp->nCount_ != 0);

        size_t nDataLen = 0;
        memcpy(&nDataLen, pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nMaxItemLen_, sizeof(size_t));

        int32_t nRemain = nDataLen - _nBufLen;
        LinkLogTrace("pop remain:%d pop:%d buflen:%d Count:%d", nRemain, nDataLen, _nBufLen, pQueueImp->nCount_);
        if (nRemain > 0) {
                memcpy(_pBuf, pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nMaxItemLen_ + sizeof(size_t), _nBufLen);
                memcpy(pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nMaxItemLen_, &nRemain, sizeof(size_t));
                memmove(pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nMaxItemLen_ + sizeof(size_t),
                       pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nMaxItemLen_ + sizeof(size_t) + _nBufLen,
                       nRemain);
                nDataLen = _nBufLen;
        } else {
                memcpy(_pBuf, pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nMaxItemLen_ + sizeof(size_t), nDataLen);
                memset(pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nMaxItemLen_, 0, pQueueImp->nMaxItemLen_);
                if (pQueueImp->nStart_ + 1 == pQueueImp->nCap_) {
                        pQueueImp->nStart_ = 0;
                } else {
                        pQueueImp->nStart_++;
                }
                pQueueImp->nCount_--;
        }
        updateQueueState_(_pQueue);
        pthread_mutex_unlock(&pQueueImp->mutex_);
        return nDataLen;
}

static size_t pushQueueByteAppend(LinkQueue *_pQueue, const char *_pData, size_t _nDataLen)
{
        if (NULL == _pData || NULL == _pQueue) {
                return LINK_QUEUE_ERROR;
        }
        LinkQueueImp *pQueueImp = (LinkQueueImp *)_pQueue;

        pthread_mutex_lock(&pQueueImp->mutex_);

        if (pQueueImp->property_ & LQP_NO_PUSH) {
                pQueueImp->nDropCnt_++;
                LinkLogWarn("Queue is no push now.");
                updateQueueState_(_pQueue);
                pthread_mutex_unlock(&pQueueImp->mutex_);
                return LINK_QUEUE_NO_PUSH;
        }

        assert(pQueueImp->property_ & LQP_BYTE_APPEND);

        if (pQueueImp->pData_ == NULL) {
                pQueueImp->pData_ = (char *)malloc(pQueueImp->nCap_);
                if (pQueueImp->pData_ == NULL) {
                        return LINK_QUEUE_NO_MEMORY;
                }
        }
        int newLen = 0;
        if (_nDataLen + pQueueImp->nCount_ > pQueueImp->nCap_) {
                newLen = (pQueueImp->nCount_ + pQueueImp->nCount_) * 3 / 2;
                char * newSpace = (char *)realloc(pQueueImp->pData_, newLen);
                if (newSpace == NULL) {
                        pthread_mutex_unlock(&pQueueImp->mutex_);
                        return LINK_QUEUE_NO_MEMORY;
                }
                pQueueImp->pData_ = newSpace;
                pQueueImp->nCap_ = newLen;
        }

        memcpy(pQueueImp->pData_ + pQueueImp->nCount_, _pData, _nDataLen);
        pQueueImp->nCount_ += _nDataLen;
        pQueueImp->nEnd_ += _nDataLen;
        pQueueImp->nPushCnt_ += _nDataLen;
        updateQueueState_(_pQueue);
        pthread_mutex_unlock(&pQueueImp->mutex_);
        return _nDataLen;

}

static size_t popQueueByteAppend(LinkQueue *_pQueue, char *_pBuf, size_t _nBufLen, const int64_t _nUSec)
{
        if (NULL == _pQueue || NULL == _pBuf) {
                return LINK_QUEUE_ERROR;
        }
        LinkQueueImp *pQueueImp = (LinkQueueImp *)_pQueue;

        pthread_mutex_lock(&pQueueImp->mutex_);

        if (pQueueImp->property_ & LQP_NO_POP) {
                pthread_mutex_unlock(&pQueueImp->mutex_);
                return 0;
        }
        if ((pQueueImp->property_ & LQP_NO_PUSH) && (pQueueImp->state_ & LQS_EMPTY)) {
                pthread_mutex_unlock(&pQueueImp->mutex_);
                return 0;
        }
        int nBuflen = pQueueImp->nCount_;
        if (_nBufLen < nBuflen) {
                pthread_mutex_unlock(&pQueueImp->mutex_);
                LinkLogWarn("[QUEUE] buffer no enough space, buffer:%d, need:%d", _nBufLen, nBuflen);
                return LINK_QUEUE_NO_SPACE;
        }

        int ret = 0;
        while (pQueueImp->nCount_ == 0) {
                if (_nUSec < 0) {
                        ret = pthread_cond_wait(&pQueueImp->condition_, &pQueueImp->mutex_);
                } else {
                        struct timeval now;
                        gettimeofday(&now, NULL);
                        struct timespec timeout;
                        timeout.tv_sec = now.tv_sec + _nUSec / 1000000;
                        timeout.tv_nsec = (now.tv_usec + _nUSec % 1000000) * 1000;
                        ret = pthread_cond_timedwait(&pQueueImp->condition_, &pQueueImp->mutex_, &timeout);
                        if (ret == ETIMEDOUT) {
                                pthread_mutex_unlock(&pQueueImp->mutex_);
                                return LINK_QUEUE_TIMEOUT;
                        }
                }
                if (pQueueImp->nCount_ == 0) {
                        pthread_mutex_unlock(&pQueueImp->mutex_);
                        return 0;
                }
        }

        assert(pQueueImp->nCount_ != 0);
        assert(pQueueImp->property_ & LQP_BYTE_APPEND);
        memcpy(_pBuf, pQueueImp->pData_, nBuflen);
        pQueueImp->nPopCnt_ += pQueueImp->nCount_;
        pQueueImp->nCount_ = 0;
        pQueueImp->nEnd_ = 0;
        updateQueueState_(_pQueue);
        pthread_mutex_unlock(&pQueueImp->mutex_);
        return nBuflen;
}


static int getInfoQueue(const LinkQueue * _pQueue, LinkQueueInfo * _pInfo)
{
        if (NULL == _pQueue || NULL == _pInfo) {
                return LINK_QUEUE_ERROR;
        }
        memset(_pInfo, 0, sizeof(LinkQueueInfo));
        LinkQueueImp *pQueueImp = (LinkQueueImp *)_pQueue;
        pthread_mutex_lock(&pQueueImp->mutex_);

        _pInfo->property = pQueueImp->property_;
        _pInfo->state = pQueueImp->state_;
        _pInfo->nMaxItemLen = (pQueueImp->nMaxItemLen_ == 1) ? 1 : pQueueImp->nMaxItemLen_ - sizeof(size_t);
        _pInfo->nCap = pQueueImp->nCap_;
        _pInfo->nCount = pQueueImp->nCount_;
        _pInfo->nPushCnt = pQueueImp->nPushCnt_;
        _pInfo->nPopCnt = pQueueImp->nPopCnt_;
        _pInfo->nDropCnt = pQueueImp->nDropCnt_;
        _pInfo->nOverwriteCnt = pQueueImp->nOverwriteCnt_;

        pthread_mutex_unlock(&pQueueImp->mutex_);
        return LINK_QUEUE_SUCCESS;
}


static int setPropertyQueue(LinkQueue * _pQueue, const LinkQueueProperty * _pProperty)
{
        if (NULL == _pQueue || NULL == _pProperty) {
                return LINK_QUEUE_ERROR;
        }
        LinkQueueImp *pQueueImp = (LinkQueueImp *)_pQueue;
        pthread_mutex_lock(&pQueueImp->mutex_);
        /* Only can set  LQP_NO_PUSH and LQP_NO_POP */
        pQueueImp->property_ |= (*_pProperty & LQP_NO_PUSH);
        pQueueImp->property_ |= (*_pProperty & LQP_NO_POP);
        pthread_mutex_unlock(&pQueueImp->mutex_);
        return LINK_QUEUE_SUCCESS;
}


int LinkNewCircleQueue(LinkQueue **_pQueue, size_t _nMaxItemLen, size_t _nInitItemCount, LinkQueueProperty _pProperty)
{
        if (NULL ==_pQueue || _nMaxItemLen < 1 || _nInitItemCount < 1) {
                return LINK_QUEUE_ERROR;
        }
        LinkQueueImp *pQueueImp = (LinkQueueImp *)malloc(sizeof(LinkQueueImp));
        if (NULL == pQueueImp) {
                return LINK_QUEUE_NO_MEMORY;
        }
        memset(pQueueImp, 0, sizeof(LinkQueueImp));

        /* Initialize property */
        pQueueImp->property_ = _pProperty;


        if (pQueueImp->property_ & LQP_BYTE_APPEND) {
                /* Asume item Length is 1 when byte append mode  */
                pQueueImp->nMaxItemLen_ = 1;
                pQueueImp->nCap_ = _nInitItemCount * _nMaxItemLen;
        } else {
                /* Add a size_t type to representation the length of data */
                pQueueImp->nMaxItemLen_ = _nMaxItemLen + sizeof(size_t);
                pQueueImp->nCap_ = _nInitItemCount;
        }

        char *pData;
        /* allocate memory for elements */
        /* at least allocate QUEUE_MIN_ALLOCATE byte for variable length to avoid fragment */
        if (pQueueImp->nMaxItemLen_ * pQueueImp->nCap_ < QUEUE_MIN_ALLOCATE) {
                pData = (char *)malloc(QUEUE_MIN_ALLOCATE);
        } else {
                pData = (char *)malloc(pQueueImp->nMaxItemLen_ * pQueueImp->nCap_);
        }
        if (NULL == pData) {
                return LINK_QUEUE_NO_MEMORY;
        }
        memset(pData, 0, pQueueImp->nMaxItemLen_ * pQueueImp->nCap_);
        pQueueImp->pData_ = pData;

        /* Initialize pthread mutex and cond */
        int ret = pthread_mutex_init(&pQueueImp->mutex_, NULL);
        if (ret != 0){
                return LINK_QUEUE_MUTEX_ERROR;
        }
        ret = pthread_cond_init(&pQueueImp->condition_, NULL);
        if (ret != 0){
                pthread_mutex_destroy(&pQueueImp->mutex_);
                return LINK_QUEUE_COND_ERROR;
        }

        /* Initialize interface */
        if (_pProperty & LQP_BYTE_APPEND) {
                pQueueImp->linkQueue_.Pop = popQueueByteAppend;
                pQueueImp->linkQueue_.Push = pushQueueByteAppend;
        } else {
                pQueueImp->linkQueue_.Pop = popQueueNormal;
                pQueueImp->linkQueue_.Push = pushQueueNormal;
        }
        pQueueImp->linkQueue_.GetInfo = getInfoQueue;
        pQueueImp->linkQueue_.SetProperty = setPropertyQueue;

        *_pQueue = (LinkQueue*)pQueueImp;
        updateQueueState_(*_pQueue);
        return LINK_QUEUE_SUCCESS;
}


int LinkDestroyQueue(LinkQueue **_pQueue)
{
        if (NULL == _pQueue || NULL == *_pQueue) {
                return LINK_QUEUE_ERROR;
        }
        LinkQueueImp *pQueueImp = (LinkQueueImp *)(*_pQueue);

        pthread_mutex_lock(&pQueueImp->mutex_);
        pQueueImp->property_ |= LQP_NO_PUSH;
        pQueueImp->property_ |= LQP_NO_POP;
        updateQueueState_(*_pQueue);
        pthread_mutex_unlock(&pQueueImp->mutex_);

        pthread_mutex_destroy(&pQueueImp->mutex_);
        pthread_cond_destroy(&pQueueImp->condition_);

        if (pQueueImp->pData_) {
                free(pQueueImp->pData_);
        }
        free(pQueueImp);
        *_pQueue = NULL;
        return LINK_QUEUE_SUCCESS;
}
