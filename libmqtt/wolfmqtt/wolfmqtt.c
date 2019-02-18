#include "wolfmqtt.h"
#include "mqtt_types.h"
#include "mqtt_packet.h"
#include "mqtt_socket.h"
#include "wolfmqtt/mqtt_client.h"
#include "mqttnet.h"
#include "log.h"

#include <signal.h>

#define MAX_PACKET_ID ((1<<16) -1)
#define DEFAULT_CON_TIMEOUT_MS 2000

#define min(a,b) ((a)>(b)? (b):(a))

static int mPacketIdLast;

//将wolfMQTT的错误码转成link错误码
static int MqttErrorStatusChange(int nStatus)
{
        switch (nStatus) {
        case MQTT_CODE_SUCCESS:
                return MQTT_SUCCESS;
        case MQTT_CODE_STDIN_WAKE:
                return MQTT_ERR_INVAL;;
        case MQTT_CODE_ERROR_BAD_ARG:
                return MQTT_ERR_INVAL;
        case MQTT_CODE_ERROR_OUT_OF_BUFFER:
                return MQTT_ERR_PAYLOAD_SIZE;
        case MQTT_CODE_ERROR_MALFORMED_DATA:
                return MQTT_ERR_INVAL;
        case MQTT_CODE_ERROR_PACKET_TYPE:
                return MQTT_ERR_INVAL;
        case MQTT_CODE_ERROR_PACKET_ID:
                return MQTT_ERR_INVAL;
        case MQTT_CODE_ERROR_TLS_CONNECT:
                return MQTT_ERR_TLS;
        case MQTT_CODE_ERROR_TIMEOUT:
                return MQTT_ERR_CONN_LOST;
        case MQTT_CODE_ERROR_NETWORK:
                return MQTT_ERR_ERRNO;
        case MQTT_CODE_ERROR_MEMORY:
                return MQTT_ERR_NOMEM;
        case MQTT_CODE_ERROR_STAT:
                return MQTT_ERR_ERRNO;
        case MQTT_CODE_ERROR_PROPERTY:
                return MQTT_ERR_INVAL;
        case MQTT_CODE_ERROR_SERVER_PROP:
                return MQTT_ERR_ERRNO;
        case MQTT_CODE_ERROR_CALLBACK:
                return MQTT_ERR_ERRNO;
        case MQTT_CODE_CONTINUE:
                return MQTT_CONTINUE;
        default:
                return MQTT_ERR_OTHERS;
    }
}

static word16 mqtt_get_packetid(void)
{
        mPacketIdLast = (mPacketIdLast >= MAX_PACKET_ID) ? 1 : mPacketIdLast + 1;
        return (word16)mPacketIdLast;
}

int LinkMqttPublish(IN const void* _pInstance, IN const char* _pTopic, IN int _nPayloadlen, IN const void* _pPayload)
{      
        struct MqttInstance* pInstance = (struct MqttInstance*)(_pInstance);
        if (_pTopic == NULL || _pPayload == NULL) {
                return MQTT_ERR_INVAL;
        }
        struct MQTTCtx* ctx = (struct MQTTCtx*)(pInstance->mosq);
        MqttClient* client = &ctx->client;
        MqttPublish publish;
        memset(&publish, 0, sizeof(MqttPublish));
        publish.retain = 0;
        publish.qos = pInstance->options.nQos;
        publish.duplicate = 0;
        publish.topic_name = _pTopic;
        publish.packet_id = mqtt_get_packetid();
        publish.buffer = (byte *)_pPayload;
        publish.total_len = (word16)_nPayloadlen;
        pthread_mutex_lock(&pInstance->listMutex);
        int rc = MqttClient_Publish(client, &publish);
        if (rc != MQTT_CODE_SUCCESS) {
                LinkLogError("MQTT Publish fail, %s(%d)\n“", MqttClient_ReturnCodeToString(rc), rc);
        }
        pthread_mutex_unlock(&pInstance->listMutex);
        usleep(1000);
        return MqttErrorStatusChange(rc);
}

