#include "tsmuxuploader.h"
#include "base.h"
#include <unistd.h>
#include "adts.h"
#ifdef __APPLE__
#include <sys/types.h>
#include <sys/sysctl.h>
#else
#include <sys/sysinfo.h>
#endif
#include "servertime.h"
#include "segmentmgr.h"
#include "kmp.h"

#ifdef USE_OWN_TSMUX
#include "tsmux.h"
#else
#include <libavformat/avformat.h>
#endif

#define FF_OUT_LEN 4096
#define QUEUE_INIT_LEN 150

#define LINK_STREAM_TYPE_AUDIO 1
#define LINK_STREAM_TYPE_VIDEO 2

typedef struct _FFTsMuxContext{
        LinkAsyncInterface asyncWait;
        LinkTsUploader *pTsUploader_;
#ifdef USE_OWN_TSMUX
        LinkTsMuxerContext *pFmtCtx_;
#else
        AVFormatContext *pFmtCtx_;
#endif
        int nOutVideoindex_;
        int nOutAudioindex_;
        int64_t nPrevAudioTimestamp;
        int64_t nPrevVideoTimestamp;
        LinkTsMuxUploader * pTsMuxUploader;
}FFTsMuxContext;

typedef struct _Token {
        char * pToken_;
        int nTokenLen_;
}Token;

typedef struct _FFTsMuxUploader{
        LinkTsMuxUploader tsMuxUploader_;
        pthread_mutex_t muxUploaderMutex_;
        pthread_mutex_t tokenMutex_;
        unsigned char *pAACBuf;
        int nAACBufLen;
        FFTsMuxContext *pTsMuxCtx;
        
        int64_t nLastTimestamp;
        int64_t nFirstTimestamp; //initial to -1
        int64_t nLastPicCallbackSystime; //upload picture need
        int nKeyFrameCount;
        int nFrameCount;
        LinkMediaArg avArg;
        LinkUploadState ffMuxSatte;
        
        int nUploadBufferSize;
        int nUpdateSegmentInterval;
        
        char deviceId_[33];
        Token token_;
        LinkTsUploadArg uploadArg;
        PictureUploader *pPicUploader;
        SegmentHandle segmentHandle;
        enum CircleQueuePolicy queueType_;
        int8_t isPause;
        int8_t isTypeOneshot;
        int8_t isQuit;
        char tsType[16];
        
        char *pUpTokenRequestUrl;
        pthread_t tokenThread;
        
        char *pVideoMeta;
        int nVideoMetaLen;
}FFTsMuxUploader;

//static int aAacfreqs[13] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050 ,16000 ,12000, 11025, 8000, 7350};
static int getUploadParamCallback(IN void *pOpaque, IN OUT LinkUploadParam *pParam);
static void linkCapturePictureCallback(void *pOpaque, int64_t nTimestamp);
static int linkTsMuxUploaderSetUploadZone(FFTsMuxUploader *pFFTsMuxUploader, LinkUploadZone _upzone);
static int linkTsMuxUploaderTokenThreadStart(FFTsMuxUploader* pFFTsMuxUploader);

static int getAacFreqIndex(int _nFreq)
{
        switch(_nFreq){
                case 96000:
                        return 0;
                case 88200:
                        return 1;
                case 64000:
                        return 2;
                case 48000:
                        return 3;
                case 44100:
                        return 4;
                case 32000:
                        return 5;
                case 24000:
                        return 6;
                case 22050:
                        return 7;
                case 16000:
                        return 8;
                case 12000:
                        return 9;
                case 11025:
                        return 10;
                case 8000:
                        return 11;
                case 7350:
                        return 12;
                default:
                        return -1;
        }
}

static void pushRecycle(FFTsMuxUploader *_pFFTsMuxUploader)
{
        if (_pFFTsMuxUploader) {
                
                if (_pFFTsMuxUploader->pTsMuxCtx) {
#ifndef USE_OWN_TSMUX
                        av_write_trailer(_pFFTsMuxUploader->pTsMuxCtx->pFmtCtx_);
#endif
                        if (_pFFTsMuxUploader->queueType_ == TSQ_APPEND)
                                _pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->NotifyDataPrapared(_pFFTsMuxUploader->pTsMuxCtx->pTsUploader_);
                        LinkLogError("push to mgr:%p", _pFFTsMuxUploader->pTsMuxCtx);
                        LinkPushFunction(_pFFTsMuxUploader->pTsMuxCtx);
                        _pFFTsMuxUploader->pTsMuxCtx = NULL;
                }
        }
        return;
}

static int writeTsPacketToMem(void *opaque, uint8_t *buf, int buf_size)
{
        FFTsMuxContext *pTsMuxCtx = (FFTsMuxContext *)opaque;
        
        int ret = pTsMuxCtx->pTsUploader_->Push(pTsMuxCtx->pTsUploader_, (char *)buf, buf_size);
        if (ret < 0){
                if (ret == LINK_Q_OVERWRIT) {
                        LinkLogWarn("write ts to queue overwrite:%d", ret);
                } else if (ret == LINK_NO_MEMORY){
                        LinkLogWarn("write ts to queue no memory:%d", ret);
                }else {
                        LinkLogDebug("write ts to queue fail:%d", ret);
                }
                return ret;
	} else if (ret == 0){
                LinkLogDebug("push queue return zero");
                return LINK_Q_WRONGSTATE;
        } else {
                LinkLogTrace("write_packet: should write:len:%d  actual:%d\n", buf_size, ret);
        }
        return ret;
}

