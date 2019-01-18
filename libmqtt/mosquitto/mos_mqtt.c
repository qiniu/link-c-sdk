#include "mos_mqtt.h"


static int MqttErrorStatusChange(int nStatus)
{       
        switch (nStatus) {
                case MOSQ_ERR_CONN_PENDING:
                        return MQTT_ERR_CONN_PENDING;
                case MOSQ_ERR_NOMEM:
                        return MQTT_ERR_NOMEM;
                case MOSQ_ERR_INVAL:
                        return MQTT_ERR_INVAL;
                case MOSQ_ERR_NO_CONN:
                        return MQTT_ERR_NO_CONN;
                case MOSQ_ERR_CONN_REFUSED:
                        return MQTT_ERR_CONN_REFUSED;
                case MOSQ_ERR_NOT_FOUND:
                        return MQTT_ERR_NOT_FOUND;
                case MOSQ_ERR_CONN_LOST:
                        return MQTT_ERR_CONN_LOST;
                case MOSQ_ERR_TLS:
                        return MQTT_ERR_TLS;
                case MOSQ_ERR_PAYLOAD_SIZE:
                        return MQTT_ERR_PAYLOAD_SIZE;
                case MOSQ_ERR_NOT_SUPPORTED:
                        return MQTT_ERR_NOT_SUPPORTED;
                case MOSQ_ERR_AUTH:
                        return MQTT_ERR_AUTH;
                case MOSQ_ERR_ACL_DENIED:
                        return MQTT_ERR_ACL_DENIED;
                case MOSQ_ERR_UNKNOWN:
                        return MQTT_ERR_UNKNOWN;
                case MOSQ_ERR_ERRNO:
                        return MQTT_ERR_ERRNO;
                case MOSQ_ERR_EAI:
                        return MQTT_ERR_EAI;
                case MOSQ_ERR_PROXY:
                        return MQTT_ERR_PROXY;
                case MOSQ_ERR_PROTOCOL:
                        return MQTT_ERR_PROTOCOL;
                case MOSQ_ERR_SUCCESS:
                        return MQTT_SUCCESS;
                default:
                        return MQTT_ERR_OTHERS;
        }
        return MQTT_ERR_OTHERS;
}

void OnLogCallback(struct mosquitto* _pMosq, void* _pObj, int level, const char* _pStr)
{       
        printf("%s\n", _pStr);
}

void OnMessageCallback(struct mosquitto* _pMosq, void* _pObj, const struct mosquitto_message* _pMessage)
{
        int rc = MOSQ_ERR_SUCCESS;
        struct MqttInstance* pInstance = (struct MqttInstance*)(_pObj);
        if (pInstance->options.callbacks.OnMessage) {
                if (memcmp(pInstance->mosq->message_topic, IO_CTR_MESSAGE, IO_CTR_MESSAGE_LENGTH) == 0) {
                        OnIOCtrlMessage(pInstance, pInstance->options.nAccountId, pInstance->mosq->message_topic,
                                        (const char *)pInstance->mosq->message, min((MAX_MQTT_MESSAGE_LEN - 1), _pMessage->buffer_len));
                } else {
                        pInstance->options.callbacks.OnMessage(_pObj, pInstance->options.nAccountId,  _pMessage->topic, _pMessage->payload, _pMessage->payloadlen);
                }
        }
}

void OnConnectCallback(struct mosquitto* _pMosq, void* _pObj, int result)
{
        int rc = MOSQ_ERR_SUCCESS;
        struct MqttInstance* pInstance = (struct MqttInstance*)(_pObj);
        printf("OnConnectCallback result %d", result);
        OnEventCallback(pInstance,
                        (result == 0) ? MQTT_CONNECT_SUCCESS : MqttErrorStatusChange(result),
                        (result == 0) ? "on connect success" : mosquitto_connack_string(result));
        if (result) {
                pInstance->connected = false;
                pInstance->status = STATUS_CONNECT_ERROR;
        }
        else {
                pInstance->status = STATUS_CONNACK_RECVD;
                pInstance->connected = true;
                pthread_mutex_lock(&pInstance->listMutex);
                Node* p = pInstance->pSubsribeList.pNext;
                while (p) {
                        mosquitto_subscribe(pInstance->mosq, NULL, p->topic, pInstance->options.nQos);
                        p = p->pNext;
                }
                pthread_mutex_unlock(&pInstance->listMutex);
        }
}

