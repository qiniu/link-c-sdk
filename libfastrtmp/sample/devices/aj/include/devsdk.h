#ifndef __DEV__SDK__H__
#define __DEV__SDK__H__

#include "media_cfg.h"
#include "data_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AJ_SDK_ERR -1
#define AJ_SDK_OK 0


#define MAX_MACADDR_LEN   18    //00:11:11:11:11:11
#define MAX_IPADDR_LEN  64

typedef enum
{
	DEV_SDK_PROCESS_IPVS = 0,
	DEV_SDK_PROCESS_P2P = 1,
	DEV_SDK_PROCESS_APP = 2,
	DEV_SDK_PROCESS_GB28181 = 3,
}DevSdkServerType;

typedef enum
{
	AUDIO_TYPE_G711,
	AUDIO_TYPE_AAC
}DevSdkAudioType;

typedef enum
{
	AJ_STREAM_INDEX_VSTREAM_MAIN=0,	//主码流
	AJ_STREAM_INDEX_VSTREAM_AUX=1,	//子码流
	AJ_STREAM_INDEX_VSTREAM_THIRD=2,	//三码流
}AjStreamNo;

typedef	enum
{
	AJ_CONFIG_LANGUAGE_CN = 0,
	AJ_CONFIG_LANGUAGE_HK,
	AJ_CONFIG_LANGUAGE_TW,
	AJ_CONFIG_LANGUAGE_EN
}AJ_CONFIG_LANGUAGE;


typedef struct
{
	int ptz_enable;
	int camera_channels;
	int dual_stream;
	int input_channels;
	int output_channels;	
	int audioin_channels;
	int audioout_channels;
}DEV_CAP;

typedef struct
{
	char sn[64];
	char firmware_info[64];

	//network
	unsigned char macaddr[6];
	char ipaddr[16];
	char gateway[16];
	char netmask[16];
}DEV_INFO;


typedef struct
{
	unsigned char   MACAddress[MAX_MACADDR_LEN];	
	int				dhcpEnable;
	char  IPAddress[MAX_IPADDR_LEN];
	char  netMask[MAX_IPADDR_LEN];
	char  gateWay[MAX_IPADDR_LEN];
	char  DNS1[MAX_IPADDR_LEN];
	char  DNS2[MAX_IPADDR_LEN];
}NetLANInfo;


typedef struct
{
	int audioEnable; //音频是否开启
	int streamID;
	int initQuant;
	int bitRate;
	int frameRate;
	char Resolution[32];
	char VencFmt[32];
	char BitRateCtl[32];
}VencInfo;

//隐私遮挡单元，尺寸以100为单位,
typedef struct 
{
	int bvalid;	//是否有效
	int left;		//0~100
	int top;		//0~100
	int right;		//0~100
	int botton;	//0~100
}AjVideoMaskUnitConfig;

#define PRIVACY_MASK_MAX_BLOCK 8
typedef struct
{
	int enable;	//启用标记
	AjVideoMaskUnitConfig config[PRIVACY_MASK_MAX_BLOCK];
}AjVideoMaskConfig;

typedef int (*ALARM_CALLBACK)(ALARM_ENTRY alarm, void *pcontext);
typedef int (*CONTROL_RESPONSE)(int msg_seq, char *xmlbuf, void *pcontext);
typedef int (*VIDEO_CALLBACK)(int streamno, char *frame, int len, int iskey, double timestatmp, unsigned long frame_index,	unsigned long keyframe_index, void *pcontext);
typedef int (*AUDIO_CALLBACK)(char *frame, int len, double timestatmp, unsigned long frame_index, void *pcontext);
typedef void (*DEBUG_CALLBACK)(int level, const char* fmt, ...);


//sdk general
const char* dev_sdk_version();
int dev_sdk_init(DevSdkServerType type);
int dev_sdk_init_common(int prockey, int prio, int readerid);//自己指定PROCESS KEY
int dev_sdk_release(void);

//sdk device infomation
int dev_sdk_get_netLanInfo(NetLANInfo *netlaninfo);
int dev_sdk_get_devcap(DEV_CAP *cap);
int dev_sdk_get_devinfo(DEV_INFO *devinfo);
int dev_sdk_get_product_type(void);
char* dev_sdk_get_dev_module();

