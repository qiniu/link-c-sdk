// Last Update:2019-02-18 10:30:30
/**
 * @file sim_dev.c
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-10-19
 */

#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "uploader.h"
#include "adts.h"
#include <stdint.h>
#include "dbg.h"
#include "dev_core.h"
#include "mymalloc.h"

#define VERSION "v1.0.0"

typedef int (*DataCallback)(void *opaque, void *pData, int nDataLen, int nFlag, int64_t timestamp, int nIsKeyFrame);
#define THIS_IS_AUDIO 1
#define THIS_IS_VIDEO 2

// start aac
static int aacfreq[13] = {96000, 88200,64000,48000,44100,32000,24000, 22050 , 16000 ,12000,11025,8000,7350};
typedef struct ADTS{
        LinkADTSFixheader fix;
        LinkADTSVariableHeader var;
}ADTS;
//end aac

typedef struct 
{
    int code;
    int flag;
    int level;
    char data[128];
} alarm_t;

typedef enum
{
    AUDIO_TYPE_G711,
    AUDIO_TYPE_AAC
} audio_type_t;

typedef int (*alarm_callback_t)(alarm_t alarm, void *pcontext);
typedef int (*video_callback_t)(int streamno, char *frame, int len, int iskey, double timestatmp, unsigned long frame_index,
                              unsigned long keyframe_index, void *pcontext);
typedef int (*audio_callback_t)(char *frame, int len, double timestatmp, unsigned long frame_index, void *pcontext);

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

enum {
    SIM_DEV_MOTION_DETECT,
    SIM_DEV_MOTION_DETECT_DISAPPEAR,
    SIM_DEV_JPEG_CAPTURED,
} motion_detec_s;

static inline int64_t getCurrentMilliSecond(){
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (tv.tv_sec*1000 + tv.tv_usec/1000);
}

static int getFileAndLength(char *_pFname, FILE **_pFile, int *_pLen)
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

static int readFileToBuf(char * _pFilename, char ** _pBuf, int *_pLen)
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

typedef struct {
        video_callback_t videoCb;
        audio_callback_t audioCb;
        pthread_t tid;
        int nStreamNo;
        char file[256];
        unsigned char isAac;
        unsigned char isStop;
        unsigned char isH265;
        unsigned char IsTestAACWithoutAdts;
        unsigned char isAudio;
        int64_t nRolloverTestBase;
        
}Stream;

typedef struct  {
        int nId;
        Stream audioStreams[2];
        Stream videoStreams[2];
}Camera;

Camera cameras[10];

static int dataCallback(void *opaque, void *pData, int nDataLen, int nFlag, int64_t timestamp, int nIsKeyFrame)
{
        Stream *pStream = (Stream*)opaque;
        int ret = 0;
        if (nFlag == THIS_IS_AUDIO){
                //fprintf(stderr, "push audio ts:%lld\n", timestamp);
                //(char *frame, int len, double timestatmp, unsigned long frame_index, void *pcontext);
                ret = pStream->audioCb( pData, nDataLen, (double)timestamp, 0, 0);
        } else {
                //printf("------->push video key:%d ts:%lld size:%d\n",nIsKeyFrame, timestamp, nDataLen);
                //(int streamno, char *frame, int len, int iskey, double timestatmp, unsigned long frame_index, unsigned long keyframe_index, void *pcontext);
                ret = pStream->videoCb(pStream->nStreamNo, pData, nDataLen, nIsKeyFrame, (double)timestamp, 0, 0, 0);
        }
        return ret;
}

