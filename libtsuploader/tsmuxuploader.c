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
#include "httptools.h"
#include "security.h"
#include "cJSON/cJSON.h"
#include "b64/urlsafe_b64.h"
#include "tsmux.h"

#define FF_OUT_LEN 4096
#define QUEUE_INIT_LEN 150
#define DEVICE_AK_LEN 40
#define DEVICE_SK_LEN 40

#define LINK_STREAM_TYPE_AUDIO 1
#define LINK_STREAM_TYPE_VIDEO 2

typedef struct _FFTsMuxContext{
        LinkAsyncInterface asyncWait;
        LinkTsUploader *pTsUploader_;

        LinkTsMuxerContext *pFmtCtx_;

        int64_t nPrevAudioTimestamp;
        int64_t nPrevVideoTimestamp;
        LinkTsMuxUploader * pTsMuxUploader;
}FFTsMuxContext;

typedef struct _Buffer {
        char *pData;
        int nLen;
        int nCap;
}Buffer;

typedef struct _Token {
        Buffer pToken_;
        Buffer pFrameToken_;
        Buffer pTsToken_;
        Buffer pFnamePrefix_;
        int isCompatableMode;
}Token;

typedef struct  {
        int updateConfigInterval;
        int nTsDuration; //millisecond
        int nSessionDuration; //millisecond
        int nSessionTimeout; //millisecond
        Buffer mgrTokenRequestUrl;
        Buffer upTokenRequestUrl;
        Buffer upHostUrl;
        int isValid;
        LinkPlanType planType;
        
        //not use now
        int   nUploaderBufferSize;
        int   nNewSegmentInterval;
        int   nUpdateIntervalSeconds;
}RemoteConfig;

typedef struct _SessionUpdateParam {
        int nType; //1 for token update
        int64_t nSeqNum;
        char sessionId[LINK_MAX_SESSION_ID_LEN+1];
}SessionUpdateParam;

typedef struct _FFTsMuxUploader{
        LinkTsMuxUploader tsMuxUploader_;
        pthread_mutex_t muxUploaderMutex_;
        pthread_mutex_t tokenMutex_;
        unsigned char *pAACBuf;
        int nAACBufLen;
        FFTsMuxContext *pTsMuxCtx;
        
        int64_t nFirstAudioFrameTimestamp;
        int64_t nLastAudioFrameTimestamp;
        int64_t nFirstVideoFrameTimestamp;
        int64_t nLastVideoFrameTimestamp;
        int64_t nFirstFrameTimestamp; //determinate if to cut ts file
        int64_t nLastFrameTimestamp;
        int64_t nLastVideoFrameTsForCheck;
        int64_t nLastAudioFrameTsForCheck;
        int64_t nLastVideoFrameTsForCheckSysTime;
        int64_t nLastAudioFrameTsForCheckSysTime;
        
        int64_t nLastPicCallbackSystime; //upload picture need
        int nKeyFrameCount;
        int nFrameCount;
        LinkMediaArg avArg;
        
        Token token_;
        LinkTsUploadArg uploadArgBak;
        PictureUploader *pPicUploader;
        SegmentHandle segmentHandle;
        CircleQueuePolicy queueType_;
        int8_t isPause;
        int8_t isQuit;
        
        pthread_t tokenThread;
        
        char *pVideoMeta;
        int nVideoMetaLen;
        int nVideoMetaCap;
        
        char ak[41];
        char sk[41];
        char *pConfigRequestUrl;
        RemoteConfig remoteConfig;
        RemoteConfig tmpRemoteConfig;
        
        LinkCircleQueue *pUpdateQueue_; //for token and remteconfig update
        char sessionId[LINK_MAX_SESSION_ID_LEN + 1];
}FFTsMuxUploader;

//static int aAacfreqs[13] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050 ,16000 ,12000, 11025, 8000, 7350};
static int uploadParamCallback(IN void *pOpaque, IN OUT LinkUploadParam *pParam, LinkUploadCbType cbtype);
static void linkCapturePictureCallback(void *pOpaque, int64_t nTimestamp);
static int linkTsMuxUploaderTokenThreadStart(FFTsMuxUploader* pFFTsMuxUploader);
static void freeRemoteConfig(RemoteConfig *pRc);

#include <net/if.h>
#include <sys/ioctl.h>
static char gSn[32];