//config by xml
int dev_sdk_get_config_xml(int id, char *xmlbuf, int *bufsize);
int dev_sdk_set_config_xml(int id, char *xmlbuf);

//control 
int dev_sdk_safe_reboot();
int dev_sdk_config_network();
int dev_sdk_EditUser(char *username,char *password, char *group, char *status);

int dev_sdk_register_callback(ALARM_CALLBACK alarmcb, CONTROL_RESPONSE crespcb, DEBUG_CALLBACK debugcb, void *pcontext);
int dev_sdk_sysctl(int msg_seq, int msg_code, char *xmlbuf);
int dev_sdk_ptzctl(int msg_seq, char *xmlbuf);

//for streaming

/*to get the yuv stream availability*/
int dev_sdk_yuvstream_available();

/*to get the third video availability*/
int dev_sdk_thirdvideo_available();


int dev_sdk_get_video_vol(int streamno, char* volbuf, unsigned int volbuf_len, unsigned int *vol_length);

/*
  camera: 0.(ipcamera fix value)
  stream: 
	0: main stream 
	1: sub stream
	2: thread stream
	3: YUV
  vcb: function for handle video stream
  pcontext: for save user context,
*/
int dev_sdk_start_video(int camera, int stream, VIDEO_CALLBACK vcb, void *pcontext);
int dev_sdk_start_audio(int camera, int stream, AUDIO_CALLBACK acb, void *pcontext);
int dev_sdk_start_audiopcm(int camera, int stream, AUDIO_CALLBACK acb, void *pcontext);
int dev_sdk_request_idr(int camera, int stream);

int dev_sdk_stop_video(int camera, int stream);
int dev_sdk_stop_audio(int camera, int stream);
int dev_sdk_stop_audiopcm(int camera, int stream);

//control api
int dev_sdk_reboot(void);
int dev_sdk_restore(void);
int dev_sdk_set_time(struct tm newtime, int tz);

//for reverse audio
int dev_sdk_start_audio_play(DevSdkAudioType audiotype);
int dev_sdk_audio_play(const char *buf, int size);
int dev_sdk_audio_play_pcm(const char *buf, int size, int samplerate, int channels);
int dev_sdk_audio_play_mp3(const char *buf, int size);
int dev_sdk_stop_audio_play(void);

//for debug infomation ouput
void dev_sdk_log(const char* format, ...);




//11111表示告警全开
#define SLOG_LVL_FATAL	 16  /* 1<<4 */
#define SLOG_LVL_ERROR	 8   /* 1<<3 */
#define SLOG_LVL_WARNING   4   /* 1<<2 */
#define SLOG_LVL_INFO	  2   /* 1<<1 */
#define SLOG_LVL_TRACE	 1


/*获取视频、音频能力集*/
const RESOLUTION_ENTRY *dev_sdk_get_video_capability(int *pItemCount);
const AUDIO_CODEC_ENTRY *dev_sdk_get_audio_capability(int *pItemCount);

/*获取时间戳，单位为毫秒,获取机制为本次系统启动后的时间*/
unsigned int dev_sdk_gettimestamp(void);


/*####################动态设置部分####################################################*/
/*动态设置视频流码率*/
int dev_sdk_dynamic_set_cfg_v_bitrate(int streamno, int bitrate);


/*
//设置标题中时间的格式
"yyyy-mm-dd hh:mm:ss",
"yyyy/mm/dd hh:mm:ss",
"yy-mm-dd hh:mm:ss",
"yy/mm/dd hh:mm:ss",
"hh:mm:ss dd/mm/yyyy",
"hh:mm:ss dd-mm-yyyy",
"hh:mm:ss mm/dd/yyyy",
"hh:mm:ss mm-dd-yyyy",	
"mm/dd/yyyy hh:mm:ss",
"mm-dd-yyyy hh:mm:ss",
*/
int dev_sdk_dynamic_overlay_set_timeformat(char *pformat);

/*动态设置标题文本，ptitle为NULL时只设置标题叠加信息*/
int dev_sdk_dynamic_overlay_set_title_utf8(char *ptitle, titleFormatEn titleformat);
int dev_sdk_dynamic_overlay_set_title_gb2312(char *ptitle, titleFormatEn titleformat);

