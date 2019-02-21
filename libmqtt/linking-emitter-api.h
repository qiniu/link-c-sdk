/**
 * @file linking-emitter-api.h
 * @author Qiniu.com
 * @copyright 2019(c) Shanghai Qiniu Information Technologies Co., Ltd.
 * @brief linking emitter api header file
 */

void LinkEmitter_Init(const char * pDak, int nDakLen, const char * pDsk, int nDskLen);

void LinkEmitter_Cleanup();

void LinkEmitter_SendLog(int nLevel, const char * pLog);
