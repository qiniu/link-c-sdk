#ifndef LINK_C_SDK_LOG_MQTT_H
#define LINK_C_SDK_LOG_MQTT_H

#include "connect_mqtt.h"

//log report module mask
#define LOG_MODULE_ALL (0xff)
#define LOG_MODULE_MEM (1)
#define LOG_MODULE_CPU (2)
#define LOG_MODULE_FP (4)
#define LOG_MODULE_AP (8)

typedef struct ConnectStatus LogBase;

struct LogReportOperations {
    int (*init)(struct LogReportObj *obj);
    void (*release)(struct LogReportObj *obj);
    void (*deal)(struct LogReportObj *obj);
    void (*display)(struct LogReportObj *obj);
};

struct LogReportObj
{
    int  nLevel;
    char *pDevName;
    int nModule;
    LogBase *pBase;
    char *pData;
    int nLen;
    struct LogReportOperations *ops;
};

struct LogReportObj *NewLogReportObj(IN const int _nLevel, IN char *_pDevName, IN int _nModule, IN LogBase *_pLogBase);
void DelLogReportObj(struct LogReportObj *_pObj);

#endif //LINK_C_SDK_LOG_MQTT_H