/*动态设置时间与标题的位置。时间与标题上下位置必须不相同*/
int dev_sdk_dynamic_overlay_setpos(osdPointEn titlePos, osdPointEn timePos);



/*动态设置隐私遮挡*/
int dev_sdk_dynamic_privacymask_set(AjVideoMaskConfig *pCfg);

/*动态设置画面水平/垂直翻转*/
int dev_sdk_dynamic_setflip(int hflip, int vflip);


int dev_sdk_get_vencInfo( AjStreamNo streamno,  VencInfo *vencInfo);
int dev_sdk_get_platformCfg(PlatformConfig *platformInfo );
int dev_sdk_get_gb28181Cfg(GB28181Config *pPlatformCfg );



/*log输出*/
void dev_sdk_log(const char* format, ...);

/*for XML package*/
int MakeXMLRequest(char *pBuf, int nSize, char *MsgType, char *MsgCode, char *MsgFlag, char * MsgBody);

/*for xml header parse*/
int GetMessageHeader(char *MsgBuf, char *MsgType, char *MsgCode, char *MsgFlag, char **msgBody, int *msgLen);


/*detail system config get*/
int dev_sdk_get_AlarmConfig(AlarmConfig *pAlarmConfig);
int dev_sdk_get_MotionDetectAlarm(MotionDetectAlarm *pMDAlm);
int dev_sdk_get_InputAlarm(InputAlarm *pInputAlm);
int dev_sdk_get_VideoLostAlarm(VideoLostAlarm *pVideoLost);
int dev_sdk_get_VideoCoverAlarm(VideoCoverAlarm *pVideoCover);
int dev_sdk_get_StorageFullAlarm(StorageFullAlarm *pSFAlm);
int dev_sdk_get_AudioAlarm(AudioAlarm *pAlm);
int dev_sdk_get_VideoGateAlarm(VideoGateAlarm *pAlm);
int dev_sdk_get_PdAlarm(PdAlarm *pAlm);

int dev_sdk_get_SystemConfig(SystemConfig *pSystemCfg);
int dev_sdk_get_PtzConfig(PTZConfig *pPtzCfg);
int dev_sdk_get_UserConfig(UserConfig *pUserCfg);
int dev_sdk_get_SyslogConfig(SyslogConfig *pSyslogCfg);
int dev_sdk_get_TimeConfig(TimeConfig *pTimeCfg);
int dev_sdk_get_MiscConfig(MiscConfig *pMiscCfg);
int dev_sdk_get_RecordConfig(RecordConfig *pRecordCfg);
int dev_sdk_get_MediaConfig(MediaConfig *pMediaCfg);

int dev_sdk_get_VideoConfig(VideoConfig *pVideoCfg);
int dev_sdk_get_VideoCaptureConfig(VideoCapture    *pCfg);
int dev_sdk_get_VideoEncodeConfig(VideoEncode    *pCfg);
int dev_sdk_get_VideoOverlayConfig(VideoOverlay     *pCfg);
int dev_sdk_get_VideoMaskConfig(VideoMaskConfig    *pCfg);
int dev_sdk_get_VideoROIConfig(VideoROI    *pCfg);

