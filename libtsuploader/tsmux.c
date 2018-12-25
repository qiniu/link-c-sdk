#include "tsmux.h"
#include "base.h"
#include <pthread.h>

#define STREAM_TYPE_PRIVATE_SECTION 0x05
#define STREAM_TYPE_PRIVATE_DATA    0x06
#define STREAM_TYPE_AUDIO_AAC       0x0f
#define STREAM_TYPE_VIDEO_H264      0x1b
#define STREAM_TYPE_VIDEO_HEVC      0x24

typedef struct PIDCounter {
        uint16_t nPID;
        uint16_t nCounter;
}PIDCounter;

typedef struct _LinkTsMuxerContext{
        LinkTsMuxerArg arg;
        pthread_mutex_t tsMutex_;
        LinkPES pes;
        int nMillisecondPatPeriod;
        uint8_t tsPacket[188];
        
        int nPidCounterMapLen;
        PIDCounter pidCounterMap[5];
        uint64_t nLastPts;
        int isTableWrited;
        int nAudioPIDDelta;
        
        uint8_t nPcrFlag; //分析ffmpeg，pcr只在pes中出现一次在最开头
        int nCurrentPos;
}LinkTsMuxerContext;

static uint16_t getPidCounter(LinkTsMuxerContext* _pMuxCtx, uint64_t _nPID)
{
        int nCount = 0;
        int i;
        
        for ( i = 0; i  < _pMuxCtx->nPidCounterMapLen; i++) {
                if (_pMuxCtx->pidCounterMap[i].nPID == _nPID) {
                        if (_pMuxCtx->pidCounterMap[i].nCounter == 0x0F){
                                _pMuxCtx->pidCounterMap[i].nCounter = 0;
                                return 0x0F;
                        }
                        nCount = _pMuxCtx->pidCounterMap[i].nCounter++;
                        return nCount;
                }
        }
        assert(0);
        return -1;
}

static void writeTable(LinkTsMuxerContext* _pMuxCtx, int64_t _nPts)
{
        if (_pMuxCtx->isTableWrited) {
                return;
        }
        pthread_mutex_lock(&_pMuxCtx->tsMutex_);
        if (_pMuxCtx->isTableWrited) {
                pthread_mutex_unlock(&_pMuxCtx->tsMutex_);
                return;
        }
        int nLen = 0;
        int nCount = 0;
        if (_pMuxCtx->nLastPts == 0 || _nPts - _pMuxCtx->nLastPts > 300 * 90) { //300毫米间隔
                /*
                 nCount =getPidCounter(_pMuxCtx, 0x11);
                 nLen = WriteSDT(_pMuxCtx->tsPacket, 1, nCount, ADAPTATION_JUST_PAYLOAD);
                 memset(&_pMuxCtx->tsPacket[nLen], 0xff, 188 - nLen);
                 _pMuxCtx->arg.output(_pMuxCtx->arg.pOpaque,_pMuxCtx->tsPacket, 188);
                 */
                
                nCount =getPidCounter(_pMuxCtx, 0x00);
                nLen = LinkWritePAT(_pMuxCtx->tsPacket, 1, nCount, LINK_ADAPTATION_JUST_PAYLOAD);
                memset(&_pMuxCtx->tsPacket[nLen], 0xff, 188 - nLen);
                _pMuxCtx->arg.output(_pMuxCtx->arg.pOpaque,_pMuxCtx->tsPacket, 188);
                
                nCount =getPidCounter(_pMuxCtx, 0x1000);
                int nAudioType = 0;
                int nVideoType = 0;
                if (_pMuxCtx->arg.nAudioFormat == LINK_AUDIO_AAC) {
                        nAudioType = STREAM_TYPE_AUDIO_AAC;
                } else if (_pMuxCtx->arg.nAudioFormat == LINK_AUDIO_PCMU || _pMuxCtx->arg.nAudioFormat == LINK_AUDIO_PCMA) {
                        nAudioType = STREAM_TYPE_PRIVATE_DATA;
                }
                if (_pMuxCtx->arg.nVideoFormat == LINK_VIDEO_H264) {
                        nVideoType = STREAM_TYPE_VIDEO_H264;
                } else if (_pMuxCtx->arg.nVideoFormat == LINK_VIDEO_H265) {
                        nVideoType = STREAM_TYPE_VIDEO_HEVC;
                }
                nLen = LinkWritePMT(_pMuxCtx->tsPacket, 1, nCount, LINK_ADAPTATION_JUST_PAYLOAD, nVideoType, nAudioType);
                memset(&_pMuxCtx->tsPacket[nLen], 0xff, 188 - nLen);
                _pMuxCtx->arg.output(_pMuxCtx->arg.pOpaque,_pMuxCtx->tsPacket, 188);
        }
        _pMuxCtx->isTableWrited = 1;
        _pMuxCtx->nCurrentPos = 188 * 2;
        pthread_mutex_unlock(&_pMuxCtx->tsMutex_);
}

