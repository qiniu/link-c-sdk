#include "mqtt.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "control.h"
#include "control_internal.h"
#include "queue.h"
#include "log.h"
#include "cJSON/cJSON.h"

struct _LinkIOCrtlInfo Session[10] = {0};
struct _LinkIOCrtlInfo LogSession = {0};

void OnIOCtrlMessage(IN const void* _pInstance, IN int _nAccountId, IN const char* _pTopic, IN const char* _pMessage, IN size_t nLength)
{
        Message *pMessage = (Message *) malloc(sizeof(Message));
        char* message = (char*) malloc(nLength);
        if ( !pMessage || !message ) {
                return;
        }
        pMessage->nMessageID = -1;
	int i;
        for (i = 0; i < 10; ++ i) {
                if (Session[i].isUsed && _pInstance == Session[i].pInstance) {
                        pMessage->nMessageID = i;
                        break;
                }
        }
        if (pMessage->nMessageID == -1) {
                free(pMessage);
                return;
        }
        memset(message, 0, nLength);
        memcpy(message, _pMessage, nLength);
        pMessage->pMessage = message;
        SendMessage(Session[pMessage->nMessageID].pQueue, pMessage);
}

int LinkInitIOCtrl(const char *_pAppId, const char *_pEncodeDeviceName, void *_pInstance)
{
        int index = MAX_SESSION_ID;
        int i;
        for (i = 0; i < MAX_SESSION_ID; ++i) {
                if (Session[i].isUsed == false) {
                        index = i;
                        break;
                 }
	}
        if (index == MAX_SESSION_ID) {
                return MQTT_ERR_INVAL;
        }
        if (_pInstance == NULL) {
                return MQTT_ERR_INVAL;
        }
        Session[index].pQueue = CreateMessageQueue(MESSAGE_QUEUE_MAX);
        if (!Session[index].pQueue) {
                LinkLogError("queue malloc fail\n");
                return MQTT_ERR_NOMEM;
        }
        memset(Session[index].pubTopic, 0, sizeof(Session[index].pubTopic));
        sprintf(Session[index].pubTopic, "/linking/v1/%s/%s/rpc/request", _pAppId, _pEncodeDeviceName);
        memset(Session[index].subTopic, 0, sizeof(Session[index].subTopic));
        sprintf(Session[index].subTopic, "/linking/v1/%s/%s/rpc/response/#", _pAppId, _pEncodeDeviceName);
        Session[index].pInstance = _pInstance;
        Session[index].isUsed = true;
        int ret = LinkMqttSubscribe(_pInstance, Session[index].subTopic);
        if (ret != MQTT_SUCCESS) {
                return ret;
        }
        return index;
}

cJSON* CreateResponse(unsigned int _nIOErrorCode, const char *_pIOCtrlData, int _nIOCtrlDataSize)
{
        cJSON *json = cJSON_CreateObject();
        if (json == NULL) {
                return NULL;
        }
        cJSON *item = cJSON_CreateNumber(_nIOErrorCode);
        if (item == NULL) {
                cJSON_Delete(json);
                return NULL;
        }
        cJSON_AddItemToObject(json, RESPONSE_ERROR_CODE, item);
        if (_nIOErrorCode == LINKING_RESPONSE_SUCCESS) {
                cJSON_AddStringToObject(json, RESPONSE_VALUE, _pIOCtrlData);
        } else {
                cJSON_AddStringToObject(json, RESPONSE_ERROR_STRING, _pIOCtrlData);
        }
        return json;
}

int LinkSendIOResponse(int nSession, unsigned int _nIOErrorCode, const char *_pIOCtrlData, int _nIOCtrlDataSize)
{
        if (nSession >= MAX_SESSION_ID) {
                return MQTT_ERR_INVAL;
        }
        if (!Session[nSession].isUsed) {
                return MQTT_ERR_INVAL;
        }
        cJSON *json = CreateResponse(_nIOErrorCode, _pIOCtrlData, _nIOCtrlDataSize);
        if (json == NULL) {
                return MQTT_ERR_NOMEM;
        }
        char* string = cJSON_Print(json);
        int ret = LinkMqttPublish(Session[nSession].pInstance, Session[nSession].pubTopic, strlen(string), string);
        free(string);
        cJSON_Delete(json);
        return ret;
}