static int push(FFTsMuxUploader *pFFTsMuxUploader, const char * _pData, int _nDataLen, int64_t _nTimestamp, int _nFlag,
                int _nIsKeyframe, int64_t nSysNanotime){
#ifndef USE_OWN_TSMUX
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.data = (uint8_t *)_pData;
        pkt.size = _nDataLen;
#endif
        
        //LinkLogTrace("push thread id:%d\n", (int)pthread_self());
        
        FFTsMuxContext *pTsMuxCtx = NULL;
        int count = 0;
        
        count = 2;
        pTsMuxCtx = pFFTsMuxUploader->pTsMuxCtx;
        while(pTsMuxCtx == NULL && count) {
                LinkLogWarn("mux context is null");
                usleep(2000);
                pTsMuxCtx = pFFTsMuxUploader->pTsMuxCtx;
                count--;
        }
        if (pTsMuxCtx == NULL) {
                LinkLogWarn("upload context is NULL");
                return 0;
        }
        
        int ret = 0;
        
        if (_nFlag == LINK_STREAM_TYPE_AUDIO){
                //fprintf(stderr, "audio frame: len:%d pts:%"PRId64"\n", _nDataLen, _nTimestamp);
                if (pTsMuxCtx->nPrevAudioTimestamp != 0 && _nTimestamp - pTsMuxCtx->nPrevAudioTimestamp <= 0) {
                        LinkLogWarn("audio pts not monotonically: prev:%"PRId64" now:%"PRId64"", pTsMuxCtx->nPrevAudioTimestamp, _nTimestamp);
                        return 0;
                }
#ifndef USE_OWN_TSMUX
                int isAdtsAdded = 0;
                pkt.pts = _nTimestamp * 90;
                pkt.stream_index = pTsMuxCtx->nOutAudioindex_;
                pkt.dts = pkt.pts;
                pTsMuxCtx->nPrevAudioTimestamp = _nTimestamp;
#endif
                unsigned char * pAData = (unsigned char * )_pData;
                if (pFFTsMuxUploader->avArg.nAudioFormat ==  LINK_AUDIO_AAC && (pAData[0] != 0xff || (pAData[1] & 0xf0) != 0xf0)) {
                        LinkADTSFixheader fixHeader;
                        LinkADTSVariableHeader varHeader;
                        LinkInitAdtsFixedHeader(&fixHeader);
                        LinkInitAdtsVariableHeader(&varHeader, _nDataLen);
                        fixHeader.channel_configuration = pFFTsMuxUploader->avArg.nChannels;
                        int nFreqIdx = getAacFreqIndex(pFFTsMuxUploader->avArg.nSamplerate);
                        fixHeader.sampling_frequency_index = nFreqIdx;
                        if (pFFTsMuxUploader->pAACBuf == NULL || pFFTsMuxUploader->nAACBufLen < varHeader.aac_frame_length) {
                                if (pFFTsMuxUploader->pAACBuf) {
                                        free(pFFTsMuxUploader->pAACBuf);
                                        pFFTsMuxUploader->pAACBuf = NULL;
                                }
                                pFFTsMuxUploader->pAACBuf = (unsigned char *)malloc(varHeader.aac_frame_length);
                                pFFTsMuxUploader->nAACBufLen = (int)varHeader.aac_frame_length;
                        }
                        if(pFFTsMuxUploader->pAACBuf == NULL || pFFTsMuxUploader->avArg.nChannels < 1 || pFFTsMuxUploader->avArg.nChannels > 2
                           || nFreqIdx < 0) {
                                if (pFFTsMuxUploader->pAACBuf == NULL) {
                                        LinkLogWarn("malloc %d size memory fail", varHeader.aac_frame_length);
                                        return LINK_NO_MEMORY;
                                } else {
                                        LinkLogWarn("wrong audio arg:channel:%d sameplerate%d", pFFTsMuxUploader->avArg.nChannels,
                                                pFFTsMuxUploader->avArg.nSamplerate);
                                        return LINK_ARG_ERROR;
                                }
                        }
                        LinkConvertAdtsHeader2Char(&fixHeader, &varHeader, pFFTsMuxUploader->pAACBuf);
                        int nHeaderLen = varHeader.aac_frame_length - _nDataLen;
                        memcpy(pFFTsMuxUploader->pAACBuf + nHeaderLen, _pData, _nDataLen);

#ifdef USE_OWN_TSMUX
                        LinkMuxerAudio(pTsMuxCtx->pFmtCtx_, (uint8_t *)pFFTsMuxUploader->pAACBuf, varHeader.aac_frame_length, _nTimestamp);
#else
                        pkt.data = (uint8_t *)pFFTsMuxUploader->pAACBuf;
                        pkt.size = varHeader.aac_frame_length;
                        isAdtsAdded = 1;
#endif
                } 
#ifdef USE_OWN_TSMUX
                else {
                        ret = LinkMuxerAudio(pTsMuxCtx->pFmtCtx_, (uint8_t*)_pData, _nDataLen, _nTimestamp);
                }
#endif
        }else{
                //fprintf(stderr, "video frame: len:%d pts:%"PRId64"\n", _nDataLen, _nTimestamp);
                if (pTsMuxCtx->nPrevVideoTimestamp != 0 && _nTimestamp - pTsMuxCtx->nPrevVideoTimestamp <= 0) {
                        LinkLogWarn("video pts not monotonically: prev:%"PRId64" now:%"PRId64"", pTsMuxCtx->nPrevVideoTimestamp, _nTimestamp);
                        return 0;
                }
#ifdef USE_OWN_TSMUX
                ret = LinkMuxerVideo(pTsMuxCtx->pFmtCtx_, (uint8_t*)_pData, _nDataLen, _nTimestamp, _nIsKeyframe);
#else
                pkt.pts = _nTimestamp * 90;
                pkt.stream_index = pTsMuxCtx->nOutVideoindex_;
                pkt.dts = pkt.pts;
                pTsMuxCtx->nPrevVideoTimestamp = _nTimestamp;
#endif
        }
        
        
#ifndef USE_OWN_TSMUX
        ret = av_interleaved_write_frame(pTsMuxCtx->pFmtCtx_, &pkt);
#endif
        if (ret == 0) {
                pTsMuxCtx->pTsUploader_->RecordTimestamp(pTsMuxCtx->pTsUploader_, _nTimestamp, nSysNanotime);
        } else {
                if (pFFTsMuxUploader->ffMuxSatte != LINK_UPLOAD_FAIL)
                        LinkLogError("Error muxing packet:%d", ret);
                pFFTsMuxUploader->ffMuxSatte = LINK_UPLOAD_FAIL;
        }
        
#ifndef USE_OWN_TSMUX
        if (isAdtsAdded) {
                free(pkt.data);
        }
#endif
        return ret;
}

static inline int getNaluType(LinkVideoFormat format, unsigned char v) {
        if (format == LINK_VIDEO_H264) {
                return v & 0x1F;
        } else {
                return ((v & 0x7E) >> 1);
        }
}

static inline int isTypeSPS(LinkVideoFormat format, int v) {
        if (format == LINK_VIDEO_H264) {
                return v == 8;
        } else {
                return v == 33;
        }
}

static int getVideoSpsAndCompare(FFTsMuxUploader *pFFTsMuxUploader, const char * _pData, int _nDataLen, int *pIsSameAsBefore) {

        LinkKMP kmp;
        unsigned char d[3] = {0x00, 0x00, 0x01};
        const unsigned char * pData = (const unsigned char *)_pData;
        LinkInitKmp(&kmp, (const unsigned char *)d, 3);
        int pos = 0;
        if (pFFTsMuxUploader->avArg.nVideoFormat != LINK_VIDEO_H264 &&
            pFFTsMuxUploader->avArg.nVideoFormat != LINK_VIDEO_H265) {
                return -2;
        }
        *pIsSameAsBefore = 0;
        const unsigned char *pSpsStart = NULL;
        do{
                pos = LinkFindPatternIndex(&kmp, pData, _nDataLen);
                if (pos < 0){
                        return -1;
                }
                pData+=pos+3;
                int type = getNaluType(pFFTsMuxUploader->avArg.nVideoFormat, pData[0]);
                if (isTypeSPS(pFFTsMuxUploader->avArg.nVideoFormat, type)) { //sps
                        pSpsStart = pData;
                        continue;
                }
                if (pSpsStart) {
                        int nMetaLen = pData - pSpsStart - 3;
                        if (*(pData-4) == 00)
                                nMetaLen--;
                        if(pFFTsMuxUploader->pVideoMeta == NULL) { //save metadata
                                pFFTsMuxUploader->pVideoMeta = (char *)malloc(nMetaLen);
                                if (pFFTsMuxUploader->pVideoMeta == NULL)
                                        return LINK_NO_MEMORY;
                                pFFTsMuxUploader->nVideoMetaLen = nMetaLen;
                                memcpy(pFFTsMuxUploader->pVideoMeta, pSpsStart, nMetaLen);
                                return 0;
                        } else { //compare metadata
                                if (pFFTsMuxUploader->nVideoMetaLen != nMetaLen ||
                                    memcmp(pFFTsMuxUploader->pVideoMeta, pSpsStart, nMetaLen) != 0) {
                                        
                                        *pIsSameAsBefore = 1;
                                        
                                        if (pFFTsMuxUploader->nVideoMetaLen != nMetaLen) {
                                                free(pFFTsMuxUploader->pVideoMeta);
                                                pFFTsMuxUploader->pVideoMeta = NULL;
                                                
                                                pFFTsMuxUploader->pVideoMeta = (char *)malloc(nMetaLen);
                                                if (pFFTsMuxUploader->pVideoMeta == NULL)
                                                        return LINK_NO_MEMORY;
                                                pFFTsMuxUploader->nVideoMetaLen = nMetaLen;
                                        }
                                        memcpy(pFFTsMuxUploader->pVideoMeta, pSpsStart, nMetaLen);
                                }
                                return 0;
                        }
                }
        }while(1);
                
        return -2;
}

