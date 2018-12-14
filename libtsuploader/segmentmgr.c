#include "segmentmgr.h"
#include "resource.h"
#include "queue.h"
#include "uploader.h"
#include "fixjson.h"
#include "servertime.h"
#include "b64/urlsafe_b64.h"
#include <qupload.h>
#include "httptools.h"
#include "fixjson.h"
#include "security.h"
#include <unistd.h>

#define SEGMENT_RELEASE 1
#define SEGMENT_UPDATE 2
#define SEGMENT_QUIT 3
typedef struct {
        SegmentHandle handle;
        uint8_t nOperation;
        LinkSession session;
}SegInfo;

typedef struct {
        int64_t nStart;
        int64_t nEnd;
        SegmentHandle handle;
        int segUploadOk;
        int segReportOk;
        char bucket[LINK_MAX_BUCKET_LEN+1];
        LinkUploadParamCallback getUploadParamCallback;
        void *pGetUploadParamCallbackArg;
        UploadStatisticCallback pUploadStatisticCb;
        void *pUploadStatArg;
        int64_t nNextUpdateSegTimeInSecond;
        int64_t nLastUpdateTime;
        LinkSession tmpSession; //TODO backup report info when next report time is not arrival
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
        const char * pSegUrl;
};

static size_t writeMoveToken(void *pTokenStr, size_t size,  size_t nmemb,  void *pUserData) {
        struct MgrToken *pToken = (struct MgrToken *)pUserData;
       
        int nTokenLen = pToken->nDataLen;
        int ret = LinkGetJsonStringByKey(pTokenStr, "\"token\"", pToken->pData, &nTokenLen);
        if (ret != LINK_SUCCESS) {
                pToken->nCurlRet = ret;
                return 0;
        }
        pToken->pToken = pToken->pData;
        pToken->nTokenLen = nTokenLen;
        pToken->pToken[nTokenLen] = 0;
        
        int len = 0;
        len = snprintf(pToken->pData+nTokenLen + 1, pToken->nDataLen - nTokenLen - 1,
                 "%s%s", pToken->pSegUrl, pToken->pUrlPath+8);
        pToken->pUrlPath = pToken->pData + nTokenLen + 1;
        pToken->nUrlPathLen = len-2;
        pToken->pUrlPath[len-2] = 0;
        
        return size * nmemb;
}

int getMoveToken(char *pBuf, int nBufLen, char *pUrl, char *oldkey, char *key, struct MgrToken *pToken, const char *pRsHost)
{
        if (pUrl == NULL || pBuf == NULL || nBufLen <= 10)
                return LINK_ARG_ERROR;

        char oldkeyB64[96] = {0};
        char keyB64[96] = {0};
        urlsafe_b64_encode(oldkey, strlen(oldkey), oldkeyB64, sizeof(oldkeyB64) - 1);
        urlsafe_b64_encode(key, strlen(key), keyB64, sizeof(keyB64) - 1);
        
        char requetBody[256] = {0};
        snprintf(requetBody, sizeof(requetBody), "{\"key\":\"/move/%s/%s/force/false\"}", oldkeyB64, keyB64);
        //printf("=======>move url:%s\n", pUrl);
        
        char httpResp[1024+256];
        int nHttpRespBufLen = sizeof(httpResp);
        int nRealRespLen = 0;
        int nBodyLen = strlen(requetBody);
        int ret = LinkSimpleHttpPost(pUrl, httpResp, nHttpRespBufLen, &nRealRespLen, requetBody, nBodyLen, NULL);
        
        if (ret != LINK_SUCCESS) {
                if (ret == LINK_BUFFER_IS_SMALL) {
                        LinkLogError("buffer is small:%d %d", sizeof(httpResp), nRealRespLen);
                }
                return ret;
        }
        
        pToken->pData = pBuf;
        pToken->nDataLen = nBufLen;
        pToken->pUrlPath = requetBody;
        pToken->nUrlPathLen = strlen(requetBody);
        pToken->nCurlRet = 0;
        pToken->pSegUrl = pRsHost;
        
        if (writeMoveToken(httpResp, nRealRespLen, 1, pToken) == 0) {
                LinkLogError("maybe response format error:%s", httpResp);
                return LINK_JSON_FORMAT;
        }
        
        return LINK_SUCCESS;
}

