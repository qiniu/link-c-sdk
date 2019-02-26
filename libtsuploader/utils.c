#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "base.h"
#include <time.h>
#include "httptools.h"

static struct timespec tmResolution;


int64_t LinkGetCurrentNanosecond()
{
        struct timespec tp;
        clock_gettime(CLOCK_REALTIME, &tp);
        return (int64_t)(tp.tv_sec * 1000000000ll + tp.tv_nsec);
}

int64_t LinkGetCurrentMillisecond()
{
        struct timespec tp;
        clock_gettime(CLOCK_REALTIME, &tp);
        return ((int64_t)(tp.tv_sec * 1000000000ll + tp.tv_nsec))/1000000;
}

int LinkInitTime() {
        clock_getres(CLOCK_MONOTONIC, &tmResolution);
        
        return LINK_SUCCESS;
}
