#include "queue.h"
#include "base.h"
#include "log/log.h"


#define QUEUE_READ_ONLY_STATE 1
#define QUEUE_TIMEOUT_STATE 2

typedef struct _CircleQueueImp{
        LinkCircleQueue circleQueue;
        char *pData_;
        int nCap_;
        int nLen_;
        int nLenInByte_;
        int nStart_;
        int nEnd_;
        volatile int nQState_;
        int nItemLen_;
        pthread_mutex_t mutex_;
        pthread_cond_t condition_;
        CircleQueuePolicy policy;
        LinkUploaderStatInfo statInfo;
	int nIsAvailableAfterTimeout;
}CircleQueueImp;

static int queueAppendPush(LinkCircleQueue *_pQueue, char *pData_, int nDataLen) {
        CircleQueueImp *pQueueImp = (CircleQueueImp *)_pQueue;
        if (nDataLen + pQueueImp->nLen_  > pQueueImp->nCap_) {
                int newLen = pQueueImp->nCap_ * 3 / 2;
                char *pTmp = (char *)realloc(pQueueImp->pData_, newLen);
                if (pTmp == NULL) {
                        pthread_mutex_unlock(&pQueueImp->mutex_);
                        return LINK_NO_MEMORY;
                }

                pQueueImp->pData_ = pTmp;
                pQueueImp->nCap_ = newLen;
                pQueueImp->nLenInByte_ = newLen;
        }
        
        memcpy(pQueueImp->pData_ + pQueueImp->nLen_, pData_, nDataLen);
        pQueueImp->statInfo.nPushDataBytes_ += nDataLen;
        pQueueImp->nLen_ += nDataLen;
        return nDataLen;
}

