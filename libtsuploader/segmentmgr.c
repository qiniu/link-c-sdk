#include "segmentmgr.h"
#include "resource.h"
#include "queue.h"
#include <qiniu/io.h>
#include <qiniu/rs.h>
#include <curl/curl.h>
#include "tsuploaderapi.h"
#include "fixjson.h"

typedef struct {
        int64_t nStart;
        int64_t nEnd;
        SegmentHandle handle;
        int isRestart;
}SegInfo;

typedef struct {
        int64_t nStart;
        int64_t nEnd;
        SegmentHandle handle;
        int isRestart;
        char ua[32];
        LinkSegmentGetTokenCallback getTokenCallback;
        void *pGetTokenCallbackArg;
        char mgrTokenRequestUrl[256];
        int nMgrTokenRequestUrlLen;
        int useHttps;
}Seg;

typedef struct {
        pthread_t segMgrThread_;
        LinkCircleQueue *pSegQueue_;
        Seg handles[8];
        int nQuit_;
}SegmentMgr;

static SegmentMgr segmentMgr;
static pthread_mutex_t segMgrMutex = PTHREAD_MUTEX_INITIALIZER;
static int segMgrStarted = 0;

struct MgrToken {
        char * pData;
        int nDataLen;
        int nCurlRet;
        char *pToken;
        int nTokenLen;
        char *pUrlPath;
        int nUrlPathLen;
};

static size_t writeMoveToken(void *pTokenStr, size_t size,  size_t nmemb,  void *pUserData) {
        struct MgrToken *pToken = (struct MgrToken *)pUserData;
        if (pToken->nDataLen < size * nmemb) {
                pToken->nCurlRet = -11;
                return 0;
        }
        char *pTokenStart = strstr(pTokenStr, "\"token\"");
        if (pTokenStart == NULL) {
                pToken->nCurlRet = LINK_JSON_FORMAT;
                return 0;
        }
        pTokenStart += strlen("\"token\"");
        while(*pTokenStart++ != '\"') {
        }
        
        char *pTokenEnd = strchr(pTokenStart, '\"');
        if (pTokenEnd == NULL) {
                pToken->nCurlRet = LINK_JSON_FORMAT;
                return 0;
        }
        int nTokenLen = pTokenEnd - pTokenStart;
        if (nTokenLen >= pToken->nDataLen) {
                pToken->nCurlRet = LINK_BUFFER_IS_SMALL;
        }
        memcpy(pToken->pData, pTokenStart, nTokenLen);
        pToken->pToken = pToken->pData;
        pToken->nTokenLen = nTokenLen;
        pToken->pToken[nTokenLen] = 0;
        
        //bucket
        char *pUrlStart = strstr(pTokenStr, "\"url\"");
        if (pUrlStart == NULL) {
                pToken->nCurlRet = LINK_JSON_FORMAT;
                return 0;
        }
        pUrlStart += strlen("\"url\"");
        while(*pUrlStart++ != '\"') {
        }
        
        char *pUrlEnd = strchr(pUrlStart, '\"');
        if (pUrlEnd == NULL) {
                pToken->nCurlRet = LINK_JSON_FORMAT;
                return 0;
        }
        int nUrlLen = pUrlEnd - pUrlStart;
        if ( nUrlLen >= pToken->nDataLen - nTokenLen - 2) {
                pToken->nCurlRet = LINK_BUFFER_IS_SMALL;
        }
        memcpy(pToken->pData + nTokenLen + 1, pUrlStart, nUrlLen);
        pToken->pUrlPath = pToken->pData + nTokenLen + 1;
        pToken->nUrlPathLen = nUrlLen;
        pToken->pToken[nTokenLen + nUrlLen + 1] = 0;
        
        return size * nmemb;
}

int getMoveToken(char *pBuf, int nBufLen, char *pUrl, char *pBody, struct MgrToken *pToken)
{
        if (pUrl == NULL || pBuf == NULL || nBufLen <= 10)
                return LINK_ARG_ERROR;

        //printf("=======>url:%s\n", pUrl);
        CURL *curl;
        curl_global_init(CURL_GLOBAL_ALL);
        curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeMoveToken);
        curl_easy_setopt(curl, CURLOPT_URL, pUrl);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, pBody);
        
        pToken->pData = pBuf;
        pToken->nDataLen = nBufLen;
        pToken->nCurlRet = 0;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, pToken);
        //memset(pBuf, 0, nBufLen);
        int ret =curl_easy_perform(curl);
        if (ret != 0) {
                curl_easy_cleanup(curl);
                return ret;
        }
        curl_easy_cleanup(curl);
        return pToken->nCurlRet;
}

