#include <qiniu/io.h>
#include <qiniu/rs.h>
#include "uploader.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <pthread.h>
#include "servertime.h"
#include <time.h>
#include <curl/curl.h>
#include "fixjson.h"

size_t getDataCallback(void* buffer, size_t size, size_t n, void* rptr);

#define TS_DIVIDE_LEN 4096

enum WaitFirstFlag {
        WF_INIT,
        WF_LOCKED,
        WF_FIRST,
        WF_QUIT,
};

typedef struct _KodoUploader{
        LinkTsUploader uploader;
#ifdef LINK_STREAM_UPLOAD
        LinkCircleQueue * pQueue_;
#else
        char *pTsData;
        int nTsDataCap;
        int nTsDataLen;
#endif
        pthread_t workerId_;
        int isThreadStarted_;
        
        LinkUploadArg uploadArg;
        
        int64_t nFirstFrameTimestamp;
        int64_t nLastFrameTimestamp;
        int64_t nTsStartTimestamp;
        LinkUploadState state;
        
        int64_t getDataBytes;
        curl_off_t nLastUlnow;
        int64_t nUlnowRecTime;
        int nLowSpeedCnt;
        int isTimeoutWithData;
        
        pthread_mutex_t waitFirstMutex_;
        enum WaitFirstFlag nWaitFirstMutexLocked_;
        LinkTsStartUploadCallback tsStartUploadCallback;
        void *pTsStartUploadCallbackArg;
}KodoUploader;

static struct timespec tmResolution;
int timeoutCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
        if (ulnow == 0) {
                return 0;
        }
        
        if (tmResolution.tv_nsec == 0) {
                clock_getres(CLOCK_MONOTONIC, &tmResolution);
        }
        struct timespec tp;
        clock_gettime(CLOCK_MONOTONIC, &tp);
        int64_t nNow = (int64_t)(tp.tv_sec * 1000000000ll + tp.tv_nsec / tmResolution.tv_nsec);
        
        KodoUploader * pUploader = (KodoUploader *)clientp;
        if (pUploader->nUlnowRecTime == 0) {
                pUploader->nLastUlnow = ulnow;
                pUploader->nUlnowRecTime = nNow;
                return 0;
        }
        
        int nDiff = (int)((nNow - pUploader->nUlnowRecTime) / 1000000000);
        if (nDiff > 0) {
                //printf("%d,==========dltotal:%"PRId64" dlnow:%"PRId64" ultotal:%"PRId64" ulnow-reculnow=%"PRId64", now - lastrectime=%"PRId64"\n",
                //       pUploader->nLowSpeedCnt, dltotal, dlnow, ultotal, ulnow - pUploader->nLastUlnow, (nNow - pUploader->nUlnowRecTime)/1000000);
                if ((ulnow - pUploader->nLastUlnow) / nDiff < 1024) { //} && !pUploader->isTimeoutWithData) {
                        pUploader->nLowSpeedCnt += nDiff;
                        if (pUploader->nLowSpeedCnt > 3) {
                                LinkLogError("accumulate upload timeout:%d %d", pUploader->nLowSpeedCnt, nDiff);
                                return -1;
                        }
                }
                if (nDiff >= 10) {
                        LinkLogError("upload timeout directly:%d", nDiff); 
                        return -1;
                } else if (nDiff >= 5) {
                        if (pUploader->nLowSpeedCnt > 1) {
                                LinkLogError("half accumulate upload timeout:%d %d", pUploader->nLowSpeedCnt, nDiff);
                                return -1;
                        }
                        LinkLogWarn("accumulate2 upload timeout:%d %d", pUploader->nLowSpeedCnt, nDiff);
                        pUploader->nLowSpeedCnt = 2;
                        
                }
                pUploader->nLastUlnow = ulnow;
                pUploader->nUlnowRecTime = nNow;
        }
        return 0;
}

static const unsigned char pr2six[256] = {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
        64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 63,
        64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
};