static int checkSwitch(LinkTsMuxUploader *_pTsMuxUploader, int64_t _nTimestamp, int nIsKeyFrame, int _isVideo, int64_t nSysNanotime, int _nIsSegStart,
                       const char * _pData, int _nDataLen)
{
        int ret;
        int shouldSwitch = 0;
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        if (pFFTsMuxUploader->nFirstTimestamp == -1) {
                pFFTsMuxUploader->nFirstTimestamp = _nTimestamp;
        }
        LinkUploadState ustate = pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->GetUploaderState(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_);
        //if (pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->GetUploaderState(pTsMuxCtx->pTsUploader_) == LINK_UPLOAD_FAIL) {
        if ( ustate != LINK_UPLOAD_INIT) {
                if (ustate == LINK_UPLOAD_FAIL && pFFTsMuxUploader->ffMuxSatte != LINK_UPLOAD_FAIL) {
                        LinkLogDebug("upload fail. drop the data");
                }
                pFFTsMuxUploader->ffMuxSatte = ustate;
                shouldSwitch = 1;
        }
        int isVideoKeyframe = 0;
        int isSameAsBefore = 0;
        if (_isVideo && nIsKeyFrame) {
                isVideoKeyframe = 1;
                //TODO video metadata
                ret = getVideoSpsAndCompare(pFFTsMuxUploader, _pData, _nDataLen, &isSameAsBefore);
                if (ret != LINK_SUCCESS) {
                        return ret;
                }
                if (isSameAsBefore) {
                        LinkLogInfo("video change sps");
                        shouldSwitch = 1;
                }
        }
        // if start new uploader, start from keyframe
        if (isVideoKeyframe || shouldSwitch) {
                if( ((_nTimestamp - pFFTsMuxUploader->nFirstTimestamp) > 4980 && pFFTsMuxUploader->nKeyFrameCount > 0)
                   //at least 1 keyframe and aoubt last 5 second
                   || (_nIsSegStart && pFFTsMuxUploader->nFrameCount != 0)// new segment is specified
                   ||  pFFTsMuxUploader->ffMuxSatte != LINK_UPLOAD_INIT){   // upload finished
                        //printf("next ts:%d %"PRId64"\n", pFFTsMuxUploader->nKeyFrameCount, _nTimestamp - pFFTsMuxUploader->nLastUploadVideoTimestamp);
                        pFFTsMuxUploader->nKeyFrameCount = 0;
                        pFFTsMuxUploader->nFrameCount = 0;
                        pFFTsMuxUploader->nFirstTimestamp = _nTimestamp;
                        pFFTsMuxUploader->ffMuxSatte = LINK_UPLOAD_INIT;
                        pushRecycle(pFFTsMuxUploader);
                        int64_t nDiff = (nSysNanotime - pFFTsMuxUploader->nLastPicCallbackSystime)/1000000;
                        if (nDiff < 1000) {
                                LinkLogWarn("get picture callback too frequency:%"PRId64"ms", nDiff);
                        }
                        pFFTsMuxUploader->nLastPicCallbackSystime = nSysNanotime;
                        if (_nIsSegStart) {
                                pFFTsMuxUploader->uploadArg.nSegmentId_ = pFFTsMuxUploader->nLastPicCallbackSystime;
                        }
                        ret = LinkTsMuxUploaderStart(_pTsMuxUploader);
                        if (ret != 0) {
                                return ret;
                        }
                }
                if (isVideoKeyframe) {
                        pFFTsMuxUploader->nKeyFrameCount++;
                }
        }
        return 0;
}

static int PushVideo(LinkTsMuxUploader *_pTsMuxUploader, const char * _pData, int _nDataLen, int64_t _nTimestamp, int nIsKeyFrame, int _nIsSegStart)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        pthread_mutex_lock(&pFFTsMuxUploader->muxUploaderMutex_);
        if (pFFTsMuxUploader->isPause) {
                pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                return LINK_PAUSED;
        }
        int ret = 0;

        int64_t nSysNanotime = LinkGetCurrentNanosecond();
        if (pFFTsMuxUploader->nKeyFrameCount == 0 && !nIsKeyFrame) {
                LinkLogWarn("first video frame not IDR. drop this frame\n");
                pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                return 0;
        }
        ret = checkSwitch(_pTsMuxUploader, _nTimestamp, nIsKeyFrame, 1, nSysNanotime, _nIsSegStart, _pData, _nDataLen);
        if (ret != 0) {
                pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                return ret;
        }
        if (pFFTsMuxUploader->nKeyFrameCount == 0 && !nIsKeyFrame) {
                LinkLogWarn("first video frame not IDR. drop this frame\n");
                pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                return 0;
        }
        if (pFFTsMuxUploader->nKeyFrameCount == 1 && nIsKeyFrame) {
                if (pFFTsMuxUploader->nLastPicCallbackSystime <= 0)
                        pFFTsMuxUploader->nLastPicCallbackSystime = LinkGetCurrentNanosecond();
                linkCapturePictureCallback(_pTsMuxUploader, nSysNanotime / 1000000);
        }
        
        ret = push(pFFTsMuxUploader, _pData, _nDataLen, _nTimestamp, LINK_STREAM_TYPE_VIDEO, nIsKeyFrame, nSysNanotime);
        if (ret == 0){
                pFFTsMuxUploader->nFrameCount++;
        }
        if (ret == LINK_NO_MEMORY) {
                if (pFFTsMuxUploader->pTsMuxCtx)
                        pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->NotifyDataPrapared(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_);
        }
        pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
        return ret;
}

static int PushAudio(LinkTsMuxUploader *_pTsMuxUploader, const char * _pData, int _nDataLen, int64_t _nTimestamp)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        pthread_mutex_lock(&pFFTsMuxUploader->muxUploaderMutex_);
        if (pFFTsMuxUploader->isPause) {
                pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                return LINK_PAUSED;
        }
        int64_t nSysNanotime = LinkGetCurrentNanosecond();
        int ret = checkSwitch(_pTsMuxUploader, _nTimestamp, 0, 0, nSysNanotime, 0, NULL, 0);
        if (ret != 0) {
                return ret;
        }
        if (pFFTsMuxUploader->nKeyFrameCount == 0) {
                pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                LinkLogDebug("no keyframe. drop audio frame");
                return 0;
        }
        ret = push(pFFTsMuxUploader, _pData, _nDataLen, _nTimestamp, LINK_STREAM_TYPE_AUDIO, 0, nSysNanotime);
        if (ret == 0){
                pFFTsMuxUploader->nFrameCount++;
        }
        if (ret == LINK_NO_MEMORY) {
                if (pFFTsMuxUploader->pTsMuxCtx)
                        pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->NotifyDataPrapared(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_);
        }
        pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
        return ret;
}