int LinkRecvIOCtrl(int nSession, unsigned int *_pIOCtrlType, char *_pIOCtrlData, int *_nIOCtrlMaxDataSize, unsigned int _nTimeout)
{
        if (nSession >= MAX_SESSION_ID) {
                return MQTT_ERR_INVAL;
        }

        if (!Session[nSession].isUsed) {
                return MQTT_ERR_INVAL;
        }
        Message *pMessage = ReceiveMessageTimeout(Session[nSession].pQueue, _nTimeout);
        if (!pMessage) {
                return MQTT_RETRY;
        }
        cJSON *json = cJSON_Parse(pMessage->pMessage);
        if (json == NULL) {
                return MQTT_ERR_NOMEM;
        }
        cJSON *item_1 = cJSON_GetObjectItem(json, "action");
        if (item_1 == NULL) {
                cJSON_Delete(json);
                return MQTT_ERR_NOMEM;
        }
        char * action = cJSON_Print(item_1);
        *_pIOCtrlType = action[0] - '0';
        free(action);
        cJSON *item_2 = cJSON_GetObjectItem(json, "params");
        if (item_2 == NULL) {
                cJSON_Delete(json);
                return MQTT_SUCCESS;
        }
        char *params = cJSON_Print(item_2);
        if (*_nIOCtrlMaxDataSize > strlen(params)) {
                *_nIOCtrlMaxDataSize = strlen(params);
        }
        memcpy(_pIOCtrlData, params, *_nIOCtrlMaxDataSize);
        cJSON_Delete(json);
        free(params);
        free(pMessage->pMessage);
        free(pMessage);
        return MQTT_SUCCESS;
}

int LinkSendIOCtrlExit()
{
        // to do
        return 0;
}

void LinkDinitIOCtrl(int nSession)
{
        if (nSession >= MAX_SESSION_ID) {
                return;
        }
        if (Session[nSession].isUsed) {
                Session[nSession].pInstance = NULL;
                memset(Session[nSession].pubTopic, 0, sizeof(Session[nSession].pubTopic));
                memset(Session[nSession].subTopic, 0, sizeof(Session[nSession].subTopic));
                Session[nSession].isUsed = false;
                DestroyMessageQueue(&Session[nSession].pQueue);
        }
}


void * LinkLogThread(void* _pData)
{
        struct _LinkIOCrtlInfo* log = (struct _LinkIOCrtlInfo*)(_pData);
        MessageQueue* pQueue = NULL;
        do {
                pQueue = log->pQueue;
                Message *pMessage = ReceiveMessageTimeout(log->pQueue, 1000);
                if (pMessage) {
                         char topic[128] = {0};
                         sprintf(topic, "%s/%d",log->pubTopic, pMessage->nMessageID);
                         LinkMqttPublish(log->pInstance, topic, strlen(pMessage->pMessage), pMessage->pMessage);
                         free(pMessage->pMessage);
                         free(pMessage);
                }

        } while(log->isUsed);

        return NULL;
}

static pthread_t t;
int LinkInitLog(const char *_pAppId, const char *_pEncodeDeviceName, void *_pInstance)
{
        if (LogSession.isUsed) {
                return MQTT_ERR_INVAL;
        }
        if (_pInstance == NULL) {
                return MQTT_ERR_INVAL;
        }
        LogSession.pQueue = CreateMessageQueue(MESSAGE_QUEUE_MAX);
        if (!LogSession.pQueue) {
                LinkLogError("queue malloc fail\n");
                return MQTT_ERR_NOMEM;
        }
        memset(LogSession.pubTopic, 0, sizeof(LogSession.pubTopic));
        sprintf(LogSession.pubTopic, "/linking/v1/%s/%s/log/", _pAppId, _pEncodeDeviceName);
        LogSession.pInstance = _pInstance;
        LogSession.isUsed = true;
        pthread_create(&t, NULL, LinkLogThread, &LogSession);
        return 1;
}

int LinkSendLog(int level, const char *pLog, int nLength)
{
        Message *pMessage = (Message *) malloc(sizeof(Message));
        char* message = (char*) malloc(nLength);
        if ( !pMessage || !message ) {
                return MQTT_ERR_NOMEM;
        }
        pMessage->nMessageID = level;
        memset(message, 0, nLength);
        memcpy(message, pLog, nLength);
        pMessage->pMessage = message;

        if (LogSession.pQueue->nSize == LogSession.pQueue->nCapacity) {
                return MQTT_QUEUE_FULL;
        }
        SendMessage(LogSession.pQueue, pMessage);
        return 1;
}

void LinkDinitLog()
{
       LogSession.isUsed = false;
       pthread_join(t, NULL);
       DestroyMessageQueue(&LogSession.pQueue);
       memset(LogSession.pubTopic, 0, sizeof(LogSession.pubTopic));
       memset(LogSession.subTopic, 0, sizeof(LogSession.subTopic));
       LogSession.pInstance = NULL;
}
