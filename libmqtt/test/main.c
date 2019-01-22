#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <stdarg.h>
#include "wolfssl/options.h"
#include "wolfssl/wolfcrypt/hmac.h"
#include "control.h"
#include "control_internal.h"
#include "mqtt.h"
#include "mqtt_internal.h"
#include "urlsafe_b64.h"
#include "main.h"

struct connect_status {
        int status;
        void* pInstance;
} connect_status;

struct connect_status Status[10];

struct DeviceObj gDevObj = {};

int HmacSha1(const char * pKey, int nKeyLen, const char * pInput, int nInputLen,
        char *pOutput, char *dsk);
static void GetUserName(char *username, int *len, char *dak);
static int GetSign(IN char *_pInput, IN int _nInLen, 
                OUT char *_pOutput, OUT int *_pOutLen, char *dsk);
void *DevMalloc(int size);
void DevFree(void *ptr);
static struct DeviceObj *GetDeviceObj();

#define DEFAULT_MQTT_PORT 1883
#define DEFAULT_MQTT_KEEPALIVE 15
#define DEFAULT_MQTT_PID "mqttPub"

#define ARGSBUF_LEN 256
#define DEV_LOG_ERR 1
#define DEV_LOG_DBG 4
void DevicePrint(IN int level, IN const char *fmt, ...)
{
        if (level <= DEV_LOG_DBG) {
                va_list args;
                char argsBuf[ARGSBUF_LEN] = {0};
                if (argsBuf == NULL) {
                        return;
                }
                va_start(args, fmt);
                vsprintf(argsBuf, fmt, args);
                printf("%s\n", argsBuf);
                va_end(args);

        }
}

void OnMessage(IN const void* _pInstance, IN int _nAccountId, IN const char* _pTopic, IN const char* _pMessage, IN size_t nLength)
{
        fprintf(stderr, "%p ###? topic %s message %s \n", _pInstance, _pTopic, _pMessage);
}

void OnEvent(IN const void* _pInstance, IN int _nAccountId, IN int _nId,  IN const char* _pReason)
{
        fprintf(stderr, "%p id %d, reason  %s \n",_pInstance, _nId, _pReason);
        struct connect_status* pStatus;
        struct MqttInstance *instance = NULL;
        instance = (struct MqttInstance *)_pInstance;

        for (int i = 0; i < 10; i++) {
                if (Status[i].pInstance == _pInstance) {
                        pStatus = &Status[i];
                        pStatus->status = _nId;
                        if (!strcmp(_pReason, "STATUS_CONNECT_TIMEOUT") || !strcmp(_pReason, "Error (Network)")) {
                                int unlen = 0;
                                int passlen = 0;
                                struct DeviceObj *devObj = GetDeviceObj();
                                GetUserName(instance->options.userInfo.pUsername, &unlen, devObj->sDak);
                                GetSign(instance->options.userInfo.pUsername, unlen,
                                        instance->options.userInfo.pPassword, &passlen, devObj->sDsk);
                        }
                }
        }
}

static void GetUserName(char *username, int *len, char *dak)
{
        char *query = (char *)DevMalloc(256);
        long timestamp = 0.0;

        timestamp = (long)time(NULL);
        sprintf(query, "dak=%s&timestamp=%ld&version=v1", dak, timestamp);
        *len = strlen(query); 
        memcpy(username, query, *len);
        DevFree(query);
}

int HmacSha1(const char * pKey, int nKeyLen, const char * pInput, int nInputLen,
        char *pOutput, char *dsk)
{

        Hmac hmac;
        memset(&hmac, 0, sizeof(hmac));
        int ret = 0;
        
        ret = wc_HmacSetKey(&hmac, SHA, (byte*)pKey, nKeyLen);
        if (ret != 0) {
                return -1;
        }
        
        if( (ret = wc_HmacUpdate(&hmac, (byte*)pInput, nInputLen)) != 0) {
                return -1;
        }
        
        if ((ret = wc_HmacFinal(&hmac, (byte*)pOutput)) != 0) {
                return -1;
        }

        return 0;
}

