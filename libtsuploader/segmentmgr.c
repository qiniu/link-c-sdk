#include "segmentmgr.h"
#include "resource.h"
#include "queue.h"
#include "uploader.h"
#include "servertime.h"
#include "b64/urlsafe_b64.h"
#include <qupload.h>
#include "httptools.h"
#include "security.h"
#include <unistd.h>
#include "cJSON/cJSON.h"

#define SEGMENT_RELEASE 1
#define SEGMENT_UPDATE 2
#define SEGMENT_QUIT 3
#define SEGMENT_UPDATE_META 4
#define SEGMENT_CLEAR_META 5
typedef struct {
        SegmentHandle handle;
        uint8_t nOperation;
        LinkSession session;
        const LinkSessionMeta* pSessionMeta;
}SegInfo;

typedef struct {
        SegmentHandle handle;
        int segReportOk;
        LinkUploadParamCallback getUploadParamCallback;
        void *pGetUploadParamCallbackArg;
        UploadStatisticCallback pUploadStatisticCb;
        void *pUploadStatArg;
        int64_t nNextUpdateSegTimeInSecond;
        LinkSession tmpSession; //TODO backup report info when next report time is not arrival
        LinkSessionMeta* pSessionMeta;
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

static int checkShouldReport(Seg* pSeg, LinkSession *pCurSession) {
        int64_t nNow = LinkGetCurrentNanosecond() / 1000000000LL;
        if (nNow <= pSeg->nNextUpdateSegTimeInSecond && pCurSession->nSessionEndResonCode == 0) {
                pSeg->tmpSession.nVideoGapFromLastReport += pCurSession->nVideoGapFromLastReport;
                pSeg->tmpSession.nAudioGapFromLastReport += pCurSession->nAudioGapFromLastReport;
                if (pCurSession->isNewSessionStarted == 0 && pCurSession->nSessionEndResonCode == 0 &&
                    pSeg->segReportOk) {
                        return 0;
                }
        }

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
        char buffer[1280];
        int nReportHostLen = 640;
        char *body = buffer + nReportHostLen;
        int nBodyLen = 0;
        LinkSession *s = &pSegInfo->session;
        memset(buffer, 0, sizeof(buffer));
        
        const LinkSessionMeta* smeta = segmentMgr.handles[idx].pSessionMeta;
        if (s->nSessionEndResonCode != 0) {
                const char * reason = "timeout";
                if (s->nSessionEndResonCode == 1)
                        reason = "normal";
                else if (s->nSessionEndResonCode == 3)
                        reason = "force";
                
                nBodyLen = sprintf(body, "{ \"session\": \"%s\", \"start\": %"PRId64", \"current\": %"PRId64", \"sequence\": %"PRId64","
                        " \"vd\": %"PRId64", \"ad\": %"PRId64", \"tvd\": %"PRId64", \"tad\": %"PRId64", \"end\":"
                        " %"PRId64", \"endReason\": \"%s\"",
                        s->sessionId, s->nSessionStartTime/1000000, LinkGetCurrentNanosecond()/1000000, s->nTsSequenceNumber,
                        s->nVideoGapFromLastReport, s->nAudioGapFromLastReport,
                        s->nAccSessionVideoDuration, s->nAccSessionAudioDuration, s->nSessionEndTime, reason);

        } else {
                nBodyLen = sprintf(body, "{ \"session\": \"%s\", \"start\": %"PRId64", \"current\": %"PRId64", \"sequence\": %"PRId64","
                        " \"vd\": %"PRId64", \"ad\": %"PRId64", \"tvd\": %"PRId64", \"tad\": %"PRId64"",
                        s->sessionId, s->nSessionStartTime/1000000, LinkGetCurrentNanosecond()/1000000, s->nTsSequenceNumber,
                        s->nVideoGapFromLastReport, s->nAudioGapFromLastReport,
                        s->nAccSessionVideoDuration, s->nAccSessionAudioDuration);
        }
        
        if (smeta && smeta->len > 0) {
                int i = 0;
                int nSessionMetaLen = sprintf(body + nBodyLen, ",\"meta\":{");
                nBodyLen += nSessionMetaLen;
                for (i = 0; i < smeta->len; i++) {
                        nBodyLen += sprintf(body + nBodyLen, "\"%s\":\"%s\",", smeta->keys[i], smeta->values[i]);
                }
                body[nBodyLen-1] = '}';
        }
        body[nBodyLen++] = '}';
        
        assert(nBodyLen <= sizeof(buffer) - nReportHostLen);
        LinkLogDebug("body:%s", body);
        
        LinkUploadParam param;
        memset(&param, 0, sizeof(param));
        char *reportHost = buffer;
        char ak[41] = {0};
        char sk[41] = {0};
        
        param.pSegUrl = reportHost;
        param.nSegUrlLen = nReportHostLen;
        param.pAk = ak;
        param.nAkLen = sizeof(ak);
        param.pSk = sk;
        param.nSkLen = sizeof(sk);

        int ret = segmentMgr.handles[idx].getUploadParamCallback(segmentMgr.handles[idx].pGetUploadParamCallbackArg,
                                                                 &param, LINK_UPLOAD_CB_GETPARAM);
        
        
        int nUrlLen = param.nSegUrlLen;
        reportHost[nUrlLen] = 0;
        char * pToken = reportHost + nUrlLen + 1;
        int nTokenOffset = snprintf(pToken, nReportHostLen-nUrlLen-1, "%s:", param.pAk);
        int nOutputLen = 30;
        //fprintf(stderr, "-------->%s:%d %s body:%s:%d\n", param.pSk, param.nSkLen,reportHost, body,nBodyLen);
        ret = GetHttpRequestSign(param.pSk, param.nSkLen, "POST", reportHost, "application/json", body,
                                     nBodyLen, pToken+nTokenOffset, &nOutputLen);
        if (ret != LINK_SUCCESS) {
                LinkLogError("getHttpRequestSign error:%d", ret);
                return ret;
        }
        assert(nReportHostLen - 1 >= nOutputLen+nTokenOffset+nUrlLen+1);
        
        char resp[256];
        memset(resp, 0, sizeof(resp));
        int respLen = sizeof(resp);

        //fprintf(stderr, "-------->body:%s:%d token:%s\n", body,nBodyLen, pToken);
        ret = LinkSimpleHttpPostWithToken(reportHost, resp, sizeof(resp), &respLen, body, nBodyLen, "application/json",
                                    pToken, nTokenOffset+nOutputLen);
        if(smeta && smeta->isOneShot) {
                free((void *)smeta);
                segmentMgr.handles[idx].pSessionMeta = NULL;
        }
        if (ret != LINK_SUCCESS) {
                LinkLogError("report session info fail:%d", ret);
                return ret;
        }
        
        cJSON * pJsonRoot = cJSON_Parse(resp);
        if (pJsonRoot == NULL) {
                return LINK_JSON_FORMAT;
        }
        
        int nextReportTime = 0;
        cJSON *pNode = cJSON_GetObjectItem(pJsonRoot, "ttl");
        if (pNode == NULL) {
                cJSON_Delete(pJsonRoot);
                return LINK_JSON_FORMAT;
        } else {
                cJSON_Delete(pJsonRoot);
                nextReportTime = pNode->valueint;
        }
        
        if (nextReportTime > 0) {
                segmentMgr.handles[idx].nNextUpdateSegTimeInSecond = nextReportTime + time(NULL);
        }
        
        
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
        
        if (!checkShouldReport(&segmentMgr.handles[idx], &pSegInfo->session)) {
                return;
        }
        if (pSegInfo->session.nAccSessionDuration <= 0) {
                LinkLogInfo("not report due to session duration is 0");
        }

        int ret = reportSegInfo(pSegInfo, idx);
        if (ret == LINK_SUCCESS)
                segmentMgr.handles[idx].segReportOk = 1;
        else
                segmentMgr.handles[idx].segReportOk = 0;
        
        return;
}

static void handleUpdateSessionMeta(SegInfo *pSegInfo) {
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
        if (segmentMgr.handles[idx].pSessionMeta) {
                free((void *)segmentMgr.handles[idx].pSessionMeta);
                segmentMgr.handles[idx].pSessionMeta = NULL;
        }
        segmentMgr.handles[idx].pSessionMeta = (const LinkSessionMeta*)pSegInfo->pSessionMeta;
}

static void handleClearSessionMeta(SegInfo *pSegInfo) {
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
        if (segmentMgr.handles[idx].pSessionMeta) {
                segmentMgr.handles[idx].pSessionMeta->isOneShot = 1;
        }
        return;
}

static void linkReleaseSegmentHandle(SegmentHandle seg) {
        pthread_mutex_lock(&segMgrMutex);
        if (seg >= 0 && seg < sizeof(segmentMgr.handles) / sizeof(SegmentHandle)) {
                segmentMgr.handles[seg].handle = -1;
                segmentMgr.handles[seg].getUploadParamCallback = NULL;
                segmentMgr.handles[seg].pGetUploadParamCallbackArg = NULL;
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
                int ret = segmentMgr.pSegQueue_->PopWithTimeout(segmentMgr.pSegQueue_, (char *)(&segInfo), sizeof(segInfo), 24 * 60 * 60 * 1000000LL);
                
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
                                switch (segInfo.nOperation) {
                                                
                                        case SEGMENT_RELEASE:
                                                linkReleaseSegmentHandle(segInfo.handle);
                                                break;
                                        case SEGMENT_UPDATE:
                                                handleReportSegInfo(&segInfo);
                                                break;
                                        case  SEGMENT_UPDATE_META:
                                                handleUpdateSessionMeta(&segInfo);
                                                break;
                                        case SEGMENT_CLEAR_META:
                                                handleClearSessionMeta(&segInfo);
                                                break;
                                        case  SEGMENT_QUIT:
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

int LinkUpdateSegmentMeta(SegmentHandle seg, const LinkSessionMeta *meta) {
        SegInfo segInfo;
        segInfo.handle = seg;
        segInfo.nOperation = SEGMENT_UPDATE_META;
        segInfo.pSessionMeta = meta;
        int ret = segmentMgr.pSegQueue_->Push(segmentMgr.pSegQueue_, (char *)&segInfo, sizeof(segInfo));
        if (ret <= 0) {
                LinkLogError("seg queue error. push update meta:%d", ret);
                return ret;
        }
        return LINK_SUCCESS;
}

int LinkClearSegmentMeta(SegmentHandle seg) {
        SegInfo segInfo;
        segInfo.handle = seg;
        segInfo.nOperation = SEGMENT_CLEAR_META;
        int ret = segmentMgr.pSegQueue_->Push(segmentMgr.pSegQueue_, (char *)&segInfo, sizeof(segInfo));
        if (ret <= 0) {
                LinkLogError("seg queue error. push clear meta:%d", ret);
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
