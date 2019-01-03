#ifndef __IO_CTRL_INTERNAL__
#define __IO_CTRL_INTERNAL__

#define MAX_SESSION_ID  10
#define MESSAGE_QUEUE_MAX (32)

#define RESPONSE_ERROR_CODE "errorCode"
#define RESPONSE_VALUE  "value"
#define RESPONSE_ERROR_STRING "error"

#include "queue.h"

struct _LinkIOCrtlInfo
{
        char pubTopic[128];
        char subTopic[128];
        void *pInstance;
        bool isUsed;
        MessageQueue *pQueue;
}LinkIOCrtlInfo;

void OnIOCtrlMessage(IN const void* _pInstance, IN int _nAccountId, IN const char* _pTopic, IN const char* _pMessage, IN size_t nLength);

int LinkInitLog(const char *_pAppId, const char *_pEncodeDeviceName, void *_pInstance);

int LinkSendLog(int level, const char *pLog, int nLength);

void LinkDinitLog();
#endif
