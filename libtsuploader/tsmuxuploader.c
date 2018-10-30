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
        int nQuit;
        char * pPrevToken_;
        int nPrevTokenLen_;
        char * pToken_;
        int nTokenLen_;
        pthread_mutex_t tokenMutex_;
}Token;

typedef struct _FFTsMuxUploader{
        LinkTsMuxUploader tsMuxUploader_;
        pthread_mutex_t muxUploaderMutex_;
        unsigned char *pAACBuf;
        int nAACBufLen;
        FFTsMuxContext *pTsMuxCtx;
        
        int64_t nLastTimestamp;
        int64_t nFirstTimestamp; //initial to -1
        int nKeyFrameCount;
        int nFrameCount;
        LinkMediaArg avArg;
        LinkUploadState ffMuxSatte;
        
        int nUploadBufferSize;
        int nNewSegmentInterval;
        
        char deviceId_[65];
        Token token_;
        LinkUploadArg uploadArg;
        PictureUploader *pPicUploader;
        enum CircleQueuePolicy queueType_;
}FFTsMuxUploader;

static int aAacfreqs[13] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050 ,16000 ,12000, 11025, 8000, 7350};

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
                        LinkLogDebug("write ts to queue overwrite:%d", ret);
                } else {
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

static int push(FFTsMuxUploader *pFFTsMuxUploader, char * _pData, int _nDataLen, int64_t _nTimestamp, int _nFlag){
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
        int isAdtsAdded = 0;
        
        if (_nFlag == LINK_STREAM_TYPE_AUDIO){
                //fprintf(stderr, "audio frame: len:%d pts:%lld\n", _nDataLen, _nTimestamp);
                if (pTsMuxCtx->nPrevAudioTimestamp != 0 && _nTimestamp - pTsMuxCtx->nPrevAudioTimestamp <= 0) {
                        LinkLogWarn("audio pts not monotonically: prev:%lld now:%lld", pTsMuxCtx->nPrevAudioTimestamp, _nTimestamp);
                        return 0;
                }
#ifndef USE_OWN_TSMUX
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
                        isAdtsAdded = 1;
#ifdef USE_OWN_TSMUX
                        LinkMuxerAudio(pTsMuxCtx->pFmtCtx_, (uint8_t *)pFFTsMuxUploader->pAACBuf, varHeader.aac_frame_length, _nTimestamp);
#else
                        pkt.data = (uint8_t *)pFFTsMuxUploader->pAACBuf;
                        pkt.size = varHeader.aac_frame_length;
#endif
                } 
#ifdef USE_OWN_TSMUX
                else {
                        ret = LinkMuxerAudio(pTsMuxCtx->pFmtCtx_, (uint8_t*)_pData, _nDataLen, _nTimestamp);
                }
#endif
        }else{
                //fprintf(stderr, "video frame: len:%d pts:%lld\n", _nDataLen, _nTimestamp);
                if (pTsMuxCtx->nPrevVideoTimestamp != 0 && _nTimestamp - pTsMuxCtx->nPrevVideoTimestamp <= 0) {
                        LinkLogWarn("video pts not monotonically: prev:%lld now:%lld", pTsMuxCtx->nPrevVideoTimestamp, _nTimestamp);
                        return 0;
                }
#ifdef USE_OWN_TSMUX
                ret = LinkMuxerVideo(pTsMuxCtx->pFmtCtx_, (uint8_t*)_pData, _nDataLen, _nTimestamp);
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
                pTsMuxCtx->pTsUploader_->RecordTimestamp(pTsMuxCtx->pTsUploader_, _nTimestamp);
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

static int checkSwitch(LinkTsMuxUploader *_pTsMuxUploader, int64_t _nTimestamp, int nIsKeyFrame, int _isVideo, int _nIsSegStart)
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
        // if start new uploader, start from keyframe
        if ((_isVideo && nIsKeyFrame) || shouldSwitch) {
                if( ((_nTimestamp - pFFTsMuxUploader->nFirstTimestamp) > 4980 && pFFTsMuxUploader->nKeyFrameCount > 0)
                   //at least 1 keyframe and aoubt last 5 second
                   || (_nIsSegStart && pFFTsMuxUploader->nFrameCount != 0)// new segment is specified
                   ||  pFFTsMuxUploader->ffMuxSatte != LINK_UPLOAD_INIT){   // upload finished
                        //printf("next ts:%d %lld\n", pFFTsMuxUploader->nKeyFrameCount, _nTimestamp - pFFTsMuxUploader->nLastUploadVideoTimestamp);
                        pFFTsMuxUploader->nKeyFrameCount = 0;
                        pFFTsMuxUploader->nFrameCount = 0;
                        pFFTsMuxUploader->nFirstTimestamp = _nTimestamp;
                        pFFTsMuxUploader->ffMuxSatte = LINK_UPLOAD_INIT;
                        pushRecycle(pFFTsMuxUploader);
                        if (_nIsSegStart) {
                                pFFTsMuxUploader->uploadArg.nSegmentId_ = LinkGetCurrentNanosecond();
                        }
                        ret = LinkTsMuxUploaderStart(_pTsMuxUploader);
                        if (ret != 0) {
                                return ret;
                        }
                }
                if (_isVideo && nIsKeyFrame) {
                        pFFTsMuxUploader->nKeyFrameCount++;
                }
        }
        return 0;
}

static int PushVideo(LinkTsMuxUploader *_pTsMuxUploader, char * _pData, int _nDataLen, int64_t _nTimestamp, int nIsKeyFrame, int _nIsSegStart)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        pthread_mutex_lock(&pFFTsMuxUploader->muxUploaderMutex_);
        int ret = 0;

        if (pFFTsMuxUploader->nKeyFrameCount == 0 && !nIsKeyFrame) {
                LinkLogWarn("first video frame not IDR. drop this frame\n");
                pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                return 0;
        }
        ret = checkSwitch(_pTsMuxUploader, _nTimestamp, nIsKeyFrame, 1, _nIsSegStart);
        if (ret != 0) {
                return ret;
        }
        if (pFFTsMuxUploader->nKeyFrameCount == 0 && !nIsKeyFrame) {
                LinkLogWarn("first video frame not IDR. drop this frame\n");
                pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                return 0;
        }
        
        ret = push(pFFTsMuxUploader, _pData, _nDataLen, _nTimestamp, LINK_STREAM_TYPE_VIDEO);
        if (ret == 0){
                pFFTsMuxUploader->nFrameCount++;
        }
        pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
        return ret;
}

