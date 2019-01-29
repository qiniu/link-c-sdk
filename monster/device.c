//
// Created by chenh on 2019/1/24.
//
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "connect.h"
#include "device.h"



extern int optind;
extern char *optarg;

struct DeviceObj gDevObj = {};
struct DevObj *GetDeviceObj();


struct UsageTab {
    char opt;
    char *pDescr;
};

static void DevShowUsage()
{
    struct UsageTab usageTab[] = {
        {7, ""},
        {'f', "printf this usage"},
        {'p', "device's dak"},
        {'u', "device's dsk"},
        {'h', "mqtt host to connect to"},
        {'P', "mqtt port to connect on"},
        {'b', "keep alive second"},
        {'r', "mqtt role, p:pub  other sub"}
    };

    int row = (int)usageTab[0].opt;
    int i;
    for (i = 1; i < row; i++) {
        DevPrint(DEV_LOG_ERR, "-%c: %s\n", usageTab[i].opt, usageTab[i].pDescr);
    }
}

static void ParseArgs(int argc, char *argv[])
{
    int rc = DEV_CODE_SUCCESS;
    char ch;

    if (argc < 2) {
        DevPrint(3, "Device app options usage\n");
        DevShowUsage();
        return;
    }

    while ((ch = getopt(argc, argv, "f:p:u:h:P:b:r:")) != -1) {
        switch (ch) {
            case 'f':
                gDevObj.stDevcfg.conf = (char *)DevMalloc(strlen(optarg) +1);
                memcpy(gDevObj.stDevcfg.conf, optarg, strlen(optarg));
                break;
            case 'p':
                gDevObj.sDak = (char *)DevMalloc(strlen(optarg) + 1);
                memset(gDevObj.sDak, 0, sizeof(optarg));
                memcpy(gDevObj.sDak, optarg, strlen(optarg));
                break;
            case 'u':
                gDevObj.sDsk = (char *)DevMalloc(strlen(optarg) + 1);
                memset(gDevObj.sDsk, 0, sizeof(optarg));
                memcpy(gDevObj.sDsk, optarg, strlen(optarg));
                break;
            case 'h':
                gDevObj.stDevcfg.stMqttcfg.sServer = (char *)DevMalloc(strlen(optarg) +1);
                memcpy(gDevObj.stDevcfg.stMqttcfg.sServer, optarg, strlen(optarg));
                break;
            case 'P':
                gDevObj.stDevcfg.stMqttcfg.nPort = atoi(optarg);
                break;
            case 'b':
                gDevObj.stDevcfg.stMqttcfg.nKeepAlive = atoi(optarg);
                break;
            case 'r':
                DevPrint(DEV_LOG_ERR, "-r : %s\n", optarg);
                gDevObj.stDevcfg.stMqttcfg.enRole =
                    optarg[0] == 'p' ? MQTT_ROLE_PUB : MQTT_ROLE_SUB;
                break;
            default :
                return;
        }
    }
}

struct DeviceObj *NewDeviceObj()
{
    return &gDevObj;
}

struct deviceObj *GetDevObj()
{
    return &gDevObj;
}

void DelDeviceObj(struct DeviceObj *devObj)
{
    if (devObj->sDak)
        DevFree(devObj->sDak);
    if (devObj->sDsk)
        DevFree(devObj->sDsk);
    if (devObj->stDevcfg.conf)
        DevFree(devObj->stDevcfg.conf);
    if (devObj->stDevcfg.stMqttcfg.sServer)
        DevFree(devObj->stDevcfg.stMqttcfg.sServer);
    if (devObj->stDevcfg.stMqttcfg.sId)
        DevFree(devObj->stDevcfg.stMqttcfg.sId);
}

int main(int argc, char *argv[])
{
    ParseArgs(argc, argv);

    struct DeviceObj *devObj = NewDeviceObj();
    devObj->stConnObj = NewConnectObj(devObj->sDak, devObj->sDsk, &devObj->stDevcfg.stMqttcfg);
    devObj->stConnObj->stOpt->SendMessage(devObj->stConnObj);
    while(1) {
        usleep(2000);
    }
    DelConnectObj(devObj->stConnObj);
    DelDeviceObj(devObj);

    return DEV_CODE_SUCCESS;
}