void LinkInitSn()
{
#ifndef __APPLE__
        #define MAXINTERFACES 16
        int fd, interface;
        struct ifreq buf[MAXINTERFACES];
        struct ifconf ifc;
        char mac[32] = {0};
        
        if (gSn[0] != 0)
                return;
        
        if((fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0) {
                int i = 0;
                ifc.ifc_len = sizeof(buf);
                ifc.ifc_buf = (caddr_t)buf;
                if (!ioctl(fd, SIOCGIFCONF, (char *)&ifc)) {
                        interface = ifc.ifc_len / sizeof(struct ifreq);
                        LinkLogDebug("interface num is %d", interface);
                        while (i < interface) {
                                LinkLogDebug("net device %s", buf[i].ifr_name);
                                if (!(ioctl(fd, SIOCGIFHWADDR, (char *)&buf[i]))) {
                                        sprintf(mac, "%02X%02X%02X%02X%02X%02X",
                                                (unsigned char)buf[i].ifr_hwaddr.sa_data[0],
                                                (unsigned char)buf[i].ifr_hwaddr.sa_data[1],
                                                (unsigned char)buf[i].ifr_hwaddr.sa_data[2],
                                                (unsigned char)buf[i].ifr_hwaddr.sa_data[3],
                                                (unsigned char)buf[i].ifr_hwaddr.sa_data[4],
                                                (unsigned char)buf[i].ifr_hwaddr.sa_data[5]);
                                        LinkLogDebug("HWaddr: %s", mac);
                                        if (memcmp(buf[i].ifr_name, "eth", 3) == 0 ||
                                            memcmp(buf[i].ifr_name, "wlan", 4) == 0 ||
                                            memcmp(buf[i].ifr_name, "lo", 2) != 0 ) {
                                                memcpy(gSn, mac, strlen(mac));
                                                break;
                                        }
                                }
                                i++;
                        }
                }
        }
        if (gSn[0] == 0)
                memcpy(gSn, "TEST_MACADDR", 12);
#else
        memcpy(gSn, "TEST_MACADDR", 12);
#endif
}



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

static void resetTimeInfo(FFTsMuxUploader *pFFTsMuxUploader) {
        pFFTsMuxUploader->nFirstAudioFrameTimestamp = 0;
        pFFTsMuxUploader->nLastAudioFrameTimestamp = 0;
        pFFTsMuxUploader->nFirstVideoFrameTimestamp = 0;
        pFFTsMuxUploader->nLastVideoFrameTimestamp = 0;
        pFFTsMuxUploader->nFirstFrameTimestamp = -1;
        pFFTsMuxUploader->nLastFrameTimestamp = 0;
}

static void getLinkReportTimeInfo(FFTsMuxUploader *_pFFTsMuxUploader, LinkReportTimeInfo *tinfo, int64_t nSysNanotime) {
        tinfo->nAudioDuration = _pFFTsMuxUploader->nLastAudioFrameTimestamp - _pFFTsMuxUploader->nFirstAudioFrameTimestamp;
        tinfo->nVideoDuration = _pFFTsMuxUploader->nLastVideoFrameTimestamp - _pFFTsMuxUploader->nFirstVideoFrameTimestamp;
        tinfo->nMediaDuation = _pFFTsMuxUploader->nLastFrameTimestamp - _pFFTsMuxUploader->nFirstFrameTimestamp;
        tinfo->nSystimestamp = nSysNanotime;
        return;
}

static void switchTs(FFTsMuxUploader *_pFFTsMuxUploader, int64_t nSysNanotime)
{
        if (_pFFTsMuxUploader) {
                if (_pFFTsMuxUploader->pTsMuxCtx) {
                        if (nSysNanotime <= 0) {
                                nSysNanotime = LinkGetCurrentNanosecond();
                                LinkLogInfo("nSysNanotime %"PRId64"", nSysNanotime);
                        }
                        LinkReportTimeInfo tinfo;
                        getLinkReportTimeInfo(_pFFTsMuxUploader, &tinfo, nSysNanotime);
                        if (_pFFTsMuxUploader->queueType_ == TSQ_APPEND && _pFFTsMuxUploader->nFirstFrameTimestamp >= 0)
                                _pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->ReportTimeInfo(_pFFTsMuxUploader->pTsMuxCtx->pTsUploader_, &tinfo, LINK_TS_END);
                        LinkResetTsMuxerContext( _pFFTsMuxUploader->pTsMuxCtx->pFmtCtx_);
                        resetTimeInfo(_pFFTsMuxUploader);
                }
        }
        return;
}

static void pushRecycle(FFTsMuxUploader *_pFFTsMuxUploader)
{
        if (_pFFTsMuxUploader) {
                
                if (_pFFTsMuxUploader->pTsMuxCtx) {

                        switchTs(_pFFTsMuxUploader, 0);
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
                if (ret == LINK_Q_OVERFLOW) {
                        LinkLogWarn("write ts to queue overflow:%d", ret);
		} else if (ret == LINK_Q_OVERWRIT) {
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
                LinkLogTrace("write_packet: should write:len:%d  actual:%d", buf_size, ret);
        }
        return ret;
}

static int push(FFTsMuxUploader *pFFTsMuxUploader, const char * _pData, int _nDataLen, int64_t _nTimestamp, int _nFlag,
                int _nIsKeyframe, int64_t nSysNanotime, int nIsForceNewSeg){
        
        //LinkLogTrace("push thread id:%d", (int)pthread_self());
        
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

                pTsMuxCtx->nPrevAudioTimestamp = _nTimestamp;
                
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

                        ret = LinkMuxerAudio(pTsMuxCtx->pFmtCtx_, (uint8_t *)pFFTsMuxUploader->pAACBuf, varHeader.aac_frame_length, _nTimestamp);

                } 

                else {
                        ret = LinkMuxerAudio(pTsMuxCtx->pFmtCtx_, (uint8_t*)_pData, _nDataLen, _nTimestamp);
                }

        }else{
                //fprintf(stderr, "video frame: len:%d pts:%"PRId64"\n", _nDataLen, _nTimestamp);
                if (pTsMuxCtx->nPrevVideoTimestamp != 0 && _nTimestamp - pTsMuxCtx->nPrevVideoTimestamp <= 0) {
                        LinkLogWarn("video pts not monotonically: prev:%"PRId64" now:%"PRId64"", pTsMuxCtx->nPrevVideoTimestamp, _nTimestamp);
                        return 0;
                }
                ret = LinkMuxerVideo(pTsMuxCtx->pFmtCtx_, (uint8_t*)_pData, _nDataLen, _nTimestamp, _nIsKeyframe);

                pTsMuxCtx->nPrevVideoTimestamp = _nTimestamp;
        }
        
        
        if (ret == 0) {
                LinkReportTimeInfo tinfo;
                if (nIsForceNewSeg || pFFTsMuxUploader->nFirstFrameTimestamp < 0) {
                        getLinkReportTimeInfo(pFFTsMuxUploader, &tinfo, nSysNanotime);
                }
                if (nIsForceNewSeg )
                        pTsMuxCtx->pTsUploader_->ReportTimeInfo(pTsMuxCtx->pTsUploader_, &tinfo, LINK_SEG_TIMESTAMP);
                if (pFFTsMuxUploader->nFirstFrameTimestamp < 0) {
                        pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->ReportTimeInfo(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_, &tinfo, LINK_TS_START);
                        pFFTsMuxUploader->nFirstFrameTimestamp = _nTimestamp;
                }
                pFFTsMuxUploader->nLastFrameTimestamp = _nTimestamp;
                if (_nFlag == LINK_STREAM_TYPE_VIDEO) {
                        if (pFFTsMuxUploader->nFirstVideoFrameTimestamp <= 0) {
                                pFFTsMuxUploader->nFirstVideoFrameTimestamp = _nTimestamp;
                        }
                        pFFTsMuxUploader->nLastVideoFrameTimestamp = _nTimestamp;
                } else{
                        if (pFFTsMuxUploader->nFirstAudioFrameTimestamp <= 0) {
                                pFFTsMuxUploader->nFirstAudioFrameTimestamp = _nTimestamp;
                        }
                        pFFTsMuxUploader->nLastAudioFrameTimestamp = _nTimestamp;
                }
        }

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
                                pFFTsMuxUploader->pVideoMeta = (char *)malloc(nMetaLen+1024);
                                if (pFFTsMuxUploader->pVideoMeta == NULL)
                                        return LINK_NO_MEMORY;
                                pFFTsMuxUploader->nVideoMetaLen = nMetaLen;
                                pFFTsMuxUploader->nVideoMetaCap = nMetaLen+1024;
                                memcpy(pFFTsMuxUploader->pVideoMeta, pSpsStart, nMetaLen);
                                return 0;
                        } else { //compare metadata
                                if (pFFTsMuxUploader->nVideoMetaLen != nMetaLen ||
                                    memcmp(pFFTsMuxUploader->pVideoMeta, pSpsStart, nMetaLen) != 0) {
                                        
                                        *pIsSameAsBefore = 1;
                                        
                                        if (pFFTsMuxUploader->nVideoMetaCap >= nMetaLen) {
                                                pFFTsMuxUploader->nVideoMetaLen = nMetaLen;
                                        } else {
                                                free(pFFTsMuxUploader->pVideoMeta);
                                                pFFTsMuxUploader->pVideoMeta = NULL;
                                                pFFTsMuxUploader->pVideoMeta = (char *)malloc(nMetaLen+1024);
                                                if (pFFTsMuxUploader->pVideoMeta == NULL)
                                                        return LINK_NO_MEMORY;
                                                
                                                pFFTsMuxUploader->nVideoMetaLen = nMetaLen;
                                                pFFTsMuxUploader->nVideoMetaCap = nMetaLen+1024;
                                        }
                                        memcpy(pFFTsMuxUploader->pVideoMeta, pSpsStart, nMetaLen);
                                }
                                return 0;
                        }
                }
        }while(1);
                
        return -2;
}

static int checkSwitch(LinkTsMuxUploader *_pTsMuxUploader, int64_t _nTimestamp, int nIsKeyFrame, int _isVideo,
                       int64_t nSysNanotime, int _nIsSegStart, const char * _pData, int _nDataLen, int *pIsVideoMetaChanged)
{
        int ret;
        int videoMetaChanged = 0;
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        int64_t bakFirstFrameTimestamp = pFFTsMuxUploader->nFirstFrameTimestamp;
        if (bakFirstFrameTimestamp < 0)
                bakFirstFrameTimestamp = _nTimestamp;
        
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
                        videoMetaChanged = 1;
                        if (pIsVideoMetaChanged)
                                *pIsVideoMetaChanged = 1;
                }
        }
        // if start new uploader, start from keyframe
        if (isVideoKeyframe || videoMetaChanged) {
                if( ((_nTimestamp - bakFirstFrameTimestamp) > pFFTsMuxUploader->remoteConfig.nTsDuration
                                      && pFFTsMuxUploader->nKeyFrameCount > 0)
                   //at least 1 keyframe and aoubt last 5 second
                   || videoMetaChanged
                   || (_nIsSegStart && pFFTsMuxUploader->nFrameCount != 0) ){// new segment is specified
                        LinkLogTrace("normal switchts:%"PRId64" %d %d-%d %d", _nTimestamp - pFFTsMuxUploader->nFirstFrameTimestamp,
                                pFFTsMuxUploader->remoteConfig.nTsDuration, _nIsSegStart, pFFTsMuxUploader->nFrameCount, videoMetaChanged);
                        //printf("next ts:%d %"PRId64"\n", pFFTsMuxUploader->nKeyFrameCount, _nTimestamp - pFFTsMuxUploader->nLastUploadVideoTimestamp);
                        pFFTsMuxUploader->nKeyFrameCount = 0;
                        pFFTsMuxUploader->nFrameCount = 0;
                        
                        switchTs(pFFTsMuxUploader, nSysNanotime);
                        
                        int64_t nDiff = (nSysNanotime - pFFTsMuxUploader->nLastPicCallbackSystime)/1000000;
                        if (nIsKeyFrame) {
                                if (nDiff < 1000) {
                                        LinkLogWarn("get picture callback too frequency:%"PRId64"ms", nDiff);
                                }
                                pFFTsMuxUploader->nLastPicCallbackSystime = nSysNanotime;
                        }

                }
                if (isVideoKeyframe) {
                        pFFTsMuxUploader->nKeyFrameCount++;
                }
        }
        return 0;
}