static int PushQueue(LinkCircleQueue *_pQueue, char *pData_, int nDataLen)
{
        if (NULL == pData_ || NULL == _pQueue) {
                        return LINK_ERROR;
        }

        CircleQueueImp *pQueueImp = (CircleQueueImp *)_pQueue;
        assert(pQueueImp->nItemLen_ - sizeof(int) >= nDataLen);

        pthread_mutex_lock(&pQueueImp->mutex_);
        
        if (!pQueueImp->nIsAvailableAfterTimeout && pQueueImp->nQState_ == QUEUE_TIMEOUT_STATE) {
                pQueueImp->statInfo.nDropped += nDataLen;
                LinkLogWarn("queue is timeout dropped:%p", _pQueue);
                pthread_mutex_unlock(&pQueueImp->mutex_);
                return 0;
        }
        if (pQueueImp->nQState_ == QUEUE_READ_ONLY_STATE) {
                pQueueImp->statInfo.nDropped += nDataLen;
                LinkLogWarn("queue is only readable now");
                pthread_mutex_unlock(&pQueueImp->mutex_);
                return LINK_NO_PUSH;
        }
        
        if (pQueueImp->policy == TSQ_APPEND) {
                int ret = queueAppendPush(_pQueue, pData_, nDataLen);
                
                pthread_mutex_unlock(&pQueueImp->mutex_);
                if (ret > 0)
                        pthread_cond_signal(&pQueueImp->condition_);
                return ret;
        }
        
        int nPos = pQueueImp->nEnd_;
        if (pQueueImp->nLen_ < pQueueImp->nCap_) {
                if(pQueueImp->nEnd_ + 1 == pQueueImp->nCap_){
                        pQueueImp->nEnd_ = 0;
                } else {
                        pQueueImp->nEnd_++;
                }
                memcpy(pQueueImp->pData_ + nPos * pQueueImp->nItemLen_, &nDataLen, sizeof(int));
                memcpy(pQueueImp->pData_ + nPos * pQueueImp->nItemLen_ + sizeof(int), pData_, nDataLen);
                pQueueImp->nLen_++;
                pQueueImp->statInfo.nPushDataBytes_ += nDataLen;
                pthread_mutex_unlock(&pQueueImp->mutex_);
                pthread_cond_signal(&pQueueImp->condition_);
                return nDataLen;
        }
        
        if (pQueueImp->nLen_ == pQueueImp->nCap_) {
                if (pQueueImp->policy == TSQ_FIX_LENGTH) {
                        pthread_mutex_unlock(&pQueueImp->mutex_);
                        pthread_cond_signal(&pQueueImp->condition_);
                        return LINK_Q_OVERFLOW;
                } else if (pQueueImp->policy == TSQ_FIX_LENGTH_CAN_OVERWRITE) {
                        if(pQueueImp->nEnd_ + 1 == pQueueImp->nCap_){
                                pQueueImp->nEnd_ = 0;
                                pQueueImp->nStart_ = 0;
                        } else {
                                pQueueImp->nEnd_++;
                                pQueueImp->nStart_++;
                        }
                        memcpy(pQueueImp->pData_ + nPos * pQueueImp->nItemLen_, &nDataLen, sizeof(int));
                        memcpy(pQueueImp->pData_ + nPos * pQueueImp->nItemLen_  + sizeof(int), pData_, nDataLen);
                        pthread_mutex_unlock(&pQueueImp->mutex_);
                        pthread_cond_signal(&pQueueImp->condition_);

                        pQueueImp->statInfo.nPushDataBytes_ += nDataLen;
                        pQueueImp->statInfo.nOverwriteCnt++;
                        return LINK_Q_OVERWRIT;
                } else {
                        //TODO TSQ_VAR_LENGTH not used now. so there is may have bug
                        int newLen = pQueueImp->nCap_ * 3 / 2;
                        char *pTmp = (char *)malloc(pQueueImp->nItemLen_ * newLen);
                        int nOriginCap = pQueueImp->nCap_;
                        if (pTmp == NULL) {
                                pthread_mutex_unlock(&pQueueImp->mutex_);
                                return LINK_NO_MEMORY;
                        }
                        
                        
                        if (pQueueImp->nStart_ == 0) {
                                memcpy(pTmp, pQueueImp->pData_, pQueueImp->nLenInByte_);
                        } else {
                                memcpy(pTmp, pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nItemLen_,
                                       pQueueImp->nLenInByte_ - pQueueImp->nStart_ * pQueueImp->nItemLen_);
                                memcpy(pTmp + pQueueImp->nLenInByte_ - pQueueImp->nStart_ * pQueueImp->nItemLen_,
                                       pQueueImp->pData_, pQueueImp->nEnd_ * pQueueImp->nItemLen_);
                        }
                        pQueueImp->nStart_ = 0;
                        
                        memcpy(pTmp + pQueueImp->nLenInByte_, &nDataLen, sizeof(int));
                        memcpy(pTmp + pQueueImp->nLenInByte_+ sizeof(int), pData_, nDataLen);
                        
                        pQueueImp->nEnd_ = nOriginCap + 1;
                        pQueueImp->nLen_++;
                        
                        pQueueImp->statInfo.nPushDataBytes_ += nDataLen;
                        
                        pQueueImp->nCap_ = newLen;
                        free(pQueueImp->pData_);
                        pQueueImp->pData_ = pTmp;
                        pQueueImp->nLenInByte_ = newLen * pQueueImp->nItemLen_;
                        
                        pthread_mutex_unlock(&pQueueImp->mutex_);
                        
                        pthread_cond_signal(&pQueueImp->condition_);

                        return nDataLen;
                }
        }
        
        pthread_mutex_unlock(&pQueueImp->mutex_);
        
        return -1;
}