char h264Aud3[3]={0, 0, 1};
int start_video_file_test(void *opaque)
{
        sleep(3);
        printf("----------_>start_video_file_test\n");
        Stream *pStream = (Stream*)opaque;
        int ret;
        
        char * pVideoData = NULL;
        int nVideoDataLen = 0;
        ret = readFileToBuf(pStream->file, &pVideoData, &nVideoDataLen);
        if (ret != 0) {
                free(pVideoData);
                printf( "map data to buffer fail:%s, please modify ipc.conf and specify the h264 and aac file\n", pStream->file);
                return -2;
        }

        int64_t nSysTimeBase = getCurrentMilliSecond();
        int64_t nNextVideoTime = nSysTimeBase;
        int64_t nNow = nSysTimeBase;
        
        int bVideoOk = 1;
        int videoOffset = 0;
        int cbRet = 0;
        int nIDR = 0;
        int nNonIDR = 0;
        
         while (!pStream->isStop && bVideoOk) {
                 int nLen;
                 int shouldReset = 1;
                 int type = -1;
                 
                 if (videoOffset+4 < nVideoDataLen) {
                         memcpy(&nLen, pVideoData+videoOffset, 4);
                         if (videoOffset + 4 + nLen < nVideoDataLen) {
                                 shouldReset = 0;
                                 if (!pStream->isH265) {
                                         if (memcmp(h264Aud3, pVideoData + videoOffset + 4, 3) == 0) {
                                                 type = pVideoData[videoOffset + 7] & 0x1F;
                                         } else {
                                                 type = pVideoData[videoOffset + 8] & 0x1F;
                                         }
                                         if (type == 1) {
                                                 nNonIDR++;
                                         } else {
                                                 nIDR++;
                                         }
                                         cbRet = dataCallback(opaque, pVideoData + videoOffset + 4, nLen, THIS_IS_VIDEO, pStream->nRolloverTestBase+nNextVideoTime-nSysTimeBase, !(type == 1));
                                         if (cbRet != 0) {
                                                 bVideoOk = 0;
                                         }
                                         videoOffset = videoOffset + 4 + nLen;
                                 }else {
                                         if (memcmp(h264Aud3, pVideoData + videoOffset + 4, 3) == 0) {
                                                 type = pVideoData[videoOffset + 7] & 0x7F;
                                         } else {
                                                 type = pVideoData[videoOffset + 8] & 0x7F;
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
                                                 cbRet = dataCallback(opaque, pVideoData + videoOffset + 4, nLen, THIS_IS_VIDEO,pStream->nRolloverTestBase+nNextVideoTime-nSysTimeBase, hevctype == HEVC_I);
                                                 if (cbRet != 0) {
                                                         bVideoOk = 0;
                                                 }
                                         }
                                         videoOffset = videoOffset + 4 + nLen;
                                 }
                         }
                 }
                 if (shouldReset){
                         videoOffset = 0;
                         continue;
                 }
                 nNextVideoTime += 40;
                 
                 int64_t nSleepTime = 0;
                 nSleepTime = (nNextVideoTime - nNow - 1) * 1000;
                 if (nSleepTime > 0) {
                         //printf("sleeptime:%lld\n", nSleepTime);
                         if (nSleepTime > 40 * 1000) {
                                 LinkLogWarn("abnormal time diff:%lld", nSleepTime);
                         }
                         usleep(nSleepTime);
                 }
                 nNow = getCurrentMilliSecond();
        }
        
        if (pVideoData) {
                free(pVideoData);
                printf("IDR:%d nonIDR:%d\n", nIDR, nNonIDR);
        }
        return 0;
}


