// Last Update:2018-05-25 15:46:37
/**
 * @file queue.c
 * @brief 
 * @author 
 * @version 0.1.00
 * @date 2018-05-25
 */

#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>
#include <errno.h>
#include <stdio.h>
#include "queue.h"

MessageQueue* CreateMessageQueue(size_t _nLength)
{
	MessageQueue* pMem = (MessageQueue*)malloc(sizeof(MessageQueue));
	if (pMem == NULL) {
		return NULL;
	}

	pMem->pAlloc = (Message**)malloc(sizeof(Message*) * _nLength);
	if (pMem->pAlloc == NULL) {
		goto create_error;
	}

	pMem->nCapacity = _nLength;
	pMem->nSize = 0;
	pMem->nNextIn = 0;
	pMem->nNextOut = 0;
        pMem->bIsValid = true;
	pthread_mutex_init(&pMem->mutex, NULL);
        pthread_mutex_init(&pMem->destroyMutex, NULL);
	pthread_cond_init(&pMem->consumerCond, NULL);

	return pMem;
create_error:
	free(pMem);
	return NULL;
}

void DestroyMessageQueue(MessageQueue** _pQueue)
{
	if (_pQueue == NULL || *_pQueue == NULL) {
		return;
        }
        pthread_mutex_lock(&(*_pQueue)->mutex);
        MessageQueue* pQueue = *_pQueue;
        pQueue->bIsValid = false;
        pthread_cond_signal(&(*_pQueue)->consumerCond);
        pthread_mutex_unlock(&(*_pQueue)->mutex);
        
        pthread_mutex_lock(&pQueue->destroyMutex);
        if (pQueue->pAlloc != NULL) {
                free(pQueue->pAlloc);
        }
        pthread_mutex_unlock(&pQueue->destroyMutex);
        pthread_mutex_destroy(&pQueue->mutex);
        pthread_mutex_destroy(&pQueue->destroyMutex);
        pthread_cond_destroy(&pQueue->consumerCond);

        free(*_pQueue);
        *_pQueue = NULL;
}

void SendMessage(MessageQueue* _pQueue, Message* _pMessage)
{
        if (_pQueue == NULL) {
                return;
        }

        pthread_mutex_lock(&_pQueue->mutex);

        // send event to queue
        _pQueue->pAlloc[_pQueue->nNextIn] = _pMessage;

        // move in ptr
        _pQueue->nNextIn++;
        _pQueue->nNextIn = _pQueue->nNextIn % _pQueue->nCapacity;
        _pQueue->nSize++;

        // acceed cap of the queue, move out ptr
        if (_pQueue->nSize > _pQueue->nCapacity) {
                _pQueue->nNextOut++;
                _pQueue->nNextOut = _pQueue->nNextOut % _pQueue->nCapacity;
                _pQueue->nSize = _pQueue->nCapacity;
        }

        // notify one receiver
        pthread_cond_signal(&_pQueue->consumerCond);

        pthread_mutex_unlock(&_pQueue->mutex);
}

Message* ReceiveMessage(MessageQueue* _pQueue)
{
        if (_pQueue == NULL) {
                return NULL;
        }

        Message* pMessage = NULL;
        pthread_mutex_lock(&_pQueue->destroyMutex);
        pthread_mutex_lock(&_pQueue->mutex);
        // block here if the queue is empty, wait for event
        while (_pQueue->nSize == 0) {
                if (!_pQueue->bIsValid) {
                        pthread_mutex_unlock(&_pQueue->mutex);
                        pthread_mutex_unlock(&_pQueue->destroyMutex);
                        return NULL;
                }
                pthread_cond_wait(&_pQueue->consumerCond, &_pQueue->mutex);
        }

        // pop one event
        pMessage = _pQueue->pAlloc[_pQueue->nNextOut];

        // move out ptr
        _pQueue->nNextOut++;
        _pQueue->nNextOut = _pQueue->nNextOut % _pQueue->nCapacity;
        _pQueue->nSize--;
        pthread_mutex_unlock(&_pQueue->mutex);
        pthread_mutex_unlock(&_pQueue->destroyMutex);
        return pMessage;
}

Message* ReceiveMessageTimeout(MessageQueue* _pQueue, int _nMilliSec)
{
        if (_pQueue == NULL) {
                return NULL;
	}

        Message* pMessage = NULL;
        struct timeval now;
        struct timespec after;
        pthread_mutex_lock(&_pQueue->destroyMutex);
        pthread_mutex_lock(&_pQueue->mutex);

        // if queue is empty, wait for events until timeout
        if (_pQueue->nSize == 0) {
                gettimeofday(&now, NULL);
                int sec = (now.tv_usec + _nMilliSec * 1000) / 1000000 + now.tv_sec;
                int nsec = ((now.tv_usec + _nMilliSec * 1000) % 1000000) * 1000;
                after.tv_sec = sec;
                after.tv_nsec = nsec;
                //DBG_LOG("ReceiveMessageTimeout %lld %d \n", after.tv_nsec, after.tv_sec);
                int nReason = pthread_cond_timedwait(&_pQueue->consumerCond, &_pQueue->mutex, &after);
                if (!_pQueue->bIsValid) {
                        pthread_mutex_unlock(&_pQueue->mutex);
                        pthread_mutex_unlock(&_pQueue->destroyMutex);
                        return NULL;
                }
                if (nReason == ETIMEDOUT) {
                        //DBG_LOG("ReceiveMessageTimeout %lld %d \n", nsec, sec);
                        pthread_mutex_unlock(&_pQueue->mutex);
                        pthread_mutex_unlock(&_pQueue->destroyMutex);
                        return NULL;
                }
        }

        if (_pQueue->nSize != 0) {
                // pop one event
                pMessage = _pQueue->pAlloc[_pQueue->nNextOut];

                // move out ptr
                _pQueue->nNextOut++;
                _pQueue->nNextOut = _pQueue->nNextOut % _pQueue->nCapacity;
                _pQueue->nSize--;
        }

        pthread_mutex_unlock(&_pQueue->mutex);
        pthread_mutex_unlock(&_pQueue->destroyMutex);
        return pMessage;
}


#if MESSAGE_QUEUE_UNIT_TEST
// --------------------------------------------------------------------------------------------------

MessageQueue* q;

void *thread_1(void *arg)
{
        while (1) {
                printf("send_1\n");
                SendMessage(q, NULL);
                usleep(500);
        }
}

void *thread_2(void *arg)
{
        while (1) {
                printf("send_2\n");
                SendMessage(q, NULL);
                usleep(100);
        }
}

void *thread_3(void *arg)
{
        while (1) {
                printf("receive_3\n");
                ReceiveMessage(q);
        }
}

int main(int argc, char** argv)
{
        q = CreateMessageQueue(10);
        if (q == NULL) {
                exit(1);
        }

        pthread_t tid_1, tid_2, tid_3;
        tid_1 = pthread_create(&tid_1, NULL, thread_1, NULL);
        tid_2 = pthread_create(&tid_2, NULL, thread_2, NULL);
        tid_3 = pthread_create(&tid_3, NULL, thread_3, NULL);

        sleep(10000);

        return 0;
}
#endif