static int PushVideo(LinkTsMuxUploader *_pTsMuxUploader, const char * _pData, int _nDataLen, int64_t _nTimestamp, int nIsKeyFrame, int _nIsSegStart, int64_t _nFrameSysTime)
{
        int64_t nSysNanotime = 0;
        if (_nFrameSysTime > 1548064836000)
                nSysNanotime = _nFrameSysTime * 1000000LL;
        else
                nSysNanotime = LinkGetCurrentNanosecond();
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        pthread_mutex_lock(&pFFTsMuxUploader->muxUploaderMutex_);
        if (pFFTsMuxUploader->nLastVideoFrameTsForCheck > 0){
                if (_nTimestamp <= pFFTsMuxUploader->nLastVideoFrameTsForCheck) {
                        LinkLogWarn("Video timestamp not monotonical:%"PRId64" %"PRId64"",_nTimestamp, pFFTsMuxUploader->nLastVideoFrameTsForCheck);
                }
                if (pFFTsMuxUploader->nLastAudioFrameTsForCheck > 0) {
                        int64_t avDiff = _nTimestamp - pFFTsMuxUploader->nLastAudioFrameTsForCheck;
                        if (avDiff < 0) {
                                avDiff = -avDiff;
                        }
                        if (avDiff > 500) {
                                LinkLogTrace("av timestamp may not align to same timebase:cv:%"PRId64" la:%"PRId64"",
                                            _nTimestamp, pFFTsMuxUploader->nLastAudioFrameTsForCheck);
                        }
                }
        }
        pFFTsMuxUploader->nLastVideoFrameTsForCheck =_nTimestamp;
        pFFTsMuxUploader->nLastVideoFrameTsForCheckSysTime = nSysNanotime;
        
        
        if (pFFTsMuxUploader->isPause) {
                pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                return LINK_PAUSED;
        }
        int ret = 0;

        if (pFFTsMuxUploader->nKeyFrameCount == 0 && !nIsKeyFrame) {
                LinkLogTrace("first video frame not IDR. drop this frame");
                pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                return 0;
        }
        int isVideoMetaChanged = 0;

        ret = checkSwitch(_pTsMuxUploader, _nTimestamp, nIsKeyFrame, 1, nSysNanotime, _nIsSegStart, _pData, _nDataLen, &isVideoMetaChanged);
        if (ret != 0) {
                pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                return ret;
        }
        if (isVideoMetaChanged)
                _nIsSegStart = isVideoMetaChanged;
        if (pFFTsMuxUploader->nKeyFrameCount == 0 && !nIsKeyFrame) {
                LinkLogWarn("first video frame not IDR. drop this frame");
                pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                return 0;
        }
        if (pFFTsMuxUploader->nKeyFrameCount == 1 && nIsKeyFrame) {
                if (pFFTsMuxUploader->nLastPicCallbackSystime <= 0)
                        pFFTsMuxUploader->nLastPicCallbackSystime = LinkGetCurrentNanosecond();
                linkCapturePictureCallback(_pTsMuxUploader, nSysNanotime / 1000000);
        }
        
        ret = push(pFFTsMuxUploader, _pData, _nDataLen, _nTimestamp, LINK_STREAM_TYPE_VIDEO, nIsKeyFrame, nSysNanotime, _nIsSegStart);
        if (ret == 0){
                pFFTsMuxUploader->nFrameCount++;
        }
        if (ret == LINK_NO_MEMORY) {
                LinkLogWarn("video nomem switchts");
                switchTs(pFFTsMuxUploader, nSysNanotime);
        }
        pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
        return ret;
}

static int PushAudio(LinkTsMuxUploader *_pTsMuxUploader, const char * _pData, int _nDataLen, int64_t _nTimestamp, int64_t _nFrameSysTime)
{
        int64_t nSysNanotime = 0;
        if (_nFrameSysTime > 1548064836000)
                nSysNanotime = _nFrameSysTime * 1000000LL;
        else
                nSysNanotime = LinkGetCurrentNanosecond();
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        pthread_mutex_lock(&pFFTsMuxUploader->muxUploaderMutex_);
        if (pFFTsMuxUploader->nLastAudioFrameTsForCheck > 0){
                if (_nTimestamp <= pFFTsMuxUploader->nLastAudioFrameTsForCheck) {
                        LinkLogWarn("Audio timestamp not monotonical:%"PRId64" %"PRId64"",_nTimestamp, pFFTsMuxUploader->nLastAudioFrameTsForCheck);
                }
        }
        pFFTsMuxUploader->nLastAudioFrameTsForCheck =_nTimestamp;

        if (pFFTsMuxUploader->isPause) {
                pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                return LINK_PAUSED;
        }

        int ret = checkSwitch(_pTsMuxUploader, _nTimestamp, 0, 0, nSysNanotime, 0, NULL, 0, NULL);
        if (ret != 0) {
                return ret;
        }
        if (pFFTsMuxUploader->nKeyFrameCount == 0) {
                pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
                LinkLogTrace("no keyframe. drop audio frame");
                return 0;
        }
        ret = push(pFFTsMuxUploader, _pData, _nDataLen, _nTimestamp, LINK_STREAM_TYPE_AUDIO, 0, nSysNanotime, 0);
        if (ret == 0){
                pFFTsMuxUploader->nFrameCount++;
        }
        if (ret == LINK_NO_MEMORY) {
                LinkLogWarn("audio nomem switchts");
                switchTs(pFFTsMuxUploader, nSysNanotime);
        }
        pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
        return ret;
}

int LinkPushPicture(IN LinkTsMuxUploader *_pTsMuxUploader,const char *pFilename,
                                int nFilenameLen, const char *_pBuf, int _nBuflen) {
        
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        
        if (pFFTsMuxUploader->pPicUploader == NULL || pFFTsMuxUploader->pTsMuxCtx == NULL ||
            pFFTsMuxUploader->pTsMuxCtx->pTsUploader_ == NULL) {
                return LINK_NOT_INITED;
        }
        assert(pFFTsMuxUploader->pPicUploader != NULL);
        
        LinkPicture pic;
        pic.pOpaque = pFFTsMuxUploader->pPicUploader;
        pic.pFilename = pFilename;
        pic.nFilenameLen = nFilenameLen;
        pic.pBuf = _pBuf;
        pic.nBuflen = _nBuflen;
        
        return LinkTsUploaderPushPic(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_, pic);
        
        //return LinkSendUploadPictureToPictureUploader(pFFTsMuxUploader->pPicUploader, pFilename, nFilenameLen, _pBuf, _nBuflen);
}

static int waitToCompleUploadAndDestroyTsMuxContext(void *_pOpaque)
{
        sleep(2);
        FFTsMuxContext *pTsMuxCtx = (FFTsMuxContext*)_pOpaque;
        
        if (pTsMuxCtx) {
                LinkUploaderStatInfo statInfo = {0};
                pTsMuxCtx->pTsUploader_->GetStatInfo(pTsMuxCtx->pTsUploader_, &statInfo);
                LinkLogDebug("uploader push:%d pop:%d remainItemCount:%d dropped:%d", statInfo.nPushDataBytes_,
                         statInfo.nPopDataBytes_, statInfo.nLen_, statInfo.nDropped);
                LinkDestroyTsUploader(&pTsMuxCtx->pTsUploader_);

                LinkDestroyTsMuxerContext(pTsMuxCtx->pFmtCtx_);

                FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)(pTsMuxCtx->pTsMuxUploader);
                if (pFFTsMuxUploader) {
                        SessionUpdateParam upparam;
                        upparam.nType = 3;
                        pFFTsMuxUploader->pUpdateQueue_->Push(pFFTsMuxUploader->pUpdateQueue_, (char *)&upparam, sizeof(SessionUpdateParam));
                        pthread_join(pFFTsMuxUploader->tokenThread, NULL);
                        if (pFFTsMuxUploader->pVideoMeta) {
                                free(pFFTsMuxUploader->pVideoMeta);
                        }
                        LinkDestroyQueue(&pFFTsMuxUploader->pUpdateQueue_);
                        LinkReleaseSegmentHandle(&pFFTsMuxUploader->segmentHandle);
                        LinkDestroyPictureUploader(&pFFTsMuxUploader->pPicUploader);
                        if (pFFTsMuxUploader->pAACBuf) {
                                free(pFFTsMuxUploader->pAACBuf);
                        }
                        if (pFFTsMuxUploader->token_.pToken_.pData) {
                                free(pFFTsMuxUploader->token_.pToken_.pData);
                                pFFTsMuxUploader->token_.pToken_.pData = NULL;
                        }
                        if (pFFTsMuxUploader->token_.pFrameToken_.pData) {
                                free(pFFTsMuxUploader->token_.pFrameToken_.pData);
                                pFFTsMuxUploader->token_.pFrameToken_.pData = NULL;
                        }
                        if (pFFTsMuxUploader->token_.pTsToken_.pData) {
                                free(pFFTsMuxUploader->token_.pTsToken_.pData);
                                pFFTsMuxUploader->token_.pTsToken_.pData = NULL;
                        }
                        if (pFFTsMuxUploader->token_.pFnamePrefix_.pData) {
                                free(pFFTsMuxUploader->token_.pFnamePrefix_.pData);
                                pFFTsMuxUploader->token_.pFnamePrefix_.pData = NULL;
                        }
                        freeRemoteConfig(&pFFTsMuxUploader->remoteConfig);
                        freeRemoteConfig(&pFFTsMuxUploader->tmpRemoteConfig);

                        pthread_mutex_destroy(&pFFTsMuxUploader->tokenMutex_);
                        pthread_mutex_destroy(&pFFTsMuxUploader->muxUploaderMutex_);
                        free(pFFTsMuxUploader);
                }
                free(pTsMuxCtx);
        }
        
        return LINK_SUCCESS;
}

#define getFFmpegErrorMsg(errcode) char msg[128];\
av_strerror(errcode, msg, sizeof(msg))

static int getBufferSize(FFTsMuxUploader *pFFTsMuxUploader) {
        return 640*1024;
}

static int newTsMuxContext(FFTsMuxContext ** _pTsMuxCtx, LinkMediaArg *_pAvArg, LinkTsUploadArg *_pUploadArg,
                           int nQBufSize, CircleQueuePolicy queueType)
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

