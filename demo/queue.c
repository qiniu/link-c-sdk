// Last Update:2018-09-18 19:12:50
/**
 * @file queue.c
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-09-12
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "queue.h"
#include "mymalloc.h"

static int Enqueue( Queue *q, void *data, int size );
static int Dequeue( Queue *q, void *data, int *outSize );
static int QueueSize( Queue *q );

Queue* NewQueue()
{
    Queue *q = ( Queue *) malloc (sizeof(Queue) );
    
    if ( !q ) {
        return NULL;
    }

    q->size = 0;
    q->pLast = q->pIn = q->pOut = NULL;
    q->enqueue = Enqueue;
    q->dequeue = Dequeue;
    q->getSize = QueueSize;
    pthread_mutex_init( &q->mutex, NULL );
    pthread_cond_init( &q->cond, NULL );

    return q;
}

static int Enqueue( Queue *q, void *data, int size )
{
    Element *elem = NULL;

    if ( !q || !data ) {
        goto err;
    }

    pthread_mutex_lock( &q->mutex );
    elem = ( Element *) malloc ( sizeof(Element) );
    if ( !elem ) {
        goto err;
    }
    memset( elem, 0, sizeof(Element) );
    elem->data = malloc( size );
    if ( !elem->data ) {
        goto err1;
    }
    elem->size = size;
    memset( elem->data, 0, size );
    memcpy( elem->data, data, size );
    if ( q->size == 0 ) {
        q->pIn = q->pOut = elem;
    } else {
        q->pIn->next = elem;
        q->pIn = elem;
    }
    q->size ++;
    pthread_cond_signal( &q->cond );
    pthread_mutex_unlock( &q->mutex );

    return 0;

err:
    pthread_mutex_unlock( &q->mutex );
    return -1;

err1:
    pthread_mutex_unlock( &q->mutex );
    free( elem );
    return -1;
}

static int Dequeue( Queue *q, void *data, int *outSize )
{
    Element *elem = NULL;

    if ( !q || !data  ) {
        goto err;
    }

    pthread_mutex_lock( &q->mutex );

    while ( q->size == 0 ) {
        pthread_cond_wait( &q->cond, &q->mutex );
    }

    if ( !q->pOut ) {
        goto err;
    }
    elem = q->pOut;
    memcpy( data, elem->data, elem->size );
    if ( outSize )
        *outSize = elem->size;
    if ( q->size > 1 ) {
        q->pOut = elem->next;
        q->pLast = elem;
    } else {
        q->pIn = q->pOut = NULL;
        free( elem->data );
        free( elem);
    }
    q->size--;
    if ( q->pLast ) {
//        printf("free the mem %x\n", q->pLast );
        free( q->pLast->data );
        free( q->pLast );
        q->pLast = NULL;
    }

    pthread_mutex_unlock( &q->mutex );

    return 0;

err:
    return -1;
}

static int QueueSize( Queue *q )
{
    return q->size;
}

