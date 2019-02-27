#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "stdio.h"
#include "stdlib.h"
#include "uploader.h"
#include "adts.h"
#include "sample_ver.h"


#define THIS_IS_AUDIO 1
#define THIS_IS_VIDEO 2

//模拟用的数据文件
#define JPG_FILE "3c.jpg"
#define VIDEO_H264_FILE "h265_aac_1_16000_h264.h264"
#define AUDIO_AAC_FILE "h265_aac_1_16000_a.aac"



//sample模拟视音频流用到的类型
typedef struct {
    LinkADTSFixheader fix_head;
    LinkADTSVariableHeader var_head;
}ps_adts;

typedef enum {
    NALU_TYPE_SLICE = 1,
    NALU_TYPE_DPA = 2,
    NALU_TYPE_DPB = 3,
    NALU_TYPE_DPC = 4,
    NALU_TYPE_IDR = 5,
    NALU_TYPE_SEI = 6,
    NALU_TYPE_SPS = 7,
    NALU_TYPE_PPS = 8,
    NALU_TYPE_AUD = 9,
    NALU_TYPE_EOSEQ = 10,
    NALU_TYPE_EOSTREAM = 11,
    NALU_TYPE_FILL = 12
}ps_nalu_type;

//link-c-sdk使用相关
//使用link-c-sdk时的设备标识
#define DAK ""
#define DSK ""
typedef int (*DataCallback)(void *opaque, void *pData,
                            int nDataLen, int nFlag, int64_t timestamp, int nIsKeyFrame);

typedef struct {
    void *pdata;
}pic_save;

typedef struct {
    LinkUploadArg userUploadArg;
    LinkTsMuxUploader *pTsMuxUploader;
    int64_t firstTimeStamp ;
    int segStartCount;
    int nByteCount;
    int nVideoKeyframeAccLen;
    char * pUrl;

}AVuploader;

static int data_callback(void *opaque, void *pData,
                         int nDataLen, int nFlag, int64_t timestamp, int nIsKeyFrame);


//!
//! 通用部分
//!

/**
 * @brief 获取当前时间，单位ms
 * @return
 */