int LinkMqttSubscribe(IN const void* _pInstance, IN const char* _pTopic)
{
        struct MqttInstance* pInstance = (struct MqttInstance*)(_pInstance);
        if (_pTopic == NULL) {
                return MQTT_ERR_INVAL;
        }
        struct MQTTCtx* ctx = (struct MQTTCtx*)(pInstance->mosq);
        memset(&ctx->subscribe, 0, sizeof(MqttSubscribe));
        MqttClient* client = &ctx->client;
        ctx->subscribe.packet_id = mqtt_get_packetid();
        ctx->subscribe.topic_count = 1; //in current time, we support 1 topic in on time.
        ctx->topics[0].qos = pInstance->options.nQos;
        ctx->topics[0].topic_filter = _pTopic;
        ctx->subscribe.topics = &ctx->topics[0];
        pthread_mutex_lock(&pInstance->listMutex);
        int rc = MqttClient_Subscribe(client, &ctx->subscribe);
        if (rc == MQTT_CODE_SUCCESS) {
                InsertNode(&pInstance->pSubsribeList, _pTopic);
        }
        else {
                LinkLogError("MQTT Subscribe: %s (%d)\n", MqttClient_ReturnCodeToString(rc), rc);
        }
        pthread_mutex_unlock(&pInstance->listMutex);
        usleep(1000);
        return MqttErrorStatusChange(rc);
}

int LinkMqttUnsubscribe(IN const void* _pInstance, IN const char* _pTopic)
{
        struct MqttInstance* pInstance = (struct MqttInstance*)(_pInstance);
        if (_pTopic == NULL) {
                LinkLogError("Error: Invalid input.\n");
                return MQTT_ERR_INVAL;
        }
        struct MQTTCtx* ctx = (struct MQTTCtx*)(pInstance->mosq);
        ctx->topics[0].qos = pInstance->options.nQos;
        ctx->topics[0].topic_filter = _pTopic;
        MqttClient* client = &ctx->client;
        ctx->unsubscribe.packet_id = mqtt_get_packetid();
        ctx->unsubscribe.topic_count = 1; //in current time, we support 1 topic in on time.
        ctx->unsubscribe.topics = &ctx->topics[0];
        pthread_mutex_lock(&pInstance->listMutex);
        int rc = MqttClient_Unsubscribe(client, &ctx->unsubscribe);
        if (rc == MQTT_CODE_SUCCESS) {
                DeleteNode(&pInstance->pSubsribeList, _pTopic);
        }
        else {
                LinkLogError("MQTT Unsubscribe: %s (%d)\n", MqttClient_ReturnCodeToString(rc), rc);
        }
        pthread_mutex_unlock(&pInstance->listMutex);
        usleep(1000);
        return MqttErrorStatusChange(rc);
}

int LinkMqttLibInit()
{
        signal(SIGPIPE, SIG_IGN);
        sigset_t signal_mask;
        sigemptyset(&signal_mask);
        sigaddset(&signal_mask, SIGPIPE);
        int rc = pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
        if (rc != 0) {
                LinkLogError("block sigpipe error\n");
        } 
        return MQTT_SUCCESS;
}

int LinkMqttLibCleanup()
{
        return MQTT_SUCCESS;
}

static int OnDisconnectCallback(MqttClient* client, int error_code, void* ctx)
{
        struct MqttInstance *pInstance = client->ctx;
        if (error_code == MQTT_CODE_ERROR_TIMEOUT) {
                return 0;
        }
        if (pInstance == NULL) {
                 return 0;
        }
        LinkLogInfo("OnDisconnectCallback result %d %p \n", error_code, pInstance);
        OnEventCallback(pInstance,
                (error_code == MQTT_CODE_SUCCESS) ? MQTT_SUCCESS : MqttErrorStatusChange(error_code),
                (error_code == 0) ? "on disconnect success" : MqttClient_ReturnCodeToString(error_code));
        LinkMqttDisconnect(pInstance);
        pInstance->connected = false;
        if (error_code == MQTT_CODE_SUCCESS) {
                pInstance->status = STATUS_IDLE;
        }
        else {
                pInstance->status = STATUS_CONNECT_ERROR;
        }
	if (!pInstance->isDestroying) {
                LinkMqttDinit(pInstance);
                LinkMqttInit(pInstance);
        }
        return 0;
}

int ReConnect(struct MqttInstance *instance, int error_code)
{
        struct MQTTCtx *mqttCtx = instance->mosq;
        int ret = OnDisconnectCallback(&mqttCtx->client, MQTT_CODE_ERROR_NETWORK, NULL);

        return ret;
}

