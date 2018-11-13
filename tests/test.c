#include <stdio.h>
#ifdef TEST_WITH_FFMPEG
#include <libavformat/avformat.h>
#endif
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include "tsuploaderapi.h"
#include "localkey.h"
#include "adts.h"
#include "flag.h"

#ifdef __APPLE__
#define MATERIAL_PATH "../../../tests/material/"
#else
#define MATERIAL_PATH "../../tests/material/"
#endif

void JustTestSegmentMgr(const char *pUpToken, const char *pMgrUrl);
void justTestSyncUploadPicture(const char *pTokenUrl);
void justTestAsyncUploadPicture(const char *pTokenUrl);

typedef struct {
        bool IsInputFromFFmpeg;
        bool IsTestAAC;
        bool IsTestAACWithoutAdts;
        bool IsTestTimestampRollover;
        bool IsTestH265;
        bool IsLocalToken;;
        bool IsNoAudio;
        bool IsNoVideo;
        bool IsTestMove;
        bool IsTwoUpload;
        bool IsTwoFileUpload;
        bool IsWriteVideoFrame;
        int nSleeptime;
        int nFirstFrameSleeptime;
        int64_t nRolloverTestBase;
        int nUptokenInterval;
        int nQbufSize;
        int nNewSegIntval;
        const char *pAFilePath;
        const char *pVFilePath;
        const char *pTokenUrl;
        const char *pToken;
#ifndef DISABLE_OPENSSL
        const char *pAk;
        const char *pSk;
        const char *pBucket;
#endif
        const char *pUa1;
        const char *pUa2;
        const char *pZone;
        LinkUploadZone zone;
        bool IsFileLoop;
        int  nLoopSleeptime;
        int nRoundCount;
        bool InvalidToken;
        bool IsQuit;
        bool IsDropFirstKeyFrame;
        int64_t nKeyFrameCount;
        int64_t nBaseAudioTime; //for loop test
        int64_t nBaseVideoTime; //for loop test
        
        bool IsJustTestSyncUploadPicture;
        bool IsJustTestAsyncUploadPicture;
        bool IsWithPicUpload;
        bool IsPicUploadSyncMode;
        
        bool IsJustTestSegment;
        const char *pMgrToken;
        LinkTsMuxUploader * pFirstUploader;
        LinkTsMuxUploader * pSecondUploader;
        int nSigQuitTimes;
}CmdArg;

typedef struct {
        LinkUserUploadArg userUploadArg;
        LinkMediaArg avArg;
        LinkTsMuxUploader *pTsMuxUploader;
        int64_t firstTimeStamp ;
        int segStartCount;
        int nByteCount;
        int nVideoKeyframeAccLen;
        const char * pAFile;
        const char * pVFile;
        char * pUrl;
        
}AVuploader;

#define VERSION "v1.0.0"
CmdArg cmdArg;

typedef int (*DataCallback)(void *opaque, void *pData, int nDataLen, int nFlag, int64_t timestamp, int nIsKeyFrame);
#define THIS_IS_AUDIO 1
#define THIS_IS_VIDEO 2


FILE *outTs;
int gTotalLen = 0;
char gtestToken[1024] = {0};

// start aac
static int aacfreq[13] = {96000, 88200,64000,48000,44100,32000,24000, 22050 , 16000 ,12000,11025,8000,7350};
typedef struct ADTS{
        LinkADTSFixheader fix;
        LinkADTSVariableHeader var;
}ADTS;
//end aac

enum HEVCNALUnitType {
        HEVC_NAL_TRAIL_N    = 0,
        HEVC_NAL_TRAIL_R    = 1,
        HEVC_NAL_TSA_N      = 2,
        HEVC_NAL_TSA_R      = 3,
        HEVC_NAL_STSA_N     = 4,
        HEVC_NAL_STSA_R     = 5,
        HEVC_NAL_RADL_N     = 6,
        HEVC_NAL_RADL_R     = 7,
        HEVC_NAL_RASL_N     = 8,
        HEVC_NAL_RASL_R     = 9,
        HEVC_NAL_VCL_N10    = 10,
        HEVC_NAL_VCL_R11    = 11,
        HEVC_NAL_VCL_N12    = 12,
        HEVC_NAL_VCL_R13    = 13,
        HEVC_NAL_VCL_N14    = 14,
        HEVC_NAL_VCL_R15    = 15,
        HEVC_NAL_BLA_W_LP   = 16,
        HEVC_NAL_BLA_W_RADL = 17,
        HEVC_NAL_BLA_N_LP   = 18,
        HEVC_NAL_IDR_W_RADL = 19,
        HEVC_NAL_IDR_N_LP   = 20,
        HEVC_NAL_CRA_NUT    = 21,
        HEVC_NAL_IRAP_VCL22 = 22,
        HEVC_NAL_IRAP_VCL23 = 23,
        HEVC_NAL_RSV_VCL24  = 24,
        HEVC_NAL_RSV_VCL25  = 25,
        HEVC_NAL_RSV_VCL26  = 26,
        HEVC_NAL_RSV_VCL27  = 27,
        HEVC_NAL_RSV_VCL28  = 28,
        HEVC_NAL_RSV_VCL29  = 29,
        HEVC_NAL_RSV_VCL30  = 30,
        HEVC_NAL_RSV_VCL31  = 31,
        HEVC_NAL_VPS        = 32,
        HEVC_NAL_SPS        = 33,
        HEVC_NAL_PPS        = 34,
        HEVC_NAL_AUD        = 35,
        HEVC_NAL_EOS_NUT    = 36,
        HEVC_NAL_EOB_NUT    = 37,
        HEVC_NAL_FD_NUT     = 38,
        HEVC_NAL_SEI_PREFIX = 39,
        HEVC_NAL_SEI_SUFFIX = 40,
};
enum HevcType {
        HEVC_META = 0,
        HEVC_I = 1,
        HEVC_B =2
};

