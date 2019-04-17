// Last Update:2018-11-14 20:57:36
/**
 * @file dbg.h
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-10-26
 */

#ifndef DBG_H
#define DBG_H
#include <stdio.h>

#define BASIC() printf("[ %s %s +%d ] ", __FILE__, __FUNCTION__, __LINE__ )
#define LOG(args...) BASIC();printf(args)
#define LOGI(args...) BASIC();printf(args)
#define LOGE(args...) BASIC();printf(args)
#define VAL( v ) LOG(#v" = %d\n", v )
#define STR( s ) LOG(#s" = %s\n", s )

extern int DbgGetMemUsed( char *memUsed );

#endif  /*DBG_H*/
