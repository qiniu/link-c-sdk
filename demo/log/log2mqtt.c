// Last Update:2019-01-30 17:44:20
/**
 * @file log2mqtt.c
 * @brief 
 * @author felix
 * @version 0.1.00
 * @date 2019-01-30
 */

#include <stdio.h>
#include <string.h>
#include "mqtt.h"

typedef struct {
    void *instance;    
    char *topic;
    int connected;
} MqttInfo;

static MqttInfo gMqttnfo;

#define LOGE(args...) printf(args)
#define LOGI(args...) printf(args)

static void OnMessage( const void* _pInstance, int _nAccountId, const char* _pTopic,
                const char* _pMessage, size_t nLength )
{
    printf("get message topic %s message %s\n", _pTopic, _pMessage );
}

static void OnEvent(const void* _pInstance, int _nAccountId, int _nId,  const char* _pReason )
{
    if ( !_pInstance ) {
        LOGE("check param error\n");
        return;
    }

    LOGI(" id %d reason %s \n", _nId, _pReason );
    if ( _nId == MQTT_SUCCESS && gMqttnfo.instance && _pInstance == gMqttnfo.instance ) {
        if (  gMqttnfo.topic ) {
        //    printf("instance : %p start to subscribe %s \n", _pInstance, gMqttnfo.topic);
        //    LinkMqttSubscribe( gMqttnfo.instance, gMqttnfo.topic );
            gMqttnfo.connected = 1;
        } else {
            printf("topic is NULL\n");
        }
    }

}

int MqttInit( char *_pClientId, int qos, char *_pUserName,
              char *_pPasswd, char *_pTopic, char *_pHost,
              int _nPort)
{
    if ( !_pClientId || !_pTopic || ! _pHost  ) {
        printf("%s check param error\n", __FUNCTION__ );
        goto err;
    }

    gMqttnfo.topic = _pTopic;

    LinkMqttLibInit();

    struct MqttOptions options, *ops = &options;
    memset( ops, 0, sizeof(struct MqttOptions) );
    ops->pId = _pClientId;
    ops->bCleanSession = false;
    ops->userInfo.nAuthenicatinMode = MQTT_AUTHENTICATION_NULL;
    ops->userInfo.pHostname = _pHost;
    ops->userInfo.nPort = _nPort;
    ops->userInfo.pCafile = NULL;
    ops->userInfo.pCertfile = NULL;
    ops->userInfo.pKeyfile = NULL;
    ops->nKeepalive = 15;
    ops->nQos = 0;
    ops->bRetain = false;
    ops->callbacks.OnMessage = &OnMessage;
    ops->callbacks.OnEvent = &OnEvent;
    gMqttnfo.instance = LinkMqttCreateInstance( ops );
    if ( !gMqttnfo.instance ) {
        LOGE("LinkMqttCreateInstance error\n");
        goto err;
    } else {
        LOGE("create mqtt instance, client : %s, broker : %s port : %d topic : %s\n",
             _pClientId, _pHost, _nPort, _pTopic );
    }

    return 0;

err:
    return -1;
}

int LogOverMQTT( char *msg )
{
    if ( !msg ) {
        LOGE("msg is null\n");
        goto err;
    }

    if ( gMqttnfo.connected && gMqttnfo.instance && gMqttnfo.topic ) {
        LOGI("send ==> %s\n", msg );
        int ret = LinkMqttPublish( gMqttnfo.instance, gMqttnfo.topic, strlen(msg), msg );
        if ( ret != MQTT_SUCCESS ) {
            LOGE("LinkMqttPublish fail, ret = %d\n", ret ); 
        } else {
            LOGI("send ok\n");
        }
    } else {
        LOGE("param error, gMqttnfo.connected = %d gMqttnfo.instance = %p, gMqttnfo.topic = %s\n",
             gMqttnfo.connected, gMqttnfo.instance, gMqttnfo.topic );
    }

    return 0;

err:
    return -1;
}

