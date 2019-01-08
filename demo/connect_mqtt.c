#include "mqtt.h"
#include "mqtt_internal.h"
#include "connect_mqtt.h"

#define CONN_NUM (2)
struct ConnectStatus Status[CONN_NUM];


void OnMessage(IN const void* _pInstance, IN int _nAccountId, IN const char* _pTopic, IN const char* _pMessage, IN size_t nLength)
{
    fprintf(stderr, "%p topic %s message %s \n", _pInstance, _pTopic, _pMessage);
}

void OnEvent(IN const void* _pInstance, IN int _nAccountId, IN int _nId,  IN const char* _pReason)
{
    fprintf(stderr, "%p id %d, reason  %s \n",_pInstance, _nId, _pReason);
    struct ConnectStatus* pStatus;
    int i;
    for (i = 0; i < CONN_NUM; i++) {
        if (Status[i].pInstance == _pInstance) {
            pStatus = &Status[i];
            pStatus->nStatus = _nId;
        }
    }
}

struct ConnectStatus *GetLogInstance(IN int _nIndex)
{
    return &Status[_nIndex];
}

void GetDefaultMqttOption(IN struct MqttOptions *options)
{
    options->bCleanSession = false;
    options->userInfo.nAuthenicatinMode = MQTT_AUTHENTICATION_USER;
    options->userInfo.pHostname = "39.107.247.14";
    options->userInfo.nPort = 1883;
    options->userInfo.pCafile = NULL;
    options->userInfo.pCertfile = NULL;
    options->userInfo.pKeyfile = NULL;
    options->nKeepalive = 10;
    options->nQos = 0;
    options->bRetain = false;
    options->callbacks.OnMessage = &OnMessage;
    options->callbacks.OnEvent = &OnEvent;
    options->userInfo.pUsername = "1002";
    options->userInfo.pPassword = "gAs2Bpg2";
    options->pId = "baseconnect";
}

void ConnectMqtt()
{
    LinkMqttLibInit();
    struct MqttOptions options;
    GetDefaultMqttOption(&options);
    Status[0].pInstance = LinkMqttCreateInstance(&options);
    while (Status[0].nStatus != MQTT_SUCCESS) {
        sleep(1);
    }
    LinkMqttSubscribe(Status[0].pInstance, "oooooo");
}

void DisconnectMqtt()
{
    LinkMqttDestroy(Status[0].pInstance);
    LinkMqttLibCleanup();
}