int dev_sdk_get_AudioConfig(AudioConfig *pAudioCfg);
int dev_sdk_get_AudioEncode(AudioEncode *pAudioCfg);
int dev_sdk_get_AudioCapture(AudioCapture *pAudioCfg);
int dev_sdk_get_MediaStreamConfig(MediaStreamConfig *pMediaStreamCfg);
int dev_sdk_get_PlatformConfig(PlatformConfig *pPlatformCfg);
int dev_sdk_get_GB28181Config(GB28181Config *pPlatformCfg);
int dev_sdk_get_NetworkConfig(NetworkConfigNew *pNetworkCfg);
int dev_sdk_get_NetworkLANConfig(LANConfig *lanCfg);
int dev_sdk_get_NetworkWIFIConfig(WIFIConfig *wifiCfg);
int dev_sdk_get_NetworkADSLConfig(ADSLConfigNew *adslCfg);
int dev_sdk_get_NetworkDDNSConfig(DDNSConfig *ddnsCfg);
int dev_sdk_get_NetworkUPNPConfig(UPNPConfig *upnpCfg);
int dev_sdk_get_NetworkP2PConfig(P2PConfig *pCfg);
int dev_sdk_get_FtpServerConfig(FtpServerList *pFtpServers);
int dev_sdk_get_SmtpServerConfig(SmtpServerList *pSmtpServers);
int dev_sdk_get_ServerConfig(ServerConfig *pServerCfg);
int dev_sdk_get_StorageInfo(STORAGE_INFO_DATA *storageInfo);
int dev_sdk_get_SystemVersionInfo(SYSTEM_VERSION_DATA *versionInfo);
int dev_sdk_get_DeviceSerialNumber(char *sn);
int dev_sdk_get_SystemControlString(char *config_str);
int dev_sdk_get_FunctionEnabled(char *function_name);
int dev_sdk_get_NetworkStatus(NETWORK_STATUS_DATA *networkStatus);
int dev_sdk_get_ReplayFilename(char *filename);
int dev_sdk_get_WifiApInfos(WIFI_AP_SCAN *apList);

/*detail system config set*/
/************ALARM*****************************/
#if 1
int dev_sdk_set_AlarmAlarm(AlarmConfig *pCfg);
int dev_sdk_set_MotionDetectAlarm(MotionDetectAlarm *pCfg);  
int dev_sdk_set_InputAlarm(InputAlarm *pCfg);
int dev_sdk_set_VideoLostAlarm(VideoLostAlarm *pCfg) ;
int dev_sdk_set_VideoCoverAlarm(VideoCoverAlarm *pCfg);
int dev_sdk_set_StorageFullAlarm(StorageFullAlarm *pCfg);
int dev_sdk_set_AudioAlarmAlarm(AudioAlarm *pCfg);
int dev_sdk_set_VideoGateAlarmAlarm(VideoGateAlarm *pCfg);
int dev_sdk_set_PdAlarmAlarm(PdAlarm *pCfg);
#endif
/*****************************************/

/************VIDEO*****************************/
#if 1
int dev_sdk_set_VideoConfig(VideoConfig *pCfg);   
int dev_sdk_set_VideoCaptureConfig(VideoCapture *pCfg); 
int dev_sdk_set_VideoEncodeConfig(VideoEncode *pCfg);
int dev_sdk_set_VideoOSDConfig(VideoOverlay *pCfg);
int dev_sdk_set_VideoUserOSDConfig(VideoUserOverlay *pCfg);
int dev_sdk_set_VideoMaskConfig(VideoMaskConfig *pCfg);
int dev_sdk_set_VideoROIConfig(VideoROI *pCfg);
#endif
/*****************************************/

/************AUDIO*****************************/
#if 1
int dev_sdk_set_AudioConfig(AudioConfig *pCfg);   
int dev_sdk_set_AudioEncode(AudioEncode *pCfg);   
int dev_sdk_set_AudioCapture(AudioCapture *pCfg);   
#endif

int dev_sdk_set_MiscConfig(MiscConfig *pCfg);  
int dev_sdk_set_RecordConfig(RecordConfig *pCfg);

int dev_sdk_set_MediaStreamConfig(MediaStreamConfig *config);  
int dev_sdk_set_PlatformConfig(PlatformConfig *pPlatformCfg);  
int dev_sdk_set_GB28181Config(GB28181Config *pPlatformCfg);
int dev_sdk_set_UserConfig(UserConfig *pUserCfg);  
int dev_sdk_set_TimeConfig(TimeConfig *pTimeCfg);  
int dev_sdk_set_NetworkLANConfig(LANConfig *config);   
int dev_sdk_set_NetworkWIFIConfig(WIFIConfig *config); 
int dev_sdk_set_NetworkConfig(NetworkConfigNew *config);   
int dev_sdk_set_SystemTime(struct tm time, int tz);
int dev_sdk_set_SystemTimeAndZone(struct tm time, int tz); 
int dev_sdk_set_ReplayFilename(char *filename);
int dev_sdk_set_IRCUTControl(int cmd); 
int dev_sdk_set_SnapJpegFile(int stream, int quality, char *path, char *filename); 
int dev_sdk_set_GateWay(char *gwIp);   
int dev_sdk_set_Ip(char *Ip, char *netmask, char* gwIp);
int dev_sdk_set_Dns(char *dns);	