int start_audio_file_test(void *opaque)
{
        sleep(3);
        printf("----------_>start_audio_file_test\n");
        Stream *pStream = (Stream*)opaque;
        int ret;
        
        char * pAudioData = NULL;
        int nAudioDataLen = 0;
        ret = readFileToBuf(pStream->file, &pAudioData, &nAudioDataLen);
        if (ret != 0) {
                printf("map data to buffer fail:%s", pStream->file);
                return -1;
        }
        
        int bAudioOk = 1;
        int64_t nSysTimeBase = getCurrentMilliSecond();
        int64_t nNextAudioTime = nSysTimeBase;
        int64_t nNow = nSysTimeBase;
        
        int audioOffset = 0;
        int isAAC = 1;
        int64_t aacFrameCount = 0;
        if (!pStream->isAac)
                isAAC = 0;
        int cbRet = 0;
        
        int duration = 0;
        
        while (!pStream->isStop && bAudioOk) {
                if (isAAC) {
                        ADTS adts;
                        if(audioOffset+7 <= nAudioDataLen) {
                                LinkParseAdtsfixedHeader((unsigned char *)(pAudioData + audioOffset), &adts.fix);
                                int hlen = adts.fix.protection_absent == 1 ? 7 : 9;
                                LinkParseAdtsVariableHeader((unsigned char *)(pAudioData + audioOffset), &adts.var);
                                if (audioOffset+hlen+adts.var.aac_frame_length <= nAudioDataLen) {
                                        
                                        if (pStream->IsTestAACWithoutAdts)
                                                cbRet = dataCallback(opaque, pAudioData + audioOffset + hlen, adts.var.aac_frame_length - hlen,
                                                                     THIS_IS_AUDIO, nNextAudioTime-nSysTimeBase+pStream->nRolloverTestBase, 0);
                                        else
                                                cbRet = dataCallback(opaque, pAudioData + audioOffset, adts.var.aac_frame_length,
                                                                     THIS_IS_AUDIO, nNextAudioTime-nSysTimeBase+pStream->nRolloverTestBase, 0);
                                        if (cbRet != 0) {
                                                bAudioOk = 0;
                                                continue;
                                        }
                                        audioOffset += adts.var.aac_frame_length;
                                        aacFrameCount++;
                                        int64_t d = ((1024*1000.0)/aacfreq[adts.fix.sampling_frequency_index]) * aacFrameCount;
                                        nNextAudioTime = nSysTimeBase + d;
                                } else {
                                        audioOffset = 0;
                                        continue;
                                }
                                if (duration == 0) {
                                        duration = ((1024*1000.0)/aacfreq[adts.fix.sampling_frequency_index]);
                                }
                        } else {
                                audioOffset = 0;
                                continue;
                        }
                } else {
                        duration = 20;
                        if(audioOffset+160 <= nAudioDataLen) {
                                cbRet = dataCallback(opaque, pAudioData + audioOffset, 160, THIS_IS_AUDIO, nNextAudioTime-nSysTimeBase+pStream->nRolloverTestBase, 0);
                                if (cbRet != 0) {
                                        bAudioOk = 0;
                                        continue;
                                }
                                audioOffset += 160;
                                nNextAudioTime += 20;
                        } else {
                                audioOffset = 0;
                                continue;
                        }
                }
               
                int64_t nSleepTime = (nNextAudioTime - nNow - 1) * 1000;
                if (nSleepTime > 0) {
                        //printf("sleeptime:%lld\n", nSleepTime);
                        if (nSleepTime > duration * 1000) {
                                LinkLogWarn("abnormal time diff:%lld", nSleepTime);
                        }
                        usleep(nSleepTime);
                }
                nNow = getCurrentMilliSecond();
        }
        
        if (pAudioData) {
                free(pAudioData);
                printf("quie audio test");
        }
        return 0;
}


void simdev_set_audio_filepath(int camera, int stream, char *pFile, int nFileLen)
{
        int nLen = nFileLen;
        if (nLen > 255) {
                nLen = 255;
        }
        cameras[camera].audioStreams[stream].file[nLen] = 0;
        memcpy(cameras[camera].audioStreams[stream].file, pFile, nLen);
}

void simdev_set_video_filepath(int camera, int stream, char *pFile, int nFileLen)
{
        int nLen = nFileLen;
        if (nLen > 255) {
                nLen = 255;
        }
        cameras[camera].videoStreams[stream].file[nLen] = 0;
        memcpy(cameras[camera].videoStreams[stream].file, pFile, nLen);
}

void simdev_set_audio_format(int camera, int stream, int isAac)
{
        cameras[camera].audioStreams[stream].isAac = isAac;
}

void simdev_set_video_format(int camera, int stream, int isH265)
{
        cameras[camera].audioStreams[stream].isH265 = isH265;
}

int simdev_init( )
{
    extern char *GetH264File();
    extern char *GetAacFile();

        memset(&cameras, 0, sizeof(cameras));
        int i = 0;
        char *h264,*aac;

        h264 = GetH264File();
        aac = GetAacFile();
        printf("h264 = %s\n", h264);
        printf("aac = %s\n", aac);
        for (i = 0; i < sizeof(cameras) / sizeof(Camera); i++) {
                cameras[i].nId = i;
                cameras[i].audioStreams[0].nStreamNo = 0;
                cameras[i].audioStreams[1].nStreamNo = 1;
                cameras[i].videoStreams[0].nStreamNo = 0;
                cameras[i].videoStreams[1].nStreamNo = 1;
                cameras[i].audioStreams[0].isAac = 1;
                cameras[i].audioStreams[1].isAac = 1;
                //TODO default file
                if ( h264 ) {
                strcpy(cameras[i].videoStreams[0].file, h264 );
                strcpy(cameras[i].videoStreams[1].file, h264 );
                }
                if ( aac) {
                    strcpy(cameras[i].audioStreams[1].file, aac );
                    strcpy(cameras[i].audioStreams[0].file, aac );
                }
        }
        
        return 0;
}

int simdev_stop_video(int camera, int stream)
{
        cameras[camera].videoStreams[stream].isStop = 1;
        pthread_t tid;
        memset(&tid, 0, sizeof(tid));
        if(memcmp(&tid, &cameras[camera].audioStreams[stream].tid, sizeof(tid)) != 0) {
                pthread_join(cameras[camera].videoStreams[stream].tid, NULL);
        }
        cameras[camera].videoStreams[stream].videoCb = NULL;
        memset(&cameras[camera].videoStreams[stream].tid, 0, sizeof(pthread_t));
        return 0;
}