static int GetSign(IN char *_pInput, IN int _nInLen, 
                OUT char *_pOutput, OUT int *_pOutLen, char *dsk)
{
        int ret = 0;
        int sha1Len = 20;
        char sha1[256] = {};

        ret = HmacSha1(dsk, strlen(dsk), _pInput, _nInLen, sha1, &sha1Len);
        if (ret != 0) {
                DevicePrint(DEV_LOG_ERR, "HmacSha1 failed.\n");
                return -1;
        }
        int outlen = urlsafe_b64_encode(sha1, 20, _pOutput, _pOutLen);
        *_pOutLen = outlen;
        return 0;
}

void *DevMalloc(int size)
{
        return malloc(size);
}

void DevFree(void *ptr)
{
        free(ptr);
}


extern int optind;
extern char *optarg;
struct TabStrStr {
    char opt;
    char *pDescr;
};

static void AppShowUsage()
{
        struct TabStrStr parseTab[] = {
            {7, ""},
            {'f', "print this usage,"},
            {'p', "device's dak"},
            {'u', "device's dsk"},
            {'h', "mqtt host to connect to"},
            {'P', "mqtt port to connect on"},
            {'b', "keep alive second"},
            {'r', "mqtt role, p:pub  other sub"}
        };
        int column;
        int i;
        int row = (int)parseTab[0].opt;
        for (i = 1; i < row; i++) {
                DevicePrint(1, "-%c: %s\n", parseTab[i].opt, parseTab[i].pDescr);
        }

}

static void ParseArgs(int argc, char *argv[])
{
        int rc = APP_CODE_SUCCESS;
        char ch;
        if (argc < 2) {
                DevicePrint(3, "Device app options usage\n");
                AppShowUsage();
                return;
        }

        while ((ch = getopt(argc, argv, "f:p:u:h:P:b:r:")) != -1) {
                switch (ch) {
                case 'f':
                        gDevObj.stDevcfg.conf = (char *)DevMalloc(strlen(optarg) +1);
                        memcpy(gDevObj.stDevcfg.conf, optarg, strlen(optarg));
                        break;
                case 'p':
                        gDevObj.sDak = (char *)DevMalloc(strlen(optarg) + 1);
                        memcpy(gDevObj.sDak, optarg, strlen(optarg));
                        break;
                case 'u':
                        gDevObj.sDsk = (char *)DevMalloc(strlen(optarg) + 1);
                        memcpy(gDevObj.sDsk, optarg, strlen(optarg));
                        break;
                case 'h':
                        gDevObj.stDevcfg.stMqttcfg.sServer = (char *)DevMalloc(strlen(optarg) +1);
                        memcpy(gDevObj.stDevcfg.stMqttcfg.sServer, optarg, strlen(optarg));
                        break;
                case 'P':
                        gDevObj.stDevcfg.stMqttcfg.nPort = atoi(optarg);
                        break;
                case 'b':
                        gDevObj.stDevcfg.stMqttcfg.nKeepAlive = atoi(optarg);
                        break;
                case 'r':
                        DevicePrint(DEV_LOG_ERR, "-r : %s\n", optarg);
                        gDevObj.stDevcfg.stMqttcfg.enRole =
                                optarg[0] == 'p' ? MQTT_ROLE_PUB : MQTT_ROLE_SUB;
                        break;
                default :
                        return;
                }
        }
}

static struct DeviceObj *NewDeviceObj()
{
        return &gDevObj;
}

static void DelDeviceObj(struct DeviceObj *devObj)
{
        if (devObj->sDak)
                DevFree(devObj->sDak);
        if (devObj->sDsk)
                DevFree(devObj->sDsk);
        if (devObj->stDevcfg.conf)
                DevFree(devObj->stDevcfg.conf);
        if (devObj->stDevcfg.stMqttcfg.sServer)
                DevFree(devObj->stDevcfg.stMqttcfg.sServer);
        if (devObj->stDevcfg.stMqttcfg.sId)
                DevFree(devObj->stDevcfg.stMqttcfg.sId);
}

static struct DeviceObj *GetDeviceObj()
{
        return &gDevObj;
}