static void updateSegmentFile(SegInfo segInfo) {
        
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
        
        char key[128] = {0};
        memset(key, 0, sizeof(key));
        char oldKey[128] = {0};
        memset(oldKey, 0, sizeof(oldKey));
        int isNewSeg = 1;
        if (segmentMgr.handles[idx].segUploadOk)
                isNewSeg = 0;
        
        LinkUploadParam param;
        memset(&param, 0, sizeof(param));
        char upHost[192] = {0};
        char app[LINK_MAX_APP_LEN+1];
        char deviceName[LINK_MAX_DEVICE_NAME_LEN+1];
        char uptoken[1536] = {0};
        
        param.pDeviceName = deviceName;
        param.nDeviceNameLen = sizeof(deviceName);
        param.pApp = app;
        param.nAppLen = sizeof(app);
        if (isNewSeg) {
                param.pTokenBuf = uptoken;
                param.nTokenBufLen = sizeof(uptoken);
                param.pUpHost = upHost;
                param.nUpHostLen = sizeof(upHost);
        } else {
                param.pSegUrl = upHost;
                param.nSegUrlLen = sizeof(upHost);
        }
        int ret = segmentMgr.handles[idx].getUploadParamCallback(segmentMgr.handles[idx].pGetUploadParamCallbackArg,
                                                                 &param, LINK_UPLOAD_CB_GETPARAM);
        if (ret != LINK_SUCCESS) {
                LinkLogError("fail to getUploadParamCallback:%d ", ret);
                return;
        }
        int64_t segStartTime = segInfo.session.nSessionStartTime / 1000000;
        int64_t endTs = segStartTime + segInfo.session.nAccSessionVideoDuration;
        if (segmentMgr.handles[idx].segUploadOk == 0) {
                snprintf(key, sizeof(key), "seg/%s/%"PRId64"/%"PRId64"", param.pDeviceName, segStartTime, endTs);

                segmentMgr.handles[idx].nStart = segStartTime;
                segmentMgr.handles[idx].nEnd = endTs;
        } else {
                if (endTs < segmentMgr.handles[idx].nEnd) {
                        LinkLogDebug("not update segment:%"PRId64" %"PRId64"", segmentMgr.handles[idx].nEnd, endTs);
                        return;
                }
                snprintf(oldKey, sizeof(oldKey), "%s:seg/%s/%"PRId64"/%"PRId64"", segmentMgr.handles[idx].bucket, param.pDeviceName,
                         segmentMgr.handles[idx].nStart, segmentMgr.handles[idx].nEnd);
                snprintf(key, sizeof(key), "%s:seg/%s/%"PRId64"/%"PRId64"", segmentMgr.handles[idx].bucket, param.pDeviceName,
                         segmentMgr.handles[idx].nStart, endTs);

                isNewSeg = 0;
        }
        
        
        LinkUploadResult uploadResult = LINK_UPLOAD_RESULT_FAIL;
        if(!isNewSeg) {
                struct MgrToken mgrToken;
                int nUrlLen = 0;
                nUrlLen = sprintf(uptoken, "http://47.105.118.51:8087/uas/%s/token/api", param.pDeviceName);
                uptoken[nUrlLen] = 0;
                ret = getMoveToken(uptoken, sizeof(uptoken), uptoken, oldKey, key, &mgrToken, upHost);
                if (ret != 0 || mgrToken.nCurlRet != 0) {
                        LinkLogError("getMoveToken fail:%d [%s]", ret, mgrToken.nCurlRet, uptoken);
                        return;
                }
                
                LinkPutret putret;
                ret = LinkMoveFile(mgrToken.pUrlPath, mgrToken.pToken, &putret);
                if (ret != 0) {
                        LinkLogError("move seg: %s to %s errorcode=%d error:%s",oldKey, key, ret, putret.error);
                } else {//http error
                        if (putret.code / 100 == 2) {
                                uploadResult = LINK_UPLOAD_RESULT_OK;
                                segmentMgr.handles[idx].nEnd = endTs;
                                LinkLogDebug("move seg:%s to %s success", oldKey, key);
                        } else {
                                if (putret.body != NULL) {
                                        LinkLogError("move seg:%s to %s httpcode=%d reqid:%s errmsg=%s",
                                                     oldKey, key, putret.code, putret.reqid, putret.body);
                                } else {
                                        LinkLogError("move seg:%s to %s httpcode=%d reqid:%s errmsg={not receive response}",
                                                     oldKey, key, putret.code, putret.reqid);
                                }
                        }
                }
                LinkFreePutret(&putret);
                
                if (segmentMgr.handles[idx].pUploadStatisticCb) {
                        segmentMgr.handles[idx].pUploadStatisticCb(segmentMgr.handles[idx].pUploadStatArg, LINK_UPLOAD_MOVE_SEG, uploadResult);
                }
                return;
        }
        
        

        int nBLen = sizeof(segmentMgr.handles[idx].bucket);
        ret = LinkGetBucketFromUptoken(uptoken, segmentMgr.handles[idx].bucket, &nBLen);
        if (ret != LINK_SUCCESS) {
                LinkLogError("get bucket from token fail");
        }
        
        LinkPutret putret;
        ret = LinkUploadBuffer("", 0, upHost, uptoken, key, NULL, 0, NULL, &putret);
       
        if (ret != 0) {
                LinkLogError("upload segment:%s errorcode=%d error:%s", key, ret, putret.error);
        } else {//http error
                if (putret.code / 100 == 2) {
                        segmentMgr.handles[idx].segUploadOk = 1;
                        uploadResult = LINK_UPLOAD_RESULT_OK;
                        LinkLogDebug("upload segment: %s success", key);
                } else {
                        if (putret.body != NULL) {
                                LinkLogError("upload segment:%s httpcode=%d reqid:%s errmsg=%s", key, putret.code, putret.reqid, putret.body);
                        } else {
                                LinkLogError("upload segment:%s httpcode=%d reqid:%s errmsg={not receive response}", key, putret.code,  putret.reqid);
                        }
                }
        }
        
        if (segmentMgr.handles[idx].pUploadStatisticCb) {
                segmentMgr.handles[idx].pUploadStatisticCb(segmentMgr.handles[idx].pUploadStatArg, LINK_UPLOAD_SEG, uploadResult);
        }
        
        return;
}