static const uint8_t *ff_avc_find_startcode_internal(const uint8_t *p, const uint8_t *end)
{
        const uint8_t *a = p + 4 - ((intptr_t)p & 3);
        
        for (end -= 3; p < a && p < end; p++) {
                if (p[0] == 0 && p[1] == 0 && p[2] == 1)
                        return p;
        }
        
        for (end -= 3; p < end; p += 4) {
                uint32_t x = *(const uint32_t*)p;
                //      if ((x - 0x01000100) & (~x) & 0x80008000) // little endian
                //      if ((x - 0x00010001) & (~x) & 0x00800080) // big endian
                if ((x - 0x01010101) & (~x) & 0x80808080) { // generic
                        if (p[1] == 0) {
                                if (p[0] == 0 && p[2] == 1)
                                        return p;
                                if (p[2] == 0 && p[3] == 1)
                                        return p+1;
                        }
                        if (p[3] == 0) {
                                if (p[2] == 0 && p[4] == 1)
                                        return p+2;
                                if (p[4] == 0 && p[5] == 1)
                                        return p+3;
                        }
                }
        }
        
        for (end += 3; p < end; p++) {
                if (p[0] == 0 && p[1] == 0 && p[2] == 1)
                        return p;
        }
        
        return end + 3;
}

static const uint8_t *ff_avc_find_startcode(const uint8_t *p, const uint8_t *end){
        const uint8_t *out= ff_avc_find_startcode_internal(p, end);
        if(p<out && out<end && !out[-1]) out--;
        return out;
}

static inline int64_t getCurrentMilliSecond(){
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (tv.tv_sec*1000 + tv.tv_usec/1000);
}

static int getFileAndLength(const char *_pFname, FILE **_pFile, int *_pLen)
{
        FILE * f = fopen(_pFname, "r");
        if ( f == NULL ) {
                return -1;
        }
        *_pFile = f;
        fseek(f, 0, SEEK_END);
        long nLen = ftell(f);
        fseek(f, 0, SEEK_SET);
        *_pLen = (int)nLen;
        return 0;
}

static int readFileToBuf(const char * _pFilename, char ** _pBuf, int *_pLen)
{
        int ret;
        FILE * pFile;
        int nLen = 0;
        ret = getFileAndLength(_pFilename, &pFile, &nLen);
        if (ret != 0) {
                fprintf(stderr, "open file %s fail\n", _pFilename);
                return -1;
        }
        char *pData = malloc(nLen);
        assert(pData != NULL);
        ret = fread(pData, 1, nLen, pFile);
        if (ret <= 0) {
                fprintf(stderr, "open file %s fail\n", _pFilename);
                fclose(pFile);
                free(pData);
                return -2;
        }
        fclose(pFile);
        *_pBuf = pData;
        *_pLen = nLen;
        return 0;
}

static int is_h265_picture(int t)
{
        switch (t) {
                case HEVC_NAL_VPS:
                case HEVC_NAL_SPS:
                case HEVC_NAL_PPS:
                case HEVC_NAL_SEI_PREFIX:
                        return HEVC_META;
                case HEVC_NAL_IDR_W_RADL:
                case HEVC_NAL_CRA_NUT:
                        return HEVC_I;
                case HEVC_NAL_TRAIL_N:
                case HEVC_NAL_TRAIL_R:
                case HEVC_NAL_RASL_N:
                case HEVC_NAL_RASL_R:
                        return HEVC_B;
                default:
                        return -1;
        }
}

