
#include "transport.h"
#include <errno.h>
#include <pthread.h>
#include "queue.h"
#include "base.h"
#include "log/log.h"
#include <string.h>
#include <stdlib.h>
#include <ghttp.h>
#include <unistd.h>
#include <signal.h>

typedef struct _WssUrl {
        const char * pSchema;
        const char * pHost;
        const char *pRequestPath;
        int nPort;
}WssUrl;

typedef struct _RtmpWssParam {
        RtmpSettings rtmpSettings;
        int state;
        pthread_t workerId;
        unsigned char useSSL;
        WssUrl wssUrl;
        char *pUri;
        int interrupted;
        char *pBuf;
        int nBufSize;
        int nTimeoutInMilli;
        int64_t nPushCount;
        int64_t nDropCount;
        ghttp_request *pRequest;
        LinkCircleQueue *pQueue;
        unsigned char isQuit;
        volatile unsigned char status;
}RtmpWssParam;

struct msg {
        void *payload; /* is malloc'd */
        size_t len;
};

LinkGhttpLog ghttpLog = NULL;
void LinkGhttpSetLog(LinkGhttpLog log) {
        ghttpLog = log;
}
void LinkGhttpLogger(const char *msg) {
        if (ghttpLog != NULL)
                ghttpLog(msg);
}

static void
destroy_message(void *_msg, RtmpWssParam *pParam)
{
        struct msg *msg = (struct msg *)_msg;
        
        if (msg->payload) {
                if (pParam->rtmpSettings.freeCb)
                        pParam->rtmpSettings.freeCb(pParam->rtmpSettings.pUser,
                                                    msg->payload, msg->len);
                else
                        free(msg->payload);
                msg->payload = NULL;
                msg->len = 0;
        }
}

static int generatePushInfo(char * pBuf, int nBufSize, const char * nPushUrl, int nPushUrlLen) {
        int nLen = 0;
        
        //1 for object type(0x03); 4 for tow object key length; 10 for key 'url' and 'publish'
        //2 for bool, 3 for amf string header; 3AMF0_OBJECT_END_MARKER
        if (nBufSize + 1 + 4 + 10 + 2 + 3 + 3 < nPushUrlLen)
                return -1;
        
        pBuf[nLen++] = 0x03;
        
        pBuf[nLen++] = 0x00;
        pBuf[nLen++] = 0x03;
        memcpy(pBuf + nLen, "url", 3);
        nLen += 3;
        
        pBuf[nLen++] = 0x02;
        pBuf[nLen++] = nPushUrlLen / 256;
        pBuf[nLen++] = nPushUrlLen % 256;
        
        memcpy(pBuf + nLen, nPushUrl, nPushUrlLen);
        nLen += nPushUrlLen;
        
        pBuf[nLen++] = 0x00;
        pBuf[nLen++] = 0x07;
        memcpy(pBuf + nLen, "publish", 7);
        nLen += 7;
        pBuf[nLen++] = 0x01;
        pBuf[nLen++] = 1;
        
        pBuf[nLen++] = 0x0;
        pBuf[nLen++] = 0x0;
        pBuf[nLen++] = 0x09;
        
        return nLen;
}

static int setFastRtmpInfo(char * pBuf, int nBufSize, const RtmpWssParam *pParam) {
        int nLen = generatePushInfo(pBuf + 11, nBufSize - 11, pParam->rtmpSettings.pRtmpUrl, pParam->rtmpSettings.nRtmpUrlLen);
        if (nLen < 0) {
                return nLen;
        }
        memset(pBuf, 0, 11);
        pBuf[0] = 0x12;
        pBuf[1] = (nLen&0x00FF0000) >> 16;
        pBuf[2] = (nLen&0xFF00) >> 8;
        pBuf[3] = nLen&0xFF;
        nLen += 11;
        
        return nLen;
}