static int doMove(const char *pUrl, const char *pToken) {
        CURL *curl;
        curl_global_init(CURL_GLOBAL_ALL);
        curl = curl_easy_init();
        
        //printf("------->url:%s\n------->token:%s\n", pUrl, pToken);
        
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
        char authHeader[1024] = {0};
        snprintf(authHeader, sizeof(authHeader), "Authorization: QBox %s", pToken);
        headers = curl_slist_append(headers, authHeader);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_URL, pUrl);

        int ret =curl_easy_perform(curl);
        if (ret != 0) {
                curl_easy_cleanup(curl);
                return ret;
        }
        curl_easy_cleanup(curl);
        return 0;
}

static void upadateSegmentFile(SegInfo segInfo) {
        
        // seg/ua/segment_start_timestamp/segment_end_timestamp
        int i, idx = -1;
        for (i = 0; i < sizeof(segmentMgr.handles) / sizeof(Seg); i++) {
                if (segmentMgr.handles[i].handle == segInfo.handle) {
                        idx = i;
                        break;
                }
        }
        if (idx < 0) {
                LinkLogWarn("wrong segment handle:%d", segInfo.handle);
                return;
        }
        
        char key[64] = {0};
        memset(key, 0, sizeof(key));
        char oldKey[64] = {0};
        memset(oldKey, 0, sizeof(oldKey));
        int isNewSeg = 1;
        if (segmentMgr.handles[idx].nStart <= 0 || segInfo.isRestart) {
                snprintf(key, sizeof(key), "seg/%s/%lld/%lld", segmentMgr.handles[idx].ua, segInfo.nStart, segInfo.nEnd);
                segmentMgr.handles[idx].nStart = segInfo.nStart;
                segmentMgr.handles[idx].nEnd = segInfo.nEnd;
        } else {
                if (segInfo.nEnd < segmentMgr.handles[idx].nEnd) {
                        LinkLogDebug("not update segment:%lld %lld", segmentMgr.handles[idx].nEnd, segInfo.nEnd);
                        return;
                }
                snprintf(oldKey, sizeof(oldKey), "seg/%s/%lld/%lld", segmentMgr.handles[idx].ua,
                         segmentMgr.handles[idx].nStart, segmentMgr.handles[idx].nEnd);
                snprintf(key, sizeof(key), "seg/%s/%lld/%lld", segmentMgr.handles[idx].ua,
                         segmentMgr.handles[idx].nStart, segInfo.nEnd);
                segmentMgr.handles[idx].nEnd = segInfo.nEnd;
                isNewSeg = 0;
        }
        
        char uptoken[1536] = {0};
        
        if(!isNewSeg) {
                struct MgrToken mgrToken;
                int nUrlLen = 0;
                nUrlLen = sprintf(uptoken, "%s", segmentMgr.handles[idx].mgrTokenRequestUrl);
                uptoken[nUrlLen] = 0;
                if (segmentMgr.handles[idx].useHttps) {
                        sprintf(uptoken + nUrlLen + 1, "{\"src\":\"%s\",\"dest\":\"%s\",\"security\":true}", oldKey, key);
                } else {
                        sprintf(uptoken + nUrlLen + 1, "{\"src\":\"%s\",\"dest\":\"%s\",\"security\":false}", oldKey, key);
                }
                int ret = getMoveToken(uptoken, sizeof(uptoken), uptoken, uptoken + nUrlLen + 1, &mgrToken);
                if (ret != 0 || mgrToken.nCurlRet != 0) {
                        LinkLogError("getMoveToken fail:%d", ret, mgrToken.nCurlRet);
                        return;
                }
                
                ret = doMove(mgrToken.pUrlPath, mgrToken.pToken);
                if (ret != 0) {
                        LinkLogError("doMove fail:%d", ret);
                }
                return;
        }
        
        int ret = segmentMgr.handles[idx].getTokenCallback(segmentMgr.handles[idx].pGetTokenCallbackArg,
                                                                      uptoken, sizeof(uptoken));
        if (ret == LINK_BUFFER_IS_SMALL) {
                LinkLogError("token buffer %d is too small. drop file:%s", sizeof(uptoken), key);
                return;
        }
        
        Qiniu_Client client;
        Qiniu_Client_InitNoAuth(&client, 1024);
        
        Qiniu_Io_PutRet putRet;
        Qiniu_Io_PutExtra putExtra;
        Qiniu_Zero(putExtra);
        
        //设置机房域名
        LinkUploadZone upZone = LinkGetuploadZone();
#ifdef DISABLE_OPENSSL
        switch(upZone) {
                case LINK_ZONE_HUABEI:
                        Qiniu_Use_Zone_Huabei(Qiniu_False);
                        break;
                case LINK_ZONE_HUANAN:
                        Qiniu_Use_Zone_Huanan(Qiniu_False);
                        break;
                case LINK_ZONE_BEIMEI:
                        Qiniu_Use_Zone_Beimei(Qiniu_False);
                        break;
                case LINK_ZONE_DONGNANYA:
                        Qiniu_Use_Zone_Dongnanya(Qiniu_False);
                        break;
                default:
                        Qiniu_Use_Zone_Huadong(Qiniu_False);
                        break;
        }
#else
        switch(upZone) {
                case LINK_ZONE_HUABEI:
                        Qiniu_Use_Zone_Huabei(Qiniu_True);
                        break;
                case LINK_ZONE_HUANAN:
                        Qiniu_Use_Zone_Huanan(Qiniu_True);
                        break;
                case LINK_ZONE_BEIMEI:
                        Qiniu_Use_Zone_Beimei(Qiniu_True);
                        break;
                case LINK_ZONE_DONGNANYA:
                        Qiniu_Use_Zone_Dongnanya(Qiniu_True);
                        break;
                default:
                        Qiniu_Use_Zone_Huadong(Qiniu_True);
                        break;
        }
#endif
        
        Qiniu_Error error;
        error = Qiniu_Io_PutBuffer(&client, &putRet, uptoken, key, "", 0, &putExtra);

        
#ifdef __ARM
        report_status( error.code, key );// add by liyq to record ts upload status
#endif
        if (error.code != 200) {
                if (error.code == 401) {
                        LinkLogError("upload picture :%s httpcode=%d errmsg=%s", key, error.code, Qiniu_Buffer_CStr(&client.b));
                } else if (error.code >= 500) {
                        const char * pFullErrMsg = Qiniu_Buffer_CStr(&client.b);
                        char errMsg[256];
                        char *pMsg = GetErrorMsg(pFullErrMsg, errMsg, sizeof(errMsg));
                        if (pMsg) {
                                LinkLogError("upload picture :%s httpcode=%d errmsg={\"error\":\"%s\"}", key, error.code, pMsg);
                        }else {
                                LinkLogError("upload picture :%s httpcode=%d errmsg=%s", key, error.code,
                                             pFullErrMsg);
                        }
                } else {
                        const char *pCurlErrMsg = curl_easy_strerror(error.code);
                        if (pCurlErrMsg != NULL) {
                                LinkLogError("upload picture :%s errorcode=%d errmsg={\"error\":\"%s\"}", key, error.code, pCurlErrMsg);
                        } else {
                                LinkLogError("upload picture :%s errorcode=%d errmsg={\"error\":\"unknown error\"}", key, error.code);
                        }
                }
        } else {
                LinkLogDebug("upload picture key:%s success", key);
        }
        
        Qiniu_Client_Cleanup(&client);
        
        return;
}