static int gnVideoFrameCount=50;
int start_file_test(const char * _pAudioFile, const char * _pVideoFile, DataCallback callback, void *opaque)
{
        assert(!(_pAudioFile == NULL && _pVideoFile == NULL));
        
        int ret;
        
        char * pAudioData = NULL;
        int nAudioDataLen = 0;
        if(_pAudioFile != NULL){
                ret = readFileToBuf(_pAudioFile, &pAudioData, &nAudioDataLen);
                if (ret != 0) {
                        printf("map data to buffer fail:%s", _pAudioFile);
                        return -1;
                }
        }
        
        char * pVideoData = NULL;
        int nVideoDataLen = 0;
        if(_pVideoFile != NULL){
                ret = readFileToBuf(_pVideoFile, &pVideoData, &nVideoDataLen);
                if (ret != 0) {
                        free(pAudioData);
                        printf( "map data to buffer fail:%s", _pVideoFile);
                        return -2;
                }
        }
        
        int bAudioOk = 1;
        int bVideoOk = 1;
        if (_pVideoFile == NULL) {
                bVideoOk = 0;
        }
        if (_pAudioFile == NULL) {
                bAudioOk = 0;
        }
        int64_t nSysTimeBase = getCurrentMilliSecond();
        int64_t nNextAudioTime = nSysTimeBase;
        int64_t nNextVideoTime = nSysTimeBase;
        int64_t nNow = nSysTimeBase;
        int audioOffset = 0;
        
        uint8_t * nextstart = (uint8_t *)pVideoData;
        uint8_t * endptr = nextstart + nVideoDataLen;
        int cbRet = 0;
        int nIDR = 0;
        int nNonIDR = 0;
        int isAAC = 0;
        int IsFirst = 1;
        int64_t aacFrameCount = 0;
        if (bAudioOk) {
                if (cmdArg.IsTestAAC)
                        isAAC = 1;
        }
        while (!cmdArg.IsQuit && (bAudioOk || bVideoOk)) {
                if (bVideoOk && nNow+1 > nNextVideoTime) {
                        
                        uint8_t * start = NULL;
                        uint8_t * end = NULL;
                        uint8_t * sendp = NULL;
                        int type = -1;
                        do{
                                start = (uint8_t *)ff_avc_find_startcode((const uint8_t *)nextstart, (const uint8_t *)endptr);
                                end = (uint8_t *)ff_avc_find_startcode(start+4, endptr);
                                
                                nextstart = end;
                                if(sendp == NULL)
                                        sendp = start;
                                
                                if(start == end || end > endptr){
                                        bVideoOk = 0;
                                        break;
                                }
                                
                                if (!cmdArg.IsTestH265) {
                                        if(start[2] == 0x01){//0x 00 00 01
                                                type = start[3] & 0x1F;
                                        }else{ // 0x 00 00 00 01
                                                type = start[4] & 0x1F;
                                        }
                                        if(type == 1 || type == 5 ){
                                                if (type == 1) {
                                                        nNonIDR++;
                                                } else {
                                                        nIDR++;
                                                        if(cmdArg.IsTestMove && !IsFirst) {;
                                                                printf("sleep %dms to wait timeout start:%"PRId64"\n", cmdArg.nSleeptime, nNextVideoTime);
                                                                if (cmdArg.IsWithPicUpload) {
                                                                        fprintf(stderr, "----->stop push(motion stop)");
                                                                        AVuploader *pAvuploader = (AVuploader*)opaque;
                                                                        LinkNotifyNomoreData(pAvuploader->pTsMuxUploader);
                                                                }
                                                                usleep(cmdArg.nSleeptime * 1000);
                                                                printf("sleep to wait timeout end\n");
                                                                nNextVideoTime += cmdArg.nSleeptime;
                                                                nNextAudioTime += cmdArg.nSleeptime;
                                                                nNow += cmdArg.nSleeptime;
							}
                                                        if(IsFirst && cmdArg.nFirstFrameSleeptime > 0) {
                                                                usleep(cmdArg.nFirstFrameSleeptime * 1000);
                                                                nNextVideoTime += cmdArg.nFirstFrameSleeptime;
                                                                nNextAudioTime += cmdArg.nFirstFrameSleeptime;
                                                                nNow += cmdArg.nFirstFrameSleeptime;
							}
						 	IsFirst = 0;
                                                }
                                                //printf("send one video(%d) frame packet:%ld", type, end - sendp);
                                                if (cmdArg.IsWriteVideoFrame) {
                                                        char fn[15] = {0};
                                                        sprintf(fn, "video%04d.bin", gnVideoFrameCount++);
                                                        FILE *f = fopen(fn, "wb+");
                                                        if (f != NULL) {
                                                                fwrite(sendp, 1, end - sendp, f);
                                                                fclose(f);
                                                        }
                                                }
                                                cbRet = callback(opaque, sendp, end - sendp, THIS_IS_VIDEO, cmdArg.nRolloverTestBase+nNextVideoTime-nSysTimeBase, type == 5);
                                                if (cbRet != 0 && cbRet != LINK_PAUSED) {
                                                        bVideoOk = 0;
                                                }
                                                nNextVideoTime += 40;
                                                break;
                                        }
                                }else{
                                        if(start[2] == 0x01){//0x 00 00 01
                                                type = start[3] & 0x7E;
                                        }else{ // 0x 00 00 00 01
                                                type = start[4] & 0x7E;
                                        }
                                        type = (type >> 1);
                                        int hevctype = is_h265_picture(type);
                                        if (hevctype == -1) {
                                                printf("unknown type:%d\n", type);
                                                continue;
                                        }
                                        if(hevctype == HEVC_I || hevctype == HEVC_B ){
                                                if (hevctype == HEVC_I) {
                                                        nIDR++;
                                                } else {
                                                        nNonIDR++;
                                                }
                                                //printf("send one video(%d) frame packet:%ld", type, end - sendp);
                                                cbRet = callback(opaque, sendp, end - sendp, THIS_IS_VIDEO,cmdArg.nRolloverTestBase+nNextVideoTime-nSysTimeBase, hevctype == HEVC_I);
                                                if (cbRet != 0  && cbRet != LINK_PAUSED) {
                                                        bVideoOk = 0;
                                                }
                                                nNextVideoTime += 40;
                                                break;
                                        }
                                }
                        }while(1);
                }
                if (bAudioOk && nNow+1 > nNextAudioTime) {
                        if (isAAC) {
                                ADTS adts;
                                if(audioOffset+7 <= nAudioDataLen) {
                                        LinkParseAdtsfixedHeader((unsigned char *)(pAudioData + audioOffset), &adts.fix);
                                        int hlen = adts.fix.protection_absent == 1 ? 7 : 9;
                                        LinkParseAdtsVariableHeader((unsigned char *)(pAudioData + audioOffset), &adts.var);
                                        if (audioOffset+hlen+adts.var.aac_frame_length <= nAudioDataLen) {

                                                if (cmdArg.IsTestAACWithoutAdts)
                                                        cbRet = callback(opaque, pAudioData + audioOffset + hlen, adts.var.aac_frame_length - hlen,
                                                                 THIS_IS_AUDIO, nNextAudioTime-nSysTimeBase+cmdArg.nRolloverTestBase, 0);
                                                else
                                                        cbRet = callback(opaque, pAudioData + audioOffset, adts.var.aac_frame_length,
                                                                 THIS_IS_AUDIO, nNextAudioTime-nSysTimeBase+cmdArg.nRolloverTestBase, 0);
                                                if (cbRet != 0 && cbRet != LINK_PAUSED) {
                                                        bAudioOk = 0;
                                                        continue;
                                                }
                                                audioOffset += adts.var.aac_frame_length;
                                                aacFrameCount++;
                                                int64_t d1 = ((1024*1000.0)/aacfreq[adts.fix.sampling_frequency_index]) * aacFrameCount;
                                                int64_t d2 = ((1024*1000.0)/aacfreq[adts.fix.sampling_frequency_index]) * (aacFrameCount - 1);
                                                nNextAudioTime += (d1 - d2); 
                                        } else {
                                                bAudioOk = 0;
                                        }
                                } else {
                                        bAudioOk = 0;
                                }
                        } else {
                                if(audioOffset+160 <= nAudioDataLen) {
                                        cbRet = callback(opaque, pAudioData + audioOffset, 160, THIS_IS_AUDIO, nNextAudioTime-nSysTimeBase+cmdArg.nRolloverTestBase, 0);
                                        if (cbRet != 0  && cbRet != LINK_PAUSED) {
                                                bAudioOk = 0;
                                                continue;
                                        }
                                        audioOffset += 160;
                                        nNextAudioTime += 20;
                                } else {
                                        bAudioOk = 0;
                                }
                        }
                }
                
                
                int64_t nSleepTime = 0;
                if (nNextAudioTime > nNextVideoTime) {
                        if (nNextVideoTime - nNow >  1)
                                nSleepTime = (nNextVideoTime - nNow - 1) * 1000;
                } else {
                        if (nNextAudioTime - nNow > 1)
                                nSleepTime = (nNextAudioTime - nNow - 1) * 1000;
                }
                if (nSleepTime != 0) {
                        //printf("sleeptime:%"PRId64"\n", nSleepTime);
                        if (nSleepTime > 40 * 1000) {
			        LinkLogWarn("abnormal time diff:%"PRId64"", nSleepTime);
			}
                        usleep(nSleepTime);
                }
                nNow = getCurrentMilliSecond();
        }
        
        if (pAudioData) {
                free(pAudioData);
                cmdArg.nBaseAudioTime += nNextAudioTime;
        }
        if (pVideoData) {
                free(pVideoData);
                printf("IDR:%d nonIDR:%d\n", nIDR, nNonIDR);
                cmdArg.nBaseVideoTime += nNextVideoTime;
        }
        if (cmdArg.nBaseVideoTime > 0 && cmdArg.nBaseAudioTime) {
                if (cmdArg.nBaseVideoTime > cmdArg.nBaseAudioTime) {
                        cmdArg.nBaseAudioTime = cmdArg.nBaseVideoTime + 40;
                        cmdArg.nBaseVideoTime += 40;
                } else {
                        cmdArg.nBaseVideoTime = cmdArg.nBaseAudioTime + 40;
                        cmdArg.nBaseAudioTime += 40;
                }
        }
        return 0;
}

