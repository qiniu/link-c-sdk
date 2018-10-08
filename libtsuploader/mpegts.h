#ifndef __LINK_MPEG_TS__
#define __LINK_MPEG_TS__
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "base.h"

/* pids */
#define LINK_PAT_PID                 0x0000
#define LINK_SDT_PID                 0x0011
//下面的pid不是标准，完全是因为简单化
#define LINK_PMT_PID 0x1000
#define LINK_VIDEO_PID 0x100
#define LINK_AUDIO_PID 0x101

/* table ids */
#define LINK_PAT_TID   0x00
#define LINK_PMT_TID   0x02
#define LINK_M4OD_TID  0x05
#define LINK_SDT_TID   0x42

//调整字段控制,。01仅含有效负载，10仅含调整字段，11含有调整字段和有效负载。为00的话解码器不进行处理。
#define LINK_ADAPTATION_INGNORE 0x0
#define LINK_ADAPTATION_JUST_PAYLOAD 0x1
#define LINK_ADAPTATION_JUST_PADDING 0x2
#define LINK_ADAPTATION_BOTH 0x3


typedef int (*LinkTsPacketCallback)(void *pOpaque, void* pTsData, int nTsDataLen);

typedef struct _LinkPES LinkPES;
typedef struct _LinkPES{
        uint8_t *pESData;
        int nESDataLen;
        int nPos; //指向pESData
        int nStreamId; //Audio streams (0xC0-0xDF), Video streams (0xE0-0xEF)
        int nPid;
        int64_t nPts;
        uint8_t nWithPcr;
        uint8_t nPrivate;
        LinkVideoFormat videoFormat;
        //设想是传入h264(或者音频)给pESData， 在封装ts时候每次应该封装多少长度的数据是应该知道的
        //也是尽量减少内存使用
}LinkPES;

void LinkInitVideoPESWithPcr(LinkPES *_pPes, LinkVideoFormat fmt, uint8_t *_pData, int _nDataLen, int64_t _nPts);
void LinkInitVideoPES(LinkPES *pPes, LinkVideoFormat fmt, uint8_t *pData, int nDataLen, int64_t nPts);
void LinkInitAudioPES(LinkPES *pPes, uint8_t *pData, int nDataLen, int64_t nPts);
void LinkInitPrivateTypePES(LinkPES *pPes, uint8_t *pData, int nDataLen, int64_t nPts);
int LinkGetPESData(LinkPES *pPes, int _nCounter, int _nPid, uint8_t *pData, int nLen); //返回0则到了EOF

int LinkWriteTsHeader(uint8_t *_pBuf, int _nUinitStartIndicator, int _nCount, int _nPid, int _nAdaptationField);
void LinkSetAdaptationFieldFlag(uint8_t *_pBuf, int _nAdaptationField);
void LinkWriteContinuityCounter(uint8_t *pBuf, int nCounter);
int LinkWriteSDT(uint8_t *_pBuf, int _nUinitStartIndicator, int _nCount, int _nAdaptationField);
int LinkWritePAT(uint8_t *_pBuf, int _nUinitStartIndicator, int _nCount, int _nAdaptationField);
int LinkWritePMT(uint8_t *_pBuf, int _nUinitStartIndicator, int _nCount, int _nAdaptationField, int _nVStreamType, int _nAStreamType);

#endif