int simdev_stop_audio(int camera, int stream)
{
        cameras[camera].audioStreams[stream].isStop = 1;
        pthread_t tid;
        memset(&tid, 0, sizeof(tid));
        if(memcmp(&tid, &cameras[camera].audioStreams[stream].tid, sizeof(tid)) != 0) {
                pthread_join(cameras[camera].audioStreams[stream].tid, NULL);
        }
        cameras[camera].audioStreams[stream].audioCb = NULL;
        memset(&cameras[camera].audioStreams[stream].tid, 0, sizeof(pthread_t));
        return 0;
}

int simdev_stop_audio_play(void)
{
        return 0;
}

void *VideoCaptureTask( void *param )
{
        start_video_file_test(param);
        return NULL;
}


int simdev_start_video(int camera, int stream, video_callback_t vcb, void *pcontext)
{
        (void)pcontext;
        Stream *pStream = &cameras[camera].videoStreams[stream];
        if (pStream->videoCb != NULL) {
                return 0;
        }
        pStream->videoCb = vcb;
        
        cameras[camera].videoStreams[stream].isStop = 0;
        pthread_create( &pStream->tid, NULL, VideoCaptureTask, (void *)pStream );
        return 0;
}

void *AudioCaptureTask( void *param )
{
        start_audio_file_test(param);
        return NULL;
}

int simdev_start_audio(int camera, int stream, audio_callback_t acb, void *pcontext)
{
        Stream *pStream = &cameras[camera].audioStreams[stream];
        if (pStream->audioCb != NULL) {
                return 0;
        }
        pStream->audioCb = acb;
        
        cameras[camera].audioStreams[stream].isStop = 0;
        pthread_create( &pStream->tid, NULL, AudioCaptureTask, (void *)pStream );
        
        return 0;
}

void *AlarmTask( void *arg )
{
    int interval = 0;
    alarm_t alam;
    alarm_callback_t alarmcb = (alarm_callback_t)arg;

    (void)arg;

    for (;;) {
        srand( time(0) );
        interval = rand()%15;
        sleep( interval );
        alam.code = SIM_DEV_MOTION_DETECT;
        alarmcb( alam, NULL );
        srand( time(0) );
        interval = rand()%15;
        sleep( interval );
        alam.code = SIM_DEV_MOTION_DETECT_DISAPPEAR;
        alarmcb( alam, NULL );
    }
    return NULL;
}

int simdev_register_callback(alarm_callback_t alarmcb , void *pcontext)
{
        pthread_t thread = 0;
        
        pthread_create( &thread, NULL, AlarmTask, (void *)alarmcb );
        return 0;
}

CaptureDevice gSimDevCaptureDev;

static int SimDevVideoGetFrameCb( int streamno, char *_pFrame,
                   int _nLen, int _nIskey, double _dTimeStamp,
                   unsigned long _nFrameIndex, unsigned long _nKeyFrameIndex,
                   void *_pContext)
{
    int stream = 0;

    if ( &gSimDevCaptureDev.subContext == _pContext ) {
        stream = STREAM_SUB;
    } else {
        stream = STREAM_MAIN;
    }
    gSimDevCaptureDev.videoCb( _pFrame, _nLen, _nIskey, _dTimeStamp, _nFrameIndex, _nKeyFrameIndex, stream );

    return 0;
}

static int SimDevAudioGetFrameCb( char *_pFrame, int _nLen, double _dTimeStamp,
                     unsigned long _nFrameIndex, void *_pContext )
{
    int stream = 0;
    static double localTimeStamp = 0, timeStamp = 0; 
    static int first = 1;

    if ( first ) {
        localTimeStamp = _dTimeStamp;
        first = 0;
    } else {
        localTimeStamp += 40;
    }

    if ( gSimDevCaptureDev.audioType == AUDIO_AAC ) {
        timeStamp = _dTimeStamp;
    } else {
        timeStamp = localTimeStamp;
    }

    if ( &gSimDevCaptureDev.subContext == _pContext ) {
        stream = STREAM_SUB;
    } else {
        stream = STREAM_MAIN;
    }
    gSimDevCaptureDev.audioCb( _pFrame, _nLen, timeStamp, _nFrameIndex, stream );

    return 0;
}