static RtmpWssParam * newRtmpWssParam(const RtmpSettings *pSettings, int size) {
        char *pBuf = (char *)malloc(size);
        if (pBuf == NULL)
                return NULL;
        
        RtmpWssParam* pParam = (RtmpWssParam *)pBuf;
        
        memset(pParam, 0, size);
        
        pParam->rtmpSettings = *pSettings;
        if (pSettings->nMaxFrameCache <= 0)
                pParam->rtmpSettings.nMaxFrameCache = 70;
        
        if (pSettings->pCertFile && pSettings->nCertFileLen > 0) {
                pParam->rtmpSettings.pCertFile = pBuf + sizeof(RtmpWssParam);
                memcpy(pParam->rtmpSettings.pCertFile, pSettings->pCertFile, pSettings->nCertFileLen);
        }
        
        pParam->rtmpSettings.pRtmpUrl = pBuf + sizeof(RtmpWssParam) + pSettings->nCertFileLen + 1;
        memcpy(pParam->rtmpSettings.pRtmpUrl, pSettings->pRtmpUrl, pSettings->nRtmpUrlLen);
        
        return pParam;
}

#define LWS_PRE 16
static void *wssWork(void *pOpaque) {
        
        sigset_t signal_mask;
        sigemptyset(&signal_mask);
        sigaddset(&signal_mask, SIGPIPE);
        pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);

        RtmpWssParam* pParam = (RtmpWssParam*)pOpaque;
       
        unsigned char * pMsg = (unsigned char*)pParam->pBuf;
        pMsg[LWS_PRE] = 0x66; //fastrtmp
        int ret = setFastRtmpInfo(pParam->pBuf + LWS_PRE + 1, pParam->nBufSize - LWS_PRE - 1, pParam);
        if (ret < 0) {
                LinkLogError("setFastRtmpInfo error, ret = %d\n", ret );
                pParam->status = FRtmpStatusError;
                return NULL;
        }
        
        pParam->pBuf[LWS_PRE+ret+1] = (ret & 0xFF000000)>>24;
        pParam->pBuf[LWS_PRE+ret+2] = (ret & 0x00FF0000)>>16;
        pParam->pBuf[LWS_PRE+ret+3] = (ret & 0x0000FF00)>>8;
        pParam->pBuf[LWS_PRE+ret+4] = ret & 0xFF;
        
        ret = ghttp_websocket_send(pParam->pRequest, (char *)pMsg + LWS_PRE, ret + 5);
        if (ret != 0) {
                if (ghttp_is_timeout(pParam->pRequest)) {
                        LinkLogError("send fastRtmpInfo fail timeout[%s]\n", ghttp_get_error(pParam->pRequest));
                } else {
                        char msg[196];
                        int len = sizeof(msg);
                        memset(msg, 0, sizeof(msg));
                        int r = ghttp_websocket_recv(pParam->pRequest, msg, len);
                        if (r == 0)
                                LinkLogError("send fastRtmpInfo fail:%s\n", msg);
                        else
                                LinkLogError("send fastRtmpInfo fail:%d %s\n", r, ghttp_get_error(pParam->pRequest));
                }
                pParam->status = FRtmpStatusError;
                return NULL;
        }
        
        struct msg amsg;
        while(pParam->status == FRtmpStatusConnectOk && !pParam->isQuit) {
                int ret = pParam->pQueue->PopWithTimeout(pParam->pQueue, (char *)(&amsg),
                                                                     sizeof(amsg), pParam->nTimeoutInMilli * 1000LL);
                if (ret == 0) {
                        LinkLogWarn("no frame in cache");
                } else if (ret < 0) {
                        if (ret == LINK_TIMEOUT) {
                                LinkLogError("pop timeout\n");
                                pParam->status = FRtmpStatusTimeout;
                        }
                        LinkLogError("pop fail:%d\n", ret);
                        pParam->status = FRtmpStatusError;
                        break;
                }
                if (amsg.payload == NULL) {
                        LinkLogError("receive null msg\n");
                        continue;
                }
                
                ret = ghttp_websocket_send(pParam->pRequest, amsg.payload+LWS_PRE, amsg.len);
                destroy_message(&amsg, pParam);
                if (ret != 0) {
                        if (ghttp_is_timeout(pParam->pRequest)) {
                                LinkLogError("send tag timeout[%s]\n", ghttp_get_error(pParam->pRequest));
                        } else {
                                char msg[196];
                                int len = sizeof(msg);
                                memset(msg, 0, sizeof(msg));
                                int r = ghttp_websocket_recv(pParam->pRequest, msg, len);
                                if (r == 0)
                                        LinkLogError("send tag fail:%s\n", msg);
                                else
                                        LinkLogError("send tag fail:%d %s\n", r, ghttp_get_error(pParam->pRequest));
                        }
                        pParam->status = FRtmpStatusError;
                        break;
                }
        }
        while(pParam->pQueue->PopWithTimeout(pParam->pQueue, (char *)(&amsg),  sizeof(amsg), 1000LL) > 0) {
                destroy_message(&amsg, pParam);
        }
        
        return NULL;
}

