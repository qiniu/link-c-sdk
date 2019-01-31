// Last Update:2018-09-26 11:27:00
/**
 * @file mymalloc.h
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-09-18
 */

#ifndef MYMALLOC_H
#define MYMALLOC_H

//#define USE_OWN_MALLOC
#ifdef USE_OWN_MALLOC
#define malloc( size ) mymalloc( size, __FUNCTION__, __LINE__ )
#define free( ptr ) myfree( ptr, __FUNCTION__, __LINE__ )
#endif

void *mymalloc( size_t size, char *function, int line );
void myfree( void *ptr, char *function, int ine );

#endif  /*MYMALLOC_H*/