static void * segmetMgrRun(void *_pOpaque) {
        
        LinkUploaderStatInfo info = {0};
        while(!segmentMgr.nQuit_ && info.nLen_ == 0) {
                SegInfo segInfo;
                segInfo.handle = -1;
                int ret = segmentMgr.pSegQueue_->PopWithTimeout(segmentMgr.pSegQueue_, (char *)(&segInfo), sizeof(segInfo), 24 * 60 * 60);
                
                segmentMgr.pSegQueue_->GetStatInfo(segmentMgr.pSegQueue_, &info);
                
                LinkLogDebug("segment queue:%d", info.nLen_);
                if (ret == LINK_TIMEOUT) {
                        continue;
                }
                if (ret == sizeof(segInfo)) {
                        LinkLogInfo("pop segment info:%d %lld %lld\n", segInfo.handle, segInfo.nStart, segInfo.nEnd);
                        if (segInfo.handle < 0) {
                                LinkLogWarn("wrong segment handle:%d", segInfo.handle);
                        } else {
                                upadateSegmentFile(segInfo);
                        }
                }
                segmentMgr.pSegQueue_->GetStatInfo(segmentMgr.pSegQueue_, &info);
        }
        
        return NULL;
}

int LinkInitSegmentMgr() {
        pthread_mutex_lock(&segMgrMutex);
        if (segMgrStarted) {
                return LINK_SUCCESS;
        }
        int i = 0;
        for (i = 0; i < sizeof(segmentMgr.handles) / sizeof(Seg); i++) {
                segmentMgr.handles[i].handle = -1;
        }
        
        LinkCircleQueue *pQueue;
        int ret = LinkNewCircleQueue(&pQueue, 0, TSQ_FIX_LENGTH, sizeof(SegInfo), 256);
        if (ret != LINK_SUCCESS) {
                return ret;
        }
        segmentMgr.pSegQueue_ = pQueue;
        
        ret = pthread_create(&segmentMgr.segMgrThread_, NULL, segmetMgrRun, NULL);
        if (ret != 0) {
                LinkDestroyQueue(&pQueue);
                return LINK_THREAD_ERROR;
        }
        
        pthread_mutex_unlock(&segMgrMutex);
        return LINK_SUCCESS;
}