int FRtmpWssInit(const char *pWsUrl, int nWsUrlLen, int nTimeoutInSecs, const RtmpSettings *pSettings, FRTMPHANDLER *pHandler) {

        LinkLogInfo("pWsUrl = %s\n", pWsUrl );
        LinkLogInfo("nTimeoutInSecs = %d\n", nTimeoutInSecs );
        LinkLogInfo("pSettings->pRtmpUrl = %s\n", pSettings->pRtmpUrl );
        LinkLogInfo("pSettings->nRtmpUrlLen = %d\n", pSettings->nRtmpUrlLen );
        LinkLogInfo("pSettings->pCertFile = %s\n", pSettings->pCertFile );
        LinkLogInfo("pSettings->nCertFileLen = %d\n", pSettings->nCertFileLen );
        
        int settingSize = sizeof(RtmpWssParam) + pSettings->nCertFileLen + 1 + pSettings->nRtmpUrlLen + 1;
        int bufSize = nWsUrlLen + 4 + 2048;
        
        RtmpWssParam *pParam = newRtmpWssParam(pSettings, settingSize + bufSize);
        if (pParam == NULL) {
                LinkLogError("pParam == NULL\n");
                return -1;
        }
        
        pParam->pUri = (char *)(pParam) + settingSize;
        if (memcmp("ws://", pWsUrl, 5) == 0) {
                memcpy(pParam->pUri, "http://", 7);
                memcpy(pParam->pUri+7, pWsUrl+5, nWsUrlLen-5);
        } else if (memcmp("wss://", pWsUrl, 6) == 0) {
                memcpy(pParam->pUri, "https://", 8);
                memcpy(pParam->pUri+8, pWsUrl+6, nWsUrlLen-6);
        } else {
                memcpy(pParam->pUri, pWsUrl, nWsUrlLen);
        }
        
       
        pParam->pBuf =(char *)(pParam) + settingSize + nWsUrlLen + 4;
        pParam->nBufSize = 2048;
        pParam->nTimeoutInMilli = nTimeoutInSecs * 1000;
        
        int ret = LinkNewCircleQueue(&pParam->pQueue, 0, TSQ_FIX_LENGTH,
                                     sizeof(struct msg), pParam->rtmpSettings.nMaxFrameCache, NULL);
        if (ret < 0) {
                LinkLogError("LinkNewCircleQueue fail:%d %d\n", ret, pParam->rtmpSettings.nMaxFrameCache);
                free(pParam);
                return -2;
        }
        
        pParam->pRequest = ghttp_request_new();
        if (pParam->pRequest == NULL) {
                LinkLogError("ghttp_request_new return null");
                LinkDestroyQueue(&pParam->pQueue);
                free(pParam);
                return -3;
        }
        if (pParam->rtmpSettings.pCertFile) {
                ghttp_set_global_cert_file_path(pParam->rtmpSettings.pCertFile, NULL);
        }
        ret = ghttp_set_uri(pParam->pRequest , pParam->pUri);
        ghttp_set_timeout(pParam->pRequest, nTimeoutInSecs);
        ghttp_status status = ghttp_process_upgrade_websocket(pParam->pRequest);
        if (status == ghttp_error) {
                if (ghttp_is_timeout(pParam->pRequest)) {
                        LinkLogError("ghttp_process_upgrade_websocket timeout[%s]\n", ghttp_get_error(pParam->pRequest));
                } else {
                        LinkLogError("ghttp_process_upgrade_websocket:%s\n", ghttp_get_error(pParam->pRequest));
                }
                ghttp_request_destroy(pParam->pRequest);
                LinkDestroyQueue(&pParam->pQueue);
                free(pParam);
                return -5;
        }
        
       
        int httpCode = ghttp_status_code(pParam->pRequest);
        if (httpCode != 101) {
                ghttp_request_destroy(pParam->pRequest);
                LinkDestroyQueue(&pParam->pQueue);
                free(pParam);
                LinkLogError("http return code:%d\n", httpCode);
                return -6;
        }
        
        
        ret = pthread_create(&pParam->workerId, NULL, wssWork, (void *)pParam);
        if (ret != 0) {
                LinkLogError("pthread_create fail:%d\n", errno);
                ghttp_request_destroy(pParam->pRequest);
                LinkDestroyQueue(&pParam->pQueue);
                free(pParam);
                return -7;
        }
        pParam->status = FRtmpStatusConnectOk;
        *pHandler = (FRTMPHANDLER*)pParam;
        
        return 0;
        
}

