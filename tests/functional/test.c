#include <stdio.h>
#ifdef TEST_WITH_FFMPEG
#include <libavformat/avformat.h>
#endif
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include "uploader.h"
#include "security.h"
#include "adts.h"
#include "flag.h"

#ifdef __APPLE__
#define MATERIAL_PATH "../../../../tests/functional/material/"
#else
#define MATERIAL_PATH "../../../tests/functional/material/"
#endif

char *gpPictureBuf = NULL;
int gnPictureBufLen = 0;

typedef struct {
#ifdef TEST_WITH_FFMPEG
        bool IsInputFromFFmpeg;
#endif
        bool IsTestAAC;
        bool IsTestAACWithoutAdts;
        bool IsTestTimestampRollover;
        bool IsTestH265;
        bool IsNoAudio;
        bool IsNoVideo;
        bool IsTestMove;
        bool IsTwoFileUpload;
        bool IsWriteVideoFrame;
        bool IsEnableTsCb;
        int nSleeptime;
        int nFirstFrameSleeptime;
        int64_t nRolloverTestBase;
        const char *pAFilePath;
        const char *pVFilePath;
        const char *pConfigUrl;
        const char *pAk;
        const char *pSk;
        bool IsFileLoop;
        int  nLoopSleeptime;
        int nRoundCount;
        bool IsQuit;
        bool IsDropFirstKeyFrame;
        int64_t nKeyFrameCount;
        int64_t nBaseAudioTime; //for loop test
        int64_t nBaseVideoTime; //for loop test
        
        bool IsWithPicUpload;
        bool IsPicUploadSyncMode;
        
        LinkTsMuxUploader * pFirstUploader;
        LinkTsMuxUploader * pSecondUploader;
        int nSigQuitTimes;
}CmdArg;

typedef struct {
        LinkUploadArg userUploadArg;
        LinkTsMuxUploader *pTsMuxUploader;
        int64_t firstTimeStamp ;
        int segStartCount;
        int nByteCount;
        int nVideoKeyframeAccLen;
        const char * pAFile;
        const char * pVFile;
        char * pUrl;
        
}AVuploader;

CmdArg cmdArg;

typedef int (*DataCallback)(void *opaque, void *pData, int nDataLen, int nFlag, int64_t timestamp, int nIsKeyFrame);
#define THIS_IS_AUDIO 1
#define THIS_IS_VIDEO 2