static int PopQueueWithTimeout(LinkCircleQueue *_pQueue, char *pBuf_, int nBufLen, int64_t nMicroSec)
{
        if (NULL == _pQueue || NULL == pBuf_) {
                return LINK_ERROR;
        }
        CircleQueueImp *pQueueImp = (CircleQueueImp *)_pQueue;
        
        pthread_mutex_lock(&pQueueImp->mutex_);
        if (!pQueueImp->nIsAvailableAfterTimeout && pQueueImp->nQState_ == QUEUE_TIMEOUT_STATE) {
                pthread_mutex_unlock(&pQueueImp->mutex_);
                return LINK_TIMEOUT;
        }
        if (pQueueImp->nQState_ == QUEUE_READ_ONLY_STATE && pQueueImp->nLen_ == 0) {
                pthread_mutex_unlock(&pQueueImp->mutex_);
                return 0;
        }
        
        int ret = 0;
        while (pQueueImp->nLen_ == 0) {
                struct timeval now;
                gettimeofday(&now, NULL);
                struct timespec timeout;
                timeout.tv_sec = now.tv_sec + (now.tv_usec + nMicroSec) / 1000000;
                timeout.tv_nsec = ((now.tv_usec + nMicroSec) % 1000000)*1000;
                
                ret = pthread_cond_timedwait(&pQueueImp->condition_, &pQueueImp->mutex_, &timeout);
                if (ret == ETIMEDOUT) {
                        pthread_mutex_unlock(&pQueueImp->mutex_);
                        pQueueImp->nQState_ = QUEUE_TIMEOUT_STATE;
                        return LINK_TIMEOUT;
                }
                if (pQueueImp->nLen_ == 0) {
                        pthread_mutex_unlock(&pQueueImp->mutex_);
                        return 0;
                }
        }
        assert (pQueueImp->nLen_ != 0);
        int nDataLen = 0;
        memcpy(&nDataLen, pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nItemLen_, sizeof(int));
        int nRemain = nDataLen - nBufLen;
        LinkLogTrace("pop remain:%d pop:%d buflen:%d len:%d", nRemain, nDataLen, nBufLen, pQueueImp->nLen_);
        if (nRemain > 0) {
                memcpy(pBuf_, pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nItemLen_ + sizeof(int), nBufLen);
                memcpy(pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nItemLen_, &nRemain, sizeof(int));
                memmove(pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nItemLen_ + sizeof(int),
                       pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nItemLen_ + sizeof(int) + nBufLen,
                       nRemain);
                nDataLen = nBufLen;
        } else {
                memcpy(pBuf_, pQueueImp->pData_ + pQueueImp->nStart_ * pQueueImp->nItemLen_ + sizeof(int), nDataLen);
                if (pQueueImp->nStart_ + 1 == pQueueImp->nCap_) {
                        pQueueImp->nStart_ = 0;
                } else {
                        pQueueImp->nStart_++;
                }
                pQueueImp->nLen_--;
        }
        
        pQueueImp->statInfo.nPopDataBytes_ += nDataLen;
        pthread_mutex_unlock(&pQueueImp->mutex_);
        return nDataLen;
}


static int PopQueue(LinkCircleQueue *_pQueue, char *pBuf_, int nBufLen, int64_t nMicroSec)
{
        CircleQueueImp *pQueueImp = (CircleQueueImp *)_pQueue;
        if (pQueueImp->statInfo.nOverwriteCnt > 0) {
                return LINK_Q_OVERWRIT;
        }
        return PopQueueWithTimeout(_pQueue, pBuf_, nBufLen, nMicroSec);
}

static int PopQueueWithNoOverwrite(LinkCircleQueue *_pQueue, char *pBuf_, int nBufLen)
{
        CircleQueueImp *pQueueImp = (CircleQueueImp *)_pQueue;
        if (pQueueImp->statInfo.nOverwriteCnt > 0) {
                return LINK_Q_OVERWRIT;
        }
        int64_t usec = 1000000;
        if (pQueueImp->statInfo.nPushDataBytes_> 0) {
                return PopQueueWithTimeout(_pQueue, pBuf_, nBufLen, usec * 1);
        } else {
                return PopQueueWithTimeout(_pQueue, pBuf_, nBufLen, usec * 60 * 60 * 24 * 365);
        }
}

static void StopPush(LinkCircleQueue *_pQueue)
{
        CircleQueueImp *pQueueImp = (CircleQueueImp *)_pQueue;
        
        pthread_mutex_lock(&pQueueImp->mutex_);
        pQueueImp->nQState_ = QUEUE_READ_ONLY_STATE;
        pthread_mutex_unlock(&pQueueImp->mutex_);
        
        pthread_cond_signal(&pQueueImp->condition_);
        return;
}