static int RecoverSub(IN struct MqttInstance *_pInstance)
{
        int rc = MQTT_CODE_SUCCESS;
        struct MqttInstance *pInstance = _pInstance;
        struct MQTTCtx *mqttCtx = pInstance->mosq;
        Node *p = pInstance->pSubsribeList.pNext;

        mqttCtx->subscribe.packet_id = mqtt_get_packetid();
        mqttCtx->subscribe.topic_count = 1;
        mqttCtx->topics[0].qos = pInstance->options.nQos;
        pthread_mutex_lock(&pInstance->listMutex);
        while (p) {
                mqttCtx->topics[0].topic_filter = p->topic;
                mqttCtx->subscribe.topics = &mqttCtx->topics[0];
                rc = MqttClient_Subscribe(&mqttCtx->client, &mqttCtx->subscribe);
                p = p->pNext;
        }
        pthread_mutex_unlock(&pInstance->listMutex);
        return MqttErrorStatusChange(rc);
}

static int OnMessageCallback(struct _MqttClient *client, MqttMessage *_pMessage, byte msg_new, byte msg_done)
{
        struct MqttInstance *pInstance = client->ctx;
        if (pInstance->options.callbacks.OnMessage) {
                memset(pInstance->mosq->message_topic, 0, MAX_MQTT_TOPIC_LEN);
                memset(pInstance->mosq->message, 0, MAX_MQTT_MESSAGE_LEN);
                memcpy(pInstance->mosq->message_topic, _pMessage->topic_name, min((MAX_MQTT_TOPIC_LEN - 1), _pMessage->topic_name_len));
                memcpy(pInstance->mosq->message, _pMessage->buffer, min((MAX_MQTT_MESSAGE_LEN - 1), _pMessage->buffer_len));
                //LinkLogDebug("topic %s \n", pInstance->mosq->message_topic);
                if (memcmp(pInstance->mosq->message_topic, IO_CTR_MESSAGE, IO_CTR_MESSAGE_LENGTH) == 0) {
                        OnIOCtrlMessage(pInstance, pInstance->options.nAccountId, pInstance->mosq->message_topic,
                                        (const char *)pInstance->mosq->message, min((MAX_MQTT_MESSAGE_LEN - 1), _pMessage->buffer_len));
                } else {
                        pInstance->options.callbacks.OnMessage(pInstance, pInstance->options.nAccountId, pInstance->mosq->message_topic,
                               (const char *)pInstance->mosq->message, min((MAX_MQTT_MESSAGE_LEN - 1), _pMessage->buffer_len));
                }
        }
        return MQTT_CODE_SUCCESS;
}

static int mqtt_tls_verify_cb(int preverify, WOLFSSL_X509_STORE_CTX* store)
{
        char buffer[WOLFSSL_MAX_ERROR_SZ];
        MQTTCtx *mqttCtx = NULL;
        char appName[MAX_BUFFER_SIZE] = {0};

        LinkLogError("MQTT TLS Verify  PreVerify %d, Error %d (%s) \n",
                preverify,
                store->error, store->error != 0 ?
                wolfSSL_ERR_error_string(store->error, buffer) : "none");
        LinkLogDebug("Subject's domain name is %s \n", store->domain);

        if (store->error != 0) {
                /* Allowing to continue */
                /* Should check certificate and return 0 if not okay */
                LinkLogError("Allowing cert anyways \n");
        }
        return 1;
}

/* Use this callback to setup TLS certificates and verify callbacks */
int mqtt_tls_cb(MqttClient* client)
{
        struct MqttInstance *pInstance;
        pInstance = client->ctx;
        int rc = WOLFSSL_SUCCESS;

        if ((pInstance->options.userInfo.nAuthenicatinMode & MQTT_AUTHENTICATION_ONEWAY_SSL) || (pInstance->options.userInfo.nAuthenicatinMode & MQTT_AUTHENTICATION_TWOWAY_SSL)) {
                 client->tls.ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
                 if (client->tls.ctx) {
                          wolfSSL_CTX_set_verify(client->tls.ctx, WOLFSSL_VERIFY_PEER,
                                                 mqtt_tls_verify_cb);
                          if (pInstance->options.userInfo.pCafile) {
                                  /* Load CA certificate file */
                                  rc = wolfSSL_CTX_load_verify_locations(client->tls.ctx,
                                                                         pInstance->options.userInfo.pCafile, NULL);
                          }
                }
        }
        LinkLogError("MQTT TLS Setup (%d)", rc);
        return rc;
}

