#ifndef LINK_C_SDK_MAIN_H
#define LINK_C_SDK_MAIN_H

#include "mqtt_internal.h"

enum MqttRole {
    MQTT_ROLE_SUB = 0,
    MQTT_ROLE_PUB
};

struct MqttConfig {
    char *sServer;
    int nPort;
    int nKeepAlive;
    char *sId;
    enum MqttRole enRole;
};

struct DeviceConfig {
    char *conf;   //配置文件
    struct MqttConfig stMqttcfg;
};

struct DeviceObj {
	char *sDak;
	char *sDsk;
	struct DeviceConfig stDevcfg;
};

enum APP_DEVICE_STATUS {
    APP_CODE_SUCCESS,
    APP_CODE_ERROR
};

#endif //LINK_C_SDK_MAIN_H