static void Base64Decode(char *bufplain, const char *bufcoded) {
        register const unsigned char *bufin;
        register unsigned char *bufout;
        register int nprbytes;
        
        bufin = (const unsigned char *) bufcoded;
        while (pr2six[*(bufin++)] <= 63);
        nprbytes = (bufin - (const unsigned char *) bufcoded) - 1;
        
        bufout = (unsigned char *) bufplain;
        bufin = (const unsigned char *) bufcoded;
        
        while (nprbytes > 4) {
                *(bufout++) = (unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
                *(bufout++) = (unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
                *(bufout++) = (unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
                bufin += 4;
                nprbytes -= 4;
        }
        
        if (nprbytes > 1)
                *(bufout++) = (unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
        if (nprbytes > 2)
                *(bufout++) = (unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
        if (nprbytes > 3)
                *(bufout++) = (unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
        
        *(bufout++) = '\0';
}

static int getExpireDays(char * pToken)
{
        char * pPolicy = strchr(pToken, ':');
        if (pPolicy == NULL) {
                return LINK_ARG_ERROR;
        }
        pPolicy++;
        pPolicy = strchr(pPolicy, ':');
        if (pPolicy == NULL) {
                return LINK_ARG_ERROR;
        }
        
        pPolicy++; //jump :
        int len = (strlen(pPolicy) + 2) * 3 / 4 + 1;
        char *pPlain = malloc(len);
        Base64Decode(pPlain, pPolicy);
        pPlain[len - 1] = 0;
        
        char *pExpireStart = strstr(pPlain, "\"deleteAfterDays\"");
        if (pExpireStart == NULL) {
                free(pPlain);
                return 0;
        }
        pExpireStart += strlen("\"deleteAfterDays\"");
        
        char days[10] = {0};
        int nStartFlag = 0;
        int nDaysLen = 0;
        char *pDaysStrat = NULL;
        while(1) {
                if (*pExpireStart >= 0x30 && *pExpireStart <= 0x39) {
                        if (nStartFlag == 0) {
                                pDaysStrat = pExpireStart;
                                nStartFlag = 1;
                        }
                        nDaysLen++;
                }else {
                        if (nStartFlag)
                                break;
                }
                pExpireStart++;
        }
        memcpy(days, pDaysStrat, nDaysLen);
        free(pPlain);
        return atoi(days);
}

static size_t writeResult(void *resp, size_t size,  size_t nmemb,  void *pUserData) {
        char **pResp = (char **)pUserData;
        int len = size * nmemb ;
        char *respTxt = (char *)malloc(len +1);
        memcpy(respTxt, resp, len);
        
        respTxt[len] = 0;
        *pResp = respTxt;
        return len;
}

static int linkPutBuffer(const char * uphost, const char *token, const char * key, const char *data, int datasize) {
        CURL *easy = curl_easy_init();
        if (easy == NULL) {
                return LINK_NO_MEMORY;
        }
        curl_mime *mime;
        curl_mimepart *part;
        
        /* Build an HTTP form with a single field named "data", */
        mime = curl_mime_init(easy);
        if (mime == NULL) {
                curl_easy_cleanup(easy);
                return LINK_NO_MEMORY;
        }
        
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "token");
        curl_mime_data(part, token, CURL_ZERO_TERMINATED);
        
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "key");
        curl_mime_data(part, key, CURL_ZERO_TERMINATED);
        
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "file");
        curl_mime_data(part, data, datasize);
        
        char *resp = NULL;
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, writeResult);
        curl_easy_setopt(easy, CURLOPT_WRITEDATA, &resp);
        
        /* Post and send it. */
        curl_easy_setopt(easy, CURLOPT_MIMEPOST, mime);
        curl_easy_setopt(easy, CURLOPT_URL, uphost);
        CURLcode curlCode = curl_easy_perform(easy);
        
        int64_t httpCode = 0;
        int retCode = -1;
        if (curlCode != 0) { //curl error
                const char *pCurlErrMsg = curl_easy_strerror(curlCode);
                if (pCurlErrMsg != NULL) {
                        LinkLogError("upload.file :%s expsize:%d errorcode=%d errmsg={\"error\":\"%s\"}", key, datasize, curlCode, pCurlErrMsg);
                } else {
                        LinkLogError("upload.file :%s expsize:%d errorcode=%d errmsg={\"error\":\"unknown error\"}", key, datasize, curlCode);
                }
        } else {
                curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &httpCode);
                if (httpCode / 100 == 2) {
                        retCode = LINK_SUCCESS;
                        LinkLogDebug("upload.file size:exp:%d key:%s success",datasize, key);
                } else {
                        if (resp != NULL) {
                                LinkLogError("upload.file :%s expsize:%d httpcode=%d errmsg=%s", key, datasize, httpCode, resp);
                        } else {
                                LinkLogError("upload.file :%s expsize:%d httpcode=%d errmsg={not receive response}", key, datasize, httpCode);
                        }
                }
        }
        
        if (resp != NULL) {
                free(resp);
        }
        
        /* Clean-up. */
        curl_easy_cleanup(easy);
        curl_mime_free(mime);
        
        return retCode;
}

#ifdef MULTI_SEG_TEST
static int newSegCount = 0;
#endif
static void * streamUpload(void *_pOpaque)
{
        KodoUploader * pUploader = (KodoUploader *)_pOpaque;
        
        char uptoken[1280] = {0};
        char suffix[16] = {0};
        Qiniu_Client client;
        int ret = 0, freeClient = 1;
        
        LinkUploadParam param;
        memset(&param, 0, sizeof(param));
        param.pTokenBuf = uptoken;
        param.nTokenBufLen = sizeof(uptoken);
        param.pTypeBuf = suffix;
        param.nTypeBufLen = sizeof(suffix);

        
        Qiniu_Io_PutRet putRet;
        Qiniu_Io_PutExtra putExtra;
        Qiniu_Zero(putExtra);
        
        const char *upHost;
        //设置机房域名
#ifdef DISABLE_OPENSSL
        switch(pUploader->uploadArg.uploadZone) {
        case LINK_ZONE_HUABEI:
                upHost = "http://upload-z1.qiniup.com";
                Qiniu_Use_Zone_Huabei(Qiniu_False);
                break;
        case LINK_ZONE_HUANAN:
                upHost = "http://upload-z2.qiniup.com";
                Qiniu_Use_Zone_Huanan(Qiniu_False);
                break;
        case LINK_ZONE_BEIMEI:
                upHost = "http://upload-na0.qiniup.com";
                Qiniu_Use_Zone_Beimei(Qiniu_False);
                break;
        case LINK_ZONE_DONGNANYA:
                upHost = "http://upload-as0.qiniup.com";
                Qiniu_Use_Zone_Dongnanya(Qiniu_False);
                break;
        default:
                upHost = "http://upload.qiniup.com";
                Qiniu_Use_Zone_Huadong(Qiniu_False);
                break;
        }
#else
        switch(pUploader->uploadArg.uploadZone) {
        case LINK_ZONE_HUABEI:
                upHost = "https://up-z1.qiniup.com";
                Qiniu_Use_Zone_Huabei(Qiniu_True);
                break;
        case LINK_ZONE_HUANAN:
                upHost = "https://up-z2.qiniup.com";
                Qiniu_Use_Zone_Huanan(Qiniu_True);
                break;
        case LINK_ZONE_BEIMEI:
                upHost = "https://up-na0.qiniup.com";
                Qiniu_Use_Zone_Beimei(Qiniu_True);
                break;
        case LINK_ZONE_DONGNANYA:
                upHost = "https://up-as0.qiniup.com";
                Qiniu_Use_Zone_Dongnanya(Qiniu_True);
                break;
        default:
                upHost = "https://up.qiniup.com";
                Qiniu_Use_Zone_Huadong(Qiniu_True);
                break;
        }
#endif
        
        //put extra
        //putExtra.upHost="http://nbxs-gate-up.qiniu.com";
        
        char key[128] = {0};
        
        enum CircleQueuePolicy qtype = pUploader->pQueue_->GetType(pUploader->pQueue_);
        // wait for first packet
        if (pUploader->nWaitFirstMutexLocked_ == WF_LOCKED) {
                pthread_mutex_lock(&pUploader->waitFirstMutex_);
                pthread_mutex_unlock(&pUploader->waitFirstMutex_);
        }
        
        if (qtype != TSQ_APPEND && pUploader->nWaitFirstMutexLocked_ != WF_FIRST) {
                goto END;
        }
        
        ret = pUploader->uploadArg.getUploadParamCallback(pUploader->uploadArg.pGetUploadParamCallbackArg,
                                                          &param);
        if (ret != LINK_SUCCESS) {
                LinkLogError("ts up getUploadParamCallback fail:%d", ret);
                return NULL;
        }
        
        int64_t tsStartTime = pUploader->nTsStartTimestamp;
        
        
        int64_t tsDuration = pUploader->nLastFrameTimestamp - pUploader->nFirstFrameTimestamp;
        
        if (pUploader->uploadArg.nSegmentId_ == 0) {
                pUploader->uploadArg.nSegmentId_ = tsStartTime;
        }
        pUploader->uploadArg.nLastUploadTsTime_ = tsStartTime;
        if (pUploader->uploadArg.UploadSegmentIdUpadate) {
                pUploader->uploadArg.UploadSegmentIdUpadate(pUploader->uploadArg.pUploadArgKeeper_, &pUploader->uploadArg, tsStartTime,
                                                            tsStartTime + tsDuration * 1000000);
        }
        uint64_t nSegmentId = pUploader->uploadArg.nSegmentId_;
        
        int nDeleteAfterDays_ = getExpireDays(uptoken);
        memset(key, 0, sizeof(key));
        
        
        if (pUploader->tsStartUploadCallback) {
                pUploader->tsStartUploadCallback(pUploader->pTsStartUploadCallbackArg, tsStartTime / 1000000);
        }
        
        LinkUploadResult uploadResult = LINK_UPLOAD_RESULT_FAIL;
        
        Qiniu_Error error;
#ifdef LINK_STREAM_UPLOAD
        client.xferinfoData = _pOpaque;
        client.xferinfoCb = timeoutCallback;
        if (qtype == TSQ_APPEND) {
                int r, l;
                char *bufData;
                freeClient = 0;
                r = LinkGetQueueBuffer(pUploader->pQueue_, &bufData, &l);
                if (r > 0) {
                        //ts/uaid/startts/endts/segment_start_ts/expiry[/type].ts
                        if (suffix[0] != 0) {
                                sprintf(key, "ts/%s/%"PRId64"/%"PRId64"/%"PRId64"/%d/%s.ts", pUploader->uploadArg.pDeviceId_,
                                        tsStartTime / 1000000, tsStartTime / 1000000 + tsDuration, nSegmentId / 1000000, nDeleteAfterDays_, suffix);
                        } else {
                                sprintf(key, "ts/%s/%"PRId64"/%"PRId64"/%"PRId64"/%d.ts", pUploader->uploadArg.pDeviceId_,
                                        tsStartTime / 1000000, tsStartTime / 1000000 + tsDuration, nSegmentId / 1000000, nDeleteAfterDays_);
                        }
                        LinkLogDebug("upload start:%s q:%p  len:%d", key, pUploader->pQueue_, l);

                        //error = Qiniu_Io_PutBuffer(&client, &putRet, uptoken, key, bufData, nBufDataLen, &putExtra);
                        int putRet = linkPutBuffer(upHost, uptoken, key, bufData, l);
                        if (putRet == LINK_SUCCESS) {
                                uploadResult = LINK_UPLOAD_RESULT_OK;
                        }
                        if (pUploader->uploadArg.pUploadStatisticCb) {
                                pUploader->uploadArg.pUploadStatisticCb(pUploader->uploadArg.pUploadStatArg, LINK_UPLOAD_TS, uploadResult);
                        }
                        return NULL;
                } else {
                        LinkLogError("LinkGetQueueBuffer get no data:%d", r);
                        goto END;
                }
        }else {
                Qiniu_Client_InitNoAuth(&client, 1024);
                //ts/uaid/startts/fragment_start_ts/expiry.ts
                sprintf(key, "ts/%s/%"PRId64"/%"PRId64"/%d.ts", pUploader->uploadArg.pDeviceId_,
                        tsStartTime / 1000000, nSegmentId / 1000000, nDeleteAfterDays_);
                LinkLogDebug("upload start:%s q:%p", key, pUploader->pQueue_);
                error = Qiniu_Io_PutStream(&client, &putRet, uptoken, key, pUploader, -1, getDataCallback, &putExtra);
        }
#else
        error = Qiniu_Io_PutBuffer(&client, &putRet, uptoken, key, (const char*)pUploader->pTsData,
                                               pUploader->nTsDataLen, &putExtra);
#endif
        
        if (error.code != 200) {
                pUploader->state = LINK_UPLOAD_FAIL;
                if (error.code == 401) {
                        LinkLogError("upload file :%s expsize:%"PRId64" httpcode=%d errmsg=%s", key, pUploader->getDataBytes, error.code, Qiniu_Buffer_CStr(&client.b));
                } else if (error.code >= 500) {
                        const char * pFullErrMsg = Qiniu_Buffer_CStr(&client.b);
                        char errMsg[256];
                        char *pMsg = GetErrorMsg(pFullErrMsg, errMsg, sizeof(errMsg));
                        if (pMsg) {
                                LinkLogError("upload file :%s httpcode=%d errmsg={\"error\":\"%s\"}", key, error.code, pMsg);
                        }else {
                                LinkLogError("upload file :%s httpcode=%d errmsg=%s", key, error.code,
                                         pFullErrMsg);
                        }
                } else {
                        const char *pCurlErrMsg = curl_easy_strerror(error.code);
                        if (pCurlErrMsg != NULL) {
                                LinkLogError("upload file :%s expsize:%"PRId64" errorcode=%d errmsg={\"error\":\"%s\"}", key, pUploader->getDataBytes, error.code, pCurlErrMsg);
                        } else {
                                LinkLogError("upload file :%s expsize:%"PRId64" errorcode=%d errmsg={\"error\":\"unknown error\"}", key, pUploader->getDataBytes, error.code);
                        }
                }
                //debug_log(&client, error);
        } else {
                pUploader->state = LINK_UPLOAD_OK;
                LinkLogDebug("upload file size:(exp:%"PRId64" real:%"PRId64") key:%s success",
                         pUploader->getDataBytes, pUploader->nLastUlnow, key);
        }
        
        if (pUploader->state == LINK_UPLOAD_OK) {
                uploadResult = LINK_UPLOAD_RESULT_OK;
        }
                
        if (pUploader->uploadArg.pUploadStatisticCb) {
                pUploader->uploadArg.pUploadStatisticCb(pUploader->uploadArg.pUploadStatArg, LINK_UPLOAD_TS, uploadResult);
        }
        
END:
        if (freeClient)
                Qiniu_Client_Cleanup(&client);
        
        return NULL;
}

#ifdef LINK_STREAM_UPLOAD
size_t getDataCallback(void* buffer, size_t size, size_t n, void* rptr)
{
        KodoUploader * pUploader = (KodoUploader *) rptr;
        int nPopLen = 0;
        if (pUploader->isTimeoutWithData != 0) {
                pUploader->isTimeoutWithData++;
                LinkLogInfo("isTimeoutWithData:%d\n", pUploader->isTimeoutWithData);;
                return 0;
        }
        nPopLen = pUploader->pQueue_->PopWithNoOverwrite(pUploader->pQueue_, buffer, size * n);
        if (nPopLen < 0) {
                if (nPopLen == LINK_TIMEOUT) {
                        if (pUploader->nLastFrameTimestamp >= 0 &&  pUploader->nFirstFrameTimestamp >= 0) {
                                return 0;
                        }
                        LinkLogError("first pop from queue timeout:%d %"PRId64" %"PRId64"", nPopLen, pUploader->nLastFrameTimestamp, pUploader->nFirstFrameTimestamp);
                }
                return CURL_READFUNC_ABORT;
        }
        if (nPopLen == 0) {
                if (LinkIsProcStatusQuit()) {
                        return CURL_READFUNC_ABORT;
                }
                return 0;
        }
        
        int nTmp = 0;
        char *pBuf = (char *)buffer;
        while (size * n - nPopLen > 0) {
                nTmp = pUploader->pQueue_->PopWithNoOverwrite(pUploader->pQueue_, pBuf + nPopLen, size * n - nPopLen);
                if (nTmp == 0)
                        break;
                if (nTmp < 0) {
                        if (nTmp == LINK_TIMEOUT) {
                                if (pUploader->nLastFrameTimestamp >= 0 &&  pUploader->nFirstFrameTimestamp >= 0) {
                                        pUploader->isTimeoutWithData = 1;
                                        goto RET;
                                }
                                LinkLogError("next pop from queue timeout:%d %"PRId64" %"PRId64"", nTmp, pUploader->nLastFrameTimestamp, pUploader->nFirstFrameTimestamp);
                        }
                        return CURL_READFUNC_ABORT;
                }
                nPopLen += nTmp;
        }
        LinkUploaderStatInfo info;
        pUploader->pQueue_->GetStatInfo(rptr, &info);
RET:
        pUploader->getDataBytes += nPopLen;
        return nPopLen;
}

static int streamUploadStart(LinkTsUploader * _pUploader)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pUploader;
        int ret = pthread_create(&pKodoUploader->workerId_, NULL, streamUpload, _pUploader);
        if (ret == 0) {
                pKodoUploader->isThreadStarted_ = 1;
                return LINK_SUCCESS;
        } else {
                LinkLogError("start upload thread fail:%d", ret);
                return LINK_THREAD_ERROR;
        }
}