void FRtmpWssDestroy(FRTMPHANDLER *pHandler) {
        RtmpWssParam *pParam  = (RtmpWssParam *)(*pHandler);
        
        pParam->isQuit = 1;
        if (pParam->status == FRtmpStatusConnectOk)
                pParam->status = FRtmpStatusQuit;
        
        struct msg amsg;
        memset(&amsg, 0, sizeof(amsg));
        int ret = 0;
        while(ret <= 0) {
                ret = pParam->pQueue->Push(pParam->pQueue, (char *)&amsg, sizeof(amsg));
                if (ret <= 0) {
                        LinkLogError("ts cbqueue error. notify quit:%d sleep 1 sec to retry", ret);
                        sleep(1);
                }
        }
        
        pthread_join(pParam->workerId, NULL);
        
        if (pParam->pRequest) {
                ghttp_request_destroy(pParam->pRequest);
        }
        LinkDestroyQueue(&pParam->pQueue);
        free(pParam);
        
        *pHandler = NULL;
        return;
}

int FRtmpPushTag(FRTMPHANDLER pHandler, char *pTag, int nTagLen) {
        RtmpWssParam *pParam  = (RtmpWssParam *)pHandler;
        struct msg amsg;
        memset(&amsg, 0, sizeof(amsg));

        if (pParam->status != FRtmpStatusConnectOk) {
                LinkLogError("pParam->status != FRtmpStatusConnectOk\n");
                return -1;
        }
        amsg.payload = pTag;
        amsg.len = nTagLen;
        pParam->nPushCount++;
       
        int ret = pParam->pQueue->Push(pParam->pQueue, (char *)&amsg, sizeof(amsg));
        if (ret <= 0) {
                LinkLogError("ret = %d\n", ret );
                destroy_message(&amsg, pParam);
                if (ret != LINK_Q_OVERFLOW) {
                        LinkLogError("push tag:%d\n", ret);
                        return -2;
                }
                LinkLogWarn("dropping tag\n");
        }
        
        return 0;
}

