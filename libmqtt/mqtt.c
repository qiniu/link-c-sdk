#ifdef WITH_WOLFMQTT
#include "wolfmqtt.h"
#endif
#ifdef WITH_MOSQUITTO
#include "mos_mqtt.h"
#endif
#include "mqtt.h"
#include "mqtt_internal.h"

static void MallocAndStrcpy(char** des, const char* src)
{
      if (src) {
              char* p = malloc(strlen(src) + 1);
              if (p) {
                      strncpy(p, src, strlen(src));
                      p[strlen(src)]= '\0';
                      *des = p;
              }
              else {
                      *des = NULL;
              }
      }
      else {
              *des = NULL;
      }
}

static void SafeFree(char* des)
{
      if (des) free(des);
}

bool InsertNode(Node* pHead, const char* val)
{
        Node* p = pHead;
        while(p->pNext) {
                p = p->pNext;
        }
        Node* pNew = malloc(sizeof(Node));
        strcpy(pNew->topic, val);
        p->pNext = pNew;
        pNew->pNext = NULL;
        return true;
}

bool DeleteNode(Node* PHead, const char * pval)
{
        int i = 0;
        Node* p = PHead;
        while (p->pNext != NULL) {
                if (strcmp(p->pNext->topic, pval) == 0) {
                        Node* temp = p->pNext;
                        p->pNext = temp->pNext;
                        free(temp);
                        return true;
               }
        }
        printf("Can't find node \n");
        return false;
}

static void ClearNode(Node* PHead)
{
        int i = 0;
        Node* p = PHead;
        while (p->pNext != NULL) {
                Node* temp = p->pNext;
                p->pNext = temp->pNext;
                free(temp);
        }
}

void OnEventCallback(struct MqttInstance* _pInstance, int rc, const char* _pStr)
{
        if (_pInstance->options.callbacks.OnEvent != NULL) {
                if (_pInstance->lastStatus < MQTT_ERR_NOMEM || rc < MQTT_ERR_NOMEM) { 
                        _pInstance->options.callbacks.OnEvent(_pInstance, _pInstance->options.nAccountId, rc, _pStr);
                }
        }
        _pInstance->lastStatus = rc;
}

static void LinkMqttInstanceInit(struct MqttInstance* _pInstance, const struct MqttOptions* _pOption)
{
        /* copy options */

        memcpy(&_pInstance->options, _pOption, sizeof(struct MqttOptions));
        MallocAndStrcpy(&_pInstance->options.pId, _pOption->pId);
        MallocAndStrcpy(&_pInstance->options.userInfo.pUsername, _pOption->userInfo.pUsername);
        MallocAndStrcpy(&_pInstance->options.userInfo.pPassword, _pOption->userInfo.pPassword);
        MallocAndStrcpy(&_pInstance->options.userInfo.pHostname, _pOption->userInfo.pHostname);
        MallocAndStrcpy(&_pInstance->options.userInfo.pCafile, _pOption->userInfo.pCafile);
        MallocAndStrcpy(&_pInstance->options.userInfo.pCertfile, _pOption->userInfo.pCertfile);
        MallocAndStrcpy(&_pInstance->options.userInfo.pKeyfile, _pOption->userInfo.pKeyfile);
        _pInstance->mosq = NULL;
        _pInstance->connected = false;
        _pInstance->status = STATUS_IDLE;
        _pInstance->isDestroying = false;
        _pInstance->pSubsribeList.pNext = NULL;
        pthread_mutex_init(&_pInstance->listMutex, NULL);
}

void * LinkMqttThread(void* _pData)
{
        int rc;

        struct MqttInstance* pInstance = (struct MqttInstance*)(_pData);

        int ret = LinkMqttInit(pInstance);
        if (ret != MQTT_SUCCESS) {
                OnEventCallback(pInstance, MQTT_ERR_INVAL, "MQTT_ERR_INVAL");
                LinkMqttDestroyInstance(pInstance);
                return NULL;
        }
        srand(time(0));
        do {
                if (!pInstance->connected && pInstance->status != STATUS_CONNECTING) {
                         pInstance->status = STATUS_CONNECTING;
                         rc = ClientOptSet(pInstance, pInstance->options.userInfo);
                         if (rc == MQTT_SUCCESS) {
                                 rc = LinkMqttConnect(pInstance);
                         }
                         if (rc != MQTT_SUCCESS) {
                                 OnEventCallback(pInstance, rc, "STATUS_CONNECT_ERROR");
                                 pInstance->status = STATUS_CONNECT_ERROR;
                         }
                } else if (pInstance->status == STATUS_CONNECTING) {
                         sleep(1);
                }
                rc = LinkMqttLoop(pInstance);
                if (rc >= MQTT_ERR_NOMEM) {
                         sleep(1);
                }
                if (rc == MQTT_ERR_CONN_LOST) {
                        sleep(3);
                        ReConnect(pInstance, MQTT_ERR_CONN_LOST);
                }
        } while (!pInstance->isDestroying);
        printf("quite !!! \n");
        if (pInstance->connected) {
                LinkMqttDisconnect(pInstance);
        }
        LinkMqttDinit(pInstance);
        LinkMqttDestroyInstance(pInstance);
        if (rc) {
                fprintf(stderr, "Error: %d\n", rc);
        }
        return NULL;
}

void LinkMqttDestroyInstance(IN const void* _pInstance)
{
        struct MqttInstance* pInstance = (struct MqttInstance*) (_pInstance);
        ClearNode(&pInstance->pSubsribeList);
        SafeFree(pInstance->options.pId);
        SafeFree(pInstance->options.userInfo.pUsername);
        SafeFree(pInstance->options.userInfo.pPassword);
        SafeFree(pInstance->options.userInfo.pHostname);
        SafeFree(pInstance->options.userInfo.pCafile);
        SafeFree(pInstance->options.userInfo.pCertfile);
        SafeFree(pInstance->options.userInfo.pKeyfile);
        pthread_mutex_destroy(&pInstance->listMutex);
        if (pInstance) free(pInstance);
}

void* LinkMqttCreateInstance(IN const struct MqttOptions* pOption)
{
        /* allocate one mosquitto instance struct */
        struct MqttInstance* pInstance = (struct MqttInstance*)malloc(sizeof(struct MqttInstance));
        if (pInstance == NULL) {
                return NULL;
        }

        LinkMqttInstanceInit(pInstance, pOption);
        pthread_t t;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&t, &attr, LinkMqttThread, pInstance);
        return pInstance;
}

void LinkMqttDestroy(IN const void* _pInstance)
{	
        struct MqttInstance* pInstance = (struct MqttInstance*)(_pInstance);
        pInstance->isDestroying = true;
}