#ifdef TEST_WITH_FFMPEG
int start_ffmpeg_test(char * _pUrl, DataCallback callback, void *opaque)
{
        AVFormatContext *pFmtCtx = NULL;
        int ret = avformat_open_input(&pFmtCtx, _pUrl, NULL, NULL);
        if (ret != 0) {
                char msg[128] = {0};
                av_strerror(ret, msg, sizeof(msg)) ;
                printf("ffmpeg err:%s (%s)\n", msg, _pUrl);
                return ret;
        }
        
        AVBSFContext *pBsfCtx = NULL;
        if ((ret = avformat_find_stream_info(pFmtCtx, 0)) < 0) {
                printf("Failed to retrieve input stream information");
                goto end;
        }

        printf("===========Input Information==========\n");
        av_dump_format(pFmtCtx, 0, _pUrl, 0);
        printf("======================================\n");
        
        int nAudioIndex = 0;
        int nVideoIndex = 0;
        for (size_t i = 0; i < pFmtCtx->nb_streams; ++i) {
                if (pFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                        printf("find audio\n");
                        nAudioIndex = i;
                        continue;
                }
                if (pFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                        printf("find video\n");
                        nVideoIndex = i;
                        continue;
                }
                printf("other type:%d\n", pFmtCtx->streams[i]->codecpar->codec_type);
        }

        const AVBitStreamFilter *filter = av_bsf_get_by_name("h264_mp4toannexb");
        if(!filter){
                av_log(NULL,AV_LOG_ERROR,"Unkonw bitstream filter");
                goto end;
        }
        
        ret = av_bsf_alloc(filter, &pBsfCtx);
        if (ret != 0) {
                goto end;
        }
        avcodec_parameters_copy(pBsfCtx->par_in, pFmtCtx->streams[nVideoIndex]->codecpar);
        av_bsf_init(pBsfCtx);
        
        AVPacket pkt;
        av_init_packet(&pkt);
        while((ret = av_read_frame(pFmtCtx, &pkt)) == 0){
                if(nVideoIndex == pkt.stream_index) {
                        ret = av_bsf_send_packet(pBsfCtx, &pkt);
                        if(ret < 0) {
                                fprintf(stderr, "av_bsf_send_packet fail: %d\n", ret);
                                goto end;
                        }
                        ret = av_bsf_receive_packet(pBsfCtx, &pkt);
                        if (AVERROR(EAGAIN) == ret){
                                av_packet_unref(&pkt);
                                continue;
                        }
                        if(ret < 0){
                                fprintf(stderr, "av_bsf_receive_packet: %d\n", ret);
                                goto end;
                        }
                        ret = callback(opaque, pkt.data, pkt.size, THIS_IS_VIDEO, pkt.pts, pkt.flags == 1);
                } else if (nAudioIndex == pkt.stream_index) {
                        ret = callback(opaque, pkt.data, pkt.size, THIS_IS_AUDIO, pkt.pts, 0);
                }

                av_packet_unref(&pkt);
        }
        if (ret != 0 && cbRet != LINK_PAUSED) {
                char msg[128] = {0};
                av_strerror(ret, msg, sizeof(msg)) ;
                printf("ffmpeg end:%s\n", msg);
        }

end:
        if (pBsfCtx)
                av_bsf_free(&pBsfCtx);
        if (pFmtCtx)
                avformat_close_input(&pFmtCtx);
        /* close output */
        if (ret < 0 && ret != AVERROR_EOF) {
                printf("Error occurred.\n");
                return -1;
        }
        
        
        return 0;
}
#endif

static int dataCallback(void *opaque, void *pData, int nDataLen, int nFlag, int64_t timestamp, int nIsKeyFrame)
{
        AVuploader *pAvuploader = (AVuploader*)opaque;
        int ret = 0;
        pAvuploader->nByteCount += nDataLen;
        if (nFlag == THIS_IS_AUDIO){
                //fprintf(stderr, "push audio ts:%"PRId64"\n", timestamp);
                ret = LinkPushAudio(pAvuploader->pTsMuxUploader, pData, nDataLen, timestamp + cmdArg.nBaseAudioTime);
        } else {
                if (pAvuploader->firstTimeStamp == -1){
                        pAvuploader->firstTimeStamp = timestamp;
                }
                int nNewSegMent = 0;
                if (nIsKeyFrame && timestamp - pAvuploader->firstTimeStamp > 30000 && pAvuploader->segStartCount == 0) {
                        nNewSegMent = 1;
                        pAvuploader->segStartCount++;
                }
                if (nIsKeyFrame && pAvuploader->nVideoKeyframeAccLen != 0) {
                        //printf("nVideoKeyframeAccLen:%d\n", nVideoKeyframeAccLen);
                        pAvuploader->nVideoKeyframeAccLen = 0;
                }
                pAvuploader->nVideoKeyframeAccLen += nDataLen;
                //printf("------->push video key:%d ts:%"PRId64" size:%d\n",nIsKeyFrame, timestamp, nDataLen);
                if (nIsKeyFrame) {
                        if (cmdArg.IsDropFirstKeyFrame) {
                                cmdArg.IsDropFirstKeyFrame = 0;
                                return 0;
                        }
                        cmdArg.nKeyFrameCount++;
                }
                
                ret = LinkPushVideo(pAvuploader->pTsMuxUploader, pData, nDataLen, timestamp + cmdArg.nBaseVideoTime, nIsKeyFrame, nNewSegMent);
        }
        return ret;
}

static void * updateToken(void * opaque) {
        int ret = 0;
        while(1) {
                sleep(cmdArg.nUptokenInterval);
                memset(gtestToken, 0, sizeof(gtestToken));
                ret = LinkGetUploadToken(gtestToken, sizeof(gtestToken), NULL, cmdArg.pTokenUrl);
                if (ret != 0) {
                        printf("update token file<<<<<<<<<<<<<\n");
                        return NULL;
                }
                printf("1token:%s\n", gtestToken);
		assert(strlen(gtestToken) < 1024);
                AVuploader *pAvuploader = (AVuploader *)opaque;
                ret = LinkUpdateToken(pAvuploader->pTsMuxUploader, gtestToken, strlen(gtestToken));
                if (ret != 0) {
                        printf("update token file<<<<<<<<<<<<<\n");
                        return NULL;
                }
        }
        return NULL;
}