int LinkPushPicture(IN LinkTsMuxUploader *_pTsMuxUploader,const char *pFilename,
                                int nFilenameLen, const char *_pBuf, int _nBuflen) {
        
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        assert(pFFTsMuxUploader->pPicUploader != NULL);
        return LinkSendUploadPictureToPictureUploader(pFFTsMuxUploader->pPicUploader, pFilename, nFilenameLen, _pBuf, _nBuflen);
}

static int waitToCompleUploadAndDestroyTsMuxContext(void *_pOpaque)
{
        FFTsMuxContext *pTsMuxCtx = (FFTsMuxContext*)_pOpaque;
        
        if (pTsMuxCtx) {
#ifndef USE_OWN_TSMUX
                if (pTsMuxCtx->pFmtCtx_) {
                        if (pTsMuxCtx->pFmtCtx_->pb) {
                                avio_flush(pTsMuxCtx->pFmtCtx_->pb);
                        }
                }
#endif
                pTsMuxCtx->pTsUploader_->UploadStop(pTsMuxCtx->pTsUploader_);
                
                LinkUploaderStatInfo statInfo = {0};
                pTsMuxCtx->pTsUploader_->GetStatInfo(pTsMuxCtx->pTsUploader_, &statInfo);
                LinkLogDebug("uploader push:%d pop:%d remainItemCount:%d dropped:%d", statInfo.nPushDataBytes_,
                         statInfo.nPopDataBytes_, statInfo.nLen_, statInfo.nDropped);
                LinkDestroyTsUploader(&pTsMuxCtx->pTsUploader_);
#ifdef USE_OWN_TSMUX
                LinkDestroyTsMuxerContext(pTsMuxCtx->pFmtCtx_);
#else
                if (pTsMuxCtx->pFmtCtx_->pb && pTsMuxCtx->pFmtCtx_->pb->buffer)  {
                        av_free(pTsMuxCtx->pFmtCtx_->pb->buffer);
                }
                if (!(pTsMuxCtx->pFmtCtx_->oformat->flags & AVFMT_NOFILE))
                        avio_close(pTsMuxCtx->pFmtCtx_->pb);
                if (pTsMuxCtx->pFmtCtx_->pb) {
                        avio_context_free(&pTsMuxCtx->pFmtCtx_->pb);
                }
                avformat_free_context(pTsMuxCtx->pFmtCtx_);
#endif
                FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)(pTsMuxCtx->pTsMuxUploader);
                if (pFFTsMuxUploader) {
                        pthread_cancel(pFFTsMuxUploader->tokenThread);
                        pthread_join(pFFTsMuxUploader->tokenThread, NULL);
                        if (pFFTsMuxUploader->pVideoMeta) {
                                free(pFFTsMuxUploader->pVideoMeta);
                        }
                        LinkReleaseSegmentHandle(&pFFTsMuxUploader->segmentHandle);
                        LinkDestroyPictureUploader(&pFFTsMuxUploader->pPicUploader);
                        if (pFFTsMuxUploader->pAACBuf) {
                                free(pFFTsMuxUploader->pAACBuf);
                        }
                        if (pFFTsMuxUploader->token_.pToken_) {
                                free(pFFTsMuxUploader->token_.pToken_);
                                pFFTsMuxUploader->token_.pToken_ = NULL;
                        }
                        free(pFFTsMuxUploader);
                }
                free(pTsMuxCtx);
        }
        
        return LINK_SUCCESS;
}

#define getFFmpegErrorMsg(errcode) char msg[128];\
av_strerror(errcode, msg, sizeof(msg))

static void inline setQBufferSize(FFTsMuxUploader *pFFTsMuxUploader, char *desc, int s)
{
        pFFTsMuxUploader->nUploadBufferSize = s;
        LinkLogInfo("desc:(%s) buffer Q size is:%d", desc, pFFTsMuxUploader->nUploadBufferSize);
        return;
}

static int getBufferSize(FFTsMuxUploader *pFFTsMuxUploader) {
        if (pFFTsMuxUploader->nUploadBufferSize != 0) {
                return pFFTsMuxUploader->nUploadBufferSize;
        }
        int nSize = 256*1024;
        int64_t nTotalMemSize = 0;
        int nRet = 0;
#ifdef __APPLE__
        int mib[2];
        size_t length;
        mib[0] = CTL_HW;
        mib[1] = HW_MEMSIZE;
        length = sizeof(int64_t);
        nRet = sysctl(mib, 2, &nTotalMemSize, &length, NULL, 0);
#else
        struct sysinfo info = {0};
        nRet = sysinfo(&info);
        nTotalMemSize = info.totalram;
#endif
        if (nRet != 0) {
                setQBufferSize(pFFTsMuxUploader, "default", nSize);
                return pFFTsMuxUploader->nUploadBufferSize;
        }
        LinkLogInfo("total memory size:%"PRId64"\n", nTotalMemSize);
        
        int64_t M = 1024 * 1024;
        if (nTotalMemSize <= 32 * M) {
                setQBufferSize(pFFTsMuxUploader, "le 32M", nSize);
                return pFFTsMuxUploader->nUploadBufferSize;
        } else if (nTotalMemSize <= 64 * M) {
                setQBufferSize(pFFTsMuxUploader, "le 64M", nSize * 2);
                return pFFTsMuxUploader->nUploadBufferSize;
        } else if (nTotalMemSize <= 128 * M) {
                setQBufferSize(pFFTsMuxUploader, "le 128M", nSize * 3);
                return pFFTsMuxUploader->nUploadBufferSize;
        } else if (nTotalMemSize <= 256 * M) {
                setQBufferSize(pFFTsMuxUploader, "le 256M", nSize * 4);
                return pFFTsMuxUploader->nUploadBufferSize;
        } else if (nTotalMemSize <= 512 * M) {
                setQBufferSize(pFFTsMuxUploader, "le 512M", nSize * 6);
                return pFFTsMuxUploader->nUploadBufferSize;
        } else if (nTotalMemSize <= 1024 * M) {
                setQBufferSize(pFFTsMuxUploader, "le 1G", nSize * 8);
                return pFFTsMuxUploader->nUploadBufferSize;
        } else if (nTotalMemSize <= 2 * 1024 * M) {
                setQBufferSize(pFFTsMuxUploader, "le 2G", nSize * 10);
                return pFFTsMuxUploader->nUploadBufferSize;
        } else if (nTotalMemSize <= 4 * 1024 * M) {
                setQBufferSize(pFFTsMuxUploader, "le 4G", nSize * 12);
                return pFFTsMuxUploader->nUploadBufferSize;
        } else {
                setQBufferSize(pFFTsMuxUploader, "gt 4G", nSize * 16);
                return pFFTsMuxUploader->nUploadBufferSize;
        }
}

static int newTsMuxContext(FFTsMuxContext ** _pTsMuxCtx, LinkMediaArg *_pAvArg, LinkTsUploadArg *_pUploadArg,
                           int nQBufSize, enum CircleQueuePolicy queueType)