static int checkShouldReport(Seg* pSeg, LinkSession *pCurSession) {
        int64_t nNow = LinkGetCurrentNanosecond() / 1000000000LL;
        if (nNow <= pSeg->nNextUpdateSegTimeInSecond && pCurSession->nSessionEndResonCode == 0) {
                pSeg->tmpSession.nVideoGapFromLastReport += pCurSession->nVideoGapFromLastReport;
                pSeg->tmpSession.nAudioGapFromLastReport += pCurSession->nAudioGapFromLastReport;
                return 0;
        }

        pSeg->nLastUpdateTime = pSeg->nNextUpdateSegTimeInSecond; // or nNow
        if (pSeg->tmpSession.nVideoGapFromLastReport > 0)
                pCurSession->nVideoGapFromLastReport += pSeg->tmpSession.nVideoGapFromLastReport;
        if (pSeg->tmpSession.nAudioGapFromLastReport > 0)
                pCurSession->nAudioGapFromLastReport += pSeg->tmpSession.nAudioGapFromLastReport;
        pSeg->tmpSession.nVideoGapFromLastReport = 0;
        pSeg->tmpSession.nAudioGapFromLastReport = 0;
        return 1;
}

/*
 {
 "session": "<session>", // 切片会话
 "start": <startTimestamp>, // 切片会话的开始时间戳，毫秒
 "current": <currentTimestamp>, // 当前时间戳, 毫秒
 "sequence": <sequence>, // 最新的切片序列号
 "vd": <videoDurationMS>, // 距离前一次上报的切片视频内容时长, 毫秒
 "ad": <audioDurationMS>, // 距离前一次上报的切片音频内容时长, 毫秒
 "tvd": <totalVideoDuration>, // 本次切片会话的视频总时长，毫秒
 "tad": <totalAudioDuration>, // 本次切片会话的音频总时长，毫秒
 "end": "<endTimestamp>", // 切片会话结束时间戳,毫秒，如果会话没结束不需要本字段
 "endReason": <endReason> // 切片会话结束的原因，如果会话没结束不需要本字段
 }
 */