static void setToken(FFTsMuxUploader* _PTsMuxUploader, Buffer token, Buffer ttoken, Buffer ftoken,
                     Buffer prefix, int isCompatableMode)
{
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)_PTsMuxUploader;
        pthread_mutex_lock(&pFFTsMuxUploader->tokenMutex_);
        if (pFFTsMuxUploader->token_.pToken_.pData != NULL) {
                free(pFFTsMuxUploader->token_.pToken_.pData);
        }
        pFFTsMuxUploader->token_.pToken_ = token;
        
        if (pFFTsMuxUploader->token_.pFrameToken_.pData != NULL) {
                free(pFFTsMuxUploader->token_.pFrameToken_.pData);
        }
        pFFTsMuxUploader->token_.pFrameToken_ = ftoken;
        
        if (pFFTsMuxUploader->token_.pTsToken_.pData != NULL) {
                free(pFFTsMuxUploader->token_.pTsToken_.pData);
        }
        pFFTsMuxUploader->token_.pTsToken_ = ttoken;
        
        
        if (pFFTsMuxUploader->token_.pFnamePrefix_.pData != NULL) {
                
                free(pFFTsMuxUploader->token_.pFnamePrefix_.pData);
        }
        pFFTsMuxUploader->token_.pFnamePrefix_ = prefix;
        pFFTsMuxUploader->token_.isCompatableMode = isCompatableMode;
        
        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
        return ;
}

static const char *sReasonStr[] ={"---", "normal", "timeout", "force"};
static void handNewSession(FFTsMuxUploader *pFFTsMuxUploader, LinkSession *pSession, int64_t nNewSessionId, int lastSessionEndReasonCode, int shouldReport) {
        SessionUpdateParam upparam;
        upparam.nType = 1;
        pSession->segHandle = pFFTsMuxUploader->segmentHandle;
        // report current session end
        pSession->nSessionEndResonCode = lastSessionEndReasonCode;
        if (pSession->nLastTsEndTime > 0)
                pSession->nSessionEndTime = pSession->nLastTsEndTime;
        else
                pSession->nSessionEndTime = nNewSessionId;
        //if (shouldReport)
        //        LinkUpdateSegment(pFFTsMuxUploader->segmentHandle, pSession);
        
        // update session
        upparam.nSeqNum = 0;
        LinkUpdateSessionId(pSession, nNewSessionId);
        int nSLen = snprintf(upparam.sessionId, sizeof(upparam.sessionId), "%s", pSession->sessionId);
        assert(nSLen < sizeof(upparam.sessionId));
        nSLen = snprintf(pFFTsMuxUploader->sessionId, LINK_MAX_SESSION_ID_LEN+1, "%s", pSession->sessionId);
        assert(nSLen < sizeof(upparam.sessionId));
        
        // update upload token
        LinkLogInfo("%s: update remote token:%"PRId64" %s", sReasonStr[lastSessionEndReasonCode], pSession->nSessionStartTime, pSession->sessionId);
        pFFTsMuxUploader->pUpdateQueue_->Push(pFFTsMuxUploader->pUpdateQueue_, (char *)&upparam, sizeof(SessionUpdateParam));
        
        pFFTsMuxUploader->uploadArgBak.nSegmentId_ = pSession->nSessionStartTime;
        pFFTsMuxUploader->uploadArgBak.nSegSeqNum = 0;
        
        //if (shouldReport)
        //        LinkUpdateSegment(pFFTsMuxUploader->segmentHandle, pSession);
        pSession->isNewSessionStarted = 0;
        pFFTsMuxUploader->uploadArgBak.nLastCheckTime = nNewSessionId;
        
        return;
}

static int updateSegmentId(void *_pOpaque, LinkSession* pSession,int64_t nTsStartSystime, int64_t nCurSystime, int64_t nCurTsDuration, int reportFlag)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)_pOpaque;
        pthread_mutex_lock(&pFFTsMuxUploader->tokenMutex_);
        int reportType = 0; // 1 for start; 2 for seg ;4 for end; 5 for end and start
        SessionUpdateParam upparam;
        upparam.nType = 2;
        if (pFFTsMuxUploader->uploadArgBak.nSegmentId_ == 0) {
               
                upparam.nSeqNum = 0;
                
                LinkUpdateSessionId(pSession, nTsStartSystime);
                snprintf(pFFTsMuxUploader->sessionId, LINK_MAX_SESSION_ID_LEN+1, "%s", pSession->sessionId);
                int nSLen = snprintf(upparam.sessionId, sizeof(upparam.sessionId), "%s", pSession->sessionId);
                assert(nSLen < sizeof(upparam.sessionId));
                
                LinkLogInfo("start: update remote config:%"PRId64" %"PRId64"\n",
                        pSession->nSessionStartTime, pFFTsMuxUploader->uploadArgBak.nSegmentId_);
                pFFTsMuxUploader->pUpdateQueue_->Push(pFFTsMuxUploader->pUpdateQueue_, (char *)&upparam, sizeof(SessionUpdateParam));

                LinkLogDebug("=========================11>%s", pSession->sessionId);
                LinkUpdateSegment(pFFTsMuxUploader->segmentHandle, pSession);
   
                pSession->isNewSessionStarted = 0;
                
                pFFTsMuxUploader->uploadArgBak.nLastCheckTime = nCurSystime;
                if (nCurSystime == 0) {
                        pFFTsMuxUploader->uploadArgBak.nLastCheckTime = nTsStartSystime;
                }
                pFFTsMuxUploader->uploadArgBak.nSegmentId_ = pSession->nSessionStartTime;
                pFFTsMuxUploader->uploadArgBak.nSegSeqNum = 0;
                
                pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                return 0;
        } else {
                if (nCurSystime == 0) {
                        pFFTsMuxUploader->uploadArgBak.nLastCheckTime = LinkGetCurrentNanosecond();
                        handNewSession(pFFTsMuxUploader, pSession, nTsStartSystime, 3, reportFlag);
                        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                        return 5;
                }
        }
        
        int isSegIdChange = 0;
        int isSeqNumChange = 0;
        if (pFFTsMuxUploader->remoteConfig.nSessionDuration > 0) {
                int64_t nDuration = (nCurSystime - pFFTsMuxUploader->uploadArgBak.nSegmentId_ - nCurTsDuration * 1000000);
                if (pFFTsMuxUploader->remoteConfig.nSessionDuration <= nDuration / 1000000LL) {
                        LinkLogDebug("normal: update remote config");
                        isSegIdChange = 1;
                } else {
                        
                        int64_t nDiff = pFFTsMuxUploader->remoteConfig.nSessionTimeout * (int64_t)1000000 + nCurTsDuration * 1000000;
                        if (pFFTsMuxUploader->remoteConfig.nSessionTimeout > 0 &&
                            nCurSystime - pFFTsMuxUploader->uploadArgBak.nLastCheckTime >= nDiff) {
                                LinkLogWarn("timeout: update remote config");
                                isSegIdChange = 2;
                        } else {
                                if (pFFTsMuxUploader->uploadArgBak.nSegSeqNum < pSession->nTsSequenceNumber) {
                                        LinkLogDebug("seqnum: report segment:%"PRId64" %"PRId64"",
                                                     pFFTsMuxUploader->uploadArgBak.nSegSeqNum, pSession->nTsSequenceNumber);
                                        isSeqNumChange = 1;
                                }
                        }
                }
        }
        pFFTsMuxUploader->uploadArgBak.nSegSeqNum = pSession->nTsSequenceNumber;
        pFFTsMuxUploader->uploadArgBak.nLastCheckTime = nCurSystime;
        if (isSegIdChange) {
                reportType = 5;
                handNewSession(pFFTsMuxUploader, pSession, nTsStartSystime, isSegIdChange, reportFlag);
        } else {

                if (isSeqNumChange) {
                        pSession->isNewSessionStarted = 0;
                        reportType = 2;
                        if (reportFlag) {
                                //LinkUpdateSegment(pFFTsMuxUploader->segmentHandle, pSession);
                        }
                }
        }

        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
        return reportType;
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

static void freeRemoteConfig(RemoteConfig *pRc) {
        if (pRc->upHostUrl.pData) {
                free(pRc->upHostUrl.pData);
                pRc->upHostUrl.pData = NULL;
        }
        
        if (pRc->mgrTokenRequestUrl.pData) {
                free(pRc->mgrTokenRequestUrl.pData);
                pRc->mgrTokenRequestUrl.pData = NULL;
        }
        
        if (pRc->upTokenRequestUrl.pData) {
                free(pRc->upTokenRequestUrl.pData);
                pRc->upTokenRequestUrl.pData = NULL;
        }
        
        return;
}

static void updateRemoteConfig(FFTsMuxUploader *pFFTsMuxUploader) {
        if (pFFTsMuxUploader->tmpRemoteConfig.planType != pFFTsMuxUploader->remoteConfig.planType) {
                if (pFFTsMuxUploader->pTsMuxCtx && pFFTsMuxUploader->pTsMuxCtx->pTsUploader_) {
                        LinkTsUploaderSetPlanType(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_, pFFTsMuxUploader->tmpRemoteConfig.planType);
                }
        }
        
        pthread_mutex_lock(&pFFTsMuxUploader->tokenMutex_);
        RemoteConfig rc = pFFTsMuxUploader->remoteConfig;
        pFFTsMuxUploader->remoteConfig = pFFTsMuxUploader->tmpRemoteConfig;
        pFFTsMuxUploader->tmpRemoteConfig = rc;
        pFFTsMuxUploader->remoteConfig.isValid = 1;
        
        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
}