#ifdef USE_OWN_TSMUX
{
        FFTsMuxContext * pTsMuxCtx = (FFTsMuxContext *)malloc(sizeof(FFTsMuxContext));
        if (pTsMuxCtx == NULL) {
                return LINK_NO_MEMORY;
        }
        memset(pTsMuxCtx, 0, sizeof(FFTsMuxContext));
        
        int ret = LinkNewTsUploader(&pTsMuxCtx->pTsUploader_, _pUploadArg, queueType, 188, nQBufSize / 188);
        if (ret != 0) {
                free(pTsMuxCtx);
                return ret;
        }
        
        LinkTsMuxerArg avArg;
        avArg.nAudioFormat = _pAvArg->nAudioFormat;
        avArg.nAudioChannels = _pAvArg->nChannels;
        avArg.nAudioSampleRate = _pAvArg->nSamplerate;
        
        avArg.output = (LinkTsPacketCallback)writeTsPacketToMem;
        avArg.nVideoFormat = _pAvArg->nVideoFormat;
        avArg.pOpaque = pTsMuxCtx;
        avArg.setKeyframeMetaInfo = LinkAppendKeyframeMetaInfo;
        avArg.pMetaInfoUserArg = pTsMuxCtx->pTsUploader_;
        
        ret = LinkNewTsMuxerContext(&avArg, &pTsMuxCtx->pFmtCtx_);
        if (ret != 0) {
                LinkDestroyTsUploader(&pTsMuxCtx->pTsUploader_);
                free(pTsMuxCtx);
                return ret;
        }
        
        
        pTsMuxCtx->asyncWait.function = waitToCompleUploadAndDestroyTsMuxContext;
        * _pTsMuxCtx = pTsMuxCtx;
        return LINK_SUCCESS;
}
#else
{
        FFTsMuxContext * pTsMuxCtx = (FFTsMuxContext *)malloc(sizeof(FFTsMuxContext));
        if (pTsMuxCtx == NULL) {
                return LINK_NO_MEMORY;
        }
        memset(pTsMuxCtx, 0, sizeof(FFTsMuxContext));
        
        int nBufsize = getBufferSize();
        int ret = LinkNewTsUploader(&pTsMuxCtx->pTsUploader_, _pUploadArg, queueType, FF_OUT_LEN, nBufsize / FF_OUT_LEN);
        if (ret != 0) {
                free(pTsMuxCtx);
                return ret;
        }
        
        uint8_t *pOutBuffer = NULL;
        //Output
        ret = avformat_alloc_output_context2(&pTsMuxCtx->pFmtCtx_, NULL, "mpegts", NULL);
        if (ret < 0) {
                getFFmpegErrorMsg(ret);
                LinkLogError("Could not create output context:%d(%s)", ret, msg);
                ret = LINK_NO_MEMORY;
                goto end;
        }
        AVOutputFormat *pOutFmt = pTsMuxCtx->pFmtCtx_->oformat;
        pOutBuffer = (unsigned char*)av_malloc(4096);
        AVIOContext *avio_out = avio_alloc_context(pOutBuffer, 4096, 1, pTsMuxCtx, NULL, writeTsPacketToMem, NULL);
        pTsMuxCtx->pFmtCtx_->pb = avio_out;
        pTsMuxCtx->pFmtCtx_->flags = AVFMT_FLAG_CUSTOM_IO;
        pOutFmt->flags |= AVFMT_NOFILE;
        pOutFmt->flags |= AVFMT_NODIMENSIONS;
        //ofmt->video_codec //是否指定为ifmt_ctx_v的视频的codec_type.同理音频也一样
        //测试下来即使video_codec和ifmt_ctx_v的视频的codec_type不一样也是没有问题的
        
        //add video
        AVStream *pOutStream = avformat_new_stream(pTsMuxCtx->pFmtCtx_, NULL);
        if (!pOutStream) {
                getFFmpegErrorMsg(ret);
                LinkLogError("Failed allocating output stream:%d(%s)", ret, msg);
                ret = LINK_NO_MEMORY;
                goto end;
        }
        pOutStream->time_base.num = 1;
        pOutStream->time_base.den = 90000;
        pTsMuxCtx->nOutVideoindex_ = pOutStream->index;
        pOutStream->codecpar->codec_tag = 0;
        pOutStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        if (_pAvArg->nVideoFormat == LINK_VIDEO_H264)
                pOutStream->codecpar->codec_id = AV_CODEC_ID_H264;
                else
                        pOutStream->codecpar->codec_id = AV_CODEC_ID_H265;
                        //end add video
                        
                        //add audio
                        pOutStream = avformat_new_stream(pTsMuxCtx->pFmtCtx_, NULL);
                        if (!pOutStream) {
                                getFFmpegErrorMsg(ret);
                                LinkLogError("Failed allocating output stream:%d(%s)", ret, msg);
                                ret = LINK_NO_MEMORY;
                                goto end;
                        }
        pOutStream->time_base.num = 1;
        pOutStream->time_base.den = 90000;
        pTsMuxCtx->nOutAudioindex_ = pOutStream->index;
        pOutStream->codecpar->codec_tag = 0;
        pOutStream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
        switch(_pAvArg->nAudioFormat){
                case LINK_AUDIO_PCMU:
                        pOutStream->codecpar->codec_id = AV_CODEC_ID_PCM_MULAW;
                        break;
                case LINK_AUDIO_PCMA:
                        pOutStream->codecpar->codec_id = AV_CODEC_ID_PCM_ALAW;
                        break;
                case LINK_AUDIO_AAC:
                        pOutStream->codecpar->codec_id = AV_CODEC_ID_AAC;
                        break;
        }
        pOutStream->codecpar->sample_rate = _pAvArg->nSamplerate;
        pOutStream->codecpar->channels = _pAvArg->nChannels;
        pOutStream->codecpar->channel_layout = av_get_default_channel_layout(pOutStream->codecpar->channels);
        //end add audio
        
        //printf("==========Output Information==========\n");
        //av_dump_format(pTsMuxCtx->pFmtCtx_, 0, "xx.ts", 1);
        //printf("======================================\n");
        
        //Open output file
        if (!(pOutFmt->flags & AVFMT_NOFILE)) {
                if ((ret = avio_open(&pTsMuxCtx->pFmtCtx_->pb, "xx.ts", AVIO_FLAG_WRITE)) < 0) {
                        getFFmpegErrorMsg(ret);
                        LinkLogError("Could not open output:%d(%s)", ret, msg);
                        ret = LINK_OPEN_TS_ERR;
                        goto end;
                }
        }
        //Write file header
        int erno = 0;
        if ((erno = avformat_write_header(pTsMuxCtx->pFmtCtx_, NULL)) < 0) {
                getFFmpegErrorMsg(erno);
                LinkLogError("fail to write ts header:%d(%s)", erno, msg);
                ret = LINK_WRITE_TS_ERR;
                goto end;
        }
        
        pTsMuxCtx->asyncWait.function = waitToCompleUploadAndDestroyTsMuxContext;
        *_pTsMuxCtx = pTsMuxCtx;
        return LINK_SUCCESS;
end:
        if (pOutBuffer) {
                av_free(pOutBuffer);
        }
        if (pTsMuxCtx->pFmtCtx_->pb)
                avio_context_free(&pTsMuxCtx->pFmtCtx_->pb);
                if (pTsMuxCtx->pFmtCtx_) {
                        if (pTsMuxCtx->pFmtCtx_ && !(pOutFmt->flags & AVFMT_NOFILE))
                                avio_close(pTsMuxCtx->pFmtCtx_->pb);
                        avformat_free_context(pTsMuxCtx->pFmtCtx_);
                }
        if (pTsMuxCtx->pTsUploader_)
                DestroyUploader(&pTsMuxCtx->pTsUploader_);
                
                return ret;
}
#endif