static void streamUploadStop(LinkTsUploader * _pUploader)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pUploader;
        if(pKodoUploader->nWaitFirstMutexLocked_ == WF_LOCKED) {
                pKodoUploader->nWaitFirstMutexLocked_ = WF_QUIT;
                pthread_mutex_unlock(&pKodoUploader->waitFirstMutex_);
        }
        pthread_mutex_lock(&pKodoUploader->waitFirstMutex_);
        pKodoUploader->nWaitFirstMutexLocked_ = WF_QUIT;
        pthread_mutex_unlock(&pKodoUploader->waitFirstMutex_);
        
        if (pKodoUploader->isThreadStarted_) {
                pKodoUploader->pQueue_->StopPush(pKodoUploader->pQueue_);
                pthread_join(pKodoUploader->workerId_, NULL);
                pKodoUploader->isThreadStarted_ = 0;
        }
        return;
}

static int streamPushData(LinkTsUploader *pTsUploader, const char * pData, int nDataLen)
{
        KodoUploader * pKodoUploader = (KodoUploader *)pTsUploader;
        
        int ret = pKodoUploader->pQueue_->Push(pKodoUploader->pQueue_, (char *)pData, nDataLen);
        if (pKodoUploader->pQueue_->GetType(pKodoUploader->pQueue_) != TSQ_APPEND && pKodoUploader->nWaitFirstMutexLocked_ == WF_LOCKED) {
                pKodoUploader->nWaitFirstMutexLocked_ = WF_FIRST;
                pthread_mutex_unlock(&pKodoUploader->waitFirstMutex_);
        }
        return ret;
}