int linkNewTsMuxUploader(LinkTsMuxUploader **_pTsMuxUploader, const LinkMediaArg *_pAvArg, const LinkUserUploadArg *_pUserUploadArg, int isWithPicAndSeg)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)malloc(sizeof(FFTsMuxUploader) + _pUserUploadArg->nConfigRequestUrlLen + 1);
        if (pFFTsMuxUploader == NULL) {
                return LINK_NO_MEMORY;
        }
        memset(pFFTsMuxUploader, 0, sizeof(FFTsMuxUploader));
        memcpy(pFFTsMuxUploader->ak, _pUserUploadArg->pDeviceAk, _pUserUploadArg->nDeviceAkLen);
        memcpy(pFFTsMuxUploader->sk, _pUserUploadArg->pDeviceSk, _pUserUploadArg->nDeviceSkLen);
        pFFTsMuxUploader->remoteConfig.nSessionDuration = 100000;
        pFFTsMuxUploader->remoteConfig.nTsDuration = 4000;
        pFFTsMuxUploader->remoteConfig.nSessionTimeout = 3000;
        
        int ret = 0;
        
        if (isWithPicAndSeg) {
                pFFTsMuxUploader->uploadArgBak.pUploadArgKeeper_ = pFFTsMuxUploader;
                pFFTsMuxUploader->uploadArgBak.UploadUpdateSegmentId = updateSegmentId;
        } else {
                pFFTsMuxUploader->uploadArgBak.UploadUpdateSegmentId = NULL;
                pFFTsMuxUploader->uploadArgBak.pUploadArgKeeper_ = NULL;
        }
        
        //pFFTsMuxUploader->uploadArg.uploadZone = _pUserUploadArg->uploadZone_; //TODO
        pFFTsMuxUploader->uploadArgBak.uploadParamCallback = uploadParamCallback;
        pFFTsMuxUploader->uploadArgBak.pGetUploadParamCallbackArg = pFFTsMuxUploader;
        pFFTsMuxUploader->uploadArgBak.pUploadStatisticCb = _pUserUploadArg->pUploadStatisticCb;
        pFFTsMuxUploader->uploadArgBak.pUploadStatArg = _pUserUploadArg->pUploadStatArg;
        pFFTsMuxUploader->pConfigRequestUrl = (char *)(pFFTsMuxUploader) + sizeof(FFTsMuxUploader);
        if (pFFTsMuxUploader->pConfigRequestUrl) {
                memcpy(pFFTsMuxUploader->pConfigRequestUrl, _pUserUploadArg->pConfigRequestUrl, _pUserUploadArg->nConfigRequestUrlLen);
                pFFTsMuxUploader->pConfigRequestUrl[_pUserUploadArg->nConfigRequestUrlLen] = 0;
        }
        
        pFFTsMuxUploader->nFirstFrameTimestamp = -1;
        
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
        pFFTsMuxUploader->tsMuxUploader_.GetUploaderBufferUsedSize = getUploaderBufferUsedSize;
        pFFTsMuxUploader->queueType_ = TSQ_APPEND;// TSQ_FIX_LENGTH;
        pFFTsMuxUploader->segmentHandle = LINK_INVALIE_SEGMENT_HANDLE;
        
        pFFTsMuxUploader->avArg = *_pAvArg;
        
        ret = LinkTsMuxUploaderStart((LinkTsMuxUploader *)pFFTsMuxUploader);
        if (ret != LINK_SUCCESS) {
                LinkTsMuxUploader * pTmp = (LinkTsMuxUploader *)pFFTsMuxUploader;
                LinkDestroyTsMuxUploader(&pTmp);
                LinkLogError("LinkTsMuxUploaderStart fail:%d", ret);
                return ret;
        }
        
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

static int uploadParamCallback(IN void *pOpaque, IN OUT LinkUploadParam *pParam, LinkUploadCbType cbtype) {
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)pOpaque;
        pthread_mutex_lock(&pFFTsMuxUploader->tokenMutex_);
        
        const char *pSrcToken = pFFTsMuxUploader->token_.pToken_.pData;
        int nSrcToken = pFFTsMuxUploader->token_.pToken_.nLen;
        if (pFFTsMuxUploader->token_.isCompatableMode && pFFTsMuxUploader->token_.pToken_.pData == NULL) {
                pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                return LINK_NOT_INITED;
        }
        if (!pFFTsMuxUploader->token_.isCompatableMode) {
                if (pFFTsMuxUploader->token_.pTsToken_.pData == NULL || pFFTsMuxUploader->token_.pFrameToken_.pData == NULL ) {
                        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                        return LINK_NOT_INITED;
                }
                if (cbtype == LINK_UPLOAD_CB_GETFRAMEPARAM) {
                        pSrcToken = pFFTsMuxUploader->token_.pFrameToken_.pData;
                        nSrcToken = pFFTsMuxUploader->token_.pFrameToken_.nLen;
                } else {
                        pSrcToken = pFFTsMuxUploader->token_.pTsToken_.pData;
                        nSrcToken = pFFTsMuxUploader->token_.pTsToken_.nLen;
                }
        }
        
        if (pParam->sessionId[0] == 0) {
                snprintf(pParam->sessionId, LINK_MAX_SESSION_ID_LEN+1, "%s",pFFTsMuxUploader->sessionId);
        }
        
        if (pParam->pTokenBuf != NULL) {
                if (pParam->nTokenBufLen - 1 < nSrcToken) {
                        LinkLogError("get token buffer is small:%d %d", nSrcToken, pParam->nTokenBufLen);
                        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                        return LINK_BUFFER_IS_SMALL;
                }
                memcpy(pParam->pTokenBuf, pSrcToken, nSrcToken);
                pParam->nTokenBufLen = nSrcToken;
                pParam->pTokenBuf[nSrcToken] = 0;
        }
        
        if (pFFTsMuxUploader->token_.isCompatableMode) {
                if (pParam->pFilePrefix != NULL) {
                        if (pParam->nFilePrefix - 1 < pFFTsMuxUploader->token_.pFnamePrefix_.nLen) {
                                LinkLogError("get token buffer is small:%d %d", pFFTsMuxUploader->token_.pFnamePrefix_.nLen, pParam->nFilePrefix);
                                pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                                return LINK_BUFFER_IS_SMALL;
                        }
                        memcpy(pParam->pFilePrefix, pFFTsMuxUploader->token_.pFnamePrefix_.pData, pFFTsMuxUploader->token_.pFnamePrefix_.nLen);
                        pParam->nFilePrefix = pFFTsMuxUploader->token_.pFnamePrefix_.nLen;
                        pParam->pFilePrefix[pFFTsMuxUploader->token_.pFnamePrefix_.nLen] = 0;
                }
        } else {
                pParam->nFilePrefix = 0;
        }
        
        
        if (pParam->pUpHost != NULL) {
                int nUpHostLen = pFFTsMuxUploader->remoteConfig.upHostUrl.nLen;
                if (pParam->nUpHostLen - 1 < nUpHostLen) {
                        LinkLogError("get uphost buffer is small:%d %d", pFFTsMuxUploader->remoteConfig.upHostUrl.pData, nUpHostLen);
                        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                        return LINK_BUFFER_IS_SMALL;
                }
                memcpy(pParam->pUpHost, pFFTsMuxUploader->remoteConfig.upHostUrl.pData, nUpHostLen);
                pParam->nUpHostLen = nUpHostLen;
                pParam->pUpHost[nUpHostLen] = 0;
        }
        
        if (pParam->pSegUrl != NULL) {
                int nSegLen = pFFTsMuxUploader->remoteConfig.mgrTokenRequestUrl.nLen;
                if (pParam->nSegUrlLen - 1 < nSegLen) {
                        LinkLogError("get segurl buffer is small:%d %d", pFFTsMuxUploader->remoteConfig.mgrTokenRequestUrl.pData, nSegLen);
                        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                        return LINK_BUFFER_IS_SMALL;
                }
                memcpy(pParam->pSegUrl, pFFTsMuxUploader->remoteConfig.mgrTokenRequestUrl.pData, nSegLen);
                pParam->nSegUrlLen = nSegLen;
                pParam->pSegUrl[nSegLen] = 0;
        }
        
        
        if (pParam->pAk != NULL) {
                int nAkLen = strlen(pFFTsMuxUploader->ak);
                assert(nAkLen < DEVICE_AK_LEN + 1);
                if (pParam->nAkLen - 1 < nAkLen) {
                        LinkLogError("get ak buffer is small:%d %d", pFFTsMuxUploader->ak, nAkLen);
                        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                        return LINK_BUFFER_IS_SMALL;
                }
                memcpy(pParam->pAk, pFFTsMuxUploader->ak, nAkLen);
                pParam->nAkLen = nAkLen;
                pParam->pAk[nAkLen] = 0;
        }
        
        if (pParam->pSk != NULL) {
                int nSkLen = strlen(pFFTsMuxUploader->sk);
                assert(nSkLen < DEVICE_SK_LEN + 1);
                if (pParam->nSkLen - 1 < nSkLen) {
                        LinkLogError("get ak buffer is small:%d %d", pFFTsMuxUploader->sk, nSkLen);
                        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                        return LINK_BUFFER_IS_SMALL;
                }
                memcpy(pParam->pSk, pFFTsMuxUploader->sk, nSkLen);
                pParam->nSkLen = nSkLen;
                pParam->pSk[nSkLen] = 0;
        }
        pParam->nTokenBufLen = nSrcToken;
        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
        
        return LINK_SUCCESS;
}