void OnDisconnectCallback(struct mosquitto* _pMosq, void* _pObj, int rc)
{
        struct MqttInstance* pInstance = (struct MqttInstance*)(_pObj);
        printf("OnDisconnectCallback result %d", rc);
        OnEventCallback(pInstance,
               (rc == 0) ? MQTT_DISCONNECT_SUCCESS : MqttErrorStatusChange(rc),
               (rc == 0) ? "on disconnect success" : mosquitto_strerror(rc));
        pInstance->connected = false;
        if (!rc) {
                pInstance->status = STATUS_IDLE;
        }
        else {
                pInstance->status = STATUS_CONNECT_ERROR;
        }
}

void OnSubscribeCallback(struct mosquitto* _pMosq, void* pObj, int mid, int qos_count, const int* pGranted_qos)
{
        //fprintf(stderr, "Subscribed (mid: %d): %d \n", mid, pGranted_qos[0]);
}

void OnUnsubscribeCallback(struct mosquitto* _pMosq, void* _pObj, int mid)
{
        //fprintf(stderr, "Unsubscribed (mid: %d) \n", mid);
}

void OnPublishCallback(struct mosquitto* _pMosq, void* _pObj, int mid)
{
        //fprintf(stderr, " my_publish_callback \n ");
        struct MqttInstance* pInstance = (struct MqttInstance*)(_pObj);
        int last_mid_sent = mid;
}

int ClientOptSet(struct MqttInstance* _pInstance, struct MqttUserInfo info)
{
        int rc = 0;
        if (info.nAuthenicatinMode & MQTT_AUTHENTICATION_USER) {
                rc = mosquitto_username_pw_set(_pInstance->mosq, info.pUsername, info.pPassword);
                if (rc)
                        return rc;
        }
        if (info.nAuthenicatinMode & MQTT_AUTHENTICATION_ONEWAY_SSL) {
                rc = mosquitto_tls_set(_pInstance->mosq, info.pCafile, NULL, NULL, NULL, NULL);
        }
        else if (info.nAuthenicatinMode & MQTT_AUTHENTICATION_TWOWAY_SSL) {
                rc = mosquitto_tls_set(_pInstance->mosq, info.pCafile, NULL, info.pCertfile, info.pKeyfile, NULL);
        }
        if (rc) {
                printf("ClientOptSet error %d\n", rc);
        }
        return MqttErrorStatusChange(rc);
}

int ReConnect(struct MqttInstance *instance, int error_code)
{
        return MqttErrorStatusChange(MOSQ_ERR_SUCCESS);
}

int LinkMqttPublish(IN const void* _pInstance, IN const char* _pTopic, IN int _nPayloadlen, IN const void* _pPayload)
{      
       struct MqttInstance* pInstance = (struct MqttInstance*)(_pInstance);
       int rc = mosquitto_publish(pInstance->mosq, NULL, _pTopic, _nPayloadlen, _pPayload, pInstance->options.nQos, pInstance->options.bRetain);
       if (rc) {
               switch (rc) {
                       case MOSQ_ERR_INVAL:
                               fprintf(stderr, "Error: Invalid input. Does your topic contain '+' or '#'?\n");
                               break;
                       case MOSQ_ERR_NOMEM:
                               fprintf(stderr, "Error: Out of memory when trying to publish message.\n");
                               break;
                       case MOSQ_ERR_NO_CONN:
                               fprintf(stderr, "Error: Client not connected when trying to publish.\n");
                               break;
                       case MOSQ_ERR_PROTOCOL: 
                               fprintf(stderr, "Error: Protocol error when communicating with broker.\n");
                               break;
                       case MOSQ_ERR_PAYLOAD_SIZE:
                               fprintf(stderr, "Error: Message payload is too large.\n");
                               break;
               }
       }
       return MqttErrorStatusChange(rc);
}

int LinkMqttSubscribe(IN const void* _pInstance, IN const char* _pTopic)
{
        struct MqttInstance* pInstance = (struct MqttInstance*)(_pInstance);
        if (_pTopic == NULL) {
               return MQTT_ERR_INVAL;
        }
        int rc = mosquitto_subscribe(pInstance->mosq, NULL, _pTopic, pInstance->options.nQos);
        if (!rc) {
                pthread_mutex_lock(&pInstance->listMutex);
                InsertNode(&pInstance->pSubsribeList, _pTopic);
                pthread_mutex_unlock(&pInstance->listMutex);
        }
        else {
               switch (rc) {
                       case MOSQ_ERR_INVAL:
                               fprintf(stderr, "Error: Invalid input.\n");
                               break;
                       case MOSQ_ERR_NOMEM:
                               fprintf(stderr, "Error: Out of memory when trying to subscribe message.\n");
                               break;
                       case MOSQ_ERR_NO_CONN:
                               fprintf(stderr, "Error: Client not connected when trying to subscribe.\n");
                               break;
                       case MOSQ_ERR_PROTOCOL:
                               fprintf(stderr, "Error: Protocol error when communicating with broker.\n");
                               break;
                       case MOSQ_ERR_PAYLOAD_SIZE:
                               fprintf(stderr, "Error: Message payload is too large.\n");
                               break;
               }
        }
        return MqttErrorStatusChange(rc);
}

