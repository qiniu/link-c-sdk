#ifndef __WOLF_MQTT__
#define __WOLF_MQTT__

#include <stdbool.h>
#include <stddef.h>
#include "mqtt_client.h"
#include "mqttnet.h"
#include "../mqtt.h"
#include "../mqtt_internal.h"
#include "../control_internal.h"

#define MAX_MQTT_TOPIC_LEN 128
#define MAX_MQTT_MESSAGE_LEN 1024
#define MAX_BUFFER_SIZE 1024

typedef struct MQTTCtx {
        /* client and net containers */
        MqttClient client;
        MqttNet net;
	void *pInstance;
        /* temp mqtt containers */
        MqttConnect connect;
        MqttMessage lwt_msg;
        MqttSubscribe subscribe;
        MqttUnsubscribe unsubscribe;
        MqttTopic topics[10];
        MqttPublish publish;
        MqttDisconnect disconnect;

        byte *tx_buf, *rx_buf;
	char *message;
	char message_topic[MAX_MQTT_TOPIC_LEN];
        word32 cmd_timeout_ms;
	int use_tls;
	int timeoutCount;
} MQTTCtx;

int ClientOptSet(struct MqttInstance* _pInstance, struct MqttUserInfo info);

#endif