int ClientOptSet(struct MqttInstance* _pInstance, struct MqttUserInfo info)
{
        int rc = 0;
        return MqttErrorStatusChange(rc);
}

int LinkMqttInit(struct MqttInstance* _pInstance)
{
        int rc = MQTT_SUCCESS;
        struct MQTTCtx *mqtt_ctx = (struct MQTTCtx*)malloc(sizeof(MQTTCtx));
	if (mqtt_ctx == NULL) {
                return MQTT_ERR_NOMEM;
        }
        memset(mqtt_ctx, 0, sizeof(MQTTCtx));
        _pInstance->mosq = mqtt_ctx;
        rc = MqttClientNet_Init(&(mqtt_ctx->net));
        if (rc != MQTT_CODE_SUCCESS) {
                return MqttErrorStatusChange(rc);
        }
        mqtt_ctx->tx_buf = (byte *)malloc(MAX_BUFFER_SIZE);
        mqtt_ctx->rx_buf = (byte *)malloc(MAX_BUFFER_SIZE);
        mqtt_ctx->message = (char *)malloc(MAX_MQTT_MESSAGE_LEN); 
        if (mqtt_ctx->tx_buf == NULL || mqtt_ctx->rx_buf == NULL || mqtt_ctx->message== NULL) {
                free(mqtt_ctx);
                return MQTT_ERR_NOMEM;
        }
        rc = MqttClient_Init(&mqtt_ctx->client, &mqtt_ctx->net, OnMessageCallback, mqtt_ctx->tx_buf, MAX_BUFFER_SIZE,
                             mqtt_ctx->rx_buf, MAX_BUFFER_SIZE, DEFAULT_CON_TIMEOUT_MS);
        mqtt_ctx->client.ctx = _pInstance;
        LinkLogDebug(" _pInstance->mosq %p _pInstance %p client  %p\n", _pInstance->mosq, _pInstance, &mqtt_ctx->client);
        MqttClient_SetDisconnectCallback(&mqtt_ctx->client, OnDisconnectCallback, NULL);
        return MqttErrorStatusChange(rc);
}

void LinkMqttDinit(struct MqttInstance* _pInstance)
{
	if (_pInstance == NULL || _pInstance->mosq == NULL) {
                return;
        }
        struct MQTTCtx *mqtt_ctx = (struct MQTTCtx*)(_pInstance->mosq);
        if (mqtt_ctx->tx_buf)
                free(mqtt_ctx->tx_buf);
        if (mqtt_ctx->rx_buf)
                free(mqtt_ctx->rx_buf);
        if (mqtt_ctx->message)
                free(mqtt_ctx->message);
        MqttClientNet_DeInit(&mqtt_ctx->net);
        free(_pInstance->mosq);
}

