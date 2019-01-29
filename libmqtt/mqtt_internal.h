#ifndef __MQTTINTERNAL__
#define __MQTTINTERNAL__


#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define STATUS_IDLE 0
#define STATUS_CONNECTING 1
#define STATUS_CONNACK_RECVD 2
#define STATUS_WAITING 3
#define STATUS_CONNECT_ERROR 4
#define MAX_MQTT_TOPIC_SIZE 128

#ifdef WITH_MOSQUITTO
#include <mosquitto.h>
#define MqttCtx mosquitto
#else
#include "wolfmqtt/wolfmqtt.h"
#define MqttCtx MQTTCtx
#endif

#define IO_CTR_MESSAGE "linking/v1/"
#define IO_CTR_MESSAGE_LENGTH 11
typedef struct Node
{
        char topic[MAX_MQTT_TOPIC_SIZE];
        struct Node *pNext;
}Node;

struct MqttInstance
{
        struct MqttCtx *mosq;
        struct MqttOptions options;
        int status;
        int lastStatus;
        bool connected;
        bool isDestroying;
        Node pSubsribeList;
        pthread_mutex_t listMutex;
}MqttInstance;

int LinkMqttInit(struct MqttInstance* pInstance);
void LinkMqttDinit(struct MqttInstance* pInstance);
MQTT_ERR_STATUS LinkMqttConnect(struct MqttInstance* pInstance);
void LinkMqttDisconnect(struct MqttInstance* pInstance);
MQTT_ERR_STATUS LinkMqttLoop(struct MqttInstance* pInstance);
int LinkMqttPublish(IN const void* _pInstance, IN const char* _pTopic, IN int _nPayloadlen, IN const void* _pPayload);
int LinkMqttSubscribe(IN const void* _pInstance, IN const char* _pTopic);
int LinkMqttUnsubscribe(IN const void* _pInstance, IN const char* _pTopic);
int LinkMqttLibInit();
int LinkMqttLibCleanup();
void LinkMqttDestroyInstance(IN const void* _pInstance);


bool InsertNode(Node* pHead, const char* val);
bool DeleteNode(Node* PHead, const char * pval);

void OnEventCallback(struct MqttInstance* _pInstance, int rc, const char* _pStr);
#endif