uint16_t Pids[5] = {LINK_AUDIO_PID, LINK_VIDEO_PID, LINK_PAT_PID, LINK_PMT_PID, LINK_SDT_PID};
static void initPidCounterMap(LinkTsMuxerContext *pTsMuxerCtx) {
        int i;
        if (pTsMuxerCtx->arg.nAudioFormat == LINK_AUDIO_PCMA) {
                pTsMuxerCtx->nAudioPIDDelta = 1;
        }
        pTsMuxerCtx->nPidCounterMapLen = 5;
        for ( i = 0; i < pTsMuxerCtx->nPidCounterMapLen; i++){
                if (i == 0)
                        pTsMuxerCtx->pidCounterMap[i].nPID = Pids[i] + pTsMuxerCtx->nAudioPIDDelta;
                else
                        pTsMuxerCtx->pidCounterMap[i].nPID = Pids[i];
                pTsMuxerCtx->pidCounterMap[i].nCounter = 0;
        }
        return;
}
int LinkNewTsMuxerContext(LinkTsMuxerArg *pArg, LinkTsMuxerContext **_pTsMuxerCtx)
{
        LinkTsMuxerContext *pTsMuxerCtx = (LinkTsMuxerContext *)malloc(sizeof(LinkTsMuxerContext));
        if (pTsMuxerCtx == NULL) {
                return LINK_NO_MEMORY;
        }
        memset(pTsMuxerCtx, 0, sizeof(LinkTsMuxerContext));
        
        pTsMuxerCtx->arg = *pArg;
        initPidCounterMap(pTsMuxerCtx);
        
        int ret = pthread_mutex_init(&pTsMuxerCtx->tsMutex_, NULL);
        if (ret != 0){
                free(pTsMuxerCtx);
                return LINK_MUTEX_ERROR;
        }
        *_pTsMuxerCtx = pTsMuxerCtx;
        return 0;
}

int LinkResetTsMuxerContext(LinkTsMuxerContext *pTsMuxerCtx) {
        LinkTsMuxerArg arg = pTsMuxerCtx->arg;
        
        memset(pTsMuxerCtx, 0, sizeof(LinkTsMuxerContext));
        
        pTsMuxerCtx->arg = arg;
        initPidCounterMap(pTsMuxerCtx);
        
        return 0;
}

static int makeTsPacket(LinkTsMuxerContext* _pMuxCtx, int _nPid, int64_t _nPts, int _nIsKeyframe)
{
        int nReadLen = 0;
        int nCount = 0;
        int nKeyFrameLen = 0;
        int nKeyframePos = _pMuxCtx->nCurrentPos;
        do {
                int nRet = 0;
                nReadLen = LinkGetPESData(&_pMuxCtx->pes, 0, _nPid, _pMuxCtx->tsPacket, 188);
                if (nReadLen == 188){
                        nCount = getPidCounter(_pMuxCtx, _nPid);
                        LinkWriteContinuityCounter(_pMuxCtx->tsPacket, nCount);
                        nRet = _pMuxCtx->arg.output(_pMuxCtx->arg.pOpaque, _pMuxCtx->tsPacket, 188);
                        if (nRet < 0) {
                                return nRet;
                        }
                        
                        _pMuxCtx->nCurrentPos += 188;
                        nKeyFrameLen += 188;
                }
                
        }while(nReadLen != 0);
        
        if (_nIsKeyframe) {
                LinkKeyFrameMetaInfo metaInfo;
                metaInfo.nLength = nKeyFrameLen;
                metaInfo.nOffset = nKeyframePos;
                metaInfo.nTimestamp90Khz = _nPts;
                if (_pMuxCtx->arg.setKeyframeMetaInfo) {
                        _pMuxCtx->arg.setKeyframeMetaInfo(_pMuxCtx->arg.pMetaInfoUserArg, &metaInfo);
                }
        }
        return 0;
}

int LinkMuxerAudio(LinkTsMuxerContext* _pMuxCtx, uint8_t *_pData, int _nDataLen, int64_t _nPts)
{
        writeTable(_pMuxCtx, 0);
        pthread_mutex_lock(&_pMuxCtx->tsMutex_);
        if (_pMuxCtx->arg.nAudioFormat == LINK_AUDIO_AAC) {
                LinkInitAudioPES(&_pMuxCtx->pes, _pData, _nDataLen, _nPts);
        } else {
                LinkInitPrivateTypePES(&_pMuxCtx->pes, _pData, _nDataLen, _nPts);
        }
        
        int nRet = makeTsPacket(_pMuxCtx, LINK_AUDIO_PID+_pMuxCtx->nAudioPIDDelta, _pMuxCtx->pes.nPts, 0);
        pthread_mutex_unlock(&_pMuxCtx->tsMutex_);
        if (nRet < 0)
                return nRet;
        return 0;
}

int LinkMuxerVideo(LinkTsMuxerContext* _pMuxCtx, uint8_t *_pData, int _nDataLen, int64_t _nPts, int nIsKeyframe)
{
        writeTable(_pMuxCtx, 0);
        pthread_mutex_lock(&_pMuxCtx->tsMutex_);
        if (_pMuxCtx->nPcrFlag == 0) {
                _pMuxCtx->nPcrFlag = 1;
                LinkInitVideoPESWithPcr(&_pMuxCtx->pes, _pMuxCtx->arg.nVideoFormat, _pData, _nDataLen, _nPts);
        } else {
                LinkInitVideoPES(&_pMuxCtx->pes, _pMuxCtx->arg.nVideoFormat, _pData, _nDataLen, _nPts);
        }
        
        int nRet = makeTsPacket(_pMuxCtx, LINK_VIDEO_PID, _pMuxCtx->pes.nPts, nIsKeyframe);
        pthread_mutex_unlock(&_pMuxCtx->tsMutex_);
        if (nRet < 0)
                return nRet;
        return 0;
}

int LinkMuxerFlush(LinkTsMuxerContext* pMuxerCtx)
{
        return 0;
}

void LinkDestroyTsMuxerContext(LinkTsMuxerContext *pTsMuxerCtx)
{
        if (pTsMuxerCtx)
                free(pTsMuxerCtx);
}
