//
// Created by chenh on 2018/12/24.
//
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include "collector.h"
#include "log_mqtt.h"
#include "dbg.h"


#define COLLECTOR_SUCCESS 0
#define COLLECTOR_FAILURE -1

static struct Collector gCollector;

static void *coll_mem_thread(void *args)
{
    struct Collector *coll = (struct Collector *)args;
    struct LogReportObj *report = (struct LogReportObj *)coll->pInstance;
    int used = 0;

//    used = DbgGetMemUsed();
    report->pData = "4096";
    report->nLen = sizeof("4096");
//    printf("%s[%d] mqtt pInstance :%p\n", __func__, __LINE__, report->pBase->pInstance);
    if (coll->nStatus == COLLECTOR_START) {
        report->ops->deal(report);
    }
}

static void *coll_cpu_thread(void *args)
{
    struct Collector *coll = (struct Collector *)args;
    struct LogReportObj *report = (struct LogReportObj *)coll->pInstance;
    float used = 0;

    used = DbgGetCpuUsage();
    report->pData = (char *)&used;
    report->nLen = sizeof(used);
    if (coll->nStatus == COLLECTOR_START) {
        report->ops->deal(report);
    }
}

static void *coll_fp_thread(void *args)
{

}

static void SigAlarmHandle(int signo)
{
    if (gCollector.nModule & LOG_MODULE_MEM) {
        pthread_t  tid_mem;

//        struct LogReportObj *report = (struct LogReportObj *)gCollector.pInstance;
//        report->pData = "1024";
//        report->nLen = sizeof("1024");
//        report->ops->deal(report);

        pthread_create(&tid_mem, NULL, coll_mem_thread, &gCollector);
    } else if (gCollector.nModule & LOG_MODULE_CPU) {
        pthread_t tid_cpu;
        pthread_create(&tid_cpu, NULL, coll_cpu_thread, &gCollector);
    } else if (gCollector.nModule & LOG_MODULE_FP) {
        pthread_t tid_fp;
        pthread_create(&tid_fp, NULL, coll_fp_thread, &gCollector);
    }
}

static int InitCollector(IN struct Collector *coll)
{
    signal(SIGALRM, SigAlarmHandle);
    return COLLECTOR_SUCCESS;
}

static void DeInitCollector(IN struct Collector *coll)
{

}

static int StartCollector(IN struct  Collector *coll)
{
    struct itimerval timer;

    timer.it_interval.tv_sec = coll->nInterval / 1000;
    timer.it_interval.tv_usec = coll->nInterval % 1000 * 1000;
    timer.it_value.tv_sec = coll->nInterval / 1000;
    timer.it_value.tv_usec = coll->nInterval % 1000 * 1000;
    coll->nStatus = COLLECTOR_START;
    setitimer(ITIMER_REAL, &timer, NULL);
    return COLLECTOR_SUCCESS;
}

static int StopCollector(IN struct Collector *coll)
{
    struct itimerval timer = {};

    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &timer, NULL);
    coll->nStatus = COLLECTOR_STOP;
    return COLLECTOR_SUCCESS;
}

static struct CollectorOperations gCollOps = {
    .init = InitCollector,
    .release = DeInitCollector,
    .start = StartCollector,
    .stop = StopCollector,
};

struct Collector *NewCollector(IN int _nModule, IN int _nInterval, IN void *_pInstance)
{
    gCollector.nStatus = COLLECTOR_INVALID;
    gCollector.nModule = _nModule;
    gCollector.pInstance = _pInstance;
    gCollector.nInterval = _nInterval;
    gCollector.ops = &gCollOps;
    gCollector.ops->init(&gCollector);
    return &gCollector;
}

void DelCollector(IN struct Collector *coll)
{

}