static void notifyDataPrapared(LinkTsUploader *pTsUploader) {
        KodoUploader * pKodoUploader = (KodoUploader *)pTsUploader;
        if (pKodoUploader->nWaitFirstMutexLocked_ == WF_LOCKED)
                pKodoUploader->nWaitFirstMutexLocked_ = WF_FIRST;
        
        pthread_mutex_unlock(&pKodoUploader->waitFirstMutex_);
        
        return;
}

#else

static int memUploadStart(TsUploader * _pUploader)
{
        return LINK_SUCCESS;
}

static void memUploadStop(TsUploader * _pUploader)
{
        return;
}

static int memPushData(TsUploader *pTsUploader, char * pData, int nDataLen)
{
        KodoUploader * pKodoUploader = (KodoUploader *)pTsUploader;
        if (pKodoUploader->pTsData == NULL) {
                pKodoUploader->pTsData = malloc(pKodoUploader->nTsDataCap);
                pKodoUploader->nTsDataLen = 0;
        }
        if (pKodoUploader->nTsDataLen + nDataLen > pKodoUploader->nTsDataCap){
                char * tmp = malloc(pKodoUploader->nTsDataCap * 2);
                memcpy(tmp, pKodoUploader->pTsData, pKodoUploader->nTsDataLen);
                free(pKodoUploader->pTsData);
                pKodoUploader->pTsData = tmp;
                pKodoUploader->nTsDataCap *= 2;
                memcpy(tmp + pKodoUploader->nTsDataLen, pData, nDataLen);
                pKodoUploader->nTsDataLen += nDataLen;
                return nDataLen;
        }
        memcpy(pKodoUploader->pTsData + pKodoUploader->nTsDataLen, pData, nDataLen);
        pKodoUploader->nTsDataLen += nDataLen;
        return nDataLen;
}
#endif