int LinkMqttUnsubscribe(IN const void* _pInstance, IN const char* _pTopic)
{
        struct MqttInstance* pInstance = (struct MqttInstance*)(_pInstance);
        if (_pTopic == NULL) {
               fprintf(stderr, "Error: Invalid input.\n");
               return MQTT_ERR_INVAL;
        }
        int rc = mosquitto_unsubscribe(pInstance->mosq, NULL, _pTopic);
        fprintf(stderr, "mos sub %d", rc);
        if (!rc) {
               pthread_mutex_lock(&pInstance->listMutex);
               DeleteNode(&pInstance->pSubsribeList, _pTopic);
               pthread_mutex_unlock(&pInstance->listMutex);
        }
        else {
               switch (rc) {
                       case MOSQ_ERR_INVAL:
                               fprintf(stderr, "Error: Invalid input.\n");
                               break;
                       case MOSQ_ERR_NOMEM:
                               fprintf(stderr, "Error: Out of memory when trying to unsubscribe message.\n");
                               break;
                       case MOSQ_ERR_NO_CONN:
                               fprintf(stderr, "Error: Client not connected when trying to unsubscribe.\n");
                               break;
                       case MOSQ_ERR_PROTOCOL:
                               fprintf(stderr, "Error: Protocol error when communicating with broker.\n");
                               break;
               }
        }
        return MqttErrorStatusChange(rc);
}

int LinkMqttLibInit()
{
        int rc = mosquitto_lib_init();
        return MqttErrorStatusChange(rc);
}

int LinkMqttLibCleanup()
{
        int rc = mosquitto_lib_cleanup();
        return MqttErrorStatusChange(rc);
}

MQTT_ERR_STATUS LinkMqttConnect(struct MqttInstance* _pInstance)
{
	printf("LinkMqttConnect \n");
        int rc = mosquitto_connect(_pInstance->mosq, _pInstance->options.userInfo.pHostname, _pInstance->options.userInfo.nPort, _pInstance->options.nKeepalive);
        printf("LinkMqttConnect %d\n", rc);
        return MqttErrorStatusChange(rc);
}

void LinkMqttDisconnect(struct MqttInstance* _pInstance)
{
        mosquitto_disconnect(_pInstance->mosq);
}

MQTT_ERR_STATUS LinkMqttLoop(struct MqttInstance* _pInstance)
{
	int rc = mosquitto_loop(_pInstance->mosq, -1, 1);
	return MqttErrorStatusChange(rc);
}

int LinkMqttInit(struct MqttInstance* _pInstance)
{
	printf("LinkMqttInit \n");
        _pInstance->mosq = mosquitto_new(_pInstance->options.pId, true, _pInstance);
        if (!_pInstance->mosq) {
                switch(errno) {
                        case ENOMEM:
                                fprintf(stderr, "Error: Out of memory.\n");
                                break;
                        case EINVAL:
                                fprintf(stderr, "Error: Invalid id.\n");
                                break;
                }
                return MQTT_ERR_INVAL;
        }
	printf("LinkMqttInit *********** %d\n", MQTT_SUCCESS);
        mosquitto_threaded_set(_pInstance->mosq, true);
        //mosquitto_log_callback_set(_pInstance->mosq, OnLogCallback);
        mosquitto_connect_callback_set(_pInstance->mosq, OnConnectCallback);
        mosquitto_disconnect_callback_set(_pInstance->mosq, OnDisconnectCallback);
        mosquitto_publish_callback_set(_pInstance->mosq, OnPublishCallback);
        mosquitto_message_callback_set(_pInstance->mosq, OnMessageCallback);
        mosquitto_subscribe_callback_set(_pInstance->mosq, OnSubscribeCallback);
        mosquitto_unsubscribe_callback_set(_pInstance->mosq, OnUnsubscribeCallback);
        return MQTT_SUCCESS;
}

void LinkMqttDinit(struct MqttInstance* _pInstance)
{
	mosquitto_destroy(_pInstance->mosq);
}
