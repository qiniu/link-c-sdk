#ifndef __LINK_CIRCLE_QUEUE_H__
#define __LINK_CIRCLE_QUEUE_H__

#include <pthread.h>
#ifndef __APPLE__
#include <stdint.h>
#endif

typedef enum _CircleQueuePolicy{
        TSQ_FIX_LENGTH,
        TSQ_FIX_LENGTH_CAN_OVERWRITE,
        TSQ_VAR_LENGTH,
        TSQ_APPEND,
        TSQ_APPEND_FIX,
} CircleQueuePolicy;

typedef struct {
        char *pMem;
        void (*freeMem)(void *p);
        void *pUser;
} LinkQueueMem;

typedef struct _LinkCircleQueue LinkCircleQueue;


typedef int(*LinkCircleQueuePush)(LinkCircleQueue *pQueue, char * pData, int nDataLen);
typedef int(*LinkCircleQueuePopWithTimeoutNoOverwrite)(LinkCircleQueue *pQueue, char * pBuf, int nBufLen, int64_t nMicroSec);
typedef int(*LinkCircleQueuePopWithNoOverwrite)(LinkCircleQueue *pQueue, char * pBuf, int nBufLen);
typedef void(*LinkCircleQueueStopPush)(LinkCircleQueue *pQueue);

typedef struct _UploaderStatInfo {
        int nPushDataBytes_;
        int nPopDataBytes_;
        int nLen_;
        int nOverwriteCnt;
        int nIsReadOnly;
	int nDropped;
        int nCap;
}LinkUploaderStatInfo;

typedef struct _LinkCircleQueue{
        LinkCircleQueuePush Push;
        LinkCircleQueuePopWithTimeoutNoOverwrite PopWithTimeout;
        LinkCircleQueuePopWithNoOverwrite PopWithNoOverwrite;
        LinkCircleQueueStopPush StopPush;
        void (*GetStatInfo)(LinkCircleQueue *pQueue, LinkUploaderStatInfo *pStatInfo);
        CircleQueuePolicy (*GetType)(LinkCircleQueue *pQueue);
}LinkCircleQueue;

int LinkNewCircleQueue(LinkCircleQueue **pQueue, int nIsAvailableAfterTimeout, CircleQueuePolicy policy, int nMaxItemLen, int nInitItemCount, LinkQueueMem *pMem);
int LinkGetQueueBuffer(LinkCircleQueue *pQueue, char ** pBuf, int *nBufLen); //just in append mode
void LinkQueueIncRefCount(LinkCircleQueue *_pQueue);
void LinkQueueDecRefCount(LinkCircleQueue *_pQueue);
int LinkDestroyQueue(LinkCircleQueue **_pQueue);

#endif