/*字符串编码转换*/
void dev_sdk_str_utf8_2_gb2312(const char *pIn, char *pOut);
void dev_sdk_str_gb2312_2_utf8(const char *pIn, char *pOut, int bBig5);


/**************for fh*************/
/***********************************************************************************
* Name         : dev_sdk_set_ra_answer
* Input Para   :
*			   : flag: 1(need) 0(not need).
*			   :
*			   : 
*			   : 
* Output Para  : 
* Return value : 0(ok),-1(not ok)
* Description  : set reverse audio need or not need press key to answer. 设置对讲/喊话是否需要按键接听
* Modification : 2017-03  Cham Li
************************************************************************************/
int dev_sdk_set_ra_answer(int flag);


/***********************************************************************************
* Name         : dev_sdk_play_audiofile_start
* Input Para   :
*			   : filename: mp3 or pcmwav file name.
*			   : times: play times. 0 or negative number means recycle play in infinite loop
*			   : bWaitPreviousPlayFinish: wait or not wait for previous audio file finished playing.
*			   : 
* Output Para  : 
* Return value : 0(ok),-1(not ok)
* Description  : start play mp3 or pcmwav file. 开始播放MP3/PCMWAV文件
* Modification : 2017-03  Cham Li
************************************************************************************/
int dev_sdk_play_audiofile_start(const char *filename, int times, int bWaitPreviousPlayFinish);


/***********************************************************************************
* Name         : dev_sdk_play_audiofile_stop
* Input Para   :
*			   :
*			   :
*			   :
*			   : 
* Output Para  : 
* Return value : 0(ok),-1(not ok)
* Description  : stop play mp3 or pcmwav file. 停止播放MP3/PCMWAV文件
* Modification : 2017-03  Cham Li
************************************************************************************/
int dev_sdk_play_audiofile_stop();


/***********************************************************************************
* Name         : dev_sdk_get_io_input_status
* Input Para   : channelno: 1-4
*			   :
*			   :
*			   :
*			   : 
* Output Para  : status: 0(Open) 1(close)
* Return value : 0(ok),-1(not ok)
* Description  : get io input open status
* Modification : 2017-03  Cham Li
************************************************************************************/
int dev_sdk_get_io_input_status(int channelno, int *status);


/***********************************************************************************
* Name         : dev_sdk_get_io_output_status
* Input Para   : channelno: 1-4
*			   :
*			   :
*			   :
*			   : 
* Output Para  : status: 0(Open) 1(close)
* Return value : 0(ok),-1(not ok)
* Description  : get io output control status
* Modification : 2017-03  Cham Li
************************************************************************************/
int dev_sdk_get_io_output_status(int channelno, int *status);


/***********************************************************************************
* Name         : dev_sdk_control_io_output
* Input Para   : channelno: 1-4
*			   : value: 0(open) 1(close)
*			   :
*			   :
*			   : 
* Output Para  :
* Return value : 0(ok),-1(not ok)
* Description  : control io output 
* Modification : 2017-03  Cham Li
************************************************************************************/
int dev_sdk_control_io_output(int channelno, int value);

/***********************************************************************************
* Name         : dev_sdk_set_audio_volumn
* Input Para   : ai_volumn: audio line in or mic volumn, 0-100
*			   : ao_volumn: audio play volumn, 0-100
*			   :
*			   :
*			   : 
* Output Para  :
* Return value : 0(ok),-1(not ok)
* Description  : set audio volumn
* Modification : 2017-03  Cham Li
************************************************************************************/
int dev_sdk_set_audio_volumn(int ai_volumn, int ao_volumn);
	

/***********************************************************************************
* Name         : dev_sdk_enable_md_alarm
* Input Para   : enable: 0(disable), 1(enable)
*			   : 
*			   :
*			   :
*			   : 
* Output Para  :
* Return value : 0(ok),-1(not ok)
* Description  : enable or disable motion detect alarm
* Modification : 2017-03  Cham Li
************************************************************************************/
int dev_sdk_enable_md_alarm(int enable);  



