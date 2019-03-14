/**
 * @file qnlinking_mqtt.c
 * @author Qiniu.com
 * @copyright 2019(c) Shanghai Qiniu Information Technologies Co., Ltd.
 * @brief qnlinking mqtt api c file
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "hmac_sha1/hmac_sha1.h"
#include "cJSON/cJSON.h"
#include "b64/urlsafe_b64.h"
#include "log/log.h"
#include "queue.h"
#include "mqtt_internal.h"
#include "mqtt.h"

#define QNLINKING_MQTT_SERVER "mqtt.qnlinking.com"
#define QNLINKING_MQTT_PORT 1883
#define QNLINKING_MQTT_KEEPALIVE 10
#define MAX_LOG_LEN 256
#define QNLINKING_MQTT_LOG_TOPIC "linking/v1/${appid}/${device}/syslog/"

typedef struct _QnlinkingMQTTLog {
        bool isUsed;
        char pubTopic[128];
        void *pInstance;
        MessageQueue *pQueue;
        pthread_t Thread;
}QnlinkingMQTTLog;

static bool gQnlinkingMQTTInitialized = false;
static void *gQnlinkingMQTTInstance = NULL;
static struct MqttOptions gQnlinkingMQTTOpt;
static char gLinkDAK[64] = {0};
static char gLinkDSK[64] = {0};
static QnlinkingMQTTLog gQnlinkingMQTTLog;

static int QnlinkingMQTT_GetUsernameSign(char *_pUsername, int *_pLen, const char *_pDak)
{
        char query[256] = {0};
        long timestamp = 0.0;
        timestamp = (long)time(NULL);
        if (!_pDak) {
                return LINK_MQTT_ERROR;
        }
        *_pLen = sprintf(query, "dak=%s&timestamp=%ld&version=v1", _pDak, timestamp);
        if (*_pLen <= 0) {
                return LINK_MQTT_ERROR;
        }
        strncpy(_pUsername, query, *_pLen + 1);
        return LINK_MQTT_SUCCESS;
}


static int QnlinkingMQTT_GetPasswordSign(const char *_pInput, int _nInLen,
                char *_pOutput, int *_pOutLen, const char *_pDsk)
{
        int ret = 0;
        char hsha1[20] = {0};

        if (!_pInput || !_pDsk) {
                return LINK_MQTT_ERROR;
        }
        ret = hmac_sha1(_pDsk, strlen(_pDsk), _pInput, _nInLen, hsha1, sizeof(hsha1));

        if (ret != 20) {
                return LINK_MQTT_ERROR;
        }
        int outlen = urlsafe_b64_encode(hsha1, 20, _pOutput, _pOutLen);
        *_pOutLen = outlen;

        return LINK_MQTT_SUCCESS;
}

static int QnlinkingMQTT_UpdateUserPasswd(const void *_pInstance)
{
        if (_pInstance) {
                struct MqttInstance* pInstance = (struct MqttInstance*) (_pInstance);
                int nUsernameLen = 0;
                int nPasswdLen = 0;
                QnlinkingMQTT_GetUsernameSign(pInstance->options.userInfo.pUsername, &nUsernameLen, gLinkDAK);
                QnlinkingMQTT_GetPasswordSign(pInstance->options.userInfo.pUsername, nUsernameLen,
                                pInstance->options.userInfo.pPassword, &nPasswdLen, gLinkDSK);
                return LINK_MQTT_SUCCESS;
        }
        return LINK_MQTT_ERROR;
}

void * QnlinkingMQTT_LogThread(void* _pData)
{
        do {
                Message *pMessage = ReceiveMessageTimeout(gQnlinkingMQTTLog.pQueue, 1000);
                if (pMessage) {
                         char topic[128] = {0};
                         sprintf(topic, "%s",gQnlinkingMQTTLog.pubTopic);
                         LinkMqttPublish(gQnlinkingMQTTLog.pInstance, topic, strlen(pMessage->pMessage), pMessage->pMessage);
                         free(pMessage->pMessage);
                         free(pMessage);
                }

        } while(gQnlinkingMQTTLog.isUsed);

        return NULL;
}

void QnlinkingMQTT_EventCallback(const void *_pInstance, int _nAccountId, int _nId, const char *_pReason)
{
    struct MqttInstance *instance = _pInstance;

    if (gQnlinkingMQTTInstance == _pInstance) {
        QnlinkingMQTT_UpdateUserPasswd(_pInstance);
    }
}

void QnlinkingMQTT_MessageCallback(const void *_pInstance, int _nAccountId, const char *_pTopic, const char *_pMessage, size_t nLength)
{
        return;
}

void QnlinkingMQTT_Init(const char * pDak, int nDakLen, const char * pDsk, int nDskLen)
{
        /* Create mqtt instance */
        if (!gQnlinkingMQTTInitialized) {

                /* Init libmqtt */
                LinkMqttLibInit();

                strncpy(gLinkDAK, pDak, nDakLen);
                strncpy(gLinkDSK, pDsk, nDakLen);
                memset(&gQnlinkingMQTTOpt, 0, sizeof(MqttOptions));
                gQnlinkingMQTTOpt.userInfo.nAuthenicatinMode = MQTT_AUTHENTICATION_USER;
                gQnlinkingMQTTOpt.userInfo.pHostname = QNLINKING_MQTT_SERVER;
                gQnlinkingMQTTOpt.userInfo.nPort = QNLINKING_MQTT_PORT;
                gQnlinkingMQTTOpt.nKeepalive = QNLINKING_MQTT_KEEPALIVE;
                gQnlinkingMQTTOpt.nQos = 0;
                gQnlinkingMQTTOpt.pId = gLinkDAK;
                gQnlinkingMQTTOpt.bRetain = false;
                gQnlinkingMQTTOpt.bCleanSession = false;
                gQnlinkingMQTTOpt.callbacks.OnEvent = QnlinkingMQTT_EventCallback;
                gQnlinkingMQTTOpt.callbacks.OnMessage = QnlinkingMQTT_MessageCallback;

                char MQTTusername[256] = {0};
                int nMQTTUsernameLen;
                char MQTTpassword[256] = {0};
                int nMQTTPasswordLen;
                if (LINK_MQTT_SUCCESS == QnlinkingMQTT_GetUsernameSign(MQTTusername, &nMQTTUsernameLen, gLinkDAK)
                                && LINK_MQTT_SUCCESS == QnlinkingMQTT_GetPasswordSign(MQTTusername, nMQTTUsernameLen, MQTTpassword, &nMQTTPasswordLen, gLinkDSK)) {
                        gQnlinkingMQTTOpt.userInfo.pUsername = MQTTusername;
                        gQnlinkingMQTTOpt.userInfo.pPassword = MQTTpassword;
                        gQnlinkingMQTTInstance = LinkMqttCreateInstance(&gQnlinkingMQTTOpt);
                        if (gQnlinkingMQTTInstance) gQnlinkingMQTTInitialized = true;
                }

                /* Init log */
                gQnlinkingMQTTLog.pQueue = CreateMessageQueue(MESSAGE_QUEUE_MAX);
                if (!gQnlinkingMQTTLog.pQueue) {
                        printf("queue malloc fail\n");
                        return;
                }
                strncpy(gQnlinkingMQTTLog.pubTopic, QNLINKING_MQTT_LOG_TOPIC, strlen(QNLINKING_MQTT_LOG_TOPIC) + 1);
                gQnlinkingMQTTLog.pInstance = gQnlinkingMQTTInstance;
                gQnlinkingMQTTLog.isUsed = true;
                pthread_create(&gQnlinkingMQTTLog.Thread, NULL, QnlinkingMQTT_LogThread, NULL);
        }

}