void signalHander(int s){
        
        if (SIGINT == s) {
                printf("SIGINT catched\n");
                cmdArg.IsQuit = true;
        }
        if (SIGQUIT == s) {
                printf("SIGQUIT catched\n");
                if (cmdArg.nSigQuitTimes % 2 == 0) {
                        LinkLogDebug("%p(first)=======>pause upload", cmdArg.pFirstUploader);
                        LinkPauseUpload(cmdArg.pFirstUploader);
                        if (cmdArg.pSecondUploader) {
                                LinkLogDebug("%p(second)=======>pause upload", cmdArg.pSecondUploader);
                                LinkPauseUpload(cmdArg.pSecondUploader);
                        }
                } else {
                        LinkLogDebug("%p(first)=======>resume upload", cmdArg.pFirstUploader);
                        LinkResumeUpload(cmdArg.pFirstUploader);
                        if (cmdArg.pSecondUploader) {
                                LinkLogDebug("%p(second)=======>resume upload", cmdArg.pSecondUploader);
                                LinkResumeUpload(cmdArg.pSecondUploader);
                        }
                }
                cmdArg.nSigQuitTimes++;
        }
}

void logCb(int _nLevel, char * pLog)
{
        fprintf(stderr, "-%s", pLog);
}

typedef struct {
        void * pData;
}GetPicSaver;

static enum LinkGetPictureSyncMode getPicCallback (void *pOpaque, void *pSvaeWhenAsync, OUT const char **pBuf, OUT int *pBufSize, OUT enum LinkPicUploadType *pType) {
        const char *file = MATERIAL_PATH"3c.jpg";
        
        int n = strlen(file);
        char * pFile = (char *)malloc(n+1);
        *pBufSize = n;
        memcpy(pFile, file, n);
        *pBuf = pFile;
        *pType = LinkPicUploadTypeFile;
        pFile[n] = 0;
        
        GetPicSaver * saver = (GetPicSaver*)pOpaque;
        if (cmdArg.IsPicUploadSyncMode) {
                return LinkGetPictureModeSync;
        }
        
        LinkSendUploadPictureSingal((LinkTsMuxUploader *)saver->pData, pSvaeWhenAsync, pFile, n, LinkPicUploadTypeFile);
        return LinkGetPictureModeAsync;
}

static int getPictureFreeCallback (char *pBuf, int nNameBufSize) {
        fprintf(stderr, "free data\n");
        free(pBuf);
        return 0;
}

static int wrapLinkCreateAndStartAVUploader(LinkTsMuxUploader **_pTsMuxUploader, LinkMediaArg *_pAvArg, LinkUserUploadArg *_pUserUploadArg) {
        int ret = 0;
        LinkPicUploadArg picArg;
        picArg.getPicCallback = getPicCallback;
        picArg.getPictureFreeCallback = getPictureFreeCallback;
        picArg.pGetPicCallbackOpaque = NULL;
        

        
        if (!cmdArg.IsWithPicUpload)
                ret = LinkCreateAndStartAVUploader(_pTsMuxUploader, _pAvArg, _pUserUploadArg);
        else {
                if (cmdArg.pMgrToken == NULL) {
                        LinkLogError("mgrtoken is NULL");
                        exit(8);
                }
                GetPicSaver * saver = malloc(sizeof(GetPicSaver));
                memset(saver, 0, sizeof(GetPicSaver));
                picArg.pGetPicCallbackOpaque = saver;
                ret = LinkCreateAndStartAll(_pTsMuxUploader, _pAvArg, _pUserUploadArg, &picArg);
                saver->pData = *_pTsMuxUploader;
        }
        return ret;
}

static void checkCmdArg(const char * name)
{
        if (cmdArg.IsTestAACWithoutAdts)
                cmdArg.IsTestAAC = true;

        if (cmdArg.IsTestMove) {
                if (cmdArg.nSleeptime == 0) {
                        cmdArg.nSleeptime = 2000;
                }
        }
#ifndef TEST_WITH_FFMPEG
        if (cmdArg.IsInputFromFFmpeg || cmdArg.IsTwoUpload) {
                LinkLogError("not enable TEST_WITH_FFMPEG");
                exit(2);
        }
#endif
#ifdef DISABLE_OPENSSL
        if (cmdArg.IsLocalToken) {
                LinkLogError("cannot from calc token from local. not enable OPENSSL");
                exit(3);
        }
#endif
        if (cmdArg.IsInputFromFFmpeg) {
                cmdArg.IsTestAAC = true;
                cmdArg.IsTestAACWithoutAdts = false;
                LinkLogError("input from ffmpeg");
        }
        if (cmdArg.IsTestTimestampRollover) {
                cmdArg.nRolloverTestBase = 95437000;
	}
        if (cmdArg.nSleeptime != 0 && cmdArg.nSleeptime < 1000) {
                LinkLogError("sleep time is milliseond. should great than 1000");
                exit(4);
	}
        if (cmdArg.IsNoAudio && cmdArg.IsNoVideo) {
                LinkLogError("no audio and video");
                exit(5);
        }
        if (cmdArg.nUptokenInterval == 0) {
                cmdArg.nUptokenInterval = 3550;
        }
        if (cmdArg.IsTwoUpload || cmdArg.IsTwoFileUpload) {
                if (cmdArg.pUa1 == NULL || cmdArg.pUa2 == NULL) {
                        LinkLogError("ua1 or ua2 is NULL");
                        exit(6);
                }
        } else {
                if (cmdArg.pUa1 == NULL) {
                        cmdArg.pUa1 = "ipc99a";
                }
        }
        if (cmdArg.pZone) {
                if (strcmp(cmdArg.pZone, "huabei") == 0) {
                        cmdArg.zone = LINK_ZONE_HUABEI;
                } else if(strcmp(cmdArg.pZone, "huanan") == 0) {
                        cmdArg.zone = LINK_ZONE_HUANAN;
                } else if(strcmp(cmdArg.pZone, "beimei") == 0) {
                        cmdArg.zone = LINK_ZONE_BEIMEI;
                } else if(strcmp(cmdArg.pZone, "dongnanya") == 0) {
                        cmdArg.zone = LINK_ZONE_DONGNANYA;
                }
        }
        
        if (cmdArg.pToken != NULL) {
                if (strlen(cmdArg.pToken) > sizeof(gtestToken) - 1) {
                        LinkLogError("token too long");
                        exit(6);
                }
#ifndef DISABLE_OPENSSL
                if (cmdArg.pAk || cmdArg.pSk || cmdArg.pBucket) {
                        LinkLogError("could not specify ak/s/bucket and token simultaneously");
                        exit(7);
                }
#endif
        } else {
                if (cmdArg.pTokenUrl == NULL) {
                        LinkLogError("must specify tokenurl");
                        exit(8);
                }
        }
        if (cmdArg.IsFileLoop) {
                if (cmdArg.pTokenUrl == NULL) {
                        LinkLogError("must specify tokenurl in loop mode");
                        exit(8);
                }
        }
#ifndef DISABLE_OPENSSL
        if (cmdArg.pAk || cmdArg.pSk || cmdArg.pBucket) {
                if (!(cmdArg.pAk && cmdArg.pSk && cmdArg.pBucket)) {
                        LinkLogError("must specify all ak/sk/bucket option");
                        exit(9);
                }
        }
#endif
        return;
}

