//
// Created by chenh on 2019/1/24.
//

#ifndef MONSTER_DEVICE_H
#define MONSTER_DEVICE_H

#include "connect.h"

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
    char *conf;
    struct MqttConfig stMqttcfg;
};

struct DeviceObj {
    char *sDak;
    char *sDsk;
    struct DeviceConfig stDevcfg;
    struct ConnectObj *stConnObj;
};



#endif //MONSTER_DEVICE_H