static void getStatInfo(LinkTsUploader *pTsUploader, LinkUploaderStatInfo *_pStatInfo)
{
        KodoUploader * pKodoUploader = (KodoUploader *)pTsUploader;
#ifdef LINK_STREAM_UPLOAD
        pKodoUploader->pQueue_->GetStatInfo(pKodoUploader->pQueue_, _pStatInfo);
#else
        _pStatInfo->nLen_ = 0;
        _pStatInfo->nPopDataBytes_ = pKodoUploader->nTsDataLen;
        _pStatInfo->nPopDataBytes_ = pKodoUploader->nTsDataLen;
#endif
        return;
}

void recordTimestamp(LinkTsUploader *_pTsUploader, int64_t _nTimestamp)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pTsUploader;
        if (pKodoUploader->nFirstFrameTimestamp == -1) {
                pKodoUploader->nTsStartTimestamp = LinkGetCurrentNanosecond();
                pKodoUploader->nFirstFrameTimestamp = _nTimestamp;
                pKodoUploader->nLastFrameTimestamp = _nTimestamp;
        }
        pKodoUploader->nLastFrameTimestamp = _nTimestamp;
        return;
}

LinkUploadState getUploaderState(LinkTsUploader *_pTsUploader)
{
        KodoUploader * pKodoUploader = (KodoUploader *)_pTsUploader;
        return pKodoUploader->state;
}