/***********************************************************************************
* Name         : dev_sdk_set_ptz_comm_cfg
* Input Para   : PTZCommonConfig
*			   : 
*			   :
*			   :
*			   : 
* Output Para  :
* Return value : 0(ok),-1(not ok)
* Description  : set ptz/485 serial params
* Modification : 2017-03  Cham Li
************************************************************************************/
int dev_sdk_set_ptz_comm_cfg(PTZCommonConfig cfg);


//PTZ/485透明数据发送
#define MAX_DEVSDK_TRANSPARENT_CMD_LEN 16
typedef struct
{
	int datalen;
	char buffer[MAX_DEVSDK_TRANSPARENT_CMD_LEN];
}DevSdkTransCmd;
/***********************************************************************************
* Name         : dev_sdk_send_ptz_transdata
* Input Para   : serialport:
*			   : DevSdkTransCmd: serial transparant data, with max length
*			   :
*			   :
*			   : 
* Output Para  :
* Return value : 0(ok),-1(not ok)
* Description  : send ptz/485 serial trans data
* Modification : 2017-03  Cham Li
************************************************************************************/
int dev_sdk_send_ptz_transdata(int serialport, DevSdkTransCmd *pData);


/***********************************************************************************
* Name         : dev_sdk_set_md_audioaction
* Input Para   : AudioPlayAction: play audio file when alarm
*			   : 
*			   :
*			   :
*			   : 
* Output Para  :
* Return value : 0(ok),-1(not ok)
* Description  : set audio play action when alarm
* Modification : 2017-03  Cham Li
************************************************************************************/
int dev_sdk_set_md_audioaction(AudioPlayAction action);

/***********************************************************************************
* Name         : dev_sdk_set_ioalarm_audioaction
* Input Para   : port: 1-4
*			   : AudioPlayAction: play audio file when alarm
*			   :
*			   :
*			   : 
* Output Para  :
* Return value : 0(ok),-1(not ok)
* Description  : set audio play action when alarm
* Modification : 2017-03  Cham Li
************************************************************************************/
int dev_sdk_set_ioalarm_audioaction(int port, AudioPlayAction action);

typedef enum
{
	DEVSDK_LED_OFF = 0, //关灯
	DEVSDK_LED_FLICKER_HARF1S = 1, //快闪(0.5秒闪一次)
	DEVSDK_LED_FLICKER_1S = 2,			//慢闪(1秒闪一次)
	DEVSDK_LED_LIGHT_ALWAYS = 100,		//常亮
}DevSdkLedStatus;

/***********************************************************************************
* Name         : dev_sdk_control_led
* Input Para   : channelno: led io output port number
*			   : status: refer to DevSdkLedStatus
*			   :
*			   :
*			   : 
* Output Para  :
* Return value : 0(ok),-1(not ok)
* Description  : control led light status
* Modification : 2017-03  Cham Li
************************************************************************************/
int dev_sdk_control_led(int port, DevSdkLedStatus status);


typedef enum
{
	DEVSDK_AUDIO_PLAY_READY,	//预备OK，未播放
	DEVSDK_AUDIO_PLAY_RESERVE,//正在播放反向音频
	DEVSDK_AUDIO_PLAY_FILE,	//正在播放文件
}DevSdkAudioPlayStatus;

/***********************************************************************************
* Name         : dev_sdk_get_audioplay_status
* Input Para   : 
*			   : 
*			   :
*			   :
*			   : 
* Output Para  :
* Return value : DevSdkAudioPlayStatus, refer to the definition
* Description  : get audio play status
* Modification : 2017-03  Cham Li
************************************************************************************/
DevSdkAudioPlayStatus dev_sdk_get_audioplay_status();


/***********************************************************************************
* Name         : dev_sdk_firmware_update
* Input Para   : rom_path: firmware full path
*			   : 
*			   :
*			   :
*			   : 
* Output Para  :
* Return value : 0(ok),-1(not ok)
* Description  : update firmware by filename
* Modification : 2017-10  Cham Li
************************************************************************************/
int dev_sdk_firmware_update(char *rom_path);

void GetMediaStreamConfig( MediaStreamConfig *config );

#ifdef __cplusplus
}
#endif

#endif