int LinkNewSegmentHandle(SegmentHandle *pSeg, SegmentArg *pArg) {
        int i = 0;
        for (i = 0; i < sizeof(segmentMgr.handles) / sizeof(Seg); i++) {
                if (segmentMgr.handles[i].handle == -1) {
                        *pSeg = i;
                        segmentMgr.handles[i].handle  = i;
                        segmentMgr.handles[i].getTokenCallback = pArg->getTokenCallback;
                        segmentMgr.handles[i].pGetTokenCallbackArg = pArg->pGetTokenCallbackArg;
                        memcpy(segmentMgr.handles[i].ua, pArg->pDeviceId, pArg->nDeviceIdLen);
                        
                        segmentMgr.handles[*pSeg].useHttps = pArg->useHttps;
                        
                        memcpy(segmentMgr.handles[*pSeg].mgrTokenRequestUrl, pArg->pMgrTokenRequestUrl, pArg->nMgrTokenRequestUrlLen);
                        sprintf(segmentMgr.handles[*pSeg].mgrTokenRequestUrl + pArg->nMgrTokenRequestUrlLen, "/%s", segmentMgr.handles[i].ua);
                        segmentMgr.handles[*pSeg].nMgrTokenRequestUrlLen = pArg->nMgrTokenRequestUrlLen + pArg->nDeviceIdLen + 1;
                        segmentMgr.handles[*pSeg].mgrTokenRequestUrl[segmentMgr.handles[*pSeg].nMgrTokenRequestUrlLen] = 0;
                        
                        return LINK_SUCCESS;
                }
        }
        return LINK_MAX_SEG;
}

void LinkReleaseSegmentHandle(SegmentHandle *pSeg) {
        if (*pSeg >= 0 && *pSeg < sizeof(segmentMgr.handles) / sizeof(SegmentHandle)) {
                segmentMgr.handles[*pSeg].handle = -1;
                segmentMgr.handles[*pSeg].nStart = 0;
                segmentMgr.handles[*pSeg].nEnd = 0;
                segmentMgr.handles[*pSeg].isRestart = 0;
                segmentMgr.handles[*pSeg].getTokenCallback = NULL;
                segmentMgr.handles[*pSeg].pGetTokenCallbackArg = NULL;
                segmentMgr.handles[*pSeg].useHttps = 0;
                segmentMgr.handles[*pSeg].nMgrTokenRequestUrlLen = 0;
                segmentMgr.handles[*pSeg].mgrTokenRequestUrl[0] = 0;
                *pSeg = -1;
        }
}

int LinkUpdateSegment(SegmentHandle seg, int64_t nStart, int64_t nEnd, int isRestart) {
        SegInfo segInfo;
        segInfo.handle = seg;
        segInfo.nStart = nStart;
        segInfo.nEnd = nEnd;
        segInfo.isRestart = isRestart;
        
        return segmentMgr.pSegQueue_->Push(segmentMgr.pSegQueue_, (char *)&segInfo, sizeof(segInfo));
}

void LinkUninitSegmentMgr() {
        segmentMgr.nQuit_ = 1;
        pthread_join(&segmentMgr.segMgrThread_, NULL);
        return;
}
