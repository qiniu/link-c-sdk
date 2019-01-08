//
// Created by chenh on 2018/12/24.
//

#ifndef LINK_C_SDK_COLLECTOR_H
#define LINK_C_SDK_COLLECTOR_H

#ifndef IN
#define IN
#define OUT
#endif

enum COLLECTOR_STATUS {
    COLLECTOR_INVALID = 0,
    COLLECTOR_START,
    COLLECTOR_STOP
};

struct CollectorOperations {
    int (*init)(struct Collector *coll);
    void (*release)(struct Collector *coll);
    int (*start)(struct Collector *coll);
    int (*stop)(struct Collector *coll);
};

struct Collector {
    void *pInstance;
    pid_t Pid; //进程id
    int nStatus;
    int nModule;
    int nInterval; //上报间隔，单位：微妙
    struct CollectorOperations *ops;
};


struct Collector *NewCollector(IN int _nModule, IN int _nInterval, IN void *_pInstance);
void DelCollector(IN struct Collector *coll);
#endif //LINK_C_SDK_COLLECTOR_H