int LinkNewTsMuxUploaderWillPicAndSeg(LinkTsMuxUploader **_pTsMuxUploader, const LinkMediaArg *_pAvArg,
                                            const LinkUserUploadArg *_pUserUploadArg, const LinkPicUploadArg *_pPicArg) {

        if (_pUserUploadArg->nDeviceAkLen > DEVICE_AK_LEN || _pUserUploadArg->nDeviceSkLen > DEVICE_SK_LEN) {
                LinkLogError("ak or sk or app or devicename is too long");
                return LINK_ARG_TOO_LONG;
        }

        if (_pUserUploadArg->nDeviceAkLen > DEVICE_AK_LEN || _pUserUploadArg->nDeviceSkLen > DEVICE_SK_LEN) {
                LinkLogError("ak or sk or app or devicename is too long");
                return LINK_ARG_TOO_LONG;
        }

        if (_pUserUploadArg->nDeviceAkLen <= 0 || _pUserUploadArg->nDeviceSkLen <= 0) {
                LinkLogError("ak or sk or app or devicename is not exits");
                return LINK_ARG_ERROR;
        }
        
	int ret = LinkInitSegmentMgr();
        if (ret != LINK_SUCCESS) {
                LinkLogError("LinkInitSegmentMgr fail:%d", ret);
                return ret;
        }
        
        //LinkTsMuxUploader *pTsMuxUploader
        ret = linkNewTsMuxUploader(_pTsMuxUploader, _pAvArg, _pUserUploadArg, 1);
        if (ret != LINK_SUCCESS) {
                LinkUninitSegmentMgr();
                return ret;
        }
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)(*_pTsMuxUploader);

        SegmentHandle segHandle;
        SegmentArg arg;
        memset(&arg, 0, sizeof(arg));
        arg.getUploadParamCallback = uploadParamCallback;
        arg.pGetUploadParamCallbackArg = *_pTsMuxUploader;
        arg.pUploadStatisticCb = _pUserUploadArg->pUploadStatisticCb;
        arg.pUploadStatArg = _pUserUploadArg->pUploadStatArg;
        ret = LinkNewSegmentHandle(&segHandle, &arg);
        if (ret != LINK_SUCCESS) {
                LinkUninitSegmentMgr();
                LinkDestroyTsMuxUploader(_pTsMuxUploader);
                LinkLogError("LinkInitSegmentMgr fail:%d", ret);
                return ret;
        }
        pFFTsMuxUploader->segmentHandle = segHandle;
        
        LinkPicUploadFullArg fullArg;
        fullArg.getPicCallback = _pPicArg->getPicCallback;
        fullArg.pGetPicCallbackOpaque = _pPicArg->pGetPicCallbackOpaque;
        fullArg.getUploadParamCallback = uploadParamCallback;
        fullArg.pGetUploadParamCallbackOpaque = *_pTsMuxUploader;
        fullArg.pUploadStatisticCb = _pUserUploadArg->pUploadStatisticCb;
        fullArg.pUploadStatArg = _pUserUploadArg->pUploadStatArg;
        PictureUploader *pPicUploader;
        ret = LinkNewPictureUploader(&pPicUploader, &fullArg);
        if (ret != LINK_SUCCESS) {
                LinkUninitSegmentMgr();
                LinkDestroyTsMuxUploader(_pTsMuxUploader);
                LinkLogError("LinkNewPictureUploader fail:%d", ret);
                return ret;
        }
        
        pFFTsMuxUploader->pPicUploader = pPicUploader;
        return ret;
}

static int dupSessionMeta(LinkSessionMeta *metas, LinkSessionMeta **pDst) {
        int idx = 0;
        int total = 0;
        for (idx =0; idx < metas->len; idx++) {
                total += (metas->keylens[idx]+1);
                total += (metas->valuelens[idx] +1);
        }
        total += sizeof(void*) * metas->len * 2;
        total += sizeof(int) * metas->len * 2;
        
        char *tmp = (char *)malloc(total + sizeof(LinkSessionMeta));
        if (tmp == NULL) {
                return -1;
        }
        memset(tmp, 0, total + sizeof(LinkSessionMeta));
        memcpy(tmp, metas, sizeof(LinkSessionMeta));
        
        LinkSessionMeta *dst = (LinkSessionMeta *)tmp;
        
        tmp += sizeof(LinkSessionMeta);
        
        char *keysAddr = tmp;
        tmp += metas->len * sizeof(void *);
        dst->keys = (const char **)keysAddr;
        
        char *keylensAddr = tmp;
        tmp += metas->len * sizeof(int);
        dst->keylens = (int *)keylensAddr;
        
        char *valuesAddr = tmp;
        tmp += metas->len * sizeof(void *);
        dst->values = (const char **)valuesAddr;
        
        char *valuelensAddr = tmp;
        tmp += metas->len * sizeof(int);
        dst->valuelens = (int *)valuelensAddr;
        
        for (idx =0; idx < metas->len; idx++) {
                memcpy(keylensAddr, &metas->keylens[idx], sizeof(int));
                keylensAddr += sizeof(int);
                memcpy(valuelensAddr, &metas->valuelens[idx], sizeof(int));
                valuelensAddr += sizeof(int);
        }
        for (idx =0; idx < metas->len; idx++) {
                memcpy(keysAddr, &tmp, sizeof(void*));
                keysAddr += sizeof(void *);
                
                memcpy(tmp, metas->keys[idx], metas->keylens[idx]);
                tmp[metas->keylens[idx]] = 0;
                tmp += (metas->keylens[idx]+1);
                
                
                memcpy(valuesAddr, &tmp, sizeof(void*));
                valuesAddr += sizeof(void *);
                
                memcpy(tmp, metas->values[idx], metas->valuelens[idx]);
                tmp[metas->valuelens[idx]] = 0;
                tmp += (metas->valuelens[idx]+1);
        }
        
        dst->isOneShot = metas->isOneShot;
        dst->len = metas->len;
        *pDst = dst;
        return LINK_SUCCESS;
}

void LinkUploaderSetTsOutputCallback(IN LinkTsMuxUploader *_pTsMuxUploader,
                               IN LinkTsOutput _pTsDataCb, IN void * _pUserArg) {
        
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        if (pFFTsMuxUploader->pTsMuxCtx) {
                LinkTsUploaderSetTsCallback(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_, _pTsDataCb, _pUserArg, pFFTsMuxUploader->avArg);
        }
        return;
}

int LinkSetTsType(IN LinkTsMuxUploader *_pTsMuxUploader, IN LinkSessionMeta *metas) {
        if (_pTsMuxUploader == NULL || metas == NULL) {
                return LINK_ARG_ERROR;
        }
        
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        pthread_mutex_lock(&pFFTsMuxUploader->tokenMutex_);
        
        LinkSessionMeta *pDup = NULL;
        int ret = dupSessionMeta(metas, &pDup);
        if (ret != LINK_SUCCESS) {
                pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                return ret;
        }
        ret = LinkUpdateSegmentMeta(pFFTsMuxUploader->segmentHandle, pDup);
        if (ret != LINK_SUCCESS) {
                free(pDup);
                pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                return ret;
        }
        
        if (pFFTsMuxUploader->pTsMuxCtx && pFFTsMuxUploader->pTsMuxCtx->pTsUploader_) {
                LinkSessionMeta *pDup2 = NULL;
                ret = dupSessionMeta(metas, &pDup2);
                if (ret != LINK_SUCCESS) {
                        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                        return ret;
                }
                LinkSetSessionMeta(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_, pDup2);
        }
        
        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
        
        return LINK_SUCCESS;
}

void LinkClearTsType(IN LinkTsMuxUploader *_pTsMuxUploader) {
        if (_pTsMuxUploader == NULL) {
                return;
        }
        
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        pthread_mutex_lock(&pFFTsMuxUploader->tokenMutex_);
        int ret = LinkClearSegmentMeta(pFFTsMuxUploader->segmentHandle);
        if (ret != LINK_SUCCESS) {
                LinkLogError("LinkClearSegmentMeta fail:%d", ret);
                return;
        }
        
        if (pFFTsMuxUploader->pTsMuxCtx && pFFTsMuxUploader->pTsMuxCtx->pTsUploader_) {
                LinkClearSessionMeta(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_);
        }
        
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
        LinkLogDebug("pause switchts");
        switchTs(pFFTsMuxUploader, 0);
        
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
                LinkSendItIsTimeToCaptureSignal(pFFTsMuxUploader->pPicUploader, nTimestamp);
}

//TODO 可能需要其它回调
static void uploadThreadNumDec(void *pOpaque, int64_t nTimestamp) {
        FFTsMuxUploader * pFFTsMuxUploader = (FFTsMuxUploader *)pOpaque;
        pthread_mutex_lock(&pFFTsMuxUploader->muxUploaderMutex_);
      
        pthread_mutex_unlock(&pFFTsMuxUploader->muxUploaderMutex_);
}