static void getStatInfo(LinkCircleQueue *_pQueue, LinkUploaderStatInfo *_pStatInfo)
{
        CircleQueueImp *pQueueImp = (CircleQueueImp *)_pQueue;
        
        _pStatInfo->nPushDataBytes_ = pQueueImp->statInfo.nPushDataBytes_;
        _pStatInfo->nPopDataBytes_ = pQueueImp->statInfo.nPopDataBytes_;
        _pStatInfo->nLen_ = pQueueImp->nLen_;
        _pStatInfo->nDropped = pQueueImp->statInfo.nDropped;
        _pStatInfo->nIsReadOnly = pQueueImp->nQState_;
        return;
}

CircleQueuePolicy getQueueType(LinkCircleQueue *_pQueue) {
        CircleQueueImp *pQueueImp = (CircleQueueImp *)_pQueue;
        return pQueueImp->policy;
}

int LinkNewCircleQueue(LinkCircleQueue **_pQueue, int nIsAvailableAfterTimeout, CircleQueuePolicy _policy, int _nMaxItemLen, int _nInitItemCount)
{
        if (NULL ==_pQueue || _nMaxItemLen < 1 || _nInitItemCount < 1) {
                    return LINK_ERROR;
        }
        int ret;
        CircleQueueImp *pQueueImp = (CircleQueueImp *)malloc(sizeof(CircleQueueImp));
        
        char *pData = (char *)malloc((_nMaxItemLen + sizeof(int)) * _nInitItemCount);
        if (pQueueImp == NULL) {
                return LINK_NO_MEMORY;
        }
        memset(pQueueImp, 0, sizeof(CircleQueueImp));

        ret = pthread_mutex_init(&pQueueImp->mutex_, NULL);
        if (ret != 0){
                return LINK_MUTEX_ERROR;
        }
        ret = pthread_cond_init(&pQueueImp->condition_, NULL);
        if (ret != 0){
                pthread_mutex_destroy(&pQueueImp->mutex_);
                return LINK_COND_ERROR;
        }
        
        pQueueImp->policy = _policy;
        pQueueImp->pData_ = pData;
        pQueueImp->nCap_ = _nInitItemCount;
        pQueueImp->nItemLen_ = _nMaxItemLen + sizeof(int); //前缀int类型的一个长度
        pQueueImp->circleQueue.PopWithTimeout = PopQueue;
        pQueueImp->circleQueue.Push = PushQueue;
        pQueueImp->circleQueue.PopWithNoOverwrite = PopQueueWithNoOverwrite;
        pQueueImp->circleQueue.StopPush = StopPush;
        pQueueImp->circleQueue.GetStatInfo = getStatInfo;
        pQueueImp->nLenInByte_ = pQueueImp->nItemLen_ * _nInitItemCount;
        pQueueImp->nIsAvailableAfterTimeout = nIsAvailableAfterTimeout;
        pQueueImp->circleQueue.GetType = getQueueType;
        
        if (TSQ_APPEND == _policy) {
                pQueueImp->nCap_ = pQueueImp->nLenInByte_;
        }
        
        *_pQueue = (LinkCircleQueue*)pQueueImp;
        return LINK_SUCCESS;
}

int LinkGetQueueBuffer(LinkCircleQueue *pQueue, char ** pBuf, int *nBufLen) {
        CircleQueueImp *pQueueImp = (CircleQueueImp *)pQueue;
        if (pQueueImp->policy != TSQ_APPEND) {
                return LINK_Q_WRONGSTATE;
        }

        pthread_mutex_lock(&pQueueImp->mutex_);
        *pBuf = pQueueImp->pData_;
        *nBufLen = pQueueImp->nLen_;
        pthread_mutex_unlock(&pQueueImp->mutex_);

        return *nBufLen;
}

int LinkDestroyQueue(LinkCircleQueue **_pQueue)
{
        if (NULL == _pQueue || NULL == *_pQueue) {
                return LINK_ERROR;
        }
        CircleQueueImp *pQueueImp = (CircleQueueImp *)(*_pQueue);

        StopPush(*_pQueue);
        
        pthread_mutex_destroy(&pQueueImp->mutex_);
        pthread_cond_destroy(&pQueueImp->condition_);

        if (pQueueImp->pData_)
                free(pQueueImp->pData_);
        free(pQueueImp);
        *_pQueue = NULL;
        return LINK_SUCCESS;
}