FILE *outTs;
int gTotalLen = 0;

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
                                                                        LinkFlushUploader(pAvuploader->pTsMuxUploader);
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
                ret = LinkPushAudio(pAvuploader->pTsMuxUploader, pData, nDataLen, timestamp + cmdArg.nBaseAudioTime, 0);
        } else {
                if (pAvuploader->firstTimeStamp == -1){
                        pAvuploader->firstTimeStamp = timestamp;
                }
                int nNewSegMent = 0;
                if (nIsKeyFrame && timestamp - pAvuploader->firstTimeStamp > 30000 && pAvuploader->segStartCount == 0) {
                        nNewSegMent = 1;
                        pAvuploader->segStartCount++;
                        
                        LinkSessionMeta metas;
                        metas.len = 3;
                        char *keys[3] = {"key1", "key23", "key345"};
                        metas.keys = (const char **)keys;
                        int keylens[3] = {4, 5, 6};
                        metas.keylens = keylens;
                        
                        char *values[3] = {"value1", "value23", "value345"};
                        metas.values = (const char **)values;
                        int valuelens[3] = {6, 7, 8};
                        metas.valuelens = valuelens;
                        
                        metas.isOneShot = 1;
                        LinkSetTsType(pAvuploader->pTsMuxUploader, &metas);
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
                
                ret = LinkPushVideo(pAvuploader->pTsMuxUploader, pData, nDataLen, timestamp + cmdArg.nBaseVideoTime, nIsKeyFrame, nNewSegMent, 0);
        }
        if (ret == LINK_NOT_INITED) {
                return LINK_SUCCESS;
        }
        return ret;
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

static void getPicCallback (void *pOpaque, const char *pFileName, int nFilenameLen) {
        GetPicSaver * saver = (GetPicSaver*)pOpaque;
        char filename[128] = {0};
        sprintf(filename, "/tmp/abc/%s", pFileName);
        int ret = LinkPushPicture((LinkTsMuxUploader *)saver->pData, filename, strlen(filename), gpPictureBuf, gnPictureBufLen);
        fprintf(stderr, "===========--------->%d\n", ret);
        return ;
}

FILE *pTsFile;
int tsToFile(const char *buffer, int size, void *userCtx, LinkMediaInfo info) {
        if (pTsFile == NULL) {
                pTsFile = fopen("./cb.ts", "wb+");
        }
        if (pTsFile != NULL && buffer && size > 0) {
                fwrite(buffer, 1, size, pTsFile);
                fflush(pTsFile);
        }
        return 0;
}

static int wrapLinkCreateAndStartAVUploader(LinkTsMuxUploader **_pTsMuxUploader, LinkUploadArg *_pUserUploadArg) {
        int ret = 0;
        
        _pUserUploadArg->getPictureCallback = getPicCallback;
        _pUserUploadArg->pGetPictureCallbackUserData = NULL;

        LinkMediaArg avArg;
        avArg.nAudioFormat = _pUserUploadArg->nAudioFormat;
        avArg.nChannels = _pUserUploadArg->nChannels;
        avArg.nSamplerate = _pUserUploadArg->nSampleRate;
        avArg.nVideoFormat = _pUserUploadArg->nVideoFormat;
        if (!cmdArg.IsWithPicUpload)
                //TODO not work now, with no pic and seg
                ret = LinkCreateAndStartAVUploader(_pTsMuxUploader, &avArg, _pUserUploadArg);
        else {
                GetPicSaver * saver = malloc(sizeof(GetPicSaver));
                memset(saver, 0, sizeof(GetPicSaver));
                _pUserUploadArg->pGetPictureCallbackUserData = saver;
                ret = LinkNewUploader(_pTsMuxUploader, _pUserUploadArg);
                if (cmdArg.IsEnableTsCb) {
                        LinkUploaderSetTsOutputCallback(*_pTsMuxUploader,tsToFile, NULL);
                }
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

#ifdef TEST_WITH_FFMPEG
        if (cmdArg.IsInputFromFFmpeg) {
                cmdArg.IsTestAAC = true;
                cmdArg.IsTestAACWithoutAdts = false;
                LinkLogError("input from ffmpeg");
        }
#endif
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
       
        if (cmdArg.pConfigUrl == NULL) {
                LinkLogError("must specify confurl ak and sk");
                exit(8);
        }

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
        avuploader.userUploadArg.nNewSegmentInterval = cmdArg.nNewSetIntval;
        
        int ret = wrapLinkCreateAndStartAVUploader(&avuploader.pTsMuxUploader, &avuploader.avArg, &avuploader.userUploadArg);
        if (ret != 0) {
                fprintf(stderr, "CreateAndStartAVUploader err:%d\n", ret);
                return NULL;
        }
        
        start_ffmpeg_test("rtmp://localhost:1935/live/movie", dataCallback, &avuploader);
        sleep(1);
        LinkFreeUploader(&avuploader.pTsMuxUploader);
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
                        LinkLogInfo(">>>>>>>>>:next round<<<<<<<<<<<<\n");
                }
        } while(cmdArg.IsFileLoop && !cmdArg.IsQuit);
}