static int reportSegInfo(SegInfo *pSegInfo, int idx) {
        char buffer[1024];
        char *body = buffer + 512;
        int nReportHostLen = 512;
        LinkSession *s = &pSegInfo->session;
        memset(buffer, 0, sizeof(buffer));
        if (s->nSessionEndResonCode != 0) {
                const char * reason = "timeout";
                if (s->nSessionEndResonCode == 1)
                        reason = "normal";
                else if (s->nSessionEndResonCode == 3)
                        reason = "force";
                sprintf(body, "{ \"session\": \"%s\", \"start\": %"PRId64", \"current\": %"PRId64", \"sequence\": %"PRId64","
                        " \"vd\": %"PRId64", \"ad\": %"PRId64", \"tvd\": %"PRId64", \"tad\": %"PRId64", \"end\":"
                        " %"PRId64", \"endReason\": \"%s\" }",
                        s->sessionId, s->nSessionStartTime, LinkGetCurrentNanosecond()/1000000LL, s->nTsSequenceNumber,
                        s->nVideoGapFromLastReport, s->nAudioGapFromLastReport,
                        s->nAccSessionVideoDuration, s->nAccSessionAudioDuration, s->nSessionEndTime, reason);
        } else {
                sprintf(body, "{ \"session\": \"%s\", \"start\": %"PRId64", \"current\": %"PRId64", \"sequence\": %"PRId64","
                        " \"vd\": %"PRId64", \"ad\": %"PRId64", \"tvd\": %"PRId64", \"tad\": %"PRId64"}",
                        s->sessionId, s->nSessionStartTime, LinkGetCurrentNanosecond()/1000000LL, s->nTsSequenceNumber,
                        s->nVideoGapFromLastReport, s->nAudioGapFromLastReport,
                        s->nAccSessionVideoDuration, s->nAccSessionAudioDuration);
        }
        LinkLogDebug("%s\n", body);
#ifndef LINK_USE_OLD_NAME
        
        LinkUploadParam param;
        memset(&param, 0, sizeof(param));
        char *reportHost = buffer;
        char ak[41];
        char sk[41];
        
        param.pSegUrl = reportHost;
        param.nSegUrlLen = nReportHostLen;
        param.pAk = ak;
        param.nAkLen = sizeof(ak);
        param.pSk = sk;
        param.nSkLen = sizeof(sk);

        int ret = segmentMgr.handles[idx].getUploadParamCallback(segmentMgr.handles[idx].pGetUploadParamCallbackArg,
                                                                 &param, LINK_UPLOAD_CB_GETPARAM);
        
        
        const char *pInput = strchr(reportHost+8, '/');
        if (pInput == NULL) {
                pInput = reportHost + param.nSegUrlLen;
        }
        reportHost[param.nSegUrlLen] = '\n';
        char *pOutput = reportHost + param.nSegUrlLen + 2;
        int nOutputLen = nReportHostLen - param.nSegUrlLen  - 2;
        
        ret = HmacSha1(sk, param.nSkLen, pInput, strlen(pInput), pOutput, &nOutputLen);
        if (ret != LINK_SUCCESS) {
                return ret;
        }
        reportHost[param.nSegUrlLen] = 0;
        
        char *pToken = pOutput + nOutputLen;
        int nTokenOffset = snprintf(pToken, 42, "%s:", ak);
        int nB64Len = urlsafe_b64_encode(pOutput, nOutputLen, pToken + nTokenOffset, 30);
        
        
        
        char resp[256];
        memset(resp, 0, sizeof(resp));
        int respLen = sizeof(resp);

        ret = LinkSimpleHttpPostWithToken(reportHost, resp, sizeof(resp), &respLen, body, strlen(body), "application/json",
                                    pToken, nTokenOffset+nB64Len);
        if (ret != LINK_SUCCESS) {
                LinkLogError("report session info fail:%d", ret);
                return ret;
        }
        
        int nextReportTime = LinkGetJsonIntByKey(resp, "\"ttl\"");
        if (nextReportTime > 0) {
                segmentMgr.handles[idx].nNextUpdateSegTimeInSecond = nextReportTime;
        }
#endif
        
        return LINK_SUCCESS;
}

