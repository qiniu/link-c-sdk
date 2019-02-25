// Last Update:2019-02-22 21:12:16
/**
 * @file telnetd.c
 * @brief 
 * @author felix
 * @version 0.1.00
 * @date 2019-02-22
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "mqtt.h"
#include "queue.h"
#include "main.h"

typedef struct {
    void *pInstance;
    int connected;
    Queue *q;
} ServerInfo;

static ServerInfo gSrvInfo;
#define BUF_LEN 4096
#ifdef LOGI
    #undef LOGI
#endif
#define LOGI(args...) printf("[ %s %s %d] ", __FILE__, __FUNCTION__, __LINE__);printf(args)

static void OnEvent(const void* _pInstance, int _nAccountId, int _nId,  const char* _pReason )
{
    char topic[64] = {0 };

    if ( !gIpc.config.devId ) {
        LOGI("gIpc.config.devId not set\n");
        return;
    }
    sprintf( topic, "/%s/telnet/sub", gIpc.config.devId );
    LOGI("_pInstance %p id %d reason %s topic  %s \n", _pInstance,  _nId, _pReason, topic );
    if ( _nId == MQTT_SUCCESS ) {
        LinkMqttSubscribe( _pInstance, topic );
    }
}

static void OnMessage( const void* _pInstance, int _nAccountId, const char* _pTopic,
                const char* _pMessage, size_t nLength )
{
    LOGI("get message _pInstance %p topic %s message %s\n", _pInstance,  _pTopic, _pMessage );
    if ( gSrvInfo.pInstance == _pInstance && gSrvInfo.q ) {
        gSrvInfo.q->enqueue( gSrvInfo.q, (void *)_pMessage, nLength );
    }
}


void TelnetdProcess()
{
    struct MqttOptions options, *ops = &options;

    LinkMqttLibInit();

    if ( !gIpc.config.devId ) {
        LOGI("check topic error\n");
        return;
    }

    memset( ops, 0, sizeof(struct MqttOptions) );
    char clientId[64] = { 0 };
    sprintf( clientId, "/%s/telnetd", gIpc.config.devId );
    ops->pId = clientId;
    ops->bCleanSession = false;
    ops->userInfo.nAuthenicatinMode = MQTT_AUTHENTICATION_NULL;
    ops->userInfo.pHostname = gIpc.config.mqtt_server;
    ops->userInfo.nPort = gIpc.config.mqtt_port;
    ops->userInfo.pCafile = NULL;
    ops->userInfo.pCertfile = NULL;
    ops->userInfo.pKeyfile = NULL;
    ops->nKeepalive = 15;
    ops->nQos = 0;
    ops->bRetain = false;
    ops->callbacks.OnMessage = &OnMessage;
    ops->callbacks.OnEvent = &OnEvent;
    gSrvInfo.pInstance = LinkMqttCreateInstance( ops );
    if ( !gSrvInfo.pInstance ) {
        LOGI("LinkMqttCreateInstance error\n");
    }

    LOGI("new mqtt instance\n\t clientId : %s\n\t broker : %s\n port : %d\n\t  device id : %s\n", 
         clientId, gIpc.config.mqtt_server, gIpc.config.mqtt_port, gIpc.config.devId );

    gSrvInfo.q = NewQueue();
    if ( !gSrvInfo.q ) {
        return;
    }

    char *pbuf = (char *) malloc (BUF_LEN);
    if ( !pbuf ) {
        return;
    }
    char *p = pbuf;

    char topic[64] = { 0 };
    sprintf( topic, "/%s/telnet/pub", gIpc.config.devId );
    for (;;) {

        if ( gSrvInfo.q ) {
            char buf[1024] = { 0 };

            gSrvInfo.q->dequeue( gSrvInfo.q, buf, NULL );
            FILE *fp = popen( buf, "r");
            if ( !fp ) {
                LOGI("popen error\n");
            }
            memset( pbuf, 0, sizeof(BUF_LEN) );
            p = pbuf;
            while( fgets( p+strlen(p), BUF_LEN, fp) ) {
            }
            LOGI("%s\n", pbuf );
            pclose( fp );
            LinkMqttPublish( gSrvInfo.pInstance, topic, strlen(pbuf), pbuf  );
        }
    }

    return;
}