static void setToken(FFTsMuxUploader* _PTsMuxUploader, char *_pToken, int _nTokenLen)
{
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)_PTsMuxUploader;
        pthread_mutex_lock(&pFFTsMuxUploader->tokenMutex_);
        if (pFFTsMuxUploader->token_.pToken_ != NULL) {
                
                free(pFFTsMuxUploader->token_.pToken_);
        }
        pFFTsMuxUploader->token_.pToken_ = _pToken;
        pFFTsMuxUploader->token_.nTokenLen_ = _nTokenLen;
        
        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
        return ;
}

static void upadateSegmentId(void *_pOpaque, void* pArg, int64_t nNow, int64_t nEnd)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)_pOpaque;
        LinkTsUploadArg *_pUploadArg = (LinkTsUploadArg *)pArg;

        LinkUpdateSegment(pFFTsMuxUploader->segmentHandle, nNow/1000000, nEnd/1000000, 0);
        
        if (pFFTsMuxUploader->uploadArg.nSegmentId_ == 0) {
                pFFTsMuxUploader->uploadArg.nLastUploadTsTime_ = _pUploadArg->nLastUploadTsTime_;
                pFFTsMuxUploader->uploadArg.nSegmentId_ = _pUploadArg->nSegmentId_;
                return;
        }

        int64_t nDiff = pFFTsMuxUploader->nUpdateSegmentInterval * 1000000000LL;
        if (nNow - pFFTsMuxUploader->uploadArg.nLastUploadTsTime_ >= nDiff) {
                pFFTsMuxUploader->uploadArg.nSegmentId_ = nNow;
                _pUploadArg->nSegmentId_ = nNow;
        }
        pFFTsMuxUploader->uploadArg.nLastUploadTsTime_ = _pUploadArg->nLastUploadTsTime_;
        return;
}

static void setUploaderBufferSize(LinkTsMuxUploader* _pTsMuxUploader, int nBufferSize)
{
        if (nBufferSize < 256) {
                LinkLogWarn("setUploaderBufferSize is to small:%d. ge 256 required", nBufferSize);
                return;
        }
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)_pTsMuxUploader;
        pFFTsMuxUploader->nUploadBufferSize = nBufferSize * 1024;
}

static int getUploaderBufferUsedSize(LinkTsMuxUploader* _pTsMuxUploader)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)_pTsMuxUploader;
        LinkUploaderStatInfo info;
        pthread_mutex_lock(&pFFTsMuxUploader->muxUploaderMutex_);
        int nUsed = 0;
        if (pFFTsMuxUploader->pTsMuxCtx) {
                pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->GetStatInfo(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_, &info);
                nUsed = info.nPushDataBytes_ - info.nPopDataBytes_;
        } else {
                return 0;
        }
        pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
        return nUsed + (nUsed/188) * 4;
}

static void setUpdateSegmentInterval(LinkTsMuxUploader* _pTsMuxUploader, int nInterval)
{
        if (nInterval < 15) {
                LinkLogWarn("setNewSegmentInterval is to small:%d. ge 15 required", nInterval);
                return;
        }
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)_pTsMuxUploader;
        pFFTsMuxUploader->nUpdateSegmentInterval = nInterval;
}

int linkNewTsMuxUploader(LinkTsMuxUploader **_pTsMuxUploader, const LinkMediaArg *_pAvArg, const LinkUserUploadArg *_pUserUploadArg, int isWithPicAndSeg)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)malloc(sizeof(FFTsMuxUploader) + _pUserUploadArg->nUpTokenRequestUrlLen + 1);
        if (pFFTsMuxUploader == NULL) {
                return LINK_NO_MEMORY;
        }
        memset(pFFTsMuxUploader, 0, sizeof(FFTsMuxUploader));
        
        int ret = 0;
        
        if (_pUserUploadArg->nDeviceIdLen_ >= sizeof(pFFTsMuxUploader->deviceId_)) {
                free(pFFTsMuxUploader);
                LinkLogError("device(ua) max support lenght is 32");
                return LINK_ARG_TOO_LONG;
        }
        memcpy(pFFTsMuxUploader->deviceId_, _pUserUploadArg->pDeviceId_, _pUserUploadArg->nDeviceIdLen_);
        pFFTsMuxUploader->uploadArg.pDeviceId_ = pFFTsMuxUploader->deviceId_;
        
        
        if (isWithPicAndSeg) {
                pFFTsMuxUploader->uploadArg.pUploadArgKeeper_ = pFFTsMuxUploader;
                pFFTsMuxUploader->uploadArg.UploadSegmentIdUpadate = upadateSegmentId;
        } else {
                pFFTsMuxUploader->uploadArg.UploadSegmentIdUpadate = NULL;
                pFFTsMuxUploader->uploadArg.pUploadArgKeeper_ = NULL;
        }
        
        //pFFTsMuxUploader->uploadArg.uploadZone = _pUserUploadArg->uploadZone_; //TODO
        pFFTsMuxUploader->uploadArg.getUploadParamCallback = getUploadParamCallback;
        pFFTsMuxUploader->uploadArg.pGetUploadParamCallbackArg = pFFTsMuxUploader;
        pFFTsMuxUploader->uploadArg.pUploadStatisticCb = _pUserUploadArg->pUploadStatisticCb;
        pFFTsMuxUploader->uploadArg.pUploadStatArg = _pUserUploadArg->pUploadStatArg;
        pFFTsMuxUploader->uploadArg.useHttps = _pUserUploadArg->useHttps;
        pFFTsMuxUploader->pUpTokenRequestUrl = (char *)(pFFTsMuxUploader) + sizeof(FFTsMuxUploader);
        if (pFFTsMuxUploader->pUpTokenRequestUrl) {
                memcpy(pFFTsMuxUploader->pUpTokenRequestUrl, _pUserUploadArg->pUpTokenRequestUrl, _pUserUploadArg->nUpTokenRequestUrlLen);
                pFFTsMuxUploader->pUpTokenRequestUrl[_pUserUploadArg->nUpTokenRequestUrlLen] = 0;
        }
        
        pFFTsMuxUploader->nUpdateSegmentInterval = 30;
        
        pFFTsMuxUploader->nFirstTimestamp = -1;
        
        ret = pthread_mutex_init(&pFFTsMuxUploader->muxUploaderMutex_, NULL);
        if (ret != 0){
                free(pFFTsMuxUploader);
                return LINK_MUTEX_ERROR;
        }
        ret = pthread_mutex_init(&pFFTsMuxUploader->tokenMutex_, NULL);
        if (ret != 0){
                pthread_mutex_destroy(&pFFTsMuxUploader->muxUploaderMutex_);
                free(pFFTsMuxUploader);
                return LINK_MUTEX_ERROR;
        }
        
        pFFTsMuxUploader->tsMuxUploader_.PushAudio = PushAudio;
        pFFTsMuxUploader->tsMuxUploader_.PushVideo = PushVideo;
        pFFTsMuxUploader->tsMuxUploader_.SetUploaderBufferSize = setUploaderBufferSize;
        pFFTsMuxUploader->tsMuxUploader_.GetUploaderBufferUsedSize = getUploaderBufferUsedSize;
        pFFTsMuxUploader->tsMuxUploader_.SetUpdateSegmentInterval = setUpdateSegmentInterval;
        pFFTsMuxUploader->queueType_ = TSQ_APPEND;// TSQ_FIX_LENGTH;
        pFFTsMuxUploader->segmentHandle = LINK_INVALIE_SEGMENT_HANDLE;
        
        pFFTsMuxUploader->avArg = *_pAvArg;
        
        ret = linkTsMuxUploaderTokenThreadStart(pFFTsMuxUploader);
        if (ret != LINK_SUCCESS){
                pthread_mutex_destroy(&pFFTsMuxUploader->muxUploaderMutex_);
                pthread_mutex_destroy(&pFFTsMuxUploader->tokenMutex_);
                free(pFFTsMuxUploader);
                return ret;
        }
        
        *_pTsMuxUploader = (LinkTsMuxUploader *)pFFTsMuxUploader;
        
        return LINK_SUCCESS;
}

