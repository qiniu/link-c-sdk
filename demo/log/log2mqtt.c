// Last Update:2019-02-11 12:00:08
/**
 * @file log2mqtt.c
 * @brief 
 * @author felix
 * @version 0.1.00
 * @date 2019-01-30
 */

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "mqtt.h"
#include "tools/queue.h"

typedef struct {
    void *instance;    
    char *topic;
    int connected;
    Queue *q;
} MqttInfo;

static MqttInfo gMqttnfo;

#define LOGE(args...) printf(args)
#define LOGI(args...) printf(args)

static void OnMessage( const void* _pInstance, int _nAccountId, const char* _pTopic,
                const char* _pMessage, size_t nLength )
{
//    printf("[ thread id : %d ] get message topic %s message %s\n", pthread_self(), _pTopic, _pMessage );
}

static void OnEvent(const void* _pInstance, int _nAccountId, int _nId,  const char* _pReason )
{
    if ( !_pInstance ) {
        LOGE("check param error\n");
        return;
    }

    LOGI("[ thread id : %d] id %d reason %s \n", pthread_self(), _nId, _pReason );
    if ( _nId == MQTT_SUCCESS && gMqttnfo.instance && _pInstance == gMqttnfo.instance ) {
        gMqttnfo.connected = 1;
    }

}

void *LogOverMQTTTask( void *arg )
{
    (void)arg;

    for (;;) {
        if ( gMqttnfo.connected && gMqttnfo.instance && gMqttnfo.q ) {
            char msg[512] = { 0 };
            int size = 0;

            gMqttnfo.q->dequeue( gMqttnfo.q, msg, &size );
            int ret = LinkMqttPublish( gMqttnfo.instance, gMqttnfo.topic, size, msg );
            if ( ret != MQTT_SUCCESS ) {
                LOGE("LinkMqttPublish fail, ret = %d\n", ret ); 
            } else {
            }
        } else {
            sleep( 2 );
        }
    }
    return NULL;
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

    pthread_t thread;
    pthread_create( &thread, NULL, LogOverMQTTTask, NULL );
    gMqttnfo.q = NewQueue();
    if ( !gMqttnfo.q ) {
        LOGE("NewQueue fail\n");
        goto err;
    }

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
        LOGE("[ thread id : %d ] create mqtt instance, client : %s, broker : %s port : %d topic : %s\n",
             pthread_self(), _pClientId, _pHost, _nPort, _pTopic );
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

    if ( gMqttnfo.q ) {
        gMqttnfo.q->enqueue( gMqttnfo.q, msg, strlen(msg)-1 );
    } else {
        LOGE("send fail, q is null\n");
    }

    return 0;

err:
    return -1;
}

