#ifndef __LINK_CIRCLE_QUEUE_H__
#define __LINK_CIRCLE_QUEUE_H__

#include <pthread.h>
#ifndef __APPLE__
#include <stdint.h>
#endif

enum CircleQueuePolicy{
        TSQ_FIX_LENGTH,
        TSQ_VAR_LENGTH,
        TSQ_APPEND
};

typedef struct _LinkCircleQueue LinkCircleQueue;


typedef int(*LinkCircleQueuePush)(LinkCircleQueue *pQueue, char * pData, int nDataLen);
typedef int(*LinkCircleQueuePopWithTimeoutNoOverwrite)(LinkCircleQueue *pQueue, char * pBuf, int nBufLen, int64_t nUsec);
typedef int(*LinkCircleQueuePopWithNoOverwrite)(LinkCircleQueue *pQueue, char * pBuf, int nBufLen);
typedef void(*LinkCircleQueueStopPush)(LinkCircleQueue *pQueue);

typedef struct _UploaderStatInfo {
        int nPushDataBytes_;
        int nPopDataBytes_;
        int nLen_;
        int nOverwriteCnt;
        int nIsReadOnly;
	int nDropped;
}LinkUploaderStatInfo;

typedef struct _LinkCircleQueue{
        LinkCircleQueuePush Push;
        LinkCircleQueuePopWithTimeoutNoOverwrite PopWithTimeout;
        LinkCircleQueuePopWithNoOverwrite PopWithNoOverwrite;
        LinkCircleQueueStopPush StopPush;
        void (*GetStatInfo)(LinkCircleQueue *pQueue, LinkUploaderStatInfo *pStatInfo);
        enum CircleQueuePolicy (*GetType)(LinkCircleQueue *pQueue);
}LinkCircleQueue;

int LinkNewCircleQueue(LinkCircleQueue **pQueue, int nIsAvailableAfterTimeout,  enum CircleQueuePolicy policy, int nMaxItemLen, int nInitItemCount);
int LinkGetQueueBuffer(LinkCircleQueue *pQueue, char ** pBuf, int *nBufLen); //just in append mode
void LinkDestroyQueue(LinkCircleQueue **_pQueue);

#endif