static int PushAudio(LinkTsMuxUploader *_pTsMuxUploader, char * _pData, int _nDataLen, int64_t _nTimestamp)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        pthread_mutex_lock(&pFFTsMuxUploader->muxUploaderMutex_);
        int ret = checkSwitch(_pTsMuxUploader, _nTimestamp, 0, 0, 0);
        if (ret != 0) {
                return ret;
        }
        if (pFFTsMuxUploader->nKeyFrameCount == 0) {
                pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                LinkLogDebug("no keyframe. drop audio frame");
                return 0;
        }
        ret = push(pFFTsMuxUploader, _pData, _nDataLen, _nTimestamp, LINK_STREAM_TYPE_AUDIO);
        if (ret == 0){
                pFFTsMuxUploader->nFrameCount++;
        }
        pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
        return ret;
}

int LinkSendUploadPictureSingal(IN LinkTsMuxUploader *_pTsMuxUploader, void *_pOpaque, const char *_pBuf, int _nBuflen, enum LinkPicUploadType _type) {
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        assert(pFFTsMuxUploader->pPicUploader != NULL);
        return LinkSendUploadPictureToPictureUploader(pFFTsMuxUploader->pPicUploader, _pOpaque, _pBuf, _nBuflen, _type);
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
                LinkDestroyUploader(&pTsMuxCtx->pTsUploader_);
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
                        if (pFFTsMuxUploader->pAACBuf) {
                                free(pFFTsMuxUploader->pAACBuf);
                        }
                        if (pFFTsMuxUploader->token_.pToken_) {
                                free(pFFTsMuxUploader->token_.pToken_);
                                pFFTsMuxUploader->token_.pToken_ = NULL;
                        }
                        if (pFFTsMuxUploader->token_.pPrevToken_) {
                                free(pFFTsMuxUploader->token_.pPrevToken_);
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
        LinkLogInfo("toto memory size:%lld\n", nTotalMemSize);
        
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

static int newTsMuxContext(FFTsMuxContext ** _pTsMuxCtx, LinkMediaArg *_pAvArg, LinkUploadArg *_pUploadArg,
                           int nQBufSize, enum CircleQueuePolicy queueType)
#ifdef USE_OWN_TSMUX
{
        FFTsMuxContext * pTsMuxCtx = (FFTsMuxContext *)malloc(sizeof(FFTsMuxContext));
        if (pTsMuxCtx == NULL) {
                return LINK_NO_MEMORY;
        }
        memset(pTsMuxCtx, 0, sizeof(FFTsMuxContext));
        
        int ret = LinkNewUploader(&pTsMuxCtx->pTsUploader_, _pUploadArg, queueType, 188, nQBufSize / 188);
        if (ret != 0) {
                free(pTsMuxCtx);
                return ret;
        }
        
        LinkTsMuxerArg avArg;
        avArg.nAudioFormat = _pAvArg->nAudioFormat;
        avArg.nAudioChannels = _pAvArg->nChannels;
        avArg.nAudioSampleRate = _pAvArg->nSamplerate;
        
        avArg.output = writeTsPacketToMem;
        avArg.nVideoFormat = _pAvArg->nVideoFormat;
        avArg.pOpaque = pTsMuxCtx;
        
        ret = LinkNewTsMuxerContext(&avArg, &pTsMuxCtx->pFmtCtx_);
        if (ret != 0) {
                LinkDestroyUploader(&pTsMuxCtx->pTsUploader_);
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
        int ret = LinkNewUploader(&pTsMuxCtx->pTsUploader_, _pUploadArg, queueType, FF_OUT_LEN, nBufsize / FF_OUT_LEN);
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

static int setToken(LinkTsMuxUploader* _PTsMuxUploader, char *_pToken, int _nTokenLen)
{
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)_PTsMuxUploader;
        if (pFFTsMuxUploader->token_.pToken_ == NULL) {
                pFFTsMuxUploader->token_.pToken_ = malloc(_nTokenLen + 1);
                if (pFFTsMuxUploader->token_.pToken_  == NULL) {
                        return LINK_NO_MEMORY;
                }
        }else {
                if (pFFTsMuxUploader->token_.pPrevToken_ != NULL) {
                        free(pFFTsMuxUploader->token_.pPrevToken_);
                }
                pFFTsMuxUploader->token_.pPrevToken_ = pFFTsMuxUploader->token_.pToken_;
                pFFTsMuxUploader->token_.nPrevTokenLen_ = pFFTsMuxUploader->token_.nTokenLen_;
                
                pFFTsMuxUploader->token_.pToken_ = malloc(_nTokenLen + 1);
                if (pFFTsMuxUploader->token_.pToken_  == NULL) {
                        return LINK_NO_MEMORY;
                }
        }
        memcpy(pFFTsMuxUploader->token_.pToken_, _pToken, _nTokenLen);
        pFFTsMuxUploader->token_.nTokenLen_ = _nTokenLen;
        pFFTsMuxUploader->token_.pToken_[_nTokenLen] = 0;
        
        pFFTsMuxUploader->uploadArg.pToken_ = _pToken;
        return LINK_SUCCESS;
}

static void upadateUploadArg(void *_pOpaque, void* pArg, int64_t nNow)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)_pOpaque;
        LinkUploadArg *_pUploadArg = (LinkUploadArg *)pArg;
        if (pFFTsMuxUploader->uploadArg.nSegmentId_ == 0) {
                pFFTsMuxUploader->uploadArg.nLastUploadTsTime_ = _pUploadArg->nLastUploadTsTime_;
                pFFTsMuxUploader->uploadArg.nSegmentId_ = _pUploadArg->nSegmentId_;
                return;
        }
        int64_t nDiff = pFFTsMuxUploader->nNewSegmentInterval * 1000000000LL;
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

static void setNewSegmentInterval(LinkTsMuxUploader* _pTsMuxUploader, int nInterval)
{
        if (nInterval < 15) {
                LinkLogWarn("setNewSegmentInterval is to small:%d. ge 15 required", nInterval);
                return;
        }
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)_pTsMuxUploader;
        pFFTsMuxUploader->nNewSegmentInterval = nInterval;
}

int LinkNewTsMuxUploader(LinkTsMuxUploader **_pTsMuxUploader, LinkMediaArg *_pAvArg, LinkUserUploadArg *_pUserUploadArg)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)malloc(sizeof(FFTsMuxUploader));
        if (pFFTsMuxUploader == NULL) {
                return LINK_NO_MEMORY;
        }
        memset(pFFTsMuxUploader, 0, sizeof(FFTsMuxUploader));
        
        int ret = 0;
        ret = setToken((LinkTsMuxUploader *)pFFTsMuxUploader, _pUserUploadArg->pToken_, _pUserUploadArg->nTokenLen_);
        if (ret != 0) {
                return ret;
        }
        
        if (_pUserUploadArg->nDeviceIdLen_ >= sizeof(pFFTsMuxUploader->deviceId_)) {
                free(pFFTsMuxUploader);
                LinkLogError("device max support lenght is 64");
                return LINK_ARG_TOO_LONG;
        }
        memcpy(pFFTsMuxUploader->deviceId_, _pUserUploadArg->pDeviceId_, _pUserUploadArg->nDeviceIdLen_);
        pFFTsMuxUploader->uploadArg.pDeviceId_ = pFFTsMuxUploader->deviceId_;
        
        pFFTsMuxUploader->uploadArg.pUploadArgKeeper_ = pFFTsMuxUploader;
        pFFTsMuxUploader->uploadArg.UploadArgUpadate = upadateUploadArg;
        pFFTsMuxUploader->uploadArg.uploadZone = _pUserUploadArg->uploadZone_;
        
        pFFTsMuxUploader->nNewSegmentInterval = 30;
        
        pFFTsMuxUploader->nFirstTimestamp = -1;
        
        ret = pthread_mutex_init(&pFFTsMuxUploader->muxUploaderMutex_, NULL);
        if (ret != 0){
                free(pFFTsMuxUploader);
                return LINK_MUTEX_ERROR;
        }
        
        pFFTsMuxUploader->tsMuxUploader_.SetToken = setToken;
        pFFTsMuxUploader->tsMuxUploader_.PushAudio = PushAudio;
        pFFTsMuxUploader->tsMuxUploader_.PushVideo = PushVideo;
        pFFTsMuxUploader->tsMuxUploader_.SetUploaderBufferSize = setUploaderBufferSize;
        pFFTsMuxUploader->tsMuxUploader_.GetUploaderBufferUsedSize = getUploaderBufferUsedSize;
        pFFTsMuxUploader->tsMuxUploader_.SetNewSegmentInterval = setNewSegmentInterval;
        pFFTsMuxUploader->queueType_ = TSQ_APPEND;// TSQ_FIX_LENGTH;
        
        pFFTsMuxUploader->avArg = *_pAvArg;
        
        *_pTsMuxUploader = (LinkTsMuxUploader *)pFFTsMuxUploader;
        
        return LINK_SUCCESS;
}