static void handleReportSegInfo(SegInfo *pSegInfo) {
        int i, idx = -1;
        for (i = 0; i < sizeof(segmentMgr.handles) / sizeof(Seg); i++) {
                if (segmentMgr.handles[i].handle == pSegInfo->handle) {
                        idx = i;
                        break;
                }
        }
        if (idx < 0) {
                LinkLogWarn("wrong segment handle:%d", pSegInfo->handle);
                return;
        }
        int ret;
        if (pSegInfo->session.isNewSessionStarted || pSegInfo->session.nSessionEndResonCode != 0) {
                ret = reportSegInfo(pSegInfo, idx);
                if (ret == LINK_SUCCESS)
                        segmentMgr.handles[idx].segReportOk = 1;
                return;
        }
        if (segmentMgr.handles[idx].segReportOk) {
                if (!checkShouldReport(&segmentMgr.handles[idx], &pSegInfo->session)) {
                        return;
                }
        }
        
        ret = reportSegInfo(pSegInfo, idx);
        if (ret == LINK_SUCCESS)
                segmentMgr.handles[idx].segReportOk = 1;
        
        return;
}

static void linkReleaseSegmentHandle(SegmentHandle seg) {
        pthread_mutex_lock(&segMgrMutex);
        if (seg >= 0 && seg < sizeof(segmentMgr.handles) / sizeof(SegmentHandle)) {
                segmentMgr.handles[seg].handle = -1;
                segmentMgr.handles[seg].nStart = 0;
                segmentMgr.handles[seg].nEnd = 0;
                segmentMgr.handles[seg].getUploadParamCallback = NULL;
                segmentMgr.handles[seg].pGetUploadParamCallbackArg = NULL;
                segmentMgr.handles[seg].bucket[0] = 0;
                segmentMgr.handles[seg].segUploadOk = 0;
                segmentMgr.handles[seg].nLastUpdateTime = 0;
                segmentMgr.handles[seg].nNextUpdateSegTimeInSecond = 0;
        }
        pthread_mutex_unlock(&segMgrMutex);
        return;
}

static void * segmetMgrRun(void *_pOpaque) {
        
        LinkUploaderStatInfo info = {0};
        while(!segmentMgr.nQuit_ || info.nLen_ != 0) {
                SegInfo segInfo = {0};
                segInfo.handle = -1;
                int ret = segmentMgr.pSegQueue_->PopWithTimeout(segmentMgr.pSegQueue_, (char *)(&segInfo), sizeof(segInfo), 24 * 60 * 60);
                
                segmentMgr.pSegQueue_->GetStatInfo(segmentMgr.pSegQueue_, &info);
                LinkLogDebug("segment queue:%d", info.nLen_);
                if (ret <= 0) {
                        if (ret != LINK_TIMEOUT) {
                                LinkLogError("seg queue error. pop:%d", ret);
                        }
                        continue;
                }
                if (ret == sizeof(segInfo)) {
                        LinkLogDebug("pop segment info:%d\n", segInfo.handle);
                        if (segInfo.handle < 0) {
                                LinkLogWarn("wrong segment handle:%d", segInfo.handle);
                        } else {
                                if (segInfo.nOperation == SEGMENT_RELEASE) {
                                        linkReleaseSegmentHandle(segInfo.handle);
                                } else if (segInfo.nOperation == SEGMENT_UPDATE) {
#ifdef LINK_USE_OLD_NAME
                                        updateSegmentFile(segInfo);
#endif
                                        handleReportSegInfo(&segInfo);
                                } else if (segInfo.nOperation == SEGMENT_QUIT) {
                                        continue;
                                }
                        }
                }
        }
        
        return NULL;
}

