// Last Update:2019-03-05 16:39:11
/**
 * @file unittest.h
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-10-26
 */

#ifndef UNITTEST_H
#define UNITTEST_H

#include <string.h>
#include <stdlib.h>

#define mu_assert(test) do { \
    if ( !(test) ) { \
        memset( gbuffer, 0, sizeof(gbuffer) );\
        sprintf( gbuffer, "[ FAIL ] run test case : %s, line : %d, "#test, __FUNCTION__, __LINE__ ); \
        return gbuffer;\
    } \
} while (0)

#define RUN_TEST_CASE(test) do { \
    char *message = test();  \
    if (message) {  return message; } else { printf( "[ PASS ] run test case : "#test"\n"); }\
} while (0)

#define ASSERT_EQUAL(a,b) do { \
    if ( !(a == b) ) { \
        printf( "[ FAIL ] run test case : %s, line : %d, "#a" = %d, "#b" = %d", __FUNCTION__,  __LINE__, a, b ); \
        exit(1); \
    } \
} while (0)

#define ASSERT_NOT_EQUAL(a,b) do { \
    if ( !(a != b) ) { \
        printf( "[ FAIL ] run test case : %s, line : %d, "#a" = %ld, "#b" = %ld", __FUNCTION__, __LINE__, (long)a, (long)b ); \
        exit( 1 ); \
    } \
} while (0)


#define ASSERT_STR_EQUAL( a, b ) mu_assert( strcmp(a, b) == 0 )

#define ASSERT_MEM_EQUAL( a, b, len ) do { \
    if ( !(memcmp_internal( a, b , len ) == 0) ) { \
        DumpBuffer( #a, a, len, __LINE__ ); \
        DumpBuffer( #b, b, len, __LINE__ ); \
        printf( "test fail line : %d "#a" = "#b"\n",  __LINE__ ); \
        exit(1);\
    } else { \
        printf("[ %d ] check mem "#a" and "#b" success\n", __LINE__ ); \
    } \
} while (0)



#endif  /*UNITTEST_H*/

