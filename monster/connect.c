#include "mqtt.h"
#include "control.h"
#include "control_internal.h"
#include "connect.h"
#include "common.h"

#define DEFAULT_MQTT_PORT 1883
#define DEFAULT_MQTT_KEEPALIVE 15

#define CONNECT_CODE_SUCCESS (0)
#define CONNECT_CODE_FAIL  (-1)

struct MqttConfig {
    char *sServer;
    int nPort;
    int nKeepAlive;
    char *sId;
};

struct MqttInstance {
    void *mosq;
    struct MqttOptions options;
    int status;
    int lastStatus;
    bool connected;
    bool isDestroying;
};

struct ConnectObj gConnObj = {};
static void StartAssign(struct ConnectObj *connObj);

void MessageCallback(const void *_pInstance, int _nAccountId, const char *_pTopic, const char *_pMessage, size_t nLength)
{
	DevPrint(DEV_LOG_ERR, "%s[%d] %p topic %s message %s\n", __func__, __LINE__, _pInstance, _pTopic, _pMessage);
    printf(DEV_LOG_ERR, "%s[%d] %p topic %s message %s\n", __func__, __LINE__, _pInstance, _pTopic, _pMessage);
}

void EventCallback(const void *_pInstance, int _nAccountId, int _nId, const char *_pReason)
{
    DevPrint(DEV_LOG_ERR, "%p id %d, reason %s \n", _pInstance, _nId, _pReason);
    struct MqttInstance *instance = &gConnObj.stInstance;

    if (gConnObj.stInstance == _pInstance) {
        gConnObj.nStatus = _nId;
        if (!strcmp(_pReason, "STATUS_CONNECT_TIMEOUT")
	    || !strcmp(_pReason, "Error (Network)")) {
            int unlen = 0;
            int passlen = 0;
            GenerateUserName(instance->options.userInfo.pUsername, &unlen, gConnObj.sDak);
            GeneratePassword(instance->options.userInfo.pUsername, unlen,
	    		instance->options.userInfo.pPassword, &passlen, gConnObj.sDsk);
        }
    }
}

static struct MqttOptions *NewMqttOptions(const char *dak, const char *dsk, struct MqttConfig cfg)
{
    struct MqttOptions *mqttOpt = DevMalloc(sizeof(struct MqttOptions));

    mqttOpt->bCleanSession = false;
    mqttOpt->userInfo.nAuthenicatinMode =  MQTT_AUTHENTICATION_USER;
    mqttOpt->userInfo.pHostname = cfg.sServer;
    mqttOpt->userInfo.nPort = cfg.nPort <= 0 ? DEFAULT_MQTT_PORT : cfg.nPort;
    mqttOpt->userInfo.pCafile = NULL;
    mqttOpt->userInfo.pCertfile = NULL;
    mqttOpt->userInfo.pKeyfile = NULL;
    mqttOpt->nKeepalive = cfg.nKeepAlive <= 0 ? DEFAULT_MQTT_KEEPALIVE : cfg.nKeepAlive;
    mqttOpt->nQos = 0;
    mqttOpt->bRetain = false;
    mqttOpt->callbacks.OnMessage = &MessageCallback;
    mqttOpt->callbacks.OnEvent = &EventCallback;
    mqttOpt->pId = (char *)DevMalloc(strlen(dak) + 1);
    memset(mqttOpt->pId, 0, strlen(dak) + 1);
    memcpy(mqttOpt->pId, dak, strlen(dak));

    int unlen = 0;
    char username[256] = {};
    int passlen = 0;
    char pass[128] = {};
    GenerateUserName(username, &unlen, dak);
    GeneratePassword(username, unlen, pass, &passlen, dsk);
    printf("username:%s\n", username);
    printf("dsk:%s\n", dsk);
    printf("pass:%s\n", pass);
    mqttOpt->userInfo.pUsername = (char *)DevMalloc(unlen +1);
    memcpy(mqttOpt->userInfo.pUsername, username, unlen);
    mqttOpt->userInfo.pPassword = (char *)DevMalloc(strlen(pass) +1);
    memcpy(mqttOpt->userInfo.pPassword, pass, strlen(pass) +1);

    return mqttOpt;
}

static void DelMqttOptions(struct MqttOptions *ptr)
{
    if (ptr->userInfo.pUsername)
        DevFree(ptr->userInfo.pUsername);
    if (ptr->userInfo.pPassword)
        DevFree(ptr->userInfo.pPassword);
    if (ptr->pId)
        DevFree(ptr->pId);
    DevFree(ptr);
}