int LinkTsMuxUploaderStart(LinkTsMuxUploader *_pTsMuxUploader)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        
        assert(pFFTsMuxUploader->pTsMuxCtx == NULL);
        
        int nBufsize = getBufferSize(pFFTsMuxUploader);
        int ret = newTsMuxContext(&pFFTsMuxUploader->pTsMuxCtx, &pFFTsMuxUploader->avArg,
                                  &pFFTsMuxUploader->uploadArgBak, nBufsize, pFFTsMuxUploader->queueType_);
        if (ret != 0) {
                return ret;
        }
        
        LinkTsUploaderSetTsEndUploadCallback(pFFTsMuxUploader->pTsMuxCtx->pTsUploader_, uploadThreadNumDec, pFFTsMuxUploader);
        
        return ret;
}

void LinkFlushUploader(IN LinkTsMuxUploader *_pTsMuxUploader) {
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)(_pTsMuxUploader);
        if (pFFTsMuxUploader->queueType_ != TSQ_APPEND) {
                return;
        }
        
        pthread_mutex_lock(&pFFTsMuxUploader->muxUploaderMutex_);
        LinkLogDebug("LinkFlushUploader switchts");
        switchTs(pFFTsMuxUploader, 0);
        pFFTsMuxUploader->nKeyFrameCount = 0;
        pFFTsMuxUploader->nFrameCount = 0;
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
        *_pTsMuxUploader = NULL;
        return;
}

static int getJsonString(Buffer *buf, const char *pFieldname, cJSON *pJsonRoot) {
        int nCpyLen = 0;
        cJSON * pNode = cJSON_GetObjectItem(pJsonRoot, pFieldname);
        if (pNode != NULL) {
                char *tmp;
                nCpyLen = strlen(pNode->valuestring);
                if (buf->nCap >= nCpyLen + 1) {
                        memcpy(buf->pData, pNode->valuestring, nCpyLen);
                        buf->pData[nCpyLen] = 0;
                        buf->nLen = nCpyLen;
                } else {
                        
                        tmp = malloc(nCpyLen + 256 + 1);
                        if (tmp == NULL) {
                                LinkLogError("no memory for json:%s", pFieldname);
                                return LINK_NO_MEMORY;
                        }
                        
                        if (buf->nCap > 0 && buf->pData) {
                                free(buf->pData);
                                buf->pData = NULL;
                        }
                        
                        buf->nCap = nCpyLen + 256 + 1;
                        buf->nLen = nCpyLen;
                        buf->pData = tmp;
                        memcpy(tmp, pNode->valuestring, nCpyLen);
                        tmp[nCpyLen] = 0;
                }
                
                return LINK_SUCCESS;
        }
        LinkLogInfo("not found %s in json", pFieldname);
        return LINK_JSON_FORMAT;
}

static int getRemoteConfig(FFTsMuxUploader* pFFTsMuxUploader, int *pUpdateConfigInterval) {

        RemoteConfig *pRc = &pFFTsMuxUploader->tmpRemoteConfig;
        
        char buf[2048] = {0};
        int nBufLen = sizeof(buf);
        int nRespLen = 0, ret = 0;
        
        int nOffsetHost = snprintf(buf, sizeof(buf), "%s", pFFTsMuxUploader->pConfigRequestUrl);
        int nOffset = snprintf(buf+nOffsetHost, sizeof(buf)-nOffsetHost, "?sn=%s", gSn);
        
        int nUrlLen = nOffsetHost + nOffset;
        buf[nUrlLen] = 0;
        char * pToken = buf + nUrlLen + 1;
        int nTokenOffset = snprintf(pToken, sizeof(buf)-nUrlLen-1, "%s:", pFFTsMuxUploader->ak);
        int nOutputLen = 30;
        
        //static int getHttpRequestSign(const char * pKey, int nKeyLen, char *method, char *pUrlWithPathAndQuery, char *pContentType,
        //char *pData, int nDataLen, char *pOutput, int *pOutputLen)
        ret = GetHttpRequestSign(pFFTsMuxUploader->sk, strlen(pFFTsMuxUploader->sk), "GET", buf, NULL, NULL,
                                        0, pToken+nTokenOffset, &nOutputLen);
        if (ret != LINK_SUCCESS) {
                LinkLogError("getHttpRequestSign error:%d", ret);
                return ret;
        }
        
        ret = LinkGetUserConfig(buf, buf, nBufLen, &nRespLen, pToken, nTokenOffset+nOutputLen);
        if (ret != LINK_SUCCESS) {
                return ret;
        }
        LinkLogInfo("remote config:%s", buf);
        
        cJSON * pJsonRoot = cJSON_Parse(buf);
        if (pJsonRoot == NULL) {
                return LINK_JSON_FORMAT;
        }
        
        cJSON *pNode = cJSON_GetObjectItem(pJsonRoot, "ttl");
        if (pNode == NULL) {
                cJSON_Delete(pJsonRoot);
                return LINK_JSON_FORMAT;
        }
        pRc->updateConfigInterval = pNode->valueint;
        *pUpdateConfigInterval = pNode->valueint;
        
        pNode = cJSON_GetObjectItem(pJsonRoot, "uploadMode");
        if (pNode != NULL) {
                pRc->planType = pNode->valueint;
        }
        
        cJSON *pSeg = cJSON_GetObjectItem(pJsonRoot, "segment");
        if (pSeg == NULL) {
                cJSON_Delete(pJsonRoot);
                return LINK_JSON_FORMAT;
        }
        
        ret = getJsonString(&pRc->upHostUrl, "uploadUrl", pSeg);
        if (ret == LINK_SUCCESS) {
                LinkLogInfo("uploadUrl:%s", pRc->upHostUrl.pData);
        } else if (ret == LINK_NO_MEMORY) {
                goto END;
        }
        
        ret = getJsonString(&pRc->upTokenRequestUrl, "uploadTokenRequestUrl", pSeg);
        if (ret == LINK_SUCCESS) {
                LinkLogInfo("tokenRequestUrl:%s", pRc->upTokenRequestUrl.pData);
        } else if (ret == LINK_NO_MEMORY) {
                goto END;
        }
        
        ret = getJsonString(&pRc->mgrTokenRequestUrl, "segmentReportUrl", pSeg);
        if (ret == LINK_SUCCESS) {
                LinkLogInfo("segReportUrl:%s", pRc->mgrTokenRequestUrl.pData);
        } else if (ret == LINK_NO_MEMORY) {
                goto END;
        }
        
        pNode = cJSON_GetObjectItem(pSeg, "tsDuration");
        if (pNode != NULL) {
                pRc->nTsDuration = pNode->valueint * 1000;
        }
        if (pRc->nTsDuration < 4000 || pRc->nTsDuration > 15000)
                pRc->nTsDuration = 4000;
        LinkLogInfo("tsDuration:%d", pRc->nTsDuration);
        
        pNode = cJSON_GetObjectItem(pSeg, "sessionDuration");
        if (pNode != NULL) {
                pRc->nSessionDuration = pNode->valueint * 1000;
        }
        LinkLogInfo("nSessionDuration:%d", pRc->nSessionDuration);
        
        pNode = cJSON_GetObjectItem(pSeg, "sessionTimeout");
        if (pNode != NULL) {
                pRc->nSessionTimeout = pNode->valueint * 1000;
        }
        if (pRc->nSessionTimeout < 1000) {
                pRc->nSessionTimeout = 3000;
        }
        LinkLogInfo("nSessionTimeout:%d", pRc->nSessionTimeout);
        
        cJSON_Delete(pJsonRoot);
        
        return LINK_SUCCESS;
        
END:
        cJSON_Delete(pJsonRoot);
        
        freeRemoteConfig(pRc);
        
        return LINK_NO_MEMORY;
}

