#ifndef LINK_C_SDK_CONNECT_MQTT_H
#define LINK_C_SDK_CONNECT_MQTT_H


struct ConnectStatus {
    void *pInstance;
    int nStatus;
    int nCount;
};

void ConnectMqtt();
void DisconnectMqtt();
struct ConnectStatus *GetLogInstance(IN int _nIndex);

#endif //LINK_C_SDK_CONNECT_MQTT_H