#ifdef TEST_WITH_FFMPEG
static void * second_test(void * opaque) {
        AVuploader avuploader;
        memset(&avuploader, 0, sizeof(avuploader));
        
        avuploader.avArg.nAudioFormat = LINK_AUDIO_AAC;
        avuploader.avArg.nSamplerate = 16000;
        avuploader.avArg.nChannels = 1;
        avuploader.avArg.nVideoFormat = LINK_VIDEO_H264;

        avuploader.userUploadArg.pToken_ = gtestToken;
        avuploader.userUploadArg.nTokenLen_ = strlen(gtestToken);
        avuploader.userUploadArg.pDeviceId_ = cmdArg.pUa2;
        avuploader.userUploadArg.nDeviceIdLen_ = strlen(cmdArg.pUa2);
        avuploader.userUploadArg.nUploaderBufferSize = cmdArg.nQbufSize;
        avuploader.userUploadArg.nNewSegmentInterval = cmdArg.nNewSetIntval;
        avuploader.userUploadArg.uploadZone_ = cmdArg.zone;
        
        int ret = wrapLinkCreateAndStartAVUploader(&avuploader.pTsMuxUploader, &avuploader.avArg, &avuploader.userUploadArg);
        if (ret != 0) {
                fprintf(stderr, "CreateAndStartAVUploader err:%d\n", ret);
                return NULL;
        }
        
        start_ffmpeg_test("rtmp://localhost:1935/live/movie", dataCallback, &avuploader);
        sleep(1);
        LinkDestroyAVUploader(&avuploader.pTsMuxUploader);
        return NULL;
}
#endif

static void do_start_file_test(AVuploader *pAvuploader){
        printf("%s\n%s\n", pAvuploader->pAFile, pAvuploader->pVFile);
        do {
                start_file_test(pAvuploader->pAFile, pAvuploader->pVFile, dataCallback, pAvuploader);
                if (cmdArg.nLoopSleeptime > 0) {
                        sleep(cmdArg.nLoopSleeptime);
                }
                if (cmdArg.IsFileLoop) {
                        cmdArg.nRoundCount++;
                        LinkLogInfo(">>>>>>>>>%s:next round<<<<<<<<<<<<\n", pAvuploader->userUploadArg.pDeviceId_);
                }
        } while(cmdArg.IsFileLoop && !cmdArg.IsQuit);
}

static void * second_file_test(void * opaque) {
        AVuploader *pAuploader = (AVuploader *)opaque;;
        AVuploader avuploader = *pAuploader;
        avuploader.userUploadArg.pDeviceId_ = cmdArg.pUa2;
        avuploader.userUploadArg.nDeviceIdLen_ = strlen(cmdArg.pUa2);
        avuploader.userUploadArg.uploadZone_ = cmdArg.zone;
        avuploader.userUploadArg.pMgrTokenRequestUrl = cmdArg.pMgrToken;
        if (cmdArg.pMgrToken != NULL)
                avuploader.userUploadArg.nMgrTokenRequestUrlLen = strlen(cmdArg.pMgrToken);
        avuploader.userUploadArg.nUpdateIntervalSeconds = cmdArg.nNewSegIntval;
        avuploader.userUploadArg.useHttps = 0;

        int ret = wrapLinkCreateAndStartAVUploader(&avuploader.pTsMuxUploader, &avuploader.avArg, &avuploader.userUploadArg);
        if (ret != 0) {
                fprintf(stderr, "CreateAndStartAVUploader err:%d\n", ret);
                return NULL;
        }
        cmdArg.pSecondUploader = avuploader.pTsMuxUploader;
        
        do_start_file_test(&avuploader);
        sleep(1);
        LinkDestroyAVUploader(&avuploader.pTsMuxUploader);
        return NULL;
}

static void uploadStatisticCallback(void *pUserOpaque, LinkUploadKind uploadKind, LinkUploadResult uploadResult) {
        switch(uploadKind) {
                case LINK_UPLOAD_TS:
                        fprintf(stderr, "====>:opaque:%p   ts upload:%s\n", pUserOpaque, uploadResult == LINK_UPLOAD_RESULT_OK ? "success" : "fail");
                        break;
                case LINK_UPLOAD_PIC:
                        fprintf(stderr, "====>:opaque:%p   pic upload:%s\n", pUserOpaque, uploadResult == LINK_UPLOAD_RESULT_OK ? "success" : "fail");
                        break;
                case LINK_UPLOAD_SEG:
                        fprintf(stderr, "====>:opaque:%p   seg upload:%s\n", pUserOpaque, uploadResult == LINK_UPLOAD_RESULT_OK ? "success" : "fail");
                        break;
                case LINK_UPLOAD_MOVE_SEG:
                        fprintf(stderr, "====>:opaque:%p   seg move:%s\n", pUserOpaque, uploadResult == LINK_UPLOAD_RESULT_OK ? "success" : "fail");
                        break;
        }
}

