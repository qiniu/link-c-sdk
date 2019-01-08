#include "mqtt_internal.h"
#include "mqtt.h"
#include "log_mqtt.h"
#include "connect_mqtt.h"

#define MQLOG_SUCCESS 0
#define MQLOG_FAILURE -1

static int gFlag = 0;

static void GetTimestamp(IN char *_pTimestamp)
{
    struct tm *tblock = NULL;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    tblock = localtime(&tv.tv_sec);
    sprintf(_pTimestamp, "%d/%d/%d %d:%d:%d", 1900 + tblock->tm_year, 1 + tblock->tm_mon,
        tblock->tm_mday, tblock->tm_hour, tblock->tm_min, tblock->tm_sec);
}

static void GetModuleStr(IN int _nModule, IN char *_pModule)
{
    if (_nModule & LOG_MODULE_CPU) {
        memcpy(_pModule, "CPU", sizeof("CPU"));
    } else if (_nModule & LOG_MODULE_MEM) {
        memcpy(_pModule, "MEM", sizeof("MEM"));
    } else if (_nModule & LOG_MODULE_AP) {
        memcpy(_pModule, "APP", sizeof("APP"));
    }
}

static int InitLog(IN struct LogReportObj *obj)
{
    LinkInitLog("publog", obj->pDevName, obj->pBase->pInstance);
    return MQLOG_SUCCESS;
}

static void DinitLog(IN struct LogReportObj *_pBase)
{
        LinkDinitLog();
}

static void DisplayLogReportObj(IN struct LogReportObj *_pObj)
{
    printf("===DisplayLogReportObj:%p===\n", _pObj);
    printf("nLevel:%d\n", _pObj->nLevel);
    printf("pDevName:%s\n", _pObj->pDevName);
    printf("nModule:0x%x\n", _pObj->nModule);
    printf("pBase:%p\n", _pObj->pBase);
    printf("pBase.nStatus:%d\n", _pObj->pBase->nStatus);
    printf("pBase.nCount:%d\n", _pObj->pBase->nCount);
    printf("pBase.pInstance:%p\n\n\n", _pObj->pBase->pInstance);
}

static void SendLog(IN struct LogReportObj *_pObj)
{
    //timestamp appid deviceName errorMsg
    char *str = malloc(256);
    char timestamp[128] = {};
    char module[32] = {};
    memset(str, 0, 256);
    GetTimestamp(&timestamp[0]);
    GetModuleStr(_pObj->nModule, &module[0]);
    int len = 0;
    len = sprintf(str, "%s %s %s %s %s", timestamp, "logmqtt", _pObj->pDevName, &module[0], _pObj->pData);
    if (LinkSendLog(_pObj->nLevel, str, len +1) != 1) {
        sleep(2);
    }
    free(str);
}

static struct LogReportOperations gLogReportOps = {
    .init = InitLog,
    .release = DinitLog,
    .display = DisplayLogReportObj,
    .deal = SendLog,
};

struct LogReportObj *NewLogReportObj(IN const int _nLevel, IN char *_pDevName, IN int _nModule, IN LogBase *_pLogBase)
{
    struct LogReportObj *obj = (struct LogReportObj *)malloc(sizeof(struct LogReportObj));

    obj->pDevName =  (char *)malloc(strlen(_pDevName)+1);
    memcpy(obj->pDevName, _pDevName, strlen(_pDevName)+1);
    obj->nLevel = _nLevel;
    obj->nModule = _nModule;
    obj->pBase = _pLogBase;
    obj->ops = &gLogReportOps;
    if (!gFlag) {
        obj->ops->init(obj);
    }
    gFlag++;
    return obj;
}

void DelLogReportObj(struct LogReportObj *_pObj)
{
    free(_pObj->pDevName);
    gFlag--;
    if (!gFlag) {
        _pObj->ops->release(_pObj);
    }
}
