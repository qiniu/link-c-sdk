#include "mqtt.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "control.h"
#include "control_internal.h"
#include <signal.h>

struct connect_status {
        int status;
        void* pInstance;
} connect_status;

struct connect_status Status[10];

void OnMessage(IN const void* _pInstance, IN int _nAccountId, IN const char* _pTopic, IN const char* _pMessage, IN size_t nLength)
{
	//fprintf(stderr, "OnMessage \n");
        fprintf(stderr, "%p topic %s message %s \n", _pInstance, _pTopic, _pMessage);
}

void OnEvent(IN const void* _pInstance, IN int _nAccountId, IN int _nId,  IN const char* _pReason)
{
        fprintf(stderr, "%p id %d, reason  %s \n",_pInstance, _nId, _pReason);
        struct connect_status* pStatus;
        for (int i = 0; i < 10; i++) {
                if (Status[i].pInstance == _pInstance) {
                        pStatus = &Status[i];
                        pStatus->status = _nId;
                }
        }
}

int main()
{
        struct MqttOptions options;
        signal(SIGPIPE, SIG_IGN);
        LinkMqttLibInit();
        options.pId = "test";
        options.bCleanSession = false;
        options.userInfo.nAuthenicatinMode = MQTT_AUTHENTICATION_USER;
        options.userInfo.pHostname = "39.107.247.14";
        //options.userInfo.pHostname = "47.105.118.51";
        //strcpy(options.bindaddress, "172.17.0.2");
        options.userInfo.pUsername = "1008";
        options.userInfo.pPassword = "m36SCkF6";
        options.userInfo.nPort = 1883;
        options.userInfo.pCafile = NULL;
        options.userInfo.pCertfile = NULL;
        options.userInfo.pKeyfile = NULL;
        options.nKeepalive = 10;
        options.nQos = 0;
        options.bRetain = false;
        options.callbacks.OnMessage = &OnMessage;
        options.callbacks.OnEvent = &OnEvent;
        void* instance = NULL;
        printf("try first sub \n");
        options.pId = "pubtest";
        options.userInfo.pUsername = "1002";
        options.userInfo.pPassword = "gAs2Bpg2";
        
        for (int i = 0; i < 1; ++i) {
                options.pId = "test111";
                instance = LinkMqttCreateInstance(&options);
                Status[0].pInstance = instance;
                Status[0].status = 0;
                Status[1].status = 0;
                while (Status[0].status != 3000) {
                        sleep(1);
                }
                LinkMqttSubscribe(instance, "test/#");
                options.pId = "pubtestaaa";
                void* pubInstance = LinkMqttCreateInstance(&options);
                Status[1].pInstance = pubInstance;
                while (Status[1].status != 3000) {
                        sleep(1);
                }
                for (int i = 0 ; i < 10; ++i) {
                        LinkMqttPublish(pubInstance, "test/pub", 10, "test_pub");
                        LinkMqttPublish(pubInstance, "test/pub3", 10, "test_pub3");
                }

                int session = LinkInitIOCtrl("test", "ctrl001", instance);
                int ret = LinkSendIOResponse(session, 0, "ctr", 3);
                LinkDinitIOCtrl(session);
                LinkInitLog("test", "ctrl0013333", instance);
                LinkSendLog(5, "ctrl0013333testest", 13);
                LinkDinitLog();
                sleep(10);
                Status[1].pInstance = NULL;
                Status[0].pInstance = NULL;
                Status[1].status = 0;
                Status[0].status = 0;
                LinkMqttDestroy(instance);
                LinkMqttDestroy(pubInstance);
        }
        printf("try second \n");
        options.userInfo.nAuthenicatinMode = MQTT_AUTHENTICATION_USER;
        options.userInfo.pHostname = "39.107.247.14";
        options.userInfo.nPort = 1883;
        options.userInfo.pUsername = "1002";
        options.userInfo.pPassword = "gAs2Bpg2";
        instance = LinkMqttCreateInstance(&options);
        Status[1].pInstance = instance;
        while (Status[1].status != 3000) {
                sleep(1);
        }
        int rc = 0;
        while (rc != 3000) {
                int rc3 = LinkMqttSubscribe(instance, "sensor/room3/#");
                int rc2 = LinkMqttSubscribe(instance, "sensor/room2/#");
                int rc1 = LinkMqttSubscribe(instance, "sensor/room1/#");
                printf("%d %d %d", rc1, rc2, rc3);
                if (rc1 == 3000 && rc2 == 3000 && rc3 == 3000) {
                         rc = 3000;
                }
                sleep(1);
        }
        //usleep(100000);
        for (int i = 0 ; i < 100; ++i) {
                LinkMqttPublish(instance, "sensor/room1/temperature", 10, "test1234456");
        }
        sleep(5);
        Status[1].pInstance = NULL;
        LinkMqttDestroy(instance);
        LinkMqttLibCleanup();
        printf("try third \n");
        options.userInfo.pCafile = "./test/ca.crt";
        options.userInfo.nPort = 8883;
        options.userInfo.nAuthenicatinMode = MQTT_AUTHENTICATION_USER | MQTT_AUTHENTICATION_ONEWAY_SSL;
        LinkMqttLibInit();
        Status[1].pInstance = NULL;
        Status[1].status = 0;

        while(1) {
                //LinkMqttLibInit();
                options.pId = "test12333333";
                instance = LinkMqttCreateInstance(&options);
                Status[0].pInstance = instance;
                while (Status[0].status != 3000) {
                        sleep(1);
                }
                rc = 0;
                while (rc != 3000) {
                        rc = LinkMqttSubscribe(instance, "sensor/room1/#");
                        sleep(1);
                }
                for (int i = 0; i < 100; ++ i) {
                        LinkMqttPublish(instance, "sensor/room1/temperature", 10, "1test1234456");
                        usleep(100000);
                }
                sleep(3);
                Status[0].pInstance = NULL;
                Status[0].status = 0;
                LinkMqttDestroy(instance);
                //LinkMqttLibCleanup();
        }
        LinkMqttLibCleanup();
        return 1;
}