static int SimDevInitIPC( int audioType, int subStreamEnable, VideoFrameCb videoCb, AudioFrameCb audioCb )
{
    int s32Ret = 0;

    s32Ret = simdev_init();
    if ( s32Ret < 0 ) {
        DBG_ERROR("simdev_init error, s32Ret = %d\n", s32Ret );
        return -1;
    }
    gSimDevCaptureDev.audioCb = audioCb;
    gSimDevCaptureDev.videoCb = videoCb;
    gSimDevCaptureDev.audioEnable = 1;
    gSimDevCaptureDev.audioType = audioType;
    gSimDevCaptureDev.subStreamEnable = subStreamEnable;

    return 0;
}

static int SimDevGetDevId( char *devId )
{
    strncpy( devId, "ipc99", 5 );

    return 0;
}

int SimDevStartStream()
{
    simdev_start_video( 0, 0, SimDevVideoGetFrameCb, &gSimDevCaptureDev.mainContext );
    DBG_LOG("gSimDevCaptureDev.mainContext = %d\n", gSimDevCaptureDev.mainContext );
    if ( gSimDevCaptureDev.audioEnable ) {
        simdev_start_audio( 0, 0, SimDevAudioGetFrameCb, &gSimDevCaptureDev.mainContext );
    }
    if ( gSimDevCaptureDev.subStreamEnable ) {
        simdev_start_video( 0, 1, SimDevVideoGetFrameCb, &gSimDevCaptureDev.subContext );
        if ( gSimDevCaptureDev.audioEnable ) {
            simdev_start_audio( 0, 1, SimDevAudioGetFrameCb, &gSimDevCaptureDev.subContext );
        }
    }
    return 0;
}

static int SimDevDeInitIPC()
{
    simdev_stop_video(0, 1);
    simdev_stop_audio(0, 1);

    return 0;
}

static int SimDevIsAudioEnable()
{
    return ( gSimDevCaptureDev.audioEnable );
}

int SimDevAlarmCallback(alarm_t _alarm, void *pcontext)
{
    int alarm = 0;

    if ( _alarm.code == SIM_DEV_MOTION_DETECT ) {
        alarm = ALARM_MOTION_DETECT;
    } else if ( _alarm.code == SIM_DEV_MOTION_DETECT_DISAPPEAR ) {
        alarm = ALARM_MOTION_DETECT_DISAPPEAR;
    } else if ( _alarm.code == SIM_DEV_JPEG_CAPTURED ) {
        printf("_alarm.flag = %d\n", _alarm.flag );
        printf("_alarm.level = %d\n", _alarm.level );
        printf("_alarm.data = %s\n", _alarm.data );
        alarm = ALARM_JPEG_CAPTURED;
    } else {
        /* do nothing */
    }
    if ( gSimDevCaptureDev.alarmCallback )
        gSimDevCaptureDev.alarmCallback( alarm, _alarm.data );
    return 0;
}
static int SimDevRegisterAlarmCb( int (*alarmCallback)(int alarm, void *data ) )
{
    gSimDevCaptureDev.alarmCallback = alarmCallback;
    simdev_register_callback( SimDevAlarmCallback, NULL );

    return 0;
}

void *CaptureJpegNotifyTask( void *arg )
{
    int alarm;
    char *file = (char*)arg;

    usleep( 500000 );// 500ms
    alarm = ALARM_JPEG_CAPTURED;
    if ( gSimDevCaptureDev.alarmCallback )
        gSimDevCaptureDev.alarmCallback( alarm, file );
    return NULL;
}

static int SimDevCaptureJpeg( int stream, int quality, char *path, char *filename)
{
    pthread_t thread = 0;
    char file[256] = { 0 };
    char cmdbuf[256] = { 0 };

    sprintf( file, "%s/%s", path, filename );
    sprintf( cmdbuf, "echo aaaaaaaaaaaaa > %s", file );
    pthread_create( &thread, NULL, CaptureJpegNotifyTask, file );
    return 0;
}

CaptureDevice gSimDevCaptureDev =
{
    0,
    0,
    0,
    0,
    0,
    NULL,
    NULL,
    SimDevInitIPC,
    SimDevDeInitIPC,
    SimDevGetDevId,
    SimDevStartStream,
    SimDevIsAudioEnable,
    SimDevRegisterAlarmCb,
    NULL,
    SimDevCaptureJpeg
};

static void __attribute__((constructor)) SimDevRegisterToCore()
{
    CaptureDeviceRegister( &gSimDevCaptureDev );
}

