// Last Update:2018-09-26 11:29:14
/**
 * @file mymalloc.c
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-09-18
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "dbg.h"

static int up = 0, down = 0;

void *mymalloc( size_t size, char *function, int line )
{
    DBG_LOG("+++ malloc, %s() ---> %d, up = %d\n", function, line,  __sync_fetch_and_add(&up,1));

    return malloc( size );
}

void myfree( void *ptr, char *function, int line )
{
    char buffer[1024] = {0};
    DBG_LOG( "+++ free, %s() ---> %d, down = %d, ptr = %p \n", function, line, __sync_fetch_and_sub(&down,1), ptr );

    free( ptr );
}