int main(int argc, const char** argv)
{
        flag_bool(&cmdArg.IsInputFromFFmpeg, "ffmpeg", "is input from ffmpeg. will set --testaac and not set noadts");
        flag_bool(&cmdArg.IsTestAAC, "testaac", "input aac audio");
        flag_bool(&cmdArg.IsTestAACWithoutAdts, "noadts", "input aac audio without adts. will set --testaac");
        flag_bool(&cmdArg.IsTestTimestampRollover, "rollover", "will set start pts to 95437000. ts will roll over about 6.x second laetr.only effect for not input from ffmpeg");
        flag_bool(&cmdArg.IsTestH265, "testh265", "input h264 video");
        flag_bool(&cmdArg.IsLocalToken, "localtoken", "use kodo server mode");
        flag_bool(&cmdArg.IsNoAudio, "na", "no audio");
        flag_bool(&cmdArg.IsNoVideo, "nv", "no video(not support now)");
        flag_bool(&cmdArg.IsTestMove, "testmove", "testmove seperated by key frame");
        flag_bool(&cmdArg.IsWriteVideoFrame, "vwrite", "write every video frame");
        flag_bool(&cmdArg.IsJustTestSyncUploadPicture, "jpicsync", "just test upload picture with sync mode");
        flag_bool(&cmdArg.IsJustTestAsyncUploadPicture, "jpicasync", "just test upload picture with async mode");
        flag_bool(&cmdArg.IsWithPicUpload, "withpic", "with picture upload start");
        flag_bool(&cmdArg.IsPicUploadSyncMode, "picsync", "get picture sync mode. default is async");
        flag_bool(&cmdArg.IsJustTestSegment, "jseg", "just test segment manager");
        flag_str(&cmdArg.pMgrToken, "mgrtoken", "where to get move token");
        
        
#ifdef TEST_WITH_FFMPEG
        flag_bool(&cmdArg.IsTwoUpload, "two", "test two instance upload. ffmpeg and file. must set ua1 nad ua2");
#endif
#ifndef DISABLE_OPENSSL
        flag_str(&cmdArg.pAk, "ak", "access key. could not specify bosh ak and token");
        flag_str(&cmdArg.pSk, "sk", "secret key. could not specify bosh sk and token");
        flag_str(&cmdArg.pBucket, "bucket", "bucket name. could not specify bosh bucket and token");
#endif
        flag_bool(&cmdArg.IsTwoFileUpload, "twofile", "test two file instance upload. must set ua1 nad ua2");
        flag_int(&cmdArg.nSleeptime, "sleeptime", "sleep time(milli) used by testmove.default(2s) if testmove is enable");
        flag_int(&cmdArg.nFirstFrameSleeptime, "fsleeptime", "first video key frame sleep time(milli)");
        flag_int(&cmdArg.nQbufSize, "qbufsize", "upload queue buffer size");
        flag_int(&cmdArg.nNewSegIntval, "segint", "new segment interval");
        flag_int(&cmdArg.nUptokenInterval, "uptokenint", "update token interval. default(3550s)");
        flag_str(&cmdArg.pAFilePath, "afpath", "set audio file path.like /root/a.aac");
        flag_str(&cmdArg.pVFilePath, "vfpath", "set video file path.like /root/a.h264");
        flag_str(&cmdArg.pTokenUrl, "tokenurl", "url where to send token request");
        flag_str(&cmdArg.pToken, "token", "upload token");
        flag_str(&cmdArg.pUa1, "ua1", "ua(deviceid) name. default value is ipc99a");
        flag_str(&cmdArg.pUa2, "ua2", "ua(deviceid) name");
        flag_str(&cmdArg.pZone, "zone", "upload zone(huadong huabei huanan beimei dongnanya). default huadong");
        flag_bool(&cmdArg.IsFileLoop, "fileloop", "in file mode and only one upload, will loop to push file");
        flag_int(&cmdArg.nLoopSleeptime, "csleeptime", "next round sleeptime");
        flag_bool(&cmdArg.InvalidToken, "invalidtoken", "use invalid token");
        flag_bool(&cmdArg.IsDropFirstKeyFrame, "drop_first_keyframe", "drop first keyframe");

        flag_parse(argc, argv, VERSION);
        if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "-help") == 0)) {
                flag_write_usage(argv[0]);
                return 0;
        }

        printf("cmdArg.IsInputFromFFmpeg=%d\n", cmdArg.IsInputFromFFmpeg);
        printf("cmdArg.IsTestAAC=%d\n", cmdArg.IsTestAAC);
        printf("cmdArg.IsTestAACWithoutAdts=%d\n", cmdArg.IsTestAACWithoutAdts);
        printf("cmdArg.IsTestTimestampRollover=%d\n", cmdArg.IsTestTimestampRollover);
        printf("cmdArg.IsTestH265=%d\n", cmdArg.IsTestH265);
        printf("cmdArg.IsLocalToken=%d\n", cmdArg.IsLocalToken);
        printf("cmdArg.IsTwoFileUpload=%d\n", cmdArg.IsTwoFileUpload);
        printf("cmdArg.IsTestMove=%d\n", cmdArg.IsTestMove);
        printf("cmdArg.nSleeptime=%d\n", cmdArg.nSleeptime);
        printf("cmdArg.pAFilePath=%s\n", cmdArg.pAFilePath);
        printf("cmdArg.pVFilePath=%s\n", cmdArg.pVFilePath);
        printf("cmdArg.pTokenUrl=%s\n", cmdArg.pTokenUrl);
        printf("cmdArg.IsFileLoop=%d\n", cmdArg.IsFileLoop);
        printf("cmdArg.nLoopSleeptime=%d\n", cmdArg.nLoopSleeptime);
        printf("cmdArg.zone=%s %d\n", cmdArg.pZone, cmdArg.zone);

        checkCmdArg(argv[0]);
        
        LinkSetLogLevel(LINK_LOG_LEVEL_DEBUG);
        //LinkSetLogLevel(LINK_LOG_LEVEL_ERROR);
        if (cmdArg.IsJustTestSyncUploadPicture) {
                justTestSyncUploadPicture(cmdArg.pTokenUrl);
                return 0;
        }else if (cmdArg.IsJustTestAsyncUploadPicture) {
                justTestAsyncUploadPicture(cmdArg.pTokenUrl);
                return 0;
        } else if (cmdArg.IsJustTestSegment) {
                if (cmdArg.pMgrToken == NULL) {
                        printf("IsJustTestSegment need to set mgrtoken\n");
                        return -1;
                }
                JustTestSegmentMgr(cmdArg.pTokenUrl, cmdArg.pMgrToken);
                return 0;
        }

        const char *pVFile = NULL;
        const char *pAFile = NULL;

        if(cmdArg.IsTestAAC) {
                pAFile = MATERIAL_PATH"h265_aac_1_16000_a.aac";
	} else {
                pAFile = MATERIAL_PATH"h265_aac_1_16000_pcmu_8000.mulaw";
        }
        if(cmdArg.IsTestH265) {
                pVFile = MATERIAL_PATH"h265_aac_1_16000_v.h265";
	} else {
                pVFile = MATERIAL_PATH"h265_aac_1_16000_h264.h264";
        }

	if (cmdArg.pAFilePath) {
                pAFile = cmdArg.pAFilePath;
        }
	if (cmdArg.pVFilePath) {
                pVFile = cmdArg.pVFilePath;
        }
        if (cmdArg.IsNoAudio)
                pAFile = NULL;
        if (cmdArg.IsNoVideo)
                pVFile = NULL;

        int ret = 0;