int LinkNewTsMuxUploader(LinkTsMuxUploader **_pTsMuxUploader, const LinkMediaArg *_pAvArg, const LinkUserUploadArg *_pUserUploadArg) {
        return linkNewTsMuxUploader(_pTsMuxUploader, _pAvArg, _pUserUploadArg, 0);
}

static int getUploadParamCallback(IN void *pOpaque, IN OUT LinkUploadParam *pParam) {
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)pOpaque;
        pthread_mutex_lock(&pFFTsMuxUploader->tokenMutex_);
        if (pFFTsMuxUploader->token_.pToken_ == NULL) {
                pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                return LINK_NOT_INITED;
        }
        if (pParam->nTokenBufLen - 1 < pFFTsMuxUploader->token_.nTokenLen_) {
                LinkLogError("get token buffer is small:%d %d", pFFTsMuxUploader->token_.nTokenLen_, pParam->nTokenBufLen);
                pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                return LINK_BUFFER_IS_SMALL;
        }
        memcpy(pParam->pTokenBuf, pFFTsMuxUploader->token_.pToken_, pFFTsMuxUploader->token_.nTokenLen_);
        pParam->nTokenBufLen = pFFTsMuxUploader->token_.nTokenLen_;
        pParam->pTokenBuf[pFFTsMuxUploader->token_.nTokenLen_] = 0;
        
        if (pParam->pTypeBuf != NULL && pParam->nTypeBufLen != 0) {
                int nTypeLen = strlen(pFFTsMuxUploader->tsType);
                if (pParam->nTypeBufLen > nTypeLen) {
                        memcpy(pParam->pTypeBuf, pFFTsMuxUploader->tsType, nTypeLen);
                        pParam->pTypeBuf[nTypeLen] = 0;
                        pParam->nTypeBufLen = nTypeLen;
                } else {
                        pParam->nTypeBufLen = 0;
                }
        }
        if (pFFTsMuxUploader->isTypeOneshot) {
                pFFTsMuxUploader->isTypeOneshot = 0;
                memset(pFFTsMuxUploader->tsType, 0, sizeof(pFFTsMuxUploader->tsType));
        }
        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
        pParam->nTokenBufLen = pFFTsMuxUploader->token_.nTokenLen_;
        return LINK_SUCCESS;
}

int LinkNewTsMuxUploaderWillPicAndSeg(LinkTsMuxUploader **_pTsMuxUploader, const LinkMediaArg *_pAvArg,
                                            const LinkUserUploadArg *_pUserUploadArg, const LinkPicUploadArg *_pPicArg) {
        
        //LinkTsMuxUploader *pTsMuxUploader
        int ret = linkNewTsMuxUploader(_pTsMuxUploader, _pAvArg, _pUserUploadArg, 1);
        if (ret != LINK_SUCCESS) {
                return ret;
        }
        
        ret = LinkInitSegmentMgr();
        if (ret != LINK_SUCCESS) {
                LinkDestroyTsMuxUploader(_pTsMuxUploader);
                LinkLogError("LinkInitSegmentMgr fail:%d", ret);
                return ret;
        }
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)(*_pTsMuxUploader);
        
        SegmentHandle segHandle;
        SegmentArg arg;
        arg.getUploadParamCallback = getUploadParamCallback;
        arg.pGetUploadParamCallbackArg = *_pTsMuxUploader;
        arg.pDeviceId = pFFTsMuxUploader->deviceId_;
        arg.nDeviceIdLen = _pUserUploadArg->nDeviceIdLen_;
        arg.pMgrTokenRequestUrl = _pUserUploadArg->pMgrTokenRequestUrl;
        arg.nMgrTokenRequestUrlLen = strlen(_pUserUploadArg->pMgrTokenRequestUrl);
        arg.pUploadStatisticCb = _pUserUploadArg->pUploadStatisticCb;
        arg.pUploadStatArg = _pUserUploadArg->pUploadStatArg;
        arg.useHttps = _pUserUploadArg->useHttps;
        arg.nUpdateIntervalSeconds = _pUserUploadArg->nUpdateIntervalSeconds;
        ret = LinkNewSegmentHandle(&segHandle, &arg);
        if (ret != LINK_SUCCESS) {
                LinkDestroyTsMuxUploader(_pTsMuxUploader);
                LinkLogError("LinkInitSegmentMgr fail:%d", ret);
                return ret;
        }
        pFFTsMuxUploader->segmentHandle = segHandle;
        
        LinkPicUploadFullArg fullArg;
        fullArg.getPicCallback = _pPicArg->getPicCallback;
        fullArg.pGetPicCallbackOpaque = _pPicArg->pGetPicCallbackOpaque;
        fullArg.getUploadParamCallback = getUploadParamCallback;
        fullArg.pGetUploadParamCallbackOpaque = *_pTsMuxUploader;
        fullArg.pUploadStatisticCb = _pUserUploadArg->pUploadStatisticCb;
        fullArg.pUploadStatArg = _pUserUploadArg->pUploadStatArg;
        fullArg.useHttps = _pUserUploadArg->useHttps;
        PictureUploader *pPicUploader;
        ret = LinkNewPictureUploader(&pPicUploader, &fullArg);
        if (ret != LINK_SUCCESS) {
                LinkDestroyTsMuxUploader(_pTsMuxUploader);
                LinkLogError("LinkNewPictureUploader fail:%d", ret);
                return ret;
        }
        
        pFFTsMuxUploader->pPicUploader = pPicUploader;
        return ret;
}

void LinkSetSegmentUpdateInterval(IN LinkTsMuxUploader *_pTsMuxUploader, int64_t _nSeconds) {
        if (_pTsMuxUploader == NULL || _nSeconds <= 0) {
                return;
        }
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        
        LinkSetSegmentUpdateInt(pFFTsMuxUploader->segmentHandle, _nSeconds);
        return;
}

int LinkSetTsTypeOneshot(IN LinkTsMuxUploader *_pTsMuxUploader, const char *_pType, IN int nTypeLen) {
        if (_pTsMuxUploader == NULL || _pType == NULL) {
                return LINK_ARG_ERROR;
        }
        
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        pthread_mutex_lock(&pFFTsMuxUploader->tokenMutex_);
        if (nTypeLen + strlen(pFFTsMuxUploader->tsType) >= sizeof(pFFTsMuxUploader->tsType) - 1) {
                return LINK_ARG_TOO_LONG;
        }
        pFFTsMuxUploader->isTypeOneshot = 1;
        memcpy(pFFTsMuxUploader->tsType + strlen(pFFTsMuxUploader->tsType), _pType, nTypeLen);
        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
        
        return LINK_SUCCESS;
}

