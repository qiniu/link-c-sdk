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

typedef struct _Token {
        char * pToken_;
        int nTokenLen_;
}Token;

typedef struct  {
        int updateConfigInterval;
        int nTsDuration; //millisecond
        int nSessionDuration; //millisecond
        int nSessionTimeout; //millisecond
        char *pMgrTokenRequestUrl;
        char *pUpTokenRequestUrl;
        char *pUpHostUrl;
        
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
        
        int64_t nFirstTimestamp; //determinate if to cut ts file
        int64_t nLastPicCallbackSystime; //upload picture need
        int nKeyFrameCount;
        int nFrameCount;
        LinkMediaArg avArg;
        LinkUploadState ffMuxSatte;
        
        int nUploadBufferSize;
        
        char deviceName_[LINK_MAX_DEVICE_NAME_LEN+1];
        char app_[LINK_MAX_APP_LEN+1];
        Token token_;
        LinkTsUploadArg uploadArgBak;
        PictureUploader *pPicUploader;
        SegmentHandle segmentHandle;
        enum CircleQueuePolicy queueType_;
        int8_t isPause;
        int8_t isTypeOneshot;
        int8_t isQuit;
        char tsType[16];
        
        pthread_t tokenThread;
        
        char *pVideoMeta;
        int nVideoMetaLen;
        
        char ak[41];
        char sk[41];
        char *pConfigRequestUrl;
        RemoteConfig remoteConfig;
        
        LinkCircleQueue *pUpdateQueue_; //for token and remteconfig update
        char sessionId[LINK_MAX_SESSION_ID_LEN+1];
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

static void switchTs(FFTsMuxUploader *_pFFTsMuxUploader)
{
        if (_pFFTsMuxUploader) {
                
                if (_pFFTsMuxUploader->pTsMuxCtx) {
                        
                        if (_pFFTsMuxUploader->queueType_ == TSQ_APPEND)
                                _pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->NotifyDataPrapared(_pFFTsMuxUploader->pTsMuxCtx->pTsUploader_);
                        LinkResetTsMuxerContext( _pFFTsMuxUploader->pTsMuxCtx->pFmtCtx_);
                }
        }
        return;
}

static void pushRecycle(FFTsMuxUploader *_pFFTsMuxUploader)
{
        if (_pFFTsMuxUploader) {
                
                if (_pFFTsMuxUploader->pTsMuxCtx) {

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
        enum LinkUploaderTimeInfoType tmtype = LINK_VIDEO_TIMESTAMP;
        if (_nFlag == LINK_STREAM_TYPE_AUDIO){
                tmtype = LINK_AUDIO_TIMESTAMP;
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

                        LinkMuxerAudio(pTsMuxCtx->pFmtCtx_, (uint8_t *)pFFTsMuxUploader->pAACBuf, varHeader.aac_frame_length, _nTimestamp);

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
                pTsMuxCtx->pTsUploader_->ReportTimeInfo(pTsMuxCtx->pTsUploader_, _nTimestamp, nSysNanotime, tmtype);
        } else {
                if (pFFTsMuxUploader->ffMuxSatte != LINK_UPLOAD_FAIL)
                        LinkLogError("Error muxing packet:%d", ret);
                pFFTsMuxUploader->ffMuxSatte = LINK_UPLOAD_FAIL;
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

static int checkSwitch(LinkTsMuxUploader *_pTsMuxUploader, int64_t _nTimestamp, int nIsKeyFrame, int _isVideo,
                       int64_t nSysNanotime, int _nIsSegStart, const char * _pData, int _nDataLen)
{
        int ret;
        int shouldSwitch = 0;
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader *)_pTsMuxUploader;
        if (pFFTsMuxUploader->nFirstTimestamp == -1) {
                pFFTsMuxUploader->nFirstTimestamp = _nTimestamp;
        }
        if (pFFTsMuxUploader->pTsMuxCtx) {
                pFFTsMuxUploader->ffMuxSatte = pFFTsMuxUploader->pTsMuxCtx->pTsUploader_->GetUploaderState(
                                        pFFTsMuxUploader->pTsMuxCtx->pTsUploader_);
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
                if( ((_nTimestamp - pFFTsMuxUploader->nFirstTimestamp) > pFFTsMuxUploader->remoteConfig.nTsDuration
                                      && pFFTsMuxUploader->nKeyFrameCount > 0)
                   //at least 1 keyframe and aoubt last 5 second
                   || shouldSwitch
                   || (_nIsSegStart && pFFTsMuxUploader->nFrameCount != 0)// new segment is specified
                   ||  pFFTsMuxUploader->ffMuxSatte != LINK_UPLOAD_INIT){   // upload finished
                        //printf("next ts:%d %"PRId64"\n", pFFTsMuxUploader->nKeyFrameCount, _nTimestamp - pFFTsMuxUploader->nLastUploadVideoTimestamp);
                        pFFTsMuxUploader->nKeyFrameCount = 0;
                        pFFTsMuxUploader->nFrameCount = 0;
                        pFFTsMuxUploader->nFirstTimestamp = _nTimestamp;
                        pFFTsMuxUploader->ffMuxSatte = LINK_UPLOAD_INIT;
                        fprintf(stderr, "normal switchts\n");
                        switchTs(pFFTsMuxUploader);
                        int64_t nDiff = (nSysNanotime - pFFTsMuxUploader->nLastPicCallbackSystime)/1000000;
                        if (nIsKeyFrame) {
                                if (nDiff < 1000) {
                                        LinkLogWarn("get picture callback too frequency:%"PRId64"ms", nDiff);
                                }
                                pFFTsMuxUploader->nLastPicCallbackSystime = nSysNanotime;
                        }
                        if (_nIsSegStart) {
                                pFFTsMuxUploader->uploadArgBak.nSegmentId_ = pFFTsMuxUploader->nLastPicCallbackSystime;
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
                fprintf(stderr, "video nomem switchts\n");
                switchTs(pFFTsMuxUploader);
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
                 fprintf(stderr, "audio nomem switchts\n");
                switchTs(pFFTsMuxUploader);
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
                        if (pFFTsMuxUploader->token_.pToken_) {
                                free(pFFTsMuxUploader->token_.pToken_);
                                pFFTsMuxUploader->token_.pToken_ = NULL;
                        }
                        freeRemoteConfig(&pFFTsMuxUploader->remoteConfig);
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

static void updateSegmentId(void *_pOpaque, void* pArg, int64_t nNow, int64_t nEnd)
{
        FFTsMuxUploader *pFFTsMuxUploader = (FFTsMuxUploader*)_pOpaque;
        LinkTsUploadArg *_pUploadArg = (LinkTsUploadArg *)pArg;

        LinkUpdateSegment(pFFTsMuxUploader->segmentHandle, nNow/1000000, nEnd/1000000, 0);
        
        if (pFFTsMuxUploader->uploadArgBak.nSegmentId_ == 0) {
                pFFTsMuxUploader->uploadArgBak.nLastEndTsTime = nEnd;
                pFFTsMuxUploader->uploadArgBak.nLastStartTime_ = _pUploadArg->nLastStartTime_;
                pFFTsMuxUploader->uploadArgBak.nSegmentId_ = _pUploadArg->nSegmentId_;
                pFFTsMuxUploader->uploadArgBak.nSegSeqNum = 0;
                _pUploadArg->nSegSeqNum = pFFTsMuxUploader->uploadArgBak.nSegSeqNum;
                return;
        }
        
        int64_t nDuration = pFFTsMuxUploader->uploadArgBak.nLastEndTsTime - pFFTsMuxUploader->uploadArgBak.nSegmentId_;
        if (pFFTsMuxUploader->remoteConfig.nSessionDuration <= nDuration / 1000000LL) {
                //TODO report seg info
        }

        int64_t nDiff = pFFTsMuxUploader->remoteConfig.nSessionTimeout * 1000000LL;
        if (nNow - pFFTsMuxUploader->uploadArgBak.nLastEndTsTime >= nDiff) {
                pFFTsMuxUploader->uploadArgBak.nSegmentId_ = nNow;
                _pUploadArg->nSegmentId_ = nNow;
                pFFTsMuxUploader->uploadArgBak.nSegSeqNum = 0;
        } else {
                pFFTsMuxUploader->uploadArgBak.nSegSeqNum++;
        }
        pFFTsMuxUploader->uploadArgBak.nLastEndTsTime = nEnd;
        _pUploadArg->nSegSeqNum = pFFTsMuxUploader->uploadArgBak.nSegSeqNum;
        pFFTsMuxUploader->uploadArgBak.nLastStartTime_ = _pUploadArg->nLastStartTime_;
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

static void freeRemoteConfig(RemoteConfig *pRc) {
        if (pRc->pUpHostUrl) {
                free(pRc->pUpHostUrl);
                pRc->pUpHostUrl = NULL;
        }
        
        if (pRc->pMgrTokenRequestUrl) {
                free(pRc->pMgrTokenRequestUrl);
                pRc->pMgrTokenRequestUrl = NULL;
        }
        
        if (pRc->pUpTokenRequestUrl) {
                free(pRc->pUpTokenRequestUrl);
                pRc->pUpTokenRequestUrl = NULL;
        }
        
        return;
}

static void updateRemoteConfig(FFTsMuxUploader *pFFTsMuxUploader, RemoteConfig *pRc) {
        pthread_mutex_lock(&pFFTsMuxUploader->tokenMutex_);
        freeRemoteConfig(&pFFTsMuxUploader->remoteConfig);
        pFFTsMuxUploader->remoteConfig = *pRc;
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
        
        int ret = 0;
        
        memcpy(pFFTsMuxUploader->deviceName_, _pUserUploadArg->pDeviceName, _pUserUploadArg->nDeviceNameLen);
        memcpy(pFFTsMuxUploader->app_, _pUserUploadArg->pApp, _pUserUploadArg->nAppLen);
        
        if (isWithPicAndSeg) {
                pFFTsMuxUploader->uploadArgBak.pUploadArgKeeper_ = pFFTsMuxUploader;
                pFFTsMuxUploader->uploadArgBak.UploadSegmentIdUpdate = updateSegmentId;
        } else {
                pFFTsMuxUploader->uploadArgBak.UploadSegmentIdUpdate = NULL;
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
        
        
        SessionUpdateParam upparam;
        upparam.nSeqNum = pParam->nSeqNum;
        if (pParam->sessionId[0] != 0) {
                strcpy(upparam.sessionId, pParam->sessionId);
                
                if (pFFTsMuxUploader->sessionId[0] == 0 || strcmp(pParam->sessionId, pFFTsMuxUploader->sessionId) != 0) {
                        //update remote config
                        upparam.nType = 2;
                        pFFTsMuxUploader->pUpdateQueue_->Push(pFFTsMuxUploader->pUpdateQueue_, (char *)&upparam, sizeof(SessionUpdateParam));
                        strcpy(pFFTsMuxUploader->sessionId, pParam->sessionId);
                }
        }
        
        upparam.nType = 1;
        if (pParam->nTokenDeadline > 1544421373) {
                int shouldUpdateToken = pParam->nTokenDeadline - (int)(LinkGetCurrentNanosecond() / 1000000000);
                if (shouldUpdateToken <= 20) {
                        pFFTsMuxUploader->pUpdateQueue_->Push(pFFTsMuxUploader->pUpdateQueue_, (char *)&upparam, sizeof(SessionUpdateParam));
                }
        }
        
        if (cbtype == LINK_UPLOAD_CB_UPTOKEN) {
                pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                return LINK_SUCCESS;
        }
        
        if (pFFTsMuxUploader->token_.pToken_ == NULL) {
                pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                return LINK_NOT_INITED;
        }
        
        if (pParam->pTokenBuf != NULL) {
                if (pParam->nTokenBufLen - 1 < pFFTsMuxUploader->token_.nTokenLen_) {
                        LinkLogError("get token buffer is small:%d %d", pFFTsMuxUploader->token_.nTokenLen_, pParam->nTokenBufLen);
                        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                        return LINK_BUFFER_IS_SMALL;
                }
                memcpy(pParam->pTokenBuf, pFFTsMuxUploader->token_.pToken_, pFFTsMuxUploader->token_.nTokenLen_);
                pParam->nTokenBufLen = pFFTsMuxUploader->token_.nTokenLen_;
                pParam->pTokenBuf[pFFTsMuxUploader->token_.nTokenLen_] = 0;
        }
        
        
        if (pParam->pUpHost != NULL) {
                int nUpHostLen = strlen(pFFTsMuxUploader->remoteConfig.pUpHostUrl);
                if (pParam->nUpHostLen - 1 < nUpHostLen) {
                        LinkLogError("get uphost buffer is small:%d %d", pFFTsMuxUploader->remoteConfig.pUpHostUrl, nUpHostLen);
                        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                        return LINK_BUFFER_IS_SMALL;
                }
                memcpy(pParam->pUpHost, pFFTsMuxUploader->remoteConfig.pUpHostUrl, nUpHostLen);
                pParam->nUpHostLen = nUpHostLen;
                pParam->pUpHost[nUpHostLen] = 0;
        }
        
        if (pParam->pSegUrl != NULL) {
                int nSegLen = strlen(pFFTsMuxUploader->remoteConfig.pMgrTokenRequestUrl);
                if (pParam->nSegUrlLen - 1 < nSegLen) {
                        LinkLogError("get segurl buffer is small:%d %d", pFFTsMuxUploader->remoteConfig.pMgrTokenRequestUrl, nSegLen);
                        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                        return LINK_BUFFER_IS_SMALL;
                }
                memcpy(pParam->pSegUrl, pFFTsMuxUploader->remoteConfig.pMgrTokenRequestUrl, nSegLen);
                pParam->nSegUrlLen = nSegLen;
                pParam->pSegUrl[nSegLen] = 0;
        }
        
        if (pParam->pTypeBuf != NULL && pParam->nTypeBufLen != 0) {
                int nTypeLen = strlen(pFFTsMuxUploader->tsType);
                if (pParam->nTypeBufLen > nTypeLen) {
                        memcpy(pParam->pTypeBuf, pFFTsMuxUploader->tsType, nTypeLen);
                        pParam->pTypeBuf[nTypeLen] = 0;
                        pParam->nTypeBufLen = nTypeLen;
                } else {
                        pParam->nTypeBufLen = 0;
                }
                if (pFFTsMuxUploader->isTypeOneshot) {
                        pFFTsMuxUploader->isTypeOneshot = 0;
                        memset(pFFTsMuxUploader->tsType, 0, sizeof(pFFTsMuxUploader->tsType));
                }
        }
        
        if (pParam->pDeviceName != NULL) {
                int nDeviceNameLen = strlen(pFFTsMuxUploader->deviceName_);
                if (pParam->nDeviceNameLen - 1 < nDeviceNameLen) {
                        LinkLogError("get segurl buffer is small:%d %d", pFFTsMuxUploader->deviceName_, nDeviceNameLen);
                        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                        return LINK_BUFFER_IS_SMALL;
                }
                memcpy(pParam->pDeviceName, pFFTsMuxUploader->deviceName_, nDeviceNameLen);
                pParam->nDeviceNameLen = nDeviceNameLen;
                pParam->pDeviceName[nDeviceNameLen] = 0;
        }
        
        if (pParam->pApp != NULL) {
                int nAppLen = strlen(pFFTsMuxUploader->app_);
                if (pParam->nAppLen - 1 < nAppLen) {
                        LinkLogError("get segurl buffer is small:%d %d", pFFTsMuxUploader->app_, nAppLen);
                        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
                        return LINK_BUFFER_IS_SMALL;
                }
                memcpy(pParam->pApp, pFFTsMuxUploader->app_, nAppLen);
                pParam->nAppLen = nAppLen;
                pParam->pApp[nAppLen] = 0;
        }
        
        pthread_mutex_unlock(&pFFTsMuxUploader->tokenMutex_);
        pParam->nTokenBufLen = pFFTsMuxUploader->token_.nTokenLen_;
        return LINK_SUCCESS;
}

int LinkNewTsMuxUploaderWillPicAndSeg(LinkTsMuxUploader **_pTsMuxUploader, const LinkMediaArg *_pAvArg,
                                            const LinkUserUploadArg *_pUserUploadArg, const LinkPicUploadArg *_pPicArg) {
        if (_pUserUploadArg->nDeviceAkLen > DEVICE_AK_LEN || _pUserUploadArg->nDeviceSkLen > DEVICE_SK_LEN
            || _pUserUploadArg->nAppLen > 32 || _pUserUploadArg->nDeviceNameLen > 32) {
                LinkLogError("ak or sk or app or devicename is too long");
                return LINK_ARG_TOO_LONG;
        }
        if (_pUserUploadArg->nDeviceAkLen <= 0 || _pUserUploadArg->nDeviceSkLen <= 0
            || _pUserUploadArg->nAppLen <= 0 || _pUserUploadArg->nDeviceNameLen <= 0) {
                LinkLogError("ak or sk or app or devicename is not exits");
                return LINK_ARG_ERROR;
        }
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
        memset(&arg, 0, sizeof(arg));
        arg.getUploadParamCallback = uploadParamCallback;
        arg.pGetUploadParamCallbackArg = *_pTsMuxUploader;
        arg.pUploadStatisticCb = _pUserUploadArg->pUploadStatisticCb;
        arg.pUploadStatArg = _pUserUploadArg->pUploadStatArg;
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
        fullArg.getUploadParamCallback = uploadParamCallback;
        fullArg.pGetUploadParamCallbackOpaque = *_pTsMuxUploader;
        fullArg.pUploadStatisticCb = _pUserUploadArg->pUploadStatisticCb;
        fullArg.pUploadStatArg = _pUserUploadArg->pUploadStatArg;
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
        fprintf(stderr, "pause switchts\n");
        switchTs(pFFTsMuxUploader);
        
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
        fprintf(stderr, "LinkFlushUploader switchts\n");
        switchTs(pFFTsMuxUploader);
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


static int getRemoteConfig(FFTsMuxUploader* pFFTsMuxUploader, RemoteConfig *pRc) {

        memset(pRc, 0, sizeof(RemoteConfig));
        char buf[2048] = {0};
        int nBufLen = sizeof(buf);
        int nRespLen = 0, ret = 0;
        
        int nOffsetHost = snprintf(buf, sizeof(buf), "%s", pFFTsMuxUploader->pConfigRequestUrl);
        int nOffset = snprintf(buf+nOffsetHost, sizeof(buf)-nOffsetHost, "?sn=%s\n", gSn);
        const char *pInput = strchr(buf+8, '/');
        
        //const char *pInput = (const char *)buf + nOffsetHost;
        char *pOutput = buf + nOffsetHost + nOffset + 1;
        int nOutputLen = sizeof(buf) - nOffset - nOffsetHost - 1;
        
        ret = HmacSha1(pFFTsMuxUploader->sk, strlen(pFFTsMuxUploader->sk), pInput, strlen(pInput), pOutput, &nOutputLen);
        if (ret != LINK_SUCCESS) {
                return ret;
        }
        buf[nOffsetHost + nOffset - 1] = 0;

        char *pToken = pOutput + nOutputLen;
        int nTokenOffset = snprintf(pToken, 42, "%s:", pFFTsMuxUploader->ak);
        int nB64Len = urlsafe_b64_encode(pOutput, nOutputLen, pToken + nTokenOffset, 30);
        
        ret = LinkGetUserConfig(buf, buf, nBufLen, &nRespLen, pToken, nTokenOffset+nB64Len);
        if (ret != LINK_SUCCESS) {
                return ret;
        }
        
        //TODO parse result
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
        
        
        cJSON *pSeg = cJSON_GetObjectItem(pJsonRoot, "segment");
        if (pSeg == NULL) {
                cJSON_Delete(pJsonRoot);
                return LINK_JSON_FORMAT;
        }
        
        int nCpyLen = 0;
        pNode = cJSON_GetObjectItem(pSeg, "uploadUrl");
        if (pNode != NULL) {
                nCpyLen = strlen(pNode->valuestring);
                pRc->pUpHostUrl = malloc(nCpyLen + 1);
                if (pRc->pUpHostUrl == NULL) {
                        goto END;
                }
                memcpy(pRc->pUpHostUrl, pNode->valuestring, nCpyLen);
                pRc->pUpHostUrl[nCpyLen] = 0;
        }
        
        pNode = cJSON_GetObjectItem(pSeg, "tokenRequestUrl");
        if (pNode != NULL) {
                nCpyLen = strlen(pNode->valuestring);
                pRc->pUpTokenRequestUrl = malloc(nCpyLen + 1);
                if (pRc->pUpTokenRequestUrl == NULL) {
                        goto END;
                }
                memcpy(pRc->pUpTokenRequestUrl, pNode->valuestring, nCpyLen);
                pRc->pUpTokenRequestUrl[nCpyLen] = 0;
                LinkLogInfo("tokenurl:%s", pRc->pUpTokenRequestUrl);
        }
        
        
        pNode = cJSON_GetObjectItem(pSeg, "segReportUrl");
        if (pNode != NULL) {
                nCpyLen = strlen(pNode->valuestring);
                pRc->pMgrTokenRequestUrl = malloc(nCpyLen + 1);
                if (pRc->pMgrTokenRequestUrl == NULL) {
                        goto END;
                }
                memcpy(pRc->pMgrTokenRequestUrl, pNode->valuestring, nCpyLen);
                pRc->pMgrTokenRequestUrl[nCpyLen] = 0;
                LinkLogInfo("segReportUrl:%s", pRc->pMgrTokenRequestUrl);
        }
        
        
        pNode = cJSON_GetObjectItem(pSeg, "tsDuration");
        if (pNode != NULL) {
                pRc->nTsDuration = pNode->valueint * 1000;
        }
        if (pRc->nTsDuration < 5000 || pRc->nTsDuration > 15000)
                pRc->nTsDuration = 5000;
        LinkLogInfo("tsDuration:%d", pRc->nTsDuration);
        
        pNode = cJSON_GetObjectItem(pSeg, "sessionDuration");
        if (pNode != NULL) {
                pRc->nSessionDuration = pNode->valueint * 1000;
        }
        if (pRc->nSessionDuration < 600 * 1000) {
                pRc->nSessionDuration = 600 * 1000;
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
        char *pBuf = (char *)malloc(1024);
        memset(pBuf, 0, 1024);
        snprintf(pBuf, 1024, "%s?session=%s&sequence=%"PRId64"", pFFTsMuxUploader->remoteConfig.pUpTokenRequestUrl,
                 pSParam->sessionId, pSParam->nSeqNum);
        
        // /v1/device/uploadtoken?session=<session>&sequence=<sequence>
        
        int ret = LinkGetUploadToken(pBuf, 1024, pDeadline, pBuf);
        if (ret != LINK_SUCCESS) {
                free(pBuf);
                LinkLogError("LinkGetUploadToken fail:%d [%s]", ret, pFFTsMuxUploader->remoteConfig.pUpTokenRequestUrl);
                return ret;
        }
        setToken(pFFTsMuxUploader, pBuf, strlen(pBuf));
        LinkLogInfo("gettoken:%s", pBuf);
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
        
        RemoteConfig rc;
        int now;
        LinkUploaderStatInfo info;
        while(!pFFTsMuxUploader->isQuit || info.nLen_ != 0) {

                int ret = 0;
                
                SessionUpdateParam param;
                param.nType = 0;
                now = (int)(LinkGetCurrentNanosecond() / 1000000000);
                
                int nWaitRc = rcSleepUntilTime - now;
                int nWaitToken = tokenSleepUntilTime - now;
                int nWait = nWaitRc;
                if (!shouldUpdateRc && shouldUpdateToken && nWaitToken < nWait) {
                        nWait = nWaitToken;
                }
                ret = pFFTsMuxUploader->pUpdateQueue_->PopWithTimeout(pFFTsMuxUploader->pUpdateQueue_, (char *)(&param),
                                                                      sizeof(SessionUpdateParam), nWait);
                
                if (ret < 0 && (ret != LINK_SUCCESS && ret != LINK_TIMEOUT)) {
                        LinkLogError("updatequeue fail:%d", ret);
                        continue;
                }
                
                if (param.nType == 3) {
                        continue;
                }

                memset(&info, 0, sizeof(info));
                pFFTsMuxUploader->pUpdateQueue_->GetStatInfo(pFFTsMuxUploader->pUpdateQueue_, &info);
                
                int getRcOk = 0; // after getRemoteConfig success,must update token
                if (param.nType == 2 || shouldUpdateRc || ret == LINK_TIMEOUT) {
                        getRcOk = 0;
                        ret = getRemoteConfig(pFFTsMuxUploader, &rc);
                        now = (int)(LinkGetCurrentNanosecond() / 1000000000);
                        if (ret != LINK_SUCCESS) {
                                if (nNextTryRcTime > 16)
                                        nNextTryRcTime = 16;
                                LinkLogInfo("sleep %d time to get remote config:%d", nNextTryRcTime);
                                rcSleepUntilTime = now + nNextTryRcTime;
                                nNextTryRcTime *= 2;
                                shouldUpdateRc = 1;
                                
                                shouldUpdateToken = 0; //rc is Higher priority than token
                                nNextTryTokenTime = 1;
                                continue;
                        }
                        updateRemoteConfig(pFFTsMuxUploader, &rc);
                        nNextTryRcTime = 1;
                        shouldUpdateRc = 0;
                        rcSleepUntilTime = now + rc.updateConfigInterval;
                        getRcOk = 1;

                }
                
                if (shouldUpdateRc) {
                        LinkLogWarn("before update token, must update remote config");
                        continue;
                }
                
                if (param.nType == 1 || shouldUpdateToken || getRcOk) { //update token
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
        
        int ret = LinkNewCircleQueue(&pFFTsMuxUploader->pUpdateQueue_, 0, TSQ_FIX_LENGTH, sizeof(SessionUpdateParam), 50);
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