static inline long int get_current_ms()
{
    struct timeval tv = {};
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

/**
 * 将[p - p+len]地址的数据以hex形式进行显示
 * @param p
 * @param len
 */
static void show_hex(const unsigned char *p, int len)
{
    unsigned int i = 0;

    printf("addr[%p - %p]\n", p, p + len);
    for (i = 0; i < len; i++) {

        if (i % 0x10 == 0)
            printf("%p:", p+i);

        printf ("%4x", p[i]);
        if ((i+1) % 0x10 == 0)
            printf("\n");
    }
    printf("\n");
}

/**
 * 读取文件内容到内存
 * @param file 要读取的文件
 * @param buf 读取存放的内存地址
 * @param len 数据的长度，单位字节
 * @return
 */
int read_file(const char *file, char **buf, int *len)
{
    int fd = open(file, O_RDONLY);
    if (fd < 0)
        return -1;

    struct stat info;
    if (fstat(fd, &info) != 0) {
        return -1;
    }

    int bytes = 0;
    *len = info.st_size;
    char *p = malloc(info.st_size);
    *buf = p;
    bytes = read(fd, p, info.st_size);
    if (bytes <= 0) {
        free(p);
        *len = 0;
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

//!
//! 模拟视音频部分
//!
/** aac音频采样率 */
static int aacfreq[13] = {96000, 88200,64000,48000,44100,32000,24000, 22050 , 16000 ,12000,11025,8000,7350};
uint64_t g_video_base_time = 0;
uint64_t g_audio_base_time = 0;
uint64_t g_rollover_base = 0;

char *pic_buf = NULL;
int pic_len = 0;

/**
 * 获取nalu类型检查
 * @param value
 * @return 成功：nalu 类型  失败：-1
 */
static int get_nalu_type(unsigned char value)
{
    char type = value & 0x1F;
    if (type >= NALU_TYPE_SLICE &&  type <= NALU_TYPE_FILL)
        return type;
    else
        return -1;
}

/**
 * 查找NALU起始码，
 * @param p 起始地址
 * @param end
 *@return
 */
static const uint8_t *ff_avc_find_startcode_internal(const uint8_t *p, const uint8_t *end)
{
    const uint8_t *a = p + 4 - ((intptr_t)p & 3);

    for (end -= 3; p < a && p < end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    for (end -= 3; p < end; p += 4) {
        uint32_t x = *(const uint32_t*)p;
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

static const unsigned char *
    ff_avc_find_startcode(const unsigned char *p,
                          const unsigned char *end)
{
    const unsigned char *out = ff_avc_find_startcode_internal(p, end);

    if (p < out && out < end && !out[-1])
        out--;

    return out;
}

static int sim_picture()
{
    char file[256] = {};
    int ret = 0;

    sprintf(file, "%s%s", SAMPLE_DIR, JPG_FILE);
    ret = read_file(file, &pic_buf, &pic_len);
    if (ret != 0) {
        printf(" null pointer\n");
        return -1;
    }
    return 0;
}

/**
 * 文件模拟的aac源
 * @param opaqueue
 * @return
 */
int sim_aac_stream(void *opaqueue)
{
    //读.aac文件到内存
    char file[64] = {};
    sprintf(file, "%s%s", SAMPLE_DIR, AUDIO_AAC_FILE);
    char *file_stream = NULL;
    int stream_len = 0;
    int ret = read_file(file, &file_stream, &stream_len);
    if (ret != 0)
        return -1;

    //获取当前时间
    int64_t nSysTimeBase = get_current_ms();
    int64_t nNextAudioTime = nSysTimeBase;
    int64_t nNow = nSysTimeBase;

    int bAudioOk = 1;
    ps_adts adts = {0};
    int offset = 0;
    int64_t aacFrameCount = 0;
    int cbRet;

    while (bAudioOk) {
        if (nNow + 1 > nNextAudioTime) {
            if (offset + 7 <= stream_len) {
                LinkParseAdtsfixedHeader((unsigned char *)(file_stream + offset), &adts.fix_head);
            }
            int head_len = adts.fix_head.protection_absent == 1? 7 : 9;
            LinkParseAdtsVariableHeader((unsigned char *)(file_stream + offset), &adts.var_head);
            if (offset + head_len + adts.var_head.aac_frame_length <= stream_len) {
                //调用link-c-sdk发送
                cbRet = data_callback(opaqueue, file_stream + offset, adts.var_head.aac_frame_length,
                    THIS_IS_AUDIO, nNextAudioTime - nSysTimeBase + g_rollover_base, 0);
                printf("###%s:%d ret = %d ###\n", __func__, __LINE__, cbRet);
                if (cbRet != 0 && cbRet != LINK_PAUSED) {
                    bAudioOk = 0;
                    continue;
                }
                offset += adts.var_head.aac_frame_length;
                aacFrameCount++;
                int64_t d1 = ((1024 * 1000.0) / aacfreq[adts.fix_head.sampling_frequency_index]) * aacFrameCount;
                int64_t d2 = ((1024 * 1000.0) / aacfreq[adts.fix_head.sampling_frequency_index]) * (aacFrameCount -1);
                nNextAudioTime += (d1 - d2);
            } else
                bAudioOk = 0;
        }

        //更新时间结点
        int64_t nSleepTime = 0;
        if (nNextAudioTime - nNow > 1)
            nSleepTime = (nNextAudioTime - nNow - 1) * 1000;
        if (nSleepTime != 0) {
            if (nSleepTime > 40 * 1000) {
                printf("abnormal time diff:%"PRId64"", nSleepTime);
            }
            usleep(nSleepTime);
        }
        nNow = get_current_ms();
    }

    if (file_stream) {
        free(file_stream);
        g_audio_base_time += nNextAudioTime;
    }

    return 0;
}

/**
 * 文件模拟h264
 * @param opaque
 * @return
 */
int sim_h264_stream(void *opaque)
{
    int ret;


    //读视频数据到内存
    char file[64] = {0};
    sprintf(file, "%s%s", SAMPLE_DIR, VIDEO_H264_FILE);
    char * file_stream = NULL;
    int stream_len = 0;
    ret = read_file(file, &file_stream, &stream_len);
    if (ret != 0)
        return -1;

    //获取系统当前时间, 模拟帧率相关内容
    int64_t nSysTimeBase = get_current_ms();
    int64_t nNextVideoTime = nSysTimeBase;   //nNextVideoTime表示按25fps，下一帧发送的时间
    int64_t nNow = nSysTimeBase;

    uint8_t * nextstart = (uint8_t *)file_stream;
    uint8_t * endptr = nextstart + stream_len;
    int cbRet = 0;
    int nIDR = 0;
    int nNonIDR = 0;
    int IsFirst = 1;
    int bVideoOk = 1;

    while (bVideoOk) {
        if (nNow+1 > nNextVideoTime) {

            unsigned char *start = NULL;
            unsigned char *end = NULL;
            unsigned char *sendp = NULL;  //第一个去了起始码的nalu地址
            char type = -1;
            do{
                //解析h264中同步码，确定一个nalu的起始和结束
                start = (uint8_t *)ff_avc_find_startcode((const uint8_t *)nextstart, (const uint8_t *)endptr);
                end = (uint8_t *)ff_avc_find_startcode(start+4, endptr);
                nextstart = end;
                if(sendp == NULL)
                    sendp = start;
                if(start == end || end > endptr){
                    bVideoOk = 0;
                    break;
                }

                //获取nalu类型
                type = get_nalu_type(start[2] == 0x01? start[3] : start[4]);
                if (type == NALU_TYPE_SLICE) {
                    nNonIDR++;
                } else if (type == NALU_TYPE_IDR) {
                    nIDR++;
                    IsFirst = 0;
                } else
                    continue;

                //通过link-c-sdk发送
                cbRet = data_callback(opaque, sendp, end - sendp,
                    THIS_IS_VIDEO, g_rollover_base +nNextVideoTime-nSysTimeBase, type == 5);
                printf("@@@%s:%d ret = %d @@@\n", __func__, __LINE__, cbRet);
                if (cbRet != 0 && cbRet != LINK_PAUSED) {
                    bVideoOk = 0;
                }
                nNextVideoTime += 40;
                break;
            }while(1);
        }

        //更新时间结点
        int64_t nSleepTime = 0;
        if (nNextVideoTime - nNow > 1)
            nSleepTime = (nNextVideoTime - nNow - 1) * 1000;
        if (nSleepTime != 0) {
            if (nSleepTime > 40 * 1000) {
                printf("abnormal time diff:%"PRId64"", nSleepTime);
            }
            usleep(nSleepTime);
        }
        nNow = get_current_ms();
    }

    if (file_stream) {
        free(file_stream);
        g_video_base_time += nNextVideoTime;
    }

    return 0;
}

/**
 * 创建模拟视音频源
 * @param pAvuploader
 */
static void sim_start(AVuploader *pAvuploader)
{

    sim_picture();
    pthread_t audio_thr, video_thr;

    pthread_create(&video_thr, NULL, sim_h264_stream, (void *)pAvuploader);
    pthread_create(&audio_thr, NULL, sim_aac_stream, (void *)pAvuploader);
    usleep(10000);
}

static void sim_stop()
{
    if (pic_buf)
        free(pic_buf);
}

//!
//! link-c-sdk访问部分
//!

AVuploader avuploader = {0};
/**
 * upload用到的配置参数
 */
static void get_default_param()
{

    avuploader.userUploadArg.nAudioFormat = LINK_AUDIO_AAC;  //输入音频格式，这里为aac
    avuploader.userUploadArg.nChannels = 1;
    avuploader.userUploadArg.nSampleRate = 16000;  //音频采样率
    avuploader.userUploadArg.nVideoFormat = LINK_VIDEO_H264;  //输入视频格式
    avuploader.userUploadArg.pGetPictureCallbackUserData = NULL;
    avuploader.userUploadArg.pConfigRequestUrl = "http://linking-device.qiniuapi.com/v1/device/config";
    avuploader.userUploadArg.nConfigRequestUrlLen = strlen("http://linking-device.qiniuapi.com/v1/device/config");
    avuploader.userUploadArg.pDeviceAk = DAK;
    avuploader.userUploadArg.nDeviceAkLen = strlen(DAK);
    avuploader.userUploadArg.nDeviceSkLen = strlen(DSK);
    avuploader.userUploadArg.pDeviceSk = DSK;

}

/**
 * 通过link-c-sdk发送缩略图
 * @param queue
 * @param filename
 * @param len
 */
void get_picture_callback(void *queue, const char *filename, int len)
{
    pic_save *save = (pic_save *)queue;

    char file[128] = {0};
    sprintf(file, "%s%s", SAMPLE_DIR, filename);
    printf("$$$%s:%d$$$\n", __func__, __LINE__);
    //通过link-c-sdk发送缩略图
    if (pic_buf && save->pdata) {
        int ret = LinkPushPicture((LinkTsMuxUploader *)save->pdata, file, strlen(file), pic_buf, pic_len);
        printf("&&&%s:%d file %s  ret = %d &&&\n", __func__, __LINE__, file, ret);
    }
}

/**
 * 通过link-c-sdk发送视频/音频数据
 * @param opaque
 * @param pData
 * @param nDataLen
 * @param nFlag
 * @param timestamp
 * @param nIsKeyFrame
 * @return
 */
static int data_callback(void *opaque, void *pData, int nDataLen, int nFlag, int64_t timestamp, int nIsKeyFrame)
{
    AVuploader *pAvuploader = (AVuploader*)opaque;
    int ret = 0;
    pAvuploader->nByteCount += nDataLen;

    //音频
    if (nFlag == THIS_IS_AUDIO){
        ret = LinkPushAudio(pAvuploader->pTsMuxUploader, pData, nDataLen, timestamp + g_audio_base_time, 0);
    }
        //视频
    else {
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
            pAvuploader->nVideoKeyframeAccLen = 0;
        }
        pAvuploader->nVideoKeyframeAccLen += nDataLen;
        printf("------->push video key:%d ts:%"PRId64" size:%d\n",nIsKeyFrame, timestamp, nDataLen);
        ret = LinkPushVideo(pAvuploader->pTsMuxUploader, pData, nDataLen, timestamp + g_video_base_time, nIsKeyFrame, nNewSegMent, 0);
    }
    if (ret == LINK_NOT_INITED) {
        return LINK_SUCCESS;
    }
    return ret;
}

/**
 * 创建上传实例
 * @return
 */
static int upload_av_with_picture_init()
{

    get_default_param();

    pic_save *save = malloc(sizeof(pic_save));
    LinkUploadArg *upload_arg = &avuploader.userUploadArg;
    upload_arg->getPictureCallback = get_picture_callback;  //图片获取回调函数, 上传缩略图时会用到
    upload_arg->pGetPictureCallbackUserData = save;

    //step1.初始化link-c-sdk
    LinkInit();

    //step2.创建上传实例：带图片的上传功能
    LinkNewUploader(&avuploader.pTsMuxUploader, upload_arg);
    save->pdata = avuploader.pTsMuxUploader;
}

/**
 * 清理上传实例
 */
static void ts_upload_release()
{

    //step4.销毁上传实例
    if (avuploader.userUploadArg.pGetPictureCallbackUserData)
        free(avuploader.userUploadArg.pGetPictureCallbackUserData);
    LinkFreeUploader(&avuploader.pTsMuxUploader);
    //step5.回收link-c-sdk资源
    LinkCleanup();
}

int main(void)
{

    //上传视频+音频+缩略图
    upload_av_with_picture_init();

    //打开数据源
    printf("do_start...\n");
    sim_start(&avuploader);
    while (1) {
        usleep(5000);
    }

    //退出
    sim_stop();
    ts_upload_release();
    return 0;

}