int LinkNewUploader(LinkTsUploader ** _pUploader, const LinkUploadArg *_pArg, enum CircleQueuePolicy _policy, int _nMaxItemLen, int _nInitItemCount)
{
        KodoUploader * pKodoUploader = (KodoUploader *) malloc(sizeof(KodoUploader));
        if (pKodoUploader == NULL) {
                return LINK_NO_MEMORY;
        }
        
        memset(pKodoUploader, 0, sizeof(KodoUploader));
        
        int ret = pthread_mutex_init(&pKodoUploader->waitFirstMutex_, NULL);
        if (ret != 0){
                free(pKodoUploader);
                return LINK_MUTEX_ERROR;
        }
        pthread_mutex_lock(&pKodoUploader->waitFirstMutex_);
        pKodoUploader->nWaitFirstMutexLocked_ = WF_LOCKED;
#ifdef LINK_STREAM_UPLOAD
        ret = LinkNewCircleQueue(&pKodoUploader->pQueue_, 0, _policy, _nMaxItemLen, _nInitItemCount);
        if (ret != 0) {
                free(pKodoUploader);
                return ret;
        }
#else
        pKodoUploader->nTsDataCap = 1024 * 1024;
#endif
        pKodoUploader->nFirstFrameTimestamp = -1;
        pKodoUploader->nLastFrameTimestamp = -1;
        pKodoUploader->uploadArg = *_pArg;
#ifdef LINK_STREAM_UPLOAD
        pKodoUploader->uploader.UploadStart = streamUploadStart;
        pKodoUploader->uploader.UploadStop = streamUploadStop;
        pKodoUploader->uploader.Push = streamPushData;
        pKodoUploader->uploader.NotifyDataPrapared = notifyDataPrapared;
#else
        pKodoUploader->uploader.UploadStart = memUploadStart;
        pKodoUploader->uploader.UploadStop = memUploadStop;
        pKodoUploader->uploader.Push = memPushData;
#endif
        pKodoUploader->uploader.GetStatInfo = getStatInfo;
        pKodoUploader->uploader.RecordTimestamp = recordTimestamp;
        pKodoUploader->uploader.GetUploaderState = getUploaderState;
        
        
        *_pUploader = (LinkTsUploader*)pKodoUploader;
        
        return LINK_SUCCESS;
}

void LinkUploaderSetTsStartUploadCallback(LinkTsUploader * _pUploader, LinkTsStartUploadCallback cb, void *pOpaque) {
        KodoUploader * pKodoUploader = (KodoUploader *)(_pUploader);
        pKodoUploader->tsStartUploadCallback = cb;
        pKodoUploader->pTsStartUploadCallbackArg = pOpaque;
}

void LinkDestroyUploader(LinkTsUploader ** _pUploader)
{
        KodoUploader * pKodoUploader = (KodoUploader *)(*_pUploader);
        
        pthread_mutex_destroy(&pKodoUploader->waitFirstMutex_);
#ifdef LINK_STREAM_UPLOAD
        if (pKodoUploader->isThreadStarted_) {
                pthread_join(pKodoUploader->workerId_, NULL);
        }
        LinkDestroyQueue(&pKodoUploader->pQueue_);
#else
        free(pKodoUploader->pTsData);
#endif
        
        free(pKodoUploader);
        * _pUploader = NULL;
        return;
}