static struct MqttOptions *NewMqttOptions(const char *dak,
    const char *dsk, struct MqttConfig cfg)
{
        struct MqttOptions *mqttOpt = DevMalloc(sizeof(struct MqttOptions));

        mqttOpt->bCleanSession = false;
        mqttOpt->userInfo.nAuthenicatinMode = MQTT_AUTHENTICATION_USER;
        mqttOpt->userInfo.pHostname = cfg.sServer;
        mqttOpt->userInfo.nPort = cfg.nPort == 0 ? DEFAULT_MQTT_PORT : cfg.nPort;
        mqttOpt->userInfo.pCafile = NULL;
        mqttOpt->userInfo.pCertfile = NULL;
        mqttOpt->userInfo.pKeyfile = NULL;
        mqttOpt->nKeepalive = cfg.nKeepAlive == 0 ? DEFAULT_MQTT_KEEPALIVE : cfg.nKeepAlive;
        mqttOpt->nQos = 0;
        mqttOpt->bRetain = false;
        mqttOpt->callbacks.OnMessage = &OnMessage;
        mqttOpt->callbacks.OnEvent = &OnEvent;
        mqttOpt->pId = (char *)DevMalloc(strlen(DEFAULT_MQTT_PID) + 1);
        memcpy(mqttOpt->pId, DEFAULT_MQTT_PID, sizeof(DEFAULT_MQTT_PID));

        int unlen = 0;
        char username[256] = {};
        int passlen = 0;
        char pass[128] = {};
        GetUserName(username, &unlen, dak);
        GetSign(username, unlen, pass, &passlen, dsk);
        mqttOpt->userInfo.pUsername = (char *)DevMalloc(unlen +1);
        memcpy(mqttOpt->userInfo.pUsername, username, unlen);
        mqttOpt->userInfo.pPassword = (char *)DevMalloc(strlen(pass) +1);
        memcpy(mqttOpt->userInfo.pPassword, pass, strlen(pass) +1);
        return mqttOpt;

}

void DelMqttOptions(struct MqttOptions *ptr)
{
        if (ptr->userInfo.pUsername)
                DevFree(ptr->userInfo.pUsername);
        if (ptr->userInfo.pPassword)
                DevFree(ptr->userInfo.pPassword);
        if (ptr->pId)
                DevFree(ptr->pId);
        DevFree(ptr);
}

int main(int argc, char *argv[])
{
        struct MqttOptions *mqttOpt;
        struct DeviceObj *devObj = NewDeviceObj();

        ParseArgs(argc, argv);
        signal(SIGPIPE, SIG_IGN);
        LinkMqttLibInit();


        mqttOpt = NewMqttOptions(devObj->sDak, devObj->sDsk, devObj->stDevcfg.stMqttcfg);
        DevicePrint(DEV_LOG_ERR, "username:%s  password:%s\n\n\n",
                    mqttOpt->userInfo.pUsername, mqttOpt->userInfo.pPassword);
        void* pubInstance = LinkMqttCreateInstance(mqttOpt);
        Status[0].pInstance = pubInstance;
        while (Status[0].status != 3000) {
                sleep(1);
        }
        if (gDevObj.stDevcfg.stMqttcfg.enRole == MQTT_ROLE_SUB) {
                DevicePrint(DEV_LOG_ERR, "=================sub=============\n");
                LinkMqttSubscribe(pubInstance, "linking/v1/${appid}/${device}/rpc/request/+/");
        }
        while(1) {
                if (gDevObj.stDevcfg.stMqttcfg.enRole == MQTT_ROLE_SUB) {
                        sleep(1);
                        continue;
                }
                DevicePrint(DEV_LOG_ERR, "==============pub============\n");
                LinkMqttPublish(pubInstance,
                                "linking/v1/${appid}/${device}/rpc/request/test001/", 10, "test_pub");
                        sleep(1);
        }


        sleep(10);
        Status[0].pInstance = NULL;
        Status[0].status = 0;
        LinkMqttDestroy(pubInstance);

        LinkMqttLibCleanup();
        DelMqttOptions(mqttOpt);
        DelDeviceObj(devObj);
        return 1;
}