#ifdef TEST_WITH_FFMPEG
      #if LIBAVFORMAT_VERSION_MAJOR < 58
        av_register_all();
	printf("av_register_all\n");
      #endif

        if (cmdArg.IsInputFromFFmpeg) {
                avformat_network_init();
 	        printf("avformat_network_init\n");
	}
#endif
        LinkSetLogCallback(logCb);
        signal(SIGINT, signalHander);
        signal(SIGQUIT, signalHander);
        
#ifndef DISABLE_OPENSSL
        if (cmdArg.IsLocalToken) {
                LinkSetAk(cmdArg.pAk);
                LinkSetSk(cmdArg.pSk);
                //计算token需要，所以需要先设置
                LinkSetBucketName(cmdArg.pBucket);
        }
#endif
        
        AVuploader avuploader;
        LinkUploadZone upzone = LINK_ZONE_UNKNOWN;
        if (!cmdArg.InvalidToken) {
                if (cmdArg.pToken == NULL) {
                        ret = LinkGetUploadToken(gtestToken, sizeof(gtestToken), &upzone, cmdArg.pTokenUrl);
                        if (ret != 0)
                                return ret;
                } else {
                        strcpy(gtestToken, cmdArg.pToken);
                }
                printf("2token:%s\n", gtestToken);

                pthread_t updateTokenThread;
                pthread_attr_t attr;
                pthread_attr_init (&attr);
                pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
                ret = pthread_create(&updateTokenThread, &attr, updateToken, (void*)&avuploader);
                if (ret != 0) {
                        printf("create update token thread fail\n");
                        return ret;
                }
                pthread_attr_destroy (&attr);
	} else {
		strcpy(gtestToken, "invalid_token");
        }
        
        memset(&avuploader, 0, sizeof(avuploader));
        avuploader.avArg.nChannels = 1;
        if (!cmdArg.IsNoAudio) {
                if(cmdArg.IsTestAAC) {
                        avuploader.avArg.nAudioFormat = LINK_AUDIO_AAC;
                        avuploader.avArg.nSamplerate = 16000;
                } else {
                        avuploader.avArg.nAudioFormat = LINK_AUDIO_PCMU;
                        avuploader.avArg.nSamplerate = 8000;
                }
        }
        if (!cmdArg.IsNoVideo) {
                if(cmdArg.IsTestH265) {
                        avuploader.avArg.nVideoFormat = LINK_VIDEO_H265;
                } else {
                        avuploader.avArg.nVideoFormat = LINK_VIDEO_H264;
                }
        }
         
        ret = LinkInitUploader();
        if (ret != 0) {
                fprintf(stderr, "InitUploader err:%d\n", ret);
                return ret;
        }
        
        avuploader.userUploadArg.pToken_ = gtestToken;
        avuploader.userUploadArg.nTokenLen_ = strlen(gtestToken);
        avuploader.userUploadArg.pDeviceId_ = cmdArg.pUa1;
        avuploader.userUploadArg.nDeviceIdLen_ = strlen(cmdArg.pUa1);
        avuploader.userUploadArg.nUploaderBufferSize = cmdArg.nQbufSize;
        avuploader.userUploadArg.nNewSegmentInterval = cmdArg.nNewSegIntval;
        avuploader.userUploadArg.pUploadStatisticCb = uploadStatisticCallback;
        avuploader.userUploadArg.pUploadStatArg = (void *)10;
        if (cmdArg.zone != LINK_ZONE_UNKNOWN) {
                avuploader.userUploadArg.uploadZone_ = cmdArg.zone;
        } else if (upzone != LINK_ZONE_UNKNOWN) {
                LinkLogDebug("set upload zone to:%d", upzone);
                avuploader.userUploadArg.uploadZone_ = upzone;
        }
        avuploader.userUploadArg.pMgrTokenRequestUrl = cmdArg.pMgrToken;
        if (cmdArg.pMgrToken != NULL)
                avuploader.userUploadArg.nMgrTokenRequestUrlLen = strlen(cmdArg.pMgrToken);
        avuploader.userUploadArg.nUpdateIntervalSeconds = cmdArg.nNewSegIntval;
        avuploader.userUploadArg.useHttps = 0;
        
        ret = wrapLinkCreateAndStartAVUploader(&avuploader.pTsMuxUploader, &avuploader.avArg, &avuploader.userUploadArg);
        if (ret != 0) {
                fprintf(stderr, "CreateAndStartAVUploader err:%d\n", ret);
                return ret;
        }
        cmdArg.pFirstUploader = avuploader.pTsMuxUploader;
        
        
        avuploader.pAFile = pAFile;
        avuploader.pVFile = pVFile;

        pthread_t secondUploadThread = 0;
        if (cmdArg.IsTwoFileUpload) {
                AVuploader avu = avuploader;
                ret = pthread_create(&secondUploadThread, NULL, second_file_test, &avu);
                if (ret != 0) {
                        printf("create update token thread fail\n");
                        return ret;
                }
                printf("two file upload\n");
                do_start_file_test(&avuploader);
#ifdef TEST_WITH_FFMPEG
	} else if (cmdArg.IsTwoUpload) {
                ret = pthread_create(&secondUploadThread, NULL, second_test, NULL);
                if (ret != 0) {
                        printf("create update token thread fail\n");
                        return ret;
                }
                
                do_start_file_test(&avuploader);
#endif
        } else {
#ifdef TEST_WITH_FFMPEG
                if (cmdArg.IsInputFromFFmpeg) {
                        start_ffmpeg_test("rtmp://localhost:1935/live/movie", dataCallback, &avuploader);
                        //start_ffmpeg_test("rtmp://live.hkstv.hk.lxdns.com/live/hks", dataCallback, NULL);
                } else {
                        do_start_file_test(&avuploader);
                }
#else
                do_start_file_test(&avuploader);
#endif
        }
        
        sleep(1);
        LinkDestroyAVUploader(&avuploader.pTsMuxUploader);
        if (cmdArg.IsTwoUpload || cmdArg.IsTwoFileUpload) {
                pthread_join(secondUploadThread, NULL);
        }
        LinkUninitUploader();
        LinkLogInfo("should total:%d\n", avuploader.nByteCount);

        return 0;
}
