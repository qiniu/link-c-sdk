#ifndef __MQTT__
#define __MQTT__

#include <stdbool.h>
#include <stddef.h>

#define IN
#define OUT

typedef enum MQTT_ERR_STATUS {
        MQTT_SUCCESS = 3000,
        MQTT_CONNECT_SUCCESS = 3000,
        MQTT_DISCONNECT_SUCCESS = 3000,
        MQTT_ERR_NOMEM,
        MQTT_ERR_PROTOCOL,
        MQTT_ERR_INVAL,
        MQTT_ERR_NO_CONN,
        MQTT_ERR_CONN_REFUSED,
        MQTT_ERR_NOT_FOUND,
        MQTT_ERR_CONN_LOST,
        MQTT_ERR_TLS,
        MQTT_ERR_PAYLOAD_SIZE,
        MQTT_ERR_NOT_SUPPORTED,
        MQTT_ERR_AUTH,
        MQTT_ERR_ACL_DENIED,
        MQTT_ERR_UNKNOWN,
        MQTT_ERR_ERRNO,
        MQTT_ERR_EAI,
        MQTT_ERR_PROXY,
        MQTT_ERR_CONN_PENDING,
        MQTT_CONTINUE,
        MQTT_RETRY,
        MQTT_QUEUE_FULL,
        MQTT_ERR_OTHERS
} MQTT_ERR_STATUS;

static const int MQTT_AUTHENTICATION_NULL = 0x0;
static const int MQTT_AUTHENTICATION_USER = 0x1;
static const int MQTT_AUTHENTICATION_ONEWAY_SSL = 0x2;
static const int MQTT_AUTHENTICATION_TWOWAY_SSL = 0x4;

typedef struct MqttOptions MqttOptions;

struct MqttCallback
{
        void (*OnMessage)(IN const void* _pInstance, IN int _nAccountId, IN const char* _pTopic, IN const char* _pMessage, IN size_t nLength);
        void (*OnEvent)(IN const void* _pInstance, IN int _nAccountId, IN int _nCode, const char* _pReason);
};

struct MqttUserInfo
{
        int nAuthenicatinMode;
        char* pUsername;
        char* pPassword;
        char* pHostname;
        int nPort;
        char* pCafile;
        char* pCertfile;
        char* pKeyfile;
        //char* pBindaddress; //not used in current time.
};

struct MqttOptions
{
        char* pId;
        int nAccountId;
        bool bCleanSession;
        struct MqttUserInfo userInfo;
        int nKeepalive;
        struct MqttCallback callbacks; // A user pointer that will be passed as an argument to any callbacks that are specified.
        int nQos;
        bool bRetain;
};

/* step 1 : Init mosquitto lib */
extern int LinkMqttLibInit();

extern int LinkMqttLibCleanup();

/* step 2 : create mosquitto instance */
extern void* LinkMqttCreateInstance(IN const struct MqttOptions* _pOption);

extern void LinkMqttDestroy(IN const void* _pInstance);

/* step 3 : mosquitto pub/sub */

extern int LinkMqttPublish(IN const void* _pInstance, IN const char* _pTopic, IN int _nPayloadlen, IN const void* _pPayload);

extern int LinkMqttSubscribe(IN const void* _pInstance, IN const char* _pTopic);

extern int LinkMqttUnsubscribe(IN const void* _pInstance, IN const char* _pTopic);

#endif
