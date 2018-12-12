#ifndef __TS_MUX__
#define __TS_MUX__
#include <stdint.h>
#include "mpegts.h"

typedef struct _LinkTsMuxerContext LinkTsMuxerContext;


typedef struct {
        LinkAudioFormat nAudioFormat;
        int nAudioSampleRate;
        int nAudioChannels;
        LinkVideoFormat nVideoFormat;
        LinkTsPacketCallback output;
        void *pOpaque;
        LinkSetKeyframeMetaInfo setKeyframeMetaInfo;
        void *pMetaInfoUserArg;
}LinkTsMuxerArg;

int LinkNewTsMuxerContext(LinkTsMuxerArg *pArg, LinkTsMuxerContext **pTsMuxerContext);
int LinkResetTsMuxerContext(LinkTsMuxerContext *pTsMuxerContext);
int LinkMuxerAudio(LinkTsMuxerContext* pMuxerCtx, uint8_t *pData, int nDataLen, int64_t nPts);
int LinkMuxerVideo(LinkTsMuxerContext* pMuxerCtx, uint8_t *pData, int nDataLen,  int64_t nPts, int nIsKeyframe);
int LinkMuxerFlush(LinkTsMuxerContext* pMuxerCtx);
void LinkDestroyTsMuxerContext(LinkTsMuxerContext *pTsMuxerCtx);

#endif