static int getTokenCallback(IN void *pOpaque, OUT char *pBuf, IN int nBuflen) {
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)pOpaque;
        memcpy(pBuf, pFFTsMuxUploader->token_.pToken_, pFFTsMuxUploader->token_.nTokenLen_);
        return LINK_SUCCESS;
}

int LinkNewTsMuxUploaderWithPictureUploader(LinkTsMuxUploader **_pTsMuxUploader, LinkMediaArg *_pAvArg,
                                            LinkUserUploadArg *_pUserUploadArg, LinkPicUploadArg *_pPicArg) {
        
        //LinkTsMuxUploader *pTsMuxUploader
        int ret = LinkNewTsMuxUploader(_pTsMuxUploader, _pAvArg, _pUserUploadArg);
        if (ret != LINK_SUCCESS) {
                return ret;
        }
        
        LinkPicUploadFullArg fullArg;
        fullArg.getPicCallback = _pPicArg->getPicCallback;
        fullArg.pGetPicCallbackOpaque = _pPicArg->pGetPicCallbackOpaque;
        fullArg.getPictureFreeCallback = _pPicArg->getPictureFreeCallback;
        fullArg.getTokenCallback = getTokenCallback;
        fullArg.pGetTokenCallbackOpaque = *_pTsMuxUploader;
        PictureUploader *pPicUploader;
        ret = LinkNewPictureUploader(&pPicUploader, &fullArg);
        if (ret != LINK_SUCCESS) {
                LinkDestroyTsMuxUploader(_pTsMuxUploader);
                LinkLogError("LinkNewPictureUploader fail:%d", ret);
                return ret;
        }
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)(*_pTsMuxUploader);
        pFFTsMuxUploader->pPicUploader = pPicUploader;
        return ret;
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
        LinkUploaderSetTsStartUploadCallback(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_, linkCapturePictureCallback, pFFTsMuxUploader);
        
        pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->UploadStart(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_);
        return LINK_SUCCESS;
}

void LinkNotiryNomoreData(IN LinkTsMuxUploader *_pTsMuxUploader) {
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
        
        LinkDestroyPictureUploader(&pFFTsMuxUploader->pPicUploader);
        
        pthread_mutex_lock(&pFFTsMuxUploader->muxUploaderMutex_);
        if (pFFTsMuxUploader->pTsMuxCtx) {
                pFFTsMuxUploader->pTsMuxCtx->pTsMuxUploader = (LinkTsMuxUploader*)pFFTsMuxUploader;
        }
        pushRecycle(pFFTsMuxUploader);
        pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
        *_pTsMuxUploader = NULL;
        return;
}
