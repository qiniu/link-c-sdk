/**
 * @file qnlinking_mqtt.h
 * @author Qiniu.com
 * @copyright 2019(c) Shanghai Qiniu Information Technologies Co., Ltd.
 * @brief qnlinking mqtt api header file
 */

#include <stdbool.h>

void QnlinkingMQTT_Init(const char * pDak, int nDakLen, const char * pDsk, int nDskLen);

void QnlinkingMQTT_Cleanup();

bool QnlinkingMQTT_Status();

void QnlinkingMQTT_SendLog(int nLevel, const char * pLog);