static int updateToken(FFTsMuxUploader* pFFTsMuxUploader, int* pDeadline, SessionUpdateParam *pSParam) {
        int nBufLen = 1024;
        char *pBuf = (char *)malloc(nBufLen);
        if (pBuf == NULL) {
                return LINK_NO_MEMORY;
        }
        memset(pBuf, 0, nBufLen);
        
        Buffer bufToken ={0};
        bufToken.nCap = nBufLen;
        bufToken.pData = pBuf;
        Buffer bufFrameToken={0};
        Buffer bufTsToken={0};
        Buffer bufPrefix={0};
        
        int nPrefixLen = 256;
        char *pPrefix = (char *)malloc(nPrefixLen);
        memset(pPrefix, 0, nPrefixLen);
        if (pPrefix == NULL) {
                free(pBuf);
                return LINK_NO_MEMORY;
        }
        bufPrefix.pData = pPrefix;
        bufPrefix.nCap = nPrefixLen;
        
        
        int nUrlLen = snprintf(pBuf, nBufLen, "%s?session=%s&sequence=%"PRId64"", pFFTsMuxUploader->remoteConfig.upTokenRequestUrl.pData,
                                             pSParam->sessionId, pSParam->nSeqNum);
        pBuf[nUrlLen] = 0;
        char * pToken = pBuf + nUrlLen + 1;
        int nTokenOffset = snprintf(pToken, sizeof(pBuf)-nUrlLen-1, "%s:", pFFTsMuxUploader->ak);
        int nOutputLen = 30;
        int ret = GetHttpRequestSign(pFFTsMuxUploader->sk, strlen(pFFTsMuxUploader->sk), "GET", pBuf, NULL, NULL,
                                 0, pToken+nTokenOffset, &nOutputLen);
        if (ret != LINK_SUCCESS) {
                free(pBuf);
                free(pPrefix);
                LinkLogError("getHttpRequestSign error:%d", ret);
                return ret;
        }
        
        cJSON *pJsonRoot = NULL;
        ret = LinkGetUploadToken(&pJsonRoot, pBuf, pToken, nTokenOffset+nOutputLen);
        if (ret != LINK_SUCCESS) {
                free(pBuf);
                free(pPrefix);
                LinkLogError("LinkGetUploadToken fail:%d [%s]", ret, pFFTsMuxUploader->remoteConfig.upTokenRequestUrl.pData);
                return ret;
        }
        
        // pBuf, 1024, pDeadline, pPrefix, nPrefixLen,
        cJSON *pNode = cJSON_GetObjectItem(pJsonRoot, "ttl");
        if (pNode == NULL) {
                cJSON_Delete(pJsonRoot);
                return LINK_JSON_FORMAT;
        }
        *pDeadline = pNode->valueint;
        
        int getTsToken = getJsonString(&bufTsToken, "tsToken", pJsonRoot);
        if (getTsToken == LINK_SUCCESS) {
                LinkLogInfo("tsToken:%s", bufTsToken.pData);
        }
        
        int getFrameToken = getJsonString(&bufFrameToken, "frameToken", pJsonRoot);
        if (getFrameToken == LINK_SUCCESS) {
                LinkLogInfo("frameToken:%s", bufFrameToken.pData);
        }
        
        ret = getJsonString(&bufToken, "token", pJsonRoot);
        if (ret == LINK_SUCCESS) {
                LinkLogInfo("token:%s", bufToken.pData);
        } else if (ret == LINK_NO_MEMORY) {
                if(getTsToken != LINK_SUCCESS || getFrameToken != LINK_SUCCESS)
                        goto END;
        }
        
        ret = getJsonString(&bufPrefix, "fnamePrefix", pJsonRoot);
        if (ret == LINK_SUCCESS) {
                LinkLogInfo("fnamePrefix:%s", bufPrefix.pData);
        } else if (ret == LINK_NO_MEMORY) {
                if(getTsToken != LINK_SUCCESS || getFrameToken != LINK_SUCCESS)
                        goto END;
        }
        ret = LINK_SUCCESS;

        if (bufPrefix.pData) {
                if (bufPrefix.pData[bufPrefix.nLen-1] == '/') {
                        bufPrefix.pData[bufPrefix.nLen-1] = 0;
                        bufPrefix.nLen--;
                }
        }
        int isCompatableMode = 0;
        if(getTsToken != LINK_SUCCESS || getFrameToken != LINK_SUCCESS)
                isCompatableMode = 1;
        setToken(pFFTsMuxUploader, bufToken, bufTsToken, bufFrameToken, bufPrefix, isCompatableMode);
END:
        if (ret != LINK_SUCCESS) {
                if (bufTsToken.pData)
                        free(bufTsToken.pData);
                if (bufFrameToken.pData)
                        free(bufFrameToken.pData);
                if (bufToken.pData)
                        free(bufToken.pData);
                if (bufPrefix.pData)
                        free(bufPrefix.pData);
        }
        
        cJSON_Delete(pJsonRoot);
        return LINK_SUCCESS;
}

static void *linkTokenAndConfigThread(void * pOpaque) {
        FFTsMuxUploader* pFFTsMuxUploader = (FFTsMuxUploader*) pOpaque;
        
        int rcSleepUntilTime = 0x7FFFFFFF;
        int nNextTryRcTime = 1;
        int shouldUpdateRc = 0;
        
        int tokenSleepUntilTime = 0x7FFFFFFF;
        int nNextTryTokenTime = 1;
        int shouldUpdateToken = 0;
        
        int  nUpdateConfigInterval = 0;
        int now;
        LinkUploaderStatInfo info;
        char sessionIdBak[LINK_MAX_SESSION_ID_LEN+1] = {0};
        
        while(!pFFTsMuxUploader->isQuit || info.nLen_ != 0) {

                int ret = 0;
                now = (int)(LinkGetCurrentNanosecond() / 1000000000);
                
                int nWaitRc = rcSleepUntilTime - now;
                int nWaitToken = tokenSleepUntilTime - now;
                int nWait = nWaitRc;
                int nRcTimeout = 1;
                if (!shouldUpdateRc && shouldUpdateToken && nWaitToken > 0 && nWaitToken < nWait) {
                        nRcTimeout = 0;
                        nWait = nWaitToken;
                }
                //fprintf(stderr, "sleep:%d\n", nWait);
                SessionUpdateParam param;
                memset(&param, 0, sizeof(param));
                if (nWait < 0) {
                        LinkLogWarn("nWait abnormal. set to 0:%d", nWait);
                        nWait = 0;
                }
                ret = pFFTsMuxUploader->pUpdateQueue_->PopWithTimeout(pFFTsMuxUploader->pUpdateQueue_, (char *)(&param),
                                                                      sizeof(SessionUpdateParam), nWait * 1000000LL);
                memset(&info, 0, sizeof(info));
                pFFTsMuxUploader->pUpdateQueue_->GetStatInfo(pFFTsMuxUploader->pUpdateQueue_, &info);
                LinkLogDebug("pUpdateQueue_ %d :%d",ret, info.nLen_);
                if (ret <= 0) {
                        if (ret == LINK_TIMEOUT) {
                                LinkLogDebug("update queue timeout:%d", nWait);
                        } else if (ret == 0) { //due to time error
                                LinkLogWarn("time error. set to timeout state");
                                ret = LINK_TIMEOUT;
                        } else {
                                LinkLogError("update queue error. popqueue fail:%d wait:%d", ret, nWait);
                                continue;
                        }
                }
                //fprintf(stderr, "pop cmd:%d %d\n", param.nType, ret);

                
                if (param.nType == 3) { //quit
                        continue;
                }
                if (param.sessionId[0] != 0) {
                        memcpy(sessionIdBak, param.sessionId, sizeof(sessionIdBak));
                }
                
                int getRcOk = 0; // after getRemoteConfig success,must update token
                if (param.nType == 2 || shouldUpdateRc || (ret == LINK_TIMEOUT && nRcTimeout)) {
                        getRcOk = 0;
                        ret = getRemoteConfig(pFFTsMuxUploader, &nUpdateConfigInterval);
                        now = (int)(LinkGetCurrentNanosecond() / 1000000000);
                        if (ret != LINK_SUCCESS) {
                          
                                if (nNextTryRcTime > 16)
                                        nNextTryRcTime = 16;
                                LinkLogInfo("sleep %d time to get remote config:%d", nNextTryRcTime, ret);
                                rcSleepUntilTime = now + nNextTryRcTime;
                                nNextTryRcTime *= 2;
                                shouldUpdateRc = 1;
                                
                                shouldUpdateToken = 0; //rc is Higher priority than token
                                nNextTryTokenTime = 1;
    
                                continue;
                        }
                        LinkLogDebug("updateRemoteConfig");
                        updateRemoteConfig(pFFTsMuxUploader);
                        nNextTryRcTime = 1;
                        shouldUpdateRc = 0;
                        shouldUpdateToken = 0; //rc is Higher priority than token
                        nNextTryTokenTime = 1;
                        if (nUpdateConfigInterval > 1544756657)
                                rcSleepUntilTime = nUpdateConfigInterval;
                        else
                                rcSleepUntilTime = now + nUpdateConfigInterval;
                        nUpdateConfigInterval = 0;
                        getRcOk = 1;

                }
                
                if (shouldUpdateRc) {
                        LinkLogWarn("before update token, must update remote config");
                        continue;
                }
                
                if (param.nType == 1 || shouldUpdateToken || getRcOk) { //update token
                        if (param.sessionId[0] == 0) {
                                 memcpy(param.sessionId, sessionIdBak, sizeof(sessionIdBak));
                        }
                        param.nSeqNum = pFFTsMuxUploader->uploadArgBak.nSegSeqNum;
                        LinkLogDebug("updateToken");
                        ret = updateToken(pFFTsMuxUploader, &tokenSleepUntilTime, &param);
                        now = (int)(LinkGetCurrentNanosecond() / 1000000000);
                        if (ret != LINK_SUCCESS) {
                                if (nNextTryTokenTime > 16) {
                                        nNextTryTokenTime = 16;
                                }
                                tokenSleepUntilTime = now + nNextTryTokenTime;
                                shouldUpdateToken = 1;
                                nNextTryTokenTime *= 2;
                                continue;
                        }
                        nNextTryTokenTime = 1;
                        shouldUpdateToken = 0;
                }
        }
        
        return NULL;
}

static int linkTsMuxUploaderTokenThreadStart(FFTsMuxUploader* pFFTsMuxUploader) {
        
        int ret = LinkNewCircleQueue(&pFFTsMuxUploader->pUpdateQueue_, 1, TSQ_FIX_LENGTH, sizeof(SessionUpdateParam), 50);
        if (ret != 0) {
                return ret;
        }
        
        ret = pthread_create(&pFFTsMuxUploader->tokenThread, NULL, linkTokenAndConfigThread, pFFTsMuxUploader);
        if (ret != 0) {
                LinkDestroyQueue(&pFFTsMuxUploader->pUpdateQueue_);
                return LINK_THREAD_ERROR;
        }

        return LINK_SUCCESS;
}