int LinkInitSegmentMgr() {
        pthread_mutex_lock(&segMgrMutex);
        if (segMgrStarted) {
                pthread_mutex_unlock(&segMgrMutex);
                return LINK_SUCCESS;
        }
        int i = 0;
        for (i = 0; i < sizeof(segmentMgr.handles) / sizeof(Seg); i++) {
                segmentMgr.handles[i].handle = -1;
        }
        
        LinkCircleQueue *pQueue;
        int ret = LinkNewCircleQueue(&pQueue, 1, TSQ_FIX_LENGTH, sizeof(SegInfo), 32);
        if (ret != LINK_SUCCESS) {
                pthread_mutex_unlock(&segMgrMutex);
                return ret;
        }
        segmentMgr.pSegQueue_ = pQueue;
        
        ret = pthread_create(&segmentMgr.segMgrThread_, NULL, segmetMgrRun, NULL);
        if (ret != 0) {
                LinkDestroyQueue(&pQueue);
                pthread_mutex_unlock(&segMgrMutex);
                return LINK_THREAD_ERROR;
        }
        segMgrStarted = 1;
        pthread_mutex_unlock(&segMgrMutex);
        return LINK_SUCCESS;
}

int LinkNewSegmentHandle(SegmentHandle *pSeg, const SegmentArg *pArg) {
       pthread_mutex_lock(&segMgrMutex);
        if (!segMgrStarted) {
                pthread_mutex_unlock(&segMgrMutex);
                return LINK_NOT_INITED;
        }
        int i = 0;
        for (i = 0; i < sizeof(segmentMgr.handles) / sizeof(Seg); i++) {
                if (segmentMgr.handles[i].handle == -1) {
                        *pSeg = i;
                        segmentMgr.handles[i].handle  = i;
                        segmentMgr.handles[i].getUploadParamCallback = pArg->getUploadParamCallback;
                        segmentMgr.handles[i].pGetUploadParamCallbackArg = pArg->pGetUploadParamCallbackArg;
                        
                        segmentMgr.handles[i].pUploadStatisticCb = pArg->pUploadStatisticCb;
                        segmentMgr.handles[i].pUploadStatArg = pArg->pUploadStatArg;
                        segmentMgr.handles[i].nNextUpdateSegTimeInSecond = 0;
                        memset(&segmentMgr.handles[i].tmpSession, 0, sizeof(LinkSession));
                        pthread_mutex_unlock(&segMgrMutex);
                        return LINK_SUCCESS;
                }
        }
        pthread_mutex_unlock(&segMgrMutex);
        return LINK_MAX_SEG;
}

void LinkReleaseSegmentHandle(SegmentHandle *pSeg) {
        if (*pSeg < 0 || !segMgrStarted) {
                return;
        }
        SegInfo segInfo;
        segInfo.handle = *pSeg;
        segInfo.nOperation = SEGMENT_RELEASE;
        *pSeg = -1;
        
        int ret = 0;
        while(ret <= 0) {
                ret = segmentMgr.pSegQueue_->Push(segmentMgr.pSegQueue_, (char *)&segInfo, sizeof(segInfo));
                if (ret <= 0) {
                        LinkLogError("seg queue error. release:%d sleep 1 sec to retry", ret);
                        sleep(1);
                }
        }
}

int LinkUpdateSegment(SegmentHandle seg, const LinkSession *pSession) {
        SegInfo segInfo;
        segInfo.handle = seg;
        segInfo.session = *pSession;
        segInfo.nOperation = SEGMENT_UPDATE;
        int ret = segmentMgr.pSegQueue_->Push(segmentMgr.pSegQueue_, (char *)&segInfo, sizeof(segInfo));
        if (ret <= 0) {
                LinkLogError("seg queue error. push update:%d", ret);
                return ret;
        }
        return LINK_SUCCESS;
}

void LinkUninitSegmentMgr() {
        pthread_mutex_lock(&segMgrMutex);
        if (!segMgrStarted) {
                pthread_mutex_unlock(&segMgrMutex);
                return;
        }
        segmentMgr.nQuit_ = 1;
        pthread_mutex_unlock(&segMgrMutex);
        
        SegInfo segInfo;
        segInfo.nOperation = SEGMENT_QUIT;
        
        int ret = 0;
        while(ret <= 0) {
                ret = segmentMgr.pSegQueue_->Push(segmentMgr.pSegQueue_, (char *)&segInfo, sizeof(segInfo));
                if (ret <= 0) {
                        LinkLogError("seg queue error. notify quit:%d sleep 1 sec to retry", ret);
                        sleep(1);
                }
        }
        
        pthread_join(segmentMgr.segMgrThread_, NULL);
        return;
}
