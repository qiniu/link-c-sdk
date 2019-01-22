// Last Update:2018-12-25 09:46:33
/**
 * @file func_trace.c
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-12-20
 */

#ifdef DEBUG_TRACE_FUNCTION

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <execinfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

typedef struct {
    void *addr;
    int count;
} func_trace_t;


static func_trace_t funcs[1024];

static int current = 0;

#define CHECK_EXIST() \
    for ( i=0; i<current; i++ ) { \
        if( funcs[i].addr == this_func ) { \
            found = 1; \
            break; \
        } \
    }

static void dump_funcs( FILE *fp )
{
    int i = 0;
    char buffer[128] = { 0 };

    if ( fp ) {
        sprintf( buffer, "current = %d\n", current );
        fwrite( buffer, 1, strlen(buffer), fp );
    }
    printf("\ncurrent = %d\n", current );
    for ( i=0; i<current; i++ ) {
        if ( funcs[i].count > 0 ) {
            printf("%p\n", funcs[i].addr );
            if ( fp ) {
                sprintf(buffer, "%p\n", funcs[i].addr );
                fwrite( buffer, 1, strlen(buffer), fp );
            }
        }
    }
}

static void __attribute__((__no_instrument_function__))
    __cyg_profile_func_enter(void *this_func, void *call_site)
{
    int i = 0, found = 0;

    CHECK_EXIST();
    if ( found ) {
        funcs[i].count++;
    } else {
        funcs[current].addr = this_func;
        funcs[current].count++;
        current++;
    }
}

static void __attribute__((__no_instrument_function__))
    __cyg_profile_func_exit(void *this_func, void *call_site)
{
    int i = 0,  found = 0;

    CHECK_EXIST();

    if ( found ) {
        funcs[i].count--;
    }
}

static void SignalHandler( int sig )
{
    char buffer[32] = { 0 };
    void *array[20];
    size_t size;
    char **strings;
    size_t i;
    int fd = 2;
    FILE *fp = fopen( "/tmp/oem/app/crash.log", "w+" );

    if ( fp ) {
        char buffer[32] = { 0 };
        void *array[20];
        size_t size;
        char **strings;
        size_t i;

        size = backtrace (array, 20);
        strings = backtrace_symbols (array, size);

        sprintf( buffer, "get sig %d\n", sig );
        fwrite( buffer, strlen(buffer), 1, fp );
        printf("%s\n", buffer );
        memset( buffer, 0, sizeof(buffer) );
        sprintf( buffer, "Obtained %zd stack frames.\n", size );
        printf("%s\n", buffer );
        fwrite( buffer, strlen(buffer), 1, fp );
        for ( i=0; i<size; i++ ) {
            printf("%s\n", strings[i] );
            fwrite( strings[i], strlen(strings[i]), 1, fp );
        }
    }


    dump_funcs( fp );

    int mapfd = open ("/proc/self/maps", O_RDONLY);
    if (mapfd != -1)
    {
        fwrite( "\nMemory map:\n\n", 1, 14, fp );

        char buf[256];
        ssize_t n;

        while ((n = read (mapfd, buf, sizeof (buf)))) {
            printf("%s\n", buf );
            fwrite( buf, 1, n, fp );
        }

        close (mapfd);
    }

    fclose( fp );

    switch( sig ) {
    case SIGINT:
    case SIGQUIT:
    case SIGABRT:
    case SIGSEGV:
    case SIGKILL:
    case SIGTERM:
        exit(0);
        break;

    }
}

static void __attribute__ ((constructor)) install_handler (void)
{
    int i = 0;

    printf("install signal handler\n");
    for ( i=0; i<32; i++ ) {
        signal( i, SignalHandler );
    }
}

#endif