int LinkSetTsType(IN LinkTsMuxUploader *_pTsMuxUploader, const char *_pType, IN int nTypeLen) {
        if (_pTsMuxUploader == NULL || _pType == NULL) {
                return LINK_ARG_ERROR;
        }
        
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        pthread_mutex_lock(&pFFTsMuxUploader->tokenMutex_);
        if (nTypeLen + strlen(pFFTsMuxUploader->tsType) >= sizeof(pFFTsMuxUploader->tsType) - 1) {
                return LINK_ARG_TOO_LONG;
        }
        pFFTsMuxUploader->isTypeOneshot = 0;
        memcpy(pFFTsMuxUploader->tsType + strlen(pFFTsMuxUploader->tsType), _pType, nTypeLen);
        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
        
        return LINK_SUCCESS;
}

void LinkClearTsType(IN LinkTsMuxUploader *_pTsMuxUploader) {
        if (_pTsMuxUploader == NULL) {
                return;
        }
        
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        pthread_mutex_lock(&pFFTsMuxUploader->tokenMutex_);
        pFFTsMuxUploader->isTypeOneshot = 0;
        memset(pFFTsMuxUploader->tsType, 0, sizeof(pFFTsMuxUploader->tsType));
        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
        
        return;
}

int LinkPauseUpload(IN LinkTsMuxUploader *_pTsMuxUploader) {
        if (_pTsMuxUploader == NULL ) {
                return LINK_ARG_ERROR;
        }
        int ret = LINK_SUCCESS;
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        pthread_mutex_lock(&pFFTsMuxUploader->muxUploaderMutex_);
        pFFTsMuxUploader->isPause = 1;
        
        pFFTsMuxUploader->nKeyFrameCount = 0;
        pFFTsMuxUploader->nFrameCount = 0;
        pFFTsMuxUploader->nFirstTimestamp = 0;
        pFFTsMuxUploader->ffMuxSatte = LINK_UPLOAD_INIT;
        pushRecycle(pFFTsMuxUploader);
        ret = LinkTsMuxUploaderStart(_pTsMuxUploader);
        
        pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);

        return ret;
}

int LinkResumeUpload(IN LinkTsMuxUploader *_pTsMuxUploader) {
        if (_pTsMuxUploader == NULL ) {
                return LINK_ARG_ERROR;
        }
        
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        
        pthread_mutex_lock(&pFFTsMuxUploader->muxUploaderMutex_);
        pFFTsMuxUploader->isPause = 0;
        pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
        
        return LINK_SUCCESS;
}

static void linkCapturePictureCallback(void *pOpaque, int64_t nTimestamp) {
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)pOpaque;
        if (pFFTsMuxUploader->pPicUploader)
                LinkSendGetPictureSingalToPictureUploader(pFFTsMuxUploader->pPicUploader, pFFTsMuxUploader->uploadArg.pDeviceId_,
                                                  strlen(pFFTsMuxUploader->uploadArg.pDeviceId_), nTimestamp);
}

int LinkTsMuxUploaderStart(LinkTsMuxUploader *_pTsMuxUploader)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        
        assert(pFFTsMuxUploader->pTsMuxCtx == NULL);
        
        int nBufsize = getBufferSize(pFFTsMuxUploader);
        int ret = newTsMuxContext(&pFFTsMuxUploader->pTsMuxCtx, &pFFTsMuxUploader->avArg,
                                  &pFFTsMuxUploader->uploadArg, nBufsize, pFFTsMuxUploader->queueType_);
        if (ret != 0) {
                free(pFFTsMuxUploader);
                return ret;
        }
        //LinkTsUploaderSetTsStartUploadCallback(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_, linkCapturePictureCallback, pFFTsMuxUploader);
        
        pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->UploadStart(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_);
        return LINK_SUCCESS;
}

static int linkTsMuxUploaderSetUploadZone(FFTsMuxUploader *pFFTsMuxUploader, LinkUploadZone _upzone) {
        
        if (pFFTsMuxUploader->pPicUploader) {
                LinkPicUploaderSetUploadZone(pFFTsMuxUploader->pPicUploader, _upzone);
        }
        if (pFFTsMuxUploader->segmentHandle != LINK_INVALIE_SEGMENT_HANDLE) {
                LinkSetSegmentUploadZone(pFFTsMuxUploader->segmentHandle, _upzone);
        }
        return LINK_SUCCESS;
}

void LinkFlushUploader(IN LinkTsMuxUploader *_pTsMuxUploader) {
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)(_pTsMuxUploader);
        if (pFFTsMuxUploader->queueType_ != TSQ_APPEND) {
                return;
        }
        
        pthread_mutex_lock(&pFFTsMuxUploader->muxUploaderMutex_);
        if (pFFTsMuxUploader->pTsMuxCtx)
                pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->NotifyDataPrapared(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_);
        pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
        return;
}

void LinkDestroyTsMuxUploader(LinkTsMuxUploader **_pTsMuxUploader)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)(*_pTsMuxUploader);
        
        pthread_mutex_lock(&pFFTsMuxUploader->muxUploaderMutex_);
        if (pFFTsMuxUploader->pTsMuxCtx) {
                pFFTsMuxUploader->pTsMuxCtx->pTsMuxUploader = (LinkTsMuxUploader*)pFFTsMuxUploader;
        }
        pFFTsMuxUploader->isQuit = 1;
        pushRecycle(pFFTsMuxUploader);
        pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
        pthread_mutex_destroy(&pFFTsMuxUploader->tokenMutex_);
        pthread_mutex_destroy(&pFFTsMuxUploader->muxUploaderMutex_);
        *_pTsMuxUploader = NULL;
        return;
}

static void *linkTokenThread(void * pOpaque) {
        FFTsMuxUploader* pFFTsMuxUploader = (FFTsMuxUploader*) pOpaque;
        
        int sleepTime = 1;
        while(!pFFTsMuxUploader->isQuit) {
                char *pBuf = (char *)malloc(1024);
                memset(pBuf, 0, 1024);
                LinkUploadZone upzone = LINK_ZONE_UNKNOWN;
                int nDeadline = 0;
                int ret = LinkGetUploadToken(pBuf, 1024, &upzone, &nDeadline, pFFTsMuxUploader->pUpTokenRequestUrl);
                if (ret != LINK_SUCCESS) {
                        LinkLogError("LinkGetUploadToken fail:%d, try after %d sec", ret, sleepTime);
                        sleepTime *= 2;
                        if (sleepTime > 10) {
                                sleepTime = 10;
                        }
                        sleep(sleepTime);
                } else {
                        sleepTime = 1;
                        linkTsMuxUploaderSetUploadZone(pFFTsMuxUploader, upzone);
                        setToken(pFFTsMuxUploader, pBuf, strlen(pBuf));
                        int64_t now = LinkGetCurrentNanosecond() / 1000000000;
                        int64_t st = nDeadline - now - 60;
                        LinkLogInfo("upload token to %"PRId64" - %d = sleep:%d", now, nDeadline, (int)st);
                        sleep((int)st);
                }
        }
        
        return NULL;
}

static int linkTsMuxUploaderTokenThreadStart(FFTsMuxUploader* pFFTsMuxUploader) {
        
        int ret = pthread_create(&pFFTsMuxUploader->tokenThread, NULL, linkTokenThread, pFFTsMuxUploader);
        if (ret != 0) {
                return LINK_THREAD_ERROR;
        }

        return LINK_SUCCESS;
}
