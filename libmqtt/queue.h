// Last Update:2018-05-25 15:47:42
/**
 * @file queue.h
 * @brief 
 * @author 
 * @version 0.1.00
 * @date 2018-05-25
 */

#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct
{
        int       nMessageID;
        void*	pReqID;
        void*     pMessage;
} Message;

typedef struct
{
        Message**          pAlloc;
        int                nNextIn;
        int                nNextOut;
        int                nCapacity;
        int                nSize;
        bool               bIsValid;
        pthread_mutex_t    mutex;
        pthread_mutex_t    destroyMutex;
        pthread_cond_t     consumerCond;
} MessageQueue;

#define MESSAGE_QUEUE_UNIT_TEST 0

MessageQueue* CreateMessageQueue(size_t _nLength);
void DestroyMessageQueue(MessageQueue** _pQueue);
void SendMessage(MessageQueue* _pQueue, Message* _pMessage);
Message* ReceiveMessage(MessageQueue* _pQueue);
Message* ReceiveMessageTimeout(MessageQueue* _pQueue, int _nMilliSec);

#endif  /*QUEUE_H*/
