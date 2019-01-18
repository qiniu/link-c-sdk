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
        MQTT_ERR_NOMEM = 3001,
        MQTT_ERR_PROTOCOL = 3002,
        MQTT_ERR_INVAL = 3003,
        MQTT_ERR_NO_CONN = 3004,
        MQTT_ERR_CONN_REFUSED = 3005,
        MQTT_ERR_NOT_FOUND = 3006,
        MQTT_ERR_CONN_LOST = 3007,
        MQTT_ERR_TLS = 3008,
        MQTT_ERR_PAYLOAD_SIZE = 3009,
        MQTT_ERR_NOT_SUPPORTED = 3010,
        MQTT_ERR_AUTH = 3011,
        MQTT_ERR_ACL_DENIED = 3012,
        MQTT_ERR_UNKNOWN = 3013,
        MQTT_ERR_ERRNO = 3014,
        MQTT_ERR_EAI = 3015,
        MQTT_ERR_PROXY = 3016,
        MQTT_ERR_CONN_PENDING = 3017,
        MQTT_CONTINUE = 3018,
        MQTT_RETRY = 3019,
        MQTT_QUEUE_FULL = 3020,
        MQTT_ERR_OTHERS = 3021
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


/**
 * @brief 初始化 MQTT SDK
 *
 * @return MQTT_ERR_CODE
 */
extern int LinkMqttLibInit();

/**
 * @brief 注销 MQTT SDK
 *
 * @return MQTT_ERR_CODE
 */
extern int LinkMqttLibCleanup();

/**
 * @brief 创建一个 MQTT 实例
 *
 * @param[out] pInstance 创建成功的 MQTT 实例指针
 * @param[in]  pMqttpOption 创建的 MQTT 参数
 * @return MQTT_ERR_CODE
 */
extern void* LinkMqttCreateInstance(IN const struct MqttOptions* _pOption);

/**
 * @brief 销毁一个 MQTT 实例
 *
 * @param[in] pInstance 需要销毁的MQTT实例
 * @return MQTT_ERR_CODE
 */
extern void LinkMqttDestroy(IN const void* _pInstance);

/**
 * @brief 上报 MQTT 消息
 *
 * @param[in] pInstance MQTT实 例
 * @param[in] pTopic 发布主题
 * @param[in] nPayloadlen 发布消息长度
 * @param[in] pPayload 发布消息负载
 * @return MQTT_ERR_CODE
 */
extern int LinkMqttPublish(IN const void* _pInstance, IN const char* _pTopic, IN int _nPayloadlen, IN const void* _pPayload);

/**
 * @brief 订阅 MQTT 消息
 *
 * @param[in] pInstance MQTT 实例
 * @param[in] pTopic 订阅主题
 * @return
 */
extern int LinkMqttSubscribe(IN const void* _pInstance, IN const char* _pTopic);

/**
 * 取消订阅 MQTT 消息
 *
 * @param[in] pInstance MQTT 实例
 * @param[in] pTopic 取消订阅主题
 * @return
 */
extern int LinkMqttUnsubscribe(IN const void* _pInstance, IN const char* _pTopic);

#endif