void QnlinkingMQTT_Cleanup()
{
        /* Clean up log */
        if (gQnlinkingMQTTLog.isUsed) {
                gQnlinkingMQTTLog.isUsed = false;
                pthread_join(gQnlinkingMQTTLog.Thread, NULL);
                DestroyMessageQueue(&gQnlinkingMQTTLog.pQueue);
                memset(gQnlinkingMQTTLog.pubTopic, 0, sizeof(gQnlinkingMQTTLog.pubTopic));
                gQnlinkingMQTTLog.pInstance = NULL;
        }

        /* Destroy mqtt instance */
        if (gQnlinkingMQTTInitialized && gQnlinkingMQTTInstance) {
                LinkMqttDestroy(gQnlinkingMQTTInstance);
                /* Uninitial libmqtt */
                LinkMqttLibCleanup();
        }
}


void QnlinkingMQTT_SendLog(int nLevel, const char * pLog)
{
        if (!pLog) {
                return;
        }
        int nLogLen = (strlen(pLog) <= MAX_LOG_LEN) ? strlen(pLog) : MAX_LOG_LEN;

        Message *pMessage = (Message *) malloc(sizeof(Message));
        char* message = (char*) malloc(nLogLen + 1);
        if ( !pMessage || !message ) {
                return;
        }
        pMessage->nMessageID = nLevel;
        memset(message, 0, nLogLen + 1);
        memcpy(message, pLog, nLogLen);
        pMessage->pMessage = message;

        if (gQnlinkingMQTTLog.pQueue->nSize == gQnlinkingMQTTLog.pQueue->nCapacity) {
                return;
        }
        SendMessage(gQnlinkingMQTTLog.pQueue, pMessage);
        return;
}

bool QnlinkingMQTT_Status()
{
        if (!gQnlinkingMQTTInstance) {
                return false;
        }
        struct MqttInstance *instance = gQnlinkingMQTTInstance;
        return instance->connected;
}