static int ConnOpen(struct ConnectObj *obj)
{
    struct MqttConfig mqttCfg = {};

    char *dak = obj->sDak;
    char *dsk = obj->sDsk;
    mqttCfg.sServer = obj->sServer;
    mqttCfg.nPort = obj->nPort;
    mqttCfg.nKeepAlive = obj->nKeepAlive;
    mqttCfg.sId = obj->sId;
    struct MqttOptions *mqttOptions = NewMqttOptions(dak, dsk, mqttCfg);
    if (mqttOptions) {
        LinkMqttLibInit();
        gConnObj.stInstance = LinkMqttCreateInstance(mqttOptions);
        while (gConnObj.nStatus != MQTT_SUCCESS) {
            sleep(1);
        }
        LinkMqttSubscribe(gConnObj.stInstance, "linking/v1/${appid}/${device}/rpc/request/+/");
        gConnObj.nSession = LinkInitIOCtrl("${appid}", "${device}", gConnObj.stInstance);
        LinkInitLog("test", "ctrl001", gConnObj.stInstance);
    }
    DelMqttOptions(mqttOptions);
    StartAssign(&gConnObj);
    return CONNECT_CODE_SUCCESS;
}

static void ConnClose(struct ConnectObj *obj)
{
    LinkDinitIOCtrl(obj->nSession);
    LinkDinitLog();
    gConnObj.nStatus = 0;
    LinkMqttDestroy(gConnObj.stInstance);
    LinkMqttLibCleanup();
}

static void ConnRecvMessage(struct ConnectObvj *obj)
{

}

static void ConnSendMessage(struct ConnectObj *obj)
{
    LinkSendLog(5, "ctrl0013333test", 13);
}

static struct ConnectOperations gConnOpt = {
    .Open = ConnOpen,
    .Close = ConnClose,
    .RecvMessage = ConnRecvMessage,
    .SendMessage = ConnSendMessage
};

struct ConnectObj  *NewConnectObj(char *dak, char *dsk, void *cfg)
{
    int ret = CONNECT_CODE_FAIL;
    struct MqttConfig  *mqttCfg = (struct MqttConfig *)cfg;

    gConnObj.sDak = dak;
    gConnObj.sDsk = dsk;
    gConnObj.sServer = mqttCfg->sServer;
    gConnObj.nPort = mqttCfg->nPort;
    gConnObj.nKeepAlive = mqttCfg->nKeepAlive;
    gConnObj.sId = mqttCfg->sId;
    gConnObj.stOpt =  &gConnOpt;

    if (gConnObj.stOpt->Open) {
        ret = gConnObj.stOpt->Open(&gConnObj);
        gConnObj.nStatus = CONNECT_OPEN;
    } else {
        gConnObj.nStatus = CONNECT_INVALID;
    }
    return &gConnObj;
}

void DelConnectObj(struct ConnectObj *obj)
{
    if (obj->stOpt->Close) {
        obj->stOpt->Close(obj);
    }
    DevPrint(DEV_LOG_ERR, "DelConnectObj %p\n");
}

static void *AssignThread(void *pData)
{
    struct ConnectObj *connObj = (struct ConnectObj *)pData;
    int ret = DEV_CODE_SUCCESS;
    int cmdParamMax = 256;
    char *cmdParam = (char *)DevMalloc(cmdParamMax);
    char *reqID = (char *)DevMalloc(128);
    int cmd = 0;

    DevPrint(DEV_LOG_ERR, "Create Assign thread.\n");
    while (connObj->nSession >= 0) {
        memset(reqID, 0, sizeof(reqID));
        ret =  LinkRecvIOCtrl(connObj->nSession, reqID, &cmd, cmdParam, &cmdParamMax, 1000);
        DevPrint(DEV_LOG_ERR, "AssignThread receiving  ret:%d...\n", ret);
        if (ret == MQTT_RETRY) {
            usleep(1000);
        } else if (ret == MQTT_SUCCESS) {
            DevPrint(DEV_LOG_ERR, "LinkRecvIOCtrl receive msg. cmd :%d\n", cmd);
            switch (cmd) {
                case MQTT_IOCTRL_CMD_SETLEVEL:
                    printf("Recv MQTT_IOCTRL_CMD_SETLEVEL set level :%s\n", cmdParam);
                    LinkSendIOResponse(connObj->nSession, reqID, 0, "Success", strlen("Success"));
                    break;
                case MQTT_IOCTRL_CMD_GETLEVEL:
                    printf("Recv MQTT_IOCTRL_CMD_GETLEVEL \n");
                    LinkSendIOResponse(connObj->nSession, reqID, 1, "Fail",  strlen("Fail"));
                    break;
                default:
                    break;
            }
        } else {
            break;
        }
    }
    DevFree(cmdParam);
    DevFree(reqID);
}

static void StartAssign(struct ConnectObj *connObj)
{
    pthread_create(&connObj->thAssign, NULL, AssignThread, connObj);
}