MQTT_ERR_STATUS LinkMqttConnect(struct MqttInstance* _pInstance)
{
        if (_pInstance == NULL || _pInstance->mosq == NULL) {
                return MQTT_ERR_NOMEM;
        }
        struct MQTTCtx* ctx = (struct MQTTCtx*)(_pInstance->mosq);
        MqttClient* client = &ctx->client;

        LinkLogError("MqttConnect host %s port %d keepalive %d \n", _pInstance->options.userInfo.pHostname, _pInstance->options.userInfo.nPort, _pInstance->options.nKeepalive);
        int rc = 0;
        MqttMessage lwt_msg;
        /* Define connect parameters */
        ctx->connect.keep_alive_sec = _pInstance->options.nKeepalive;
        ctx->connect.clean_session = _pInstance->options.bCleanSession;
        ctx->connect.client_id = _pInstance->options.pId;
        /* Last will and testament sent by broker to subscribers of topic when broker
           connection is lost */
        memset(&lwt_msg, 0, sizeof(lwt_msg));
        ctx->connect.lwt_msg = &lwt_msg;
        ctx->connect.enable_lwt = false; //to do
        if (ctx->connect.enable_lwt) {
                lwt_msg.qos = 0;
                lwt_msg.retain = 0;
                lwt_msg.topic_name = "lwttopic";
		// to do
        }

        if ((_pInstance->options.userInfo.nAuthenicatinMode & MQTT_AUTHENTICATION_ONEWAY_SSL) || (_pInstance->options.userInfo.nAuthenicatinMode & MQTT_AUTHENTICATION_TWOWAY_SSL)) {
                ctx->use_tls = 1;
        }
        /* Optional authentication */
        ctx->connect.username = _pInstance->options.userInfo.pUsername;
        ctx->connect.password = _pInstance->options.userInfo.pPassword;
	LinkLogDebug("MqttConnect username %s, password %s  tls %d\n", ctx->connect.username, ctx->connect.password, ctx->use_tls); 
	rc = MqttClient_NetConnect(client, _pInstance->options.userInfo.pHostname, _pInstance->options.userInfo.nPort,
                                   5000, ctx->use_tls, mqtt_tls_cb);
        if (rc != MQTT_CODE_SUCCESS) {
            sleep(1);
            LinkLogError("EstablishConnect failed rc %d %s \n", rc, MqttClient_ReturnCodeToString(rc));
            return MqttErrorStatusChange(rc);
        }
        /* Send Connect and wait for Connect Ack */
        rc = MqttClient_Connect(client, &ctx->connect);
        if (rc != MQTT_CODE_SUCCESS) {
                LinkLogError("MQTT Connect: %s (%d)\n", MqttClient_ReturnCodeToString(rc), rc);
        } else {
                _pInstance->connected = true;
                _pInstance->status = STATUS_CONNACK_RECVD;
        }
        usleep(100000);
        OnEventCallback(_pInstance,
               (rc == MQTT_CODE_SUCCESS) ? MQTT_SUCCESS : MqttErrorStatusChange(rc),
               (rc == 0) ? "on connect success" : MqttClient_ReturnCodeToString(rc));
//	LinkLogError("MQTT Connect: %s (%d)\n", MqttClient_ReturnCodeToString(rc), rc);
	RecoverSub(_pInstance);
	return MqttErrorStatusChange(rc);
}

void LinkMqttDisconnect(struct MqttInstance* _pInstance)
{
        if (_pInstance == NULL || _pInstance->mosq == NULL) {
                return;
        }
        struct MQTTCtx* ctx = (struct MQTTCtx*)(_pInstance->mosq);
        MqttClient* client = &ctx->client;
        if (client->tls.ctx) {
                wolfSSL_CTX_free(client->tls.ctx);
                client->tls.ctx = NULL;
        }

        int rc = MqttClient_NetDisconnect(client);
        if (rc != MQTT_CODE_SUCCESS) {
                LinkLogError("MQTT Disconnect: %s (%d)\n", MqttClient_ReturnCodeToString(rc), rc);
        }
        _pInstance->connected = false;
}

MQTT_ERR_STATUS LinkMqttLoop(struct MqttInstance* _pInstance)
{
        if (_pInstance == NULL || _pInstance->mosq == NULL) {
                return MQTT_ERR_NOMEM;
        }
        struct MQTTCtx* ctx = (struct MQTTCtx*)(_pInstance->mosq);
        MqttClient client = ctx->client;
        pthread_mutex_lock(&_pInstance->listMutex);
        int rc = MqttClient_WaitMessage(&client, DEFAULT_CON_TIMEOUT_MS);
        if (rc == MQTT_CODE_ERROR_TIMEOUT) {
                ++ ctx->timeoutCount;
                /* Keep Alive */
//                LinkLogError("Keep-alive timeout, sending ping , timeoutCount:%d\n", ctx->timeoutCount);
                if (ctx->timeoutCount * DEFAULT_CON_TIMEOUT_MS
                    > ctx->connect.keep_alive_sec * 1000) {
                        rc = MqttClient_Ping(&client);
                        if (rc == MQTT_CODE_CONTINUE) {
                                return MqttErrorStatusChange(rc);
                        }
                        else if (rc != MQTT_CODE_SUCCESS) {
                                LinkLogError("MQTT Ping Keep Alive Error: %s (%d)",
                                             MqttClient_ReturnCodeToString(rc), rc);
                                ctx->timeoutCount = 0;
                        }
                        ctx->timeoutCount = 0;
                } else if (rc == MQTT_CODE_ERROR_TIMEOUT) {
                        rc = MQTT_CODE_SUCCESS;
                }
        }
        pthread_mutex_unlock(&_pInstance->listMutex);
        usleep(1000);
        if (rc != MQTT_CODE_SUCCESS && rc != MQTT_CODE_ERROR_TIMEOUT) {
                LinkLogError("MQTT WaitMessage error %d", rc);
        }
	return MqttErrorStatusChange(rc);
}
