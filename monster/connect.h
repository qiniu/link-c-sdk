//
// Created by chenh on 2019/1/26.
//

#ifndef LINK_C_SDK_CONNECT_H
#define LINK_C_SDK_CONNECT_H

struct ConnectStatus {
    void *pInstance;
    int nStatus;
    int nCount;
};

enum ConnStatus {
    CONNECT_INVALID = 0,
    CONNECT_OPEN,
    CONNECT_START,
    CONNECT_STOP,
    CONNECT_CLOSE
};

struct ConnectOperations {
    int (*Open)(struct ConnectObj *obj);
    void (*Close)(struct ConnectObj *obj);
    void (*RecvMessage)(struct ConnectObj *obj);
    void (*RecvEvent)(struct ConnectObj *obj);
    void (*SendMessage)(struct ConnectObj *obj);
};

struct ConnectObj {
    struct MqttInstance *stInstance;
    struct ConnectOperations *stOpt;
//    enum ConnStatus enStatus;
    int nStatus;
    int nSession;
    pthread_t thAssign;
//    struct RouterObj *stRouter;

    char *sDak;
    char *sDsk;
    char *sServer;
    int nPort;
    int nKeepAlive;
    char *sId;
};

#define MQTT_IOCTRL_CMD_SETLEVEL 7000
#define MQTT_IOCTRL_CMD_GETLEVEL 7001

void DelConnectObj(struct ConnectObj *obj);
struct ConnectObj *NewConnectObj(char *dak, char *dsk, void *cfg);
#endif //LINK_C_SDK_CONNECT_H
