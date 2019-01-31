/**
 * @file linking-emitter.c
 * @author Qiniu.com
 * @copyright 2019(c) Shanghai Qiniu Information Technologies Co., Ltd.
 * @brief linking emitter api c file
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/hmac.h>
#include "cJSON/cJSON.h"
#include "b64/urlsafe_b64.h"
#include "mqtt_internal.h"
#include "mqtt.h"

#define LINK_EMITTER_SERVER "mqtt.qnlinking.com"
#define LINK_EMITTER_PORT 1883
#define LINK_EMITTER_KEEPALIVE 15

static bool gLinkEmitterInitialized = false;
static void *gLinkEmitterInstance = NULL;
static struct MqttOptions gLinkEmitterMqttOpt;
static char gLinkEmitterDAK[64] = {0};
static char gLinkEmitterDSK[64] = {0};

static int LinkEmitter_HmacSha1(const char * pKey, int nKeyLen, const char * pInput, int nInputLen,
        char *pOutput, int *pOutputLen)
{
        Hmac hmac;
        memset(&hmac, 0, sizeof(hmac));
        int ret = 0;

        ret = wc_HmacSetKey(&hmac, SHA, (byte*)pKey, nKeyLen);
        if (ret != 0) {
                return LINK_MQTT_ERROR;
        }

        if( (ret = wc_HmacUpdate(&hmac, (byte*)pInput, nInputLen)) != 0) {
                return LINK_MQTT_ERROR;
        }

        if ((ret = wc_HmacFinal(&hmac, (byte*)pOutput)) != 0) {
                return LINK_MQTT_ERROR;
        }
        *pOutputLen = 20;
        return LINK_MQTT_SUCCESS;
}

static int LinkEmitter_GetUsernameSign(char *_pUsername, int *_pLen, const char *_pDak)
{
        char query[256] = {0};
        long timestamp = 0.0;
        timestamp = (long)time(NULL);
        if (!_pDak) {
                return LINK_MQTT_ERROR;
        }
        sprintf(query, "dak=%s&timestamp=%ld&version=v1", _pDak, timestamp);
        *_pLen = strlen(query);
        memcpy(_pUsername, query, *_pLen);
        return LINK_MQTT_SUCCESS;
}


static int LinkEmitter_GetPasswordSign(const char *_pInput, int _nInLen,
                char *_pOutput, int *_pOutLen, const char *_pDsk)
{
        int ret = 0;
        int sha1Len = 20;
        char sha1[256] = {0};

        if (!_pInput || !_pDsk) {
                return LINK_MQTT_ERROR;
        }
        ret = LinkEmitter_HmacSha1(_pDsk, strlen(_pDsk), _pInput, _nInLen, sha1, &sha1Len);

        if (ret != 0) {
                return LINK_MQTT_ERROR;
        }
        int outlen = urlsafe_b64_encode(sha1, 20, _pOutput, _pOutLen);
        *_pOutLen = outlen;
        return LINK_MQTT_SUCCESS;
}

static int LinkEmitter_UpdateUserPasswd(const void *_pInstance)
{
        if (_pInstance) {
                struct MqttInstance* pInstance = (struct MqttInstance*) (_pInstance);
                int nUsernameLen = 0;
                int nPasswdLen = 0;
                LinkEmitter_GetUsernameSign(pInstance->options.userInfo.pUsername, &nUsernameLen, gLinkEmitterDAK);
                LinkEmitter_GetPasswordSign(pInstance->options.userInfo.pUsername, nUsernameLen,
                                pInstance->options.userInfo.pPassword, &nPasswdLen, gLinkEmitterDSK);
                return LINK_MQTT_SUCCESS;
        }
        return LINK_MQTT_ERROR;
}


void LinkEmitter_EventCallback(const void *_pInstance, int _nAccountId, int _nId, const char *_pReason)
{
    struct MqttInstance *instance = _pInstance;

    if (gLinkEmitterInstance == _pInstance) {
        LinkEmitter_UpdateUserPasswd(_pInstance);
    }
}

void LinkEmitter_MessageCallback(const void *_pInstance, int _nAccountId, const char *_pTopic, const char *_pMessage, size_t nLength)
{
        return;
}

void LinkEmitter_Init(const char * pDak, int nDakLen, const char * pDsk, int nDskLen)
{
        /* Create mqtt instance */
        if (!gLinkEmitterInitialized) {

                /* Init libmqtt */
                LinkMqttLibInit();

                strncpy(gLinkEmitterDAK, pDak, nDakLen);
                strncpy(gLinkEmitterDSK, pDsk, nDakLen);
                memset(&gLinkEmitterMqttOpt, 0, sizeof(MqttOptions));
                gLinkEmitterMqttOpt.userInfo.nAuthenicatinMode = MQTT_AUTHENTICATION_USER;
                gLinkEmitterMqttOpt.userInfo.pHostname = LINK_EMITTER_SERVER;
                gLinkEmitterMqttOpt.userInfo.nPort = LINK_EMITTER_PORT;
                gLinkEmitterMqttOpt.nKeepalive = LINK_EMITTER_KEEPALIVE;
                gLinkEmitterMqttOpt.nQos = 1;
                gLinkEmitterMqttOpt.pId = gLinkEmitterDAK;
                gLinkEmitterMqttOpt.bRetain = false;
                gLinkEmitterMqttOpt.bCleanSession = false;
                gLinkEmitterMqttOpt.callbacks.OnEvent = LinkEmitter_EventCallback;
                gLinkEmitterMqttOpt.callbacks.OnMessage = LinkEmitter_MessageCallback;

                char MQTTusername[256] = {0};
                int nMQTTUsernameLen;
                char MQTTpassword[256] = {0};
                int nMQTTPasswordLen;
                if (LINK_MQTT_SUCCESS == LinkEmitter_GetUsernameSign(MQTTusername, &nMQTTUsernameLen, gLinkEmitterDAK)
                                && LINK_MQTT_SUCCESS == LinkEmitter_GetPasswordSign(MQTTusername, nMQTTUsernameLen, MQTTpassword, &nMQTTPasswordLen, gLinkEmitterDSK)) {
                        gLinkEmitterMqttOpt.userInfo.pUsername = MQTTusername;
                        gLinkEmitterMqttOpt.userInfo.pPassword = MQTTpassword;
                        gLinkEmitterInstance = LinkMqttCreateInstance(&gLinkEmitterMqttOpt);
                        if (gLinkEmitterInstance) gLinkEmitterInitialized = true;
                }
        }

}

void LinkEmitter_Cleanup()
{
        /* Destroy mqtt instance */
        if (gLinkEmitterInitialized && gLinkEmitterInstance) {
                LinkMqttDestroy(gLinkEmitterInstance);
                /* Uninitial libmqtt */
                LinkMqttLibCleanup();
        }
}