static void * second_file_test(void * opaque) {
        AVuploader *pAuploader = (AVuploader *)opaque;;
        AVuploader avuploader = *pAuploader;
       

        int ret = wrapLinkCreateAndStartAVUploader(&avuploader.pTsMuxUploader, &avuploader.userUploadArg);
        if (ret != 0) {
                fprintf(stderr, "CreateAndStartAVUploader err:%d\n", ret);
                return NULL;
        }
        cmdArg.pSecondUploader = avuploader.pTsMuxUploader;
        
        do_start_file_test(&avuploader);
        sleep(1);
        LinkFreeUploader(&avuploader.pTsMuxUploader);
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

extern const char *gVersionAgent;
int main(int argc, const char** argv)
{
#ifdef TEST_WITH_FFMPEG
        flag_bool(&cmdArg.IsInputFromFFmpeg, "ffmpeg", "is input from ffmpeg. will set --testaac and not set noadts");
#endif
        flag_bool(&cmdArg.IsTestAAC, "testaac", "input aac audio");
        flag_bool(&cmdArg.IsTestAACWithoutAdts, "noadts", "input aac audio without adts. will set --testaac");
        flag_bool(&cmdArg.IsTestTimestampRollover, "rollover", "will set start pts to 95437000. ts will roll over about 6.x second laetr.only effect for not input from ffmpeg");
        flag_bool(&cmdArg.IsTestH265, "testh265", "input h264 video");
        flag_bool(&cmdArg.IsNoAudio, "na", "no audio");
        flag_bool(&cmdArg.IsNoVideo, "nv", "no video(not support now)");
        flag_bool(&cmdArg.IsTestMove, "testmove", "testmove seperated by key frame");
        flag_bool(&cmdArg.IsWriteVideoFrame, "vwrite", "write every video frame");
        flag_bool(&cmdArg.IsWithPicUpload, "withpic", "with picture upload start");
        flag_bool(&cmdArg.IsPicUploadSyncMode, "picsync", "get picture sync mode. default is async");
        flag_bool(&cmdArg.IsEnableTsCb, "tscb", "enable ts callback");
        
        flag_int(&cmdArg.nSleeptime, "sleeptime", "sleep time(milli) used by testmove.default(2s) if testmove is enable");
        flag_int(&cmdArg.nFirstFrameSleeptime, "fsleeptime", "first video key frame sleep time(milli)");
        flag_str(&cmdArg.pAFilePath, "afpath", "set audio file path.like /root/a.aac");
        flag_str(&cmdArg.pVFilePath, "vfpath", "set video file path.like /root/a.h264");
        flag_str(&cmdArg.pConfigUrl, "confurl", "url where to get config");
        flag_str(&cmdArg.pAk, "ak", "device access token(not kodo ak)");
        flag_str(&cmdArg.pSk, "sk", "device secret token(not kodo sk)");
        flag_bool(&cmdArg.IsFileLoop, "fileloop", "in file mode and only one upload, will loop to push file");
        flag_int(&cmdArg.nLoopSleeptime, "csleeptime", "next round sleeptime");
        flag_bool(&cmdArg.IsDropFirstKeyFrame, "drop_first_keyframe", "drop first keyframe");

        flag_parse(argc, argv, gVersionAgent);
        if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "-help") == 0)) {
                flag_write_usage(argv[0]);
                return 0;
        }


        printf("cmdArg.IsTestAAC=%d\n", cmdArg.IsTestAAC);
        printf("cmdArg.IsTestAACWithoutAdts=%d\n", cmdArg.IsTestAACWithoutAdts);
        printf("cmdArg.IsTestTimestampRollover=%d\n", cmdArg.IsTestTimestampRollover);
        printf("cmdArg.IsTestH265=%d\n", cmdArg.IsTestH265);
        printf("cmdArg.pAFilePath=%s\n", cmdArg.pAFilePath);
        printf("cmdArg.pVFilePath=%s\n", cmdArg.pVFilePath);
        printf("cmdArg.pConfigUrl=%s\n", cmdArg.pConfigUrl);
        printf("cmdArg.IsFileLoop=%d\n", cmdArg.IsFileLoop);

        checkCmdArg(argv[0]);
        
        const char *file = MATERIAL_PATH"3c.jpg";
        if (gpPictureBuf == NULL) {
                int ret = readFileToBuf(file, &gpPictureBuf, &gnPictureBufLen);
                if (ret != 0) {
                        printf("map data to buffer fail:%s", file);
                        return -22;
                }
        }
        
        LinkSetLogLevel(LINK_LOG_LEVEL_DEBUG);
        //LinkSetLogLevel(LINK_LOG_LEVEL_ERROR);

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
        
        
        AVuploader avuploader;
        
        memset(&avuploader, 0, sizeof(avuploader));
        avuploader.userUploadArg.nChannels = 1;
        if (!cmdArg.IsNoAudio) {
                if(cmdArg.IsTestAAC) {
                        avuploader.userUploadArg.nAudioFormat = LINK_AUDIO_AAC;
                        avuploader.userUploadArg.nSampleRate = 16000;
                } else {
                        avuploader.userUploadArg.nAudioFormat = LINK_AUDIO_PCMU;
                        avuploader.userUploadArg.nSampleRate = 8000;
                }
        }
        if (!cmdArg.IsNoVideo) {
                if(cmdArg.IsTestH265) {
                        avuploader.userUploadArg.nVideoFormat = LINK_VIDEO_H265;
                } else {
                        avuploader.userUploadArg.nVideoFormat = LINK_VIDEO_H264;
                }
        }
         
        ret = LinkInit();
        if (ret != 0) {
                fprintf(stderr, "InitUploader err:%d\n", ret);
                return ret;
        }
        
        //avuploader.userUploadArg.pUploadStatisticCb = uploadStatisticCallback; //TODO
        //avuploader.userUploadArg.pUploadStatArg = (void *)10;
        avuploader.userUploadArg.pConfigRequestUrl = cmdArg.pConfigUrl;
        avuploader.userUploadArg.nConfigRequestUrlLen = strlen(cmdArg.pConfigUrl);
        avuploader.userUploadArg.pDeviceAk = cmdArg.pAk;
        avuploader.userUploadArg.nDeviceAkLen = strlen(cmdArg.pAk);
        avuploader.userUploadArg.pDeviceSk = cmdArg.pSk;
        avuploader.userUploadArg.nDeviceSkLen = strlen(cmdArg.pSk);
        
        ret = wrapLinkCreateAndStartAVUploader(&avuploader.pTsMuxUploader, &avuploader.userUploadArg);
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
                printf("two file upload: threadid:%x\n", secondUploadThread);
                do_start_file_test(&avuploader);

        } else {
                do_start_file_test(&avuploader);
        }
        
        sleep(1);
        LinkFreeUploader(&avuploader.pTsMuxUploader);
        if (cmdArg.IsTwoFileUpload) {
                pthread_join(secondUploadThread, NULL);
        }
        LinkCleanup();
        LinkLogInfo("should total:%d\n", avuploader.nByteCount);

        return 0;
}
