#ifndef _____DATA_STRUCT_H____
#define _____DATA_STRUCT_H____

#if defined(LINUX) && !defined(NETSDK)
#include "platform.h"
#define PLATFORM_DEFAULT_VALUE 0
#endif
#if defined(WIN32) || defined(NETSDK)
#define PLATFORM_DEFAULT_VALUE 1
#endif

#include <time.h>
#include "media_cfg.h"

#ifndef PLATFORM_HI35XX
#define PLATFORM_HI35XX PLATFORM_DEFAULT_VALUE
#endif

#ifndef PLATFORM_HI3516A
#define PLATFORM_HI3516A PLATFORM_DEFAULT_VALUE
#endif

#ifndef PLATFORM_HI3518EV200
#define PLATFORM_HI3518EV200 PLATFORM_DEFAULT_VALUE
#endif

#ifndef PLATFORM_HI3516CV300
#define PLATFORM_HI3516CV300 PLATFORM_DEFAULT_VALUE
#endif


#ifndef PLATFORM_DM365
//#define PLATFORM_DM365 PLATFORM_DEFAULT_VALUE
#endif

#ifndef PLATFORM_AMBAR_S2L
#define PLATFORM_AMBAR_S2L PLATFORM_DEFAULT_VALUE
#endif

#ifndef PLATFORM_AMBAR_S2LM
#define PLATFORM_AMBAR_S2LM PLATFORM_DEFAULT_VALUE
#endif

#ifndef PLATFORM_AMBAR_S2LM2
#define PLATFORM_AMBAR_S2LM2 PLATFORM_DEFAULT_VALUE
#endif

#ifndef PLATFORM_MSTAR
#define PLATFORM_MSTAR PLATFORM_DEFAULT_VALUE
#endif


#ifndef PLATFORM_X86
#define PLATFORM_X86 0
#endif

#define VIDEO_PRIVATE_HEADER_MAGIC 0x1a2b3c4d
typedef struct
{
	unsigned long flag;			//VIDEO_PRIVATE_HEADER_MAGIC
	unsigned long data;
	unsigned long frame_index;
	unsigned long keyframe_index;
}VIDEO_FRAME_HEADER;



#define MAX_USER_SESSION_COUNT 		20
#define MAX_USER_SESSION			40
#define MAX_SESSION_TIMEOUT_SECOND	2400

#define MAX_IP_NAME_LEN 64


#define GROUP_NAME_MAX_LEN 32
typedef struct 
{
	char groupName[GROUP_NAME_MAX_LEN];
}Group;

#define ACCOUNT_STATUS_MAX_LEN 8
#define ACCOUNT_NAME_MAX_LEN 40
#define ACCOUNT_PASSWORD_MAX_LEN  40

typedef struct
{
	char 	userName[ACCOUNT_NAME_MAX_LEN];
	char 	password[ACCOUNT_PASSWORD_MAX_LEN];
	Group 	group;
	char    status[ACCOUNT_STATUS_MAX_LEN];
}UserAccount;

#define MAX_ACCOUNT_COUNT 10

typedef struct
{
	int count;
	UserAccount accounts[MAX_ACCOUNT_COUNT];
}UserConfig;

typedef struct
{
	char  				serverIP[MAX_IP_NAME_LEN];
	unsigned short  	serverPort;
	unsigned int  		refreshInterval;
}NTPConfig;

#define TIME_MODE_MAX_LEN 32
#define TIME_MODE_NAME_NTP  "NTP"
#define TIME_MODE_NAME_MANUAL "MANUAL"
#define TIME_MODE_NAME_P2P  "P2P"

typedef struct
{
	char modeName[TIME_MODE_MAX_LEN];
}TimeMode;

typedef struct
{
	unsigned char nEnable;			//启用标记
	unsigned char bAuto;			//根据时区自动配置
	short nOffsetMin;		//偏移时间(分钟)
	unsigned char nStartMonth;
	unsigned char nStartWeek;		//该月第几周
	unsigned char nStartWeekday;	//周几
	unsigned char nStartHour;		//小时
	unsigned char nToMonth;
	unsigned char nToWeek;		//该月第几周
	unsigned char nToWeekday;	//周几
	unsigned char nToHour;		//小时
}SummerTimeConfig;

typedef struct
{
	TimeMode  	timeMode;
	int 	  	timeZone;
	NTPConfig 	ntpConfig;
	SummerTimeConfig summerConfig;
}TimeConfig;

#define PTZ_PROTOCOL_NAME_MAX_LEN 32
typedef struct
{
	char protocolName[PTZ_PROTOCOL_NAME_MAX_LEN];
}PTZProtocol;


#define VERIFY_NAME_MAX_LEN 32
typedef struct
{
	char verifyName[VERIFY_NAME_MAX_LEN];
}Verify;


#define FLOW_CONTROL_MAX_LEN 32
typedef struct
{
	char flowControlName[FLOW_CONTROL_MAX_LEN];
}FlowControl;

#define PTZ_FUNCTION_TYPE_MAX_LEN 28

typedef struct
{
	char typeName[PTZ_FUNCTION_TYPE_MAX_LEN];
}PTZFunctionType;

#define PTZ_FUNCTION_NAME_MAX_LEN 32

typedef struct
{
	char functionName[PTZ_FUNCTION_NAME_MAX_LEN];
	int  presetNum;
	PTZFunctionType functionType;
	int reserveValue;
}PTZFunction;


#define MAX_PTZFUCTION_COUNT 64 
typedef struct
{
	int functionCnt;
	PTZFunction functions[MAX_PTZFUCTION_COUNT];
}PTZAdvanceConfig;


typedef struct
{
	PTZProtocol  		ptzProtocol;
	unsigned  int 		comPort;
	unsigned  int 		baudrate;
	unsigned  int 		dataBits;
	unsigned  int 		stopBits;
	Verify 		 		verify;
	FlowControl  		flowControl;
	unsigned int 		bootAction;
}PTZCommonConfig;


typedef struct
{
	PTZCommonConfig    commonCfg;
	PTZAdvanceConfig    advanceCfg;
}PTZConfig;

#define FTP_NAME_MAX_LEN 128
#define FTP_PASSWORD_MAX_LEN 128
#define FTP_PATH_MAX_LEN 256


#define SMTP_NAME_MAX_LEN			128
#define SMTP_PASSWORD_MAX_LEN		128
#define SMTP_ACCOUNT_MAX_LEN		256
#define SMTP_SUBJECT_MAX_LEN        256


#define LOG_LEVEL_NAME_MAX_LEN 200
typedef struct
{
	char levelName[LOG_LEVEL_NAME_MAX_LEN];
}LogLevel;

#define STORE_MEDIA_NAME_MAX_LEN 32

typedef struct
{
	char mediaName[STORE_MEDIA_NAME_MAX_LEN];
}StoreMedia;

#define STORE_POLICY_NAME_MAX_LEN 32

typedef struct
{
	char policyName[STORE_POLICY_NAME_MAX_LEN];
}StorePolicy;

#define BACKUP_WAY_NAME_MAX_LEN 32

typedef struct
{
	char wayName[BACKUP_WAY_NAME_MAX_LEN];
}BackupWay;

#define SYSLOG_FILE_NAME_MAX_LEN   32 

typedef struct
{
	LogLevel   			logLevel;
	unsigned int	 	maxDays;
	//unsigned int   		maxEventPerday;
	StoreMedia   		storeMedia;
	StorePolicy   		storePolicy;
	int   				autoBackup; // 1  for auto  0 for manual
	BackupWay   		backupWay;
}SyslogConfig;


#define MAX_LANGUAGE_LEN 32

typedef struct
{
	char language[MAX_LANGUAGE_LEN];
}MiscConfig;

typedef struct
{
	int enable;
	int day; //0: erverday, 1: monday,-- 7: sunday
	DayTime time;
}MaintainConfig;

#define MAX_ALOW_IP_NUM 5
typedef struct
{
	int enable;
	unsigned int nAllowIp[MAX_ALOW_IP_NUM];
}SysAlowIpConfig;


#define MAX_ALARM_CLOCK_NUM 5
typedef struct
{
	unsigned char enable;
	unsigned char hour;
	unsigned char minute;
	unsigned char second;	
}AlarmClock;//闹钟设置

typedef struct
{
	unsigned char enable;	
	unsigned char from_hour;
	unsigned char from_minute;
	unsigned char from_second;	
	unsigned char to_hour;
	unsigned char to_minute;
	unsigned char to_second;	
	unsigned char reserved;
}AlarmOClock;//整点报时

typedef struct
{
	AlarmOClock oclock;
	AlarmClock alarmclock[MAX_ALARM_CLOCK_NUM];
}AlarmClockConfig;

typedef struct
{
	PTZConfig     ptzCfg;
	TimeConfig    timeCfg;
	UserConfig    userCfg;
	SyslogConfig  syslogCfg;
	MiscConfig	  miscCfg;
	MaintainConfig maintainCfg;
	SysAlowIpConfig alowipCfg;
#if PLATFORM_MSTAR	| PLATFORM_HI35XX
	AlarmClockConfig clockSetting;//闹钟设置
#endif	
}SystemConfig;

// media config

#define TIME_FORMAT_MAX_LEN 32

typedef struct
{
	char format[TIME_FORMAT_MAX_LEN];
}TimeFormat;

typedef struct
{
	int posX;//当posType=POSITION_TYPE_BY_FOUR_CORNER时，只能是0/1/2。否则为比例0-100%
	int posY;//当posType=POSITION_TYPE_BY_FOUR_CORNER时，只能是0/1/2。否则为比例0-100%
	TimeFormat timeFormat;
	Positiontype posType;
}TimeOverlay;

#define TITLE_MAX_LEN 200
typedef struct
{
	int posX;	//当posType=POSITION_TYPE_BY_FOUR_CORNER时，只能是0/1/2。否则为比例0-100%
	int posY;	//当posType=POSITION_TYPE_BY_FOUR_CORNER时，只能是0/1/2。否则为比例0-100%
	char title_utf8[TITLE_MAX_LEN];//当titleType=BMP时，这里存放BMP路径
	Positiontype posType;
	Titletype titleType; 	
}TitleOverlay;

typedef struct
{
	int enable;
	int transparency; //titleFormatEn。叠加信息
	TimeOverlay timeOverlay;
	TitleOverlay titleOverlay;
	short style;	//AjOsdOverlayStyle
	short bDsplayWeek;	//0 1 是否显示星期几
	short bOverlayFps;	
	short fontsize; // 0 标准  1 大字体 2 超大字体
	short real_transparency; //透明度。0-100
	short time24or12;// 0: 24 1: 12
}VideoOverlay;


typedef	struct
{
	int enable;
	
	int pos_xscale;//画面宽度比例位置0-100
	int pos_yscale;//画面高度比例位置0-100

	int	color_front;	//前景颜色
	int color_back;		//背景颜色
	
	short transparency; //透明度。0-100
	short fontsize; // 0 标准  1 大字体 2 超大字体
	char title_utf8[TITLE_MAX_LEN];//当titleType=BMP时，这里存放BMP路径
	Titletype titleType; 	
}UserOSD;

#define MAX_USER_OSD_NUM 5

typedef	struct
{
	UserOSD data[MAX_USER_OSD_NUM];
}VideoUserOverlay;

#define RESOLUTION_NAME_MAX_LEN 32

typedef struct
{
	char name[RESOLUTION_NAME_MAX_LEN];
}Resolution;

#define VIDEO_ENCODE_FORAMT_MAX_LEN 32
typedef struct
{
	char name[VIDEO_ENCODE_FORAMT_MAX_LEN];
}VideoEncodeFormat;


#define BITRATE_CONTROL_MAX_LEN 32
typedef struct
{
	char name[BITRATE_CONTROL_MAX_LEN];
}BitRateControl;

typedef enum
{
	VIDEO_QUALITY_CUSTOM = 0,//自定义
	VIDEO_QUALITY_WORSER = 1, //更差
	VIDEO_QUALITY_WORSE = 2,	 //较差
	VIDEO_QUALITY_NORMAL = 3,    //正常
	VIDEO_QUALITY_GOOD = 4,		 //好
	VIDEO_QUALITY_BEST = 5,      //更好
}VideoQualityEnum;

typedef struct 
{
	int enable;
	int streamID;
	Resolution   resolution;
	VideoEncodeFormat encodeFormat;
	BitRateControl bitRateControl;
	int initQuant;
	int bitRate;
	int frameRate; 
	int display_frameRate; 

#if (PLATFORM_AMBAR_S2L|PLATFORM_AMBAR_S2LM | PLATFORM_AMBAR_S2LM2)
	LbrControl lbrConfig;
#endif	

	VideoQualityEnum bitRateQuality;
}VideoEncodeCfg;

typedef struct
{
	int enable;
	int stream;
	int quality;
	int framerate;
}JpegEncodeCfg;

#define VIDEO_NUMBER_MAX 3 //定义视频流数量

typedef struct
{
	VideoEncodeCfg encodeCfg[VIDEO_NUMBER_MAX];
	//encode profile, 0: default, 1: baseprofile
	int encode_profile;
	int disable_private_data; //0: enalbe, 1: disable
#if (PLATFORM_AMBAR_S2L|PLATFORM_AMBAR_S2LM | PLATFORM_AMBAR_S2LM2)
	int encode_mode;//0,4
	int noice_level;//0-10
#endif	
}VideoEncode;

#define VIDEO_FORMAT_MAX_LEN 32

typedef struct
{
	char name[VIDEO_FORMAT_MAX_LEN];
}VideoFormat;

typedef struct
{
	short shutter_mode_day;//0-1	//快门模式:自动/手动
	short shutter_mode_night;//0-1	//快门模式:自动/手动
	short shutter_speed_day;//10-10000	//快门速度
	short shutter_speed_night;//10-10000 //快门速度
}VideoShutter;

typedef enum
{
	IRCUT_Mode_Active = 0, //主动模式/自动模式, ISP自动判断SENSOR增益，控制IRCUT和灯板
	IRCUT_Mode_DayNight =1, //日夜模式，根据时间段来控制IRCUT和图像彩转灰
	IRCUT_Mode_Passive = 2,	//被动模式/外部控制，根据灯板的光敏电阻给的硬件信号，来控制IRCUT
	IRCUT_Mode_Manual = 3,	//手动模式，不根据灯板和SENSOR增益，由调用者来手动切换
	IRCUT_Mode_ReversePassive =	4, //反向被动模式
}IRCutMode;

typedef enum
{
	LED_PURE_INFRAED = 0, //纯红外
	LED_PURE_WHITE = 1, //纯白光
	LED_INFRAED_THEN_WHITE = 2, //红外触发时白光
	LED_WHITE_THEN_INFRAED = 3, //白光触发时红外
}LedMode;

typedef enum
{
	LED_IMAGE_NORMAL = 0,// 正常 
	LED_IMAGE_FACE_EXPOSURE_PREVENTION =1,//防人脸过曝 
	LED_IMAGE_CHEPAI_MODE=2,//: 照车牌模式
}LedImageMode;//补光图像模式

typedef enum
{
	IRCUT_OPENLED_ON_ILLUMINATION_0_01 = 0,// 0.01 
	IRCUT_OPENLED_ON_ILLUMINATION_0_05 = 1,// 0.05
	IRCUT_OPENLED_ON_ILLUMINATION_0_08 = 2,// 0.08 
	IRCUT_OPENLED_ON_ILLUMINATION_0_10 = 3,// 0.10 
	IRCUT_OPENLED_ON_ILLUMINATION_0_20 = 4,// 0.20 
	IRCUT_OPENLED_ON_ILLUMINATION_0_30 = 5,// 0.30 
}IrcutOpenLedOnIllum;

typedef enum
{
	VIDEO_ISP_MODE_NORMAL = 0,// 正常模式 
	VIDEO_ISP_MODE_FORCE_FRAMERATE =1,//强制帧率模式 
	VIDEO_ISP_MODE_SUPERSTAR=2,//超星光模式
}VideoEncodeMode;

typedef struct
{
	int brightness;
	int contrast;
	int saturation;
	int sharpness;
	
	unsigned char tvsystem; // 0: NTSC (60HZ) 1: PAL (50HZ)
	unsigned char forct_antiflicker;
	short reserved;
	unsigned short cropxpix;	//X轴裁剪像素
	unsigned short cropypix;	//Y轴裁剪像素

	int hflip;		//水平翻转 0 1 
	int vflip;		//垂直翻转 0 1 
	int rotate;		//走廊模式 0 1 

	int whitebalance;	//enable R G B 4个值组合 
						//enable=(whitebalance>>24)&0xff; R=(whitebalance>>16)&0xff; G=(whitebalance>>8)&0xff; B=(whitebalance)&0xff
	int backlight;		//背光(逆光补偿0-255)
	int HLC;	//强光抑制 //0-255
	int tnf;	//2d降噪 //0-255
	int snf;	//3D降噪 //0-255

	////增益配置////	
	int bManualGain;//0: 自动增益 1: 手动增益
	int gainValue;//手动增益值

	////////宽动态////////
	int wdr_mode; //0: disable, 1: always open, 2: open at work time
	DayTimeSpan wdr_worktime;
	int wdr_value;

	////////去雾////////
	int dfrog_flag;
	int dfrog_value;

	////////电子快门////////
	VideoShutter shutterSetting;

	//图像ISP效果选项
	int isp_mode_color;	//ISP彩色模式 0-3
	int isp_mode_night; //ISP夜间模式 0-3

	//图像模式
	int videoEncodeMode; //VideoEncodeMode

	////////IRCUT与补光相关////////	
	IRCutMode ircut_mode; 
	unsigned char ircut_sensitivity; //0 to 100 //未用到
	unsigned char ircut_openled_delay;//补光延时 //IrcutOpenLedOnIllum
	unsigned char led_brightness_mode;	//补光亮度控制:0自动 1手动
	unsigned char led_brightness_value;//补光亮度:10%-100%
	DayTimeSpan ircut_nighttime;
	int ircut_keepcolor; //20120419
	LedMode led_mode;
	LedImageMode ispadvmode;//0: 正常 1: 防人脸过曝 2: 照车牌模式
	////////IRCUT与补光相关////////	
}VideoCapture;

#define MAX_VIDEO_MASK_AREA 4
typedef struct
{
	int xPos;
	int yPos;
	int width;
	int height;
}MASK_AREA_ENTRY;

typedef struct
{
	MASK_AREA_ENTRY mainStreamMaskList[MAX_VIDEO_MASK_AREA];
	MASK_AREA_ENTRY subStreamMaskList[MAX_VIDEO_MASK_AREA];
}VideoMaskConfig;


#define MAX_VIDEO_ROI_AREA 4
typedef struct
{
	int xPos;
	int yPos;
	int width;
	int height;
}ROI_AREA_ENTRY;

typedef struct
{
	int enable;
	ROI_AREA_ENTRY roi[MAX_VIDEO_ROI_AREA];
}VideoROI;

typedef struct
{
	VideoCapture   videoCapture;
	VideoEncode    videoEncode;
	JpegEncodeCfg  jpegCfg;
	VideoOverlay   overlay;
	VideoMaskConfig videoMask;
	VideoROI		roiCfg;
	VideoUserOverlay   useroverlay;	
}VideoConfig;

typedef struct
{
	int channels;
	int bitspersample;
	int samplerate;
	short volume_capture;
	short volume_play;
	int amplify;	//是否需要内部功放
	int ra_answer;//反向音频是否需要按键接听
}AudioCapture;
#define AUDIO_ENCODE_TYPE_MAX_LEN 32

typedef struct
{
	char typeName[AUDIO_ENCODE_TYPE_MAX_LEN];
}AudioEncodeType;

typedef struct
{
	int enable;
	int sampleRate;
	AudioEncodeType   audioEncodeType;
	int bitRate;
}AudioEncode;

typedef struct
{
	AudioCapture  audioCapture;
	AudioEncode   audioEncode;
}AudioConfig;

typedef struct
{
	VideoConfig   videoConfig;
	AudioConfig   audioConfig;
}MediaConfig;

typedef struct
{
	int index;
	char  serverIP[MAX_IP_NAME_LEN];
	int serverPort;
	char userName[FTP_NAME_MAX_LEN];
	char password[FTP_PASSWORD_MAX_LEN];
	char filePath[FTP_PATH_MAX_LEN];
	int fileSize;
}FtpServer;

typedef struct
{
	int index;
	char toMail[SMTP_ACCOUNT_MAX_LEN];
	char ccMail[SMTP_ACCOUNT_MAX_LEN];
	char subject[SMTP_SUBJECT_MAX_LEN];
}SmtpServer;

enum
{
	//FTP_INDEX_FOR_RECORD_UPLOAD,
	FTP_INDEX_FOR_ALARM_UPLOAD,
	FTP_INDEX_FOR_LOG_BACKUP,
	FTP_INDEX_FOR_CONFIG_BACKUP,
	FTP_INDEX_FOR_UPDATE,
	FTP_SERVER_COUNT
};

enum
{
	//SMTP_INDEX_FOR_RECORD_UPLOAD,
	SMTP_INDEX_FOR_ALARM_UPLOAD,
	SMTP_INDEX_FOR_LOG_BACKUP,
	SMTP_INDEX_FOR_CONFIG_BACKUP,
	SMTP_SERVER_COUNT
};

//#define FTP_SERVER_COUNT 5

typedef struct
{
	FtpServer ftpServers[FTP_SERVER_COUNT];
}FtpServerList;

//#define SMTP_SERVER_COUNT 4

typedef struct
{
	char  	   		serverIP[MAX_IP_NAME_LEN];
	unsigned  int 	serverPort;
	int             auth;
	char 			userName[SMTP_NAME_MAX_LEN];
	char 			password[SMTP_PASSWORD_MAX_LEN];
	char 			fromMail[SMTP_ACCOUNT_MAX_LEN];
	SmtpServer 		smtpServers[SMTP_SERVER_COUNT];
}SmtpServerList;

typedef struct
{
	FtpServer  ftpServers[FTP_SERVER_COUNT];
	SmtpServerList   smtpServers;
}ServerConfig;

typedef struct
{
	short enable;		
	unsigned short Port;
	unsigned int Ip;
}MulticastStruct;	

typedef struct
{
	int enable_onvif;
	int enable_web;
	int onvif_auth;
	int webPort;
}WebConfig;

typedef struct
{
	int enable;
	short port;
	short auth;
}HikConfig;

#define MAX_RTMP_APP_NAME_LEN 64
#define MAX_RTMP_STREAMID_LEN 256

typedef struct
{
	int enable;
	char server[MAX_IP_NAME_LEN];
	short port;
	short streamno;// 0: main 1: sub 2: third
	char appname[MAX_RTMP_APP_NAME_LEN];
	char streamid[MAX_RTMP_STREAMID_LEN];
	int type;//reserve
}RtmpConfig;

typedef struct
{
	int enable_rtsp;
	int rtsp_auth;
	int rtpoverrtsp;
	int videoPort;
}RtspConfig;

typedef struct
{
	int enable;
	int ptzPort;
}CommConfig;


typedef struct
{
	MulticastStruct StreamMulticast[2];	
}MulticastConfig;

typedef struct
{
	RtspConfig rtspConfig;
	CommConfig commConfig;
	WebConfig webConfig;
	HikConfig hikConfig;
	RtmpConfig rtmpConfig;
	MulticastConfig multicastConfig;
}MediaStreamConfig;


typedef struct
{
	int			enable;
	char			server[MAX_IP_NAME_LEN];
	unsigned short  port;
	char username[ACCOUNT_NAME_MAX_LEN];
	char password[ACCOUNT_PASSWORD_MAX_LEN];
	unsigned short PlayTone;//启动播放声音
	unsigned char InRingTimes;//来电振铃次数后自动接通
	unsigned char reserved;
}VmPlatformConfig;

typedef struct
{
	VmPlatformConfig cfg;
}PlatformConfig;

#define PLATFORM_REGISGER_RESULT_FILE "/tmp/flag_plat_reg_result"
typedef enum
{
	PLAT_REG_RESULT_REG_NO = 0,
	PLAT_REG_RESULT_REGGING,
	PLAT_REG_RESULT_REG_FAILED,
	PLAT_REG_RESULT_REG_OK,
}PlatRegStatus;
typedef struct
{
	PlatRegStatus result;	//0: 未注册 1: 注册中 2: 注册失败 3: 注册成功
	char szPlatDevId[32];
	char reservedInfo[128];
}PlatRegResult;

#define GB28181_ID_MAX_LEN 32
#define GB28181_IP_MAX_LEN 32
#define GB28181_NAME_MAX_LEN 64
#define GB28181_PWD_MAX_LEN 32
typedef struct
{
	int enable;
	int nstreams;		//可用码流:0 主码流 1子码流 2 双码流
	int nChannelNum;	//通道数量
	
	char hcId[GB28181_ID_MAX_LEN];
	char hcIp[GB28181_IP_MAX_LEN];
	char hcName[GB28181_NAME_MAX_LEN];
	char hcPwd[GB28181_PWD_MAX_LEN];
	int hcPort;
	
	char lcId[GB28181_ID_MAX_LEN];
	char lcName[GB28181_NAME_MAX_LEN];
	char lcPwd[GB28181_PWD_MAX_LEN];
	int lcPort;
	
	char camId[GB28181_ID_MAX_LEN];
	char alarmId[GB28181_ID_MAX_LEN];
}GB28181Config;


#define RECORD_FILEFORMAT_MAX_LEN 32

typedef struct
{
	char formatName[RECORD_FILEFORMAT_MAX_LEN];
}RecordFileFormat;

#define RECORD_MEDIA_TYPE_MAX_LEN 32
typedef struct
{
	char typeName[RECORD_MEDIA_TYPE_MAX_LEN];
}RecordMediaType;

#define RECORD_STORAGE_POLICY_MAX_LEN 32
typedef struct
{
	char policyName[RECORD_STORAGE_POLICY_MAX_LEN];
}RecordStoragePolicy;


#define MAX_DAYTIMESPAN_COUNT 24

typedef struct
{
	int workday;
	int timeSpancnt;
	DayTimeSpan timeSpans[MAX_DAYTIMESPAN_COUNT];
}WorkDayTime;


#define MAX_WORDDAYTIME_COUNT 8
typedef struct
{
	int workdayCnt;
	WorkDayTime workdayTimes[MAX_WORDDAYTIME_COUNT];
}TimeSpanList;

#define MAX_STORAGE_SEQUENCE_NAME_LEN 256
#define MAX_REMOTE_MOUNT_PARAM_LEN 256

typedef enum
{
	NETWORK_STORAGE_DISABLE=0,
	NETWORK_STORAGE_TYPE_NFS=1,	
}NetworkStorageType;

typedef struct
{
	int		localEnable;
	NetworkStorageType		remoteEnable;
	char    storageSequence[MAX_STORAGE_SEQUENCE_NAME_LEN];
	char    mountParam[MAX_REMOTE_MOUNT_PARAM_LEN];	
	RecordStoragePolicy storePolicy;	
	short timelapseEnable;
	short recordFileSize;
	int recordFileKeeyDays;//录像保留最大天数
}RecordCommConfig;

typedef struct
{
	int					stream; // 0:主码流 1:子码流 2:主子码流同时录像
	RecordFileFormat	fileFormat;
	RecordMediaType		mediaType;
	int					localStore;
	int					remoteStore;
	TimeSpanList		timeSpanList;
	
	int 				jpgInterval;
	int					ftpUpload;
	int					emailUpload;
}ScheduleRecordConfig;

typedef struct
{
	int					stream; // 0:主码流 1:子码流 2:主子码流同时录像
	RecordFileFormat	fileFormat;
	RecordMediaType		mediaType;
	int 				precordTime;
	int 				recordTime;
	int					localStore;
	int					remoteStore;
	int					ftpUpload;
	int					emailUpload;
	int 				stopNoAlarm; //if noalarmstop=1, recordTime means the record time after alarm disappear
}AlarmRecordConfig;

typedef struct
{
	short preTakeTime;
	short sendoutInterval;//FTP/EMAIL发送频率,秒.0标识不控制
	int totalTakeTime;
	int localStore;
	int remoteStore;
	int ftpUpload;
	int emailUpload;
	int 	stopNoAlarm; //if noalarmstop=1, recordTime means the record time after alarm disappear
	int	stream;	//抓图使用的码流
}AlarmCaptureConfig;

typedef struct
{
	RecordCommConfig		commonCfg;
	ScheduleRecordConfig	scheduleRecordCfg;
	AlarmRecordConfig   motionRecordCfg;
	AlarmCaptureConfig  motionCaptureCfg;
	AlarmRecordConfig   inputAlarmRecordCfg;
	AlarmCaptureConfig  inputAlarmCaptureCfg;
	//20120904
	AlarmRecordConfig   linkdownRecordCfg;
	AlarmCaptureConfig  linkdownCaptureCfg;	
}RecordConfig;

#define MAX_PTZPOSITION_COUNT 20

typedef struct
{
	int positionIndex;
}PTZPosition;

typedef struct
{
	PTZPosition  postion;
}PositionPreset;

typedef struct
{
	int interval;
	int duration;
	int positionCount;
	PTZPosition ptzPositions[MAX_PTZPOSITION_COUNT];
}PositionLoop;

typedef struct
{
	int interval;
	int walkCount;
	int positionCount;
	PTZPosition ptzPositions[MAX_PTZPOSITION_COUNT];
}PositionWalk;

#define PTZ_ACTION_TYPE_MAX_LEN 32
typedef  struct
{
	char actionName[PTZ_ACTION_TYPE_MAX_LEN];
}PTZActionType;

typedef struct
{
	int enable;
	PTZActionType actionType;	
	union Action
	{
		PositionPreset preset;
		PositionLoop  loop;
		PositionWalk  walk;
	}action;
}PTZAction;


#define TRIGGER_TYPE_NAME_MAX_LEN 32
typedef struct
{
	char name[TRIGGER_TYPE_NAME_MAX_LEN];
}TriggerType;

#define CHANNEL_TYPE_NAME_MAX_LEN 32
typedef struct
{
	char name[CHANNEL_TYPE_NAME_MAX_LEN];
}ChannelType;

typedef struct
{
	int enable;
	int portIndex;
	ChannelType channelType;
	TriggerType triggerType;
	int duration;
}AlarmOutputAction;

#define AUDIO_ACTION_LEN_FILENAME 128
typedef struct
{
	int enable;
	int times;//播放次数 <=0表示一直播。大于0表示次数
	char filename[AUDIO_ACTION_LEN_FILENAME];
}AudioPlayAction;


typedef struct
{
	AlarmOutputAction outputAction;
	PTZAction         ptzAction;
	AudioPlayAction	  audioAction;
}InputAction;

typedef struct
{
	int enable;
	int portIndex;
	ChannelType		channelType;	
	TriggerType		triggerType;
	TimeSpanList	timeSpanList;
	InputAction		alarmAction;
}AlarmChannel;

#define MAX_ALARMCHANNEL_COUNT 4

typedef struct
{
	int channelCnt;
	AlarmChannel alarmChannels[MAX_ALARMCHANNEL_COUNT];
}InputAlarm;

typedef struct
{
	AlarmOutputAction outputAction;
	PTZAction         ptzAction;
	AudioPlayAction	  audioAction;
}MotionDetectAction;

typedef struct
{
	int enable_babycry;//婴儿哭声
	int sensity_babycry;
	int enable_lsd;	//高分贝声音
	int sensity_lsd;	
}AudioAlarm;

#define PLATFORM_DM365						0



#if (PLATFORM_DM365)
#define MD_MAX_GRID_ROW 4
#define MD_MAX_GRID_COL 4
#define MD_CONFIG_STRING_LEN (MD_MAX_GRID_ROW*MD_MAX_GRID_COL+4)
#define MAX_MOTIONDETECT_CONFIG_STRING 32
#else
#define MD_MAX_GRID_ROW 18
#define MD_MAX_GRID_COL 22
#define MD_CONFIG_STRING_LEN (MD_MAX_GRID_ROW*MD_MAX_GRID_COL+4)
#define MAX_MOTIONDETECT_CONFIG_STRING MD_CONFIG_STRING_LEN//32
#endif

//移动侦测组定义，左上、右下角block位置
typedef struct
{
	int left;
	int top;
	int right;
	int bottom;
}MdGroupConfig;

typedef struct
{
	int enable;
	TimeSpanList  timeSpanList;
	int blockCount; // high word for row , low  word for column;
	char blockCfg[MAX_MOTIONDETECT_CONFIG_STRING]; // ''\0' terminate
	int sensitivity;
	int alarmThreshold;
	int dayNightSwitch;
	int nightSensitivity;
	int nightAlarmThreshold;
	DayTimeSpan nightTime;
	MotionDetectAction alarmAction;
}MotionDetectAlarm;


typedef struct
{
	int draw_rect_enable;	//画框
	AlarmOutputAction outputAction;
	AudioPlayAction	  audioAction;
}PdAction;


typedef struct
{
	int xPos;
	int yPos;
	int width;
	int height;
}PD_AREA_ENTRY;

typedef struct
{
	int enable_day;
	int enable_night;
	PD_AREA_ENTRY area;
	PdAction alarmAction;
	TimeSpanList  timeSpanList;
}PdAlarm;


typedef struct
{
	AlarmOutputAction outputAction;
}VideoLostAction;

typedef struct
{
	int enable;
	TimeSpanList  timeSpanList;
	VideoLostAction alarmAction;
}VideoLostAlarm;

typedef struct
{
	AlarmOutputAction outputAction;
}VideoCoverAction;

typedef struct
{
	int enable;
	TimeSpanList		timeSpanList;
	VideoCoverAction	alarmAction;
}VideoCoverAlarm;

typedef struct
{
	AlarmOutputAction outputAction;
}StorageFullAction;

typedef struct
{
	int enable;
	int threshold;
	StorageFullAction alarmAction;
}StorageFullAlarm;

typedef struct
{
	int x0Pos;
	int y0Pos;
	int x1Pos;
	int y1Pos;
}VideoLineStruct;
typedef struct
{
	AlarmOutputAction outputAction;
	AudioPlayAction	  audioAction;
}VideoGateAction;

#define MAX_VIDEO_VG_LINE	2
typedef struct
{
	int enable;
	int sensitivity;
	VideoLineStruct data[MAX_VIDEO_VG_LINE];

	VideoGateAction alarmAction;
	TimeSpanList  timeSpanList;
}VideoGateAlarm;


typedef struct
{
	InputAlarm			inputAlarm;
	MotionDetectAlarm   motionDetectAlarm;
	VideoLostAlarm		videoLostAlarm;  
	VideoCoverAlarm		videoCoverAlarm;
	StorageFullAlarm	storageFullAlarm;
	AudioAlarm 			audioAlarm;
	VideoGateAlarm		vgAlarm;
	PdAlarm				pdAlarm;
}AlarmConfig;

/*
typedef struct
{
	char  IPAddress[MAX_IP_NAME_LEN];
	char  netMask[MAX_IP_NAME_LEN];
	char  gateWay[MAX_IP_NAME_LEN];
	char  DNS1[MAX_IP_NAME_LEN];
	char  DNS2[MAX_IP_NAME_LEN];
}StaticIPConfig;

typedef struct
{
	char reserved[16];
}DHCPConfig;

typedef struct
{
	int enable;
	int interval;
}ADSLAutoConnect;

typedef struct
{
	char IPAddress[MAX_IP_NAME_LEN];
	char netMask[MAX_IP_NAME_LEN];
	char gateWay[MAX_IP_NAME_LEN];
}ADSLLanConfig;

#define ADSL_NAME_MAX_LEN 32
#define ADSL_PASSWORD_MAX_LEN 32

typedef struct
{
	char userName[ADSL_NAME_MAX_LEN];
	char password[ADSL_PASSWORD_MAX_LEN];
	ADSLAutoConnect autoConnect;
	ADSLLanConfig   adslLanCfg;
}ADSLConfig;

*/

#define MAX_WEPENCRYPT_AUTHMODE_NAME_LEN 64
#define MAX_WEPENCRYPT_ENCRYPTTYPE_NAME_LEN 64
#define MAX_WEPENCRYPT_KEYMODE_NAME_LEN 64
#define MAX_WEPENCRYPT_KEYVALUE_LEN 64

#define WEPENCRYPT_AUTHMODE_NAME_OPENSYSTEM "open system"
#define WEPENCRYPT_AUTHMODE_NAME_SHAREDKEY "shared key"

#define WEPENCRYPT_AUTHMODE_VALUE_OPENSYSTEM 0
#define WEPENCRYPT_AUTHMODE_VALUE_SHAREDKEY 1
#define WEPENCRYPT_AUTHMODE_VALUE_UNKNOWN -1

typedef struct
{
   char authMode[MAX_WEPENCRYPT_AUTHMODE_NAME_LEN];
   char encryptType[MAX_WEPENCRYPT_ENCRYPTTYPE_NAME_LEN];
   char keyMode[MAX_WEPENCRYPT_KEYMODE_NAME_LEN];
   char keyValue[MAX_WEPENCRYPT_KEYVALUE_LEN];
   int  keyIndex;
}WepEncrypt;


#define MAX_WPAENCRYPT_ENCRYPTTYPE_NAME_LEN 64
#define MAX_WPAENCRYPT_AUTHMODE_NAME_LEN 64
#define MAX_WPAENCRYPT_KEYVALUE_LEN 64

#define WPAENCRYPT_ENCRYPTTYPE_NAME_TKIP  "tkip"
#define WPAENCRYPT_ENCRYPTTYPE_NAME_AES   "aes"

typedef struct
{
	char encryptType[MAX_WPAENCRYPT_ENCRYPTTYPE_NAME_LEN];
    char authMode[MAX_WPAENCRYPT_AUTHMODE_NAME_LEN];
	char keyValue[MAX_WPAENCRYPT_KEYVALUE_LEN];
}WpaEncrypt;

#define MAX_WIRELESSENCRYPT_ENCRYPTTYPE_NAME_LEN 64
#define WIRELESSENCRYPT_ENCRYPTTYPE_NAME_WEP  "wep"
#define WIRELESSENCRYPT_ENCRYPTTYPE_NAME_WPA  "wpa"


typedef struct
{
	int enable;
	char encryptType[MAX_WIRELESSENCRYPT_ENCRYPTTYPE_NAME_LEN];
	WepEncrypt  wepEncrypt;
	WpaEncrypt  wpaEncrypt;
}WirelessEncrypt;


#define MAC_ADDRESS_LEN   18    //00:11:11:11:11:11
#define MAX_WIRELESS_OPERATIONMODE_NAME_LEN 32
#define MAX_WIRELESS_MACMODE_NAME_LEN 64
#define MAX_WIRELESS_ESSID_NAME_LEN 64
#define MAX_WIRELESS_REGION_NAME_LEN 64
#define MAX_WIRELESS_BITRATE_NAME_LEN 10
#define MAX_WIFI_VERSION_LEN 10

#define WIRELESS_OPERATIONMODE_MASTER_NAME "master"
#define WIRELESS_OPERATIONMODE_MANAGED_NAME "managed"

#define WIRELESS_MACMODE_NAME_A "A"
#define WIRELESS_MACMODE_NAME_B "B"
#define WIRELESS_MACMODE_NAME_G "G"
#define WIRELESS_MACMODE_NAME_MIXED "MIXED"

#define WIRELESS_MACMODE_VALUE_A  4
#define WIRELESS_MACMODE_VALUE_B  3
#define WIRELESS_MACMODE_VALUE_G  2	
#define WIRELESS_MACMODE_VALUE_MIXED  1
#define WIRELESS_MACMODE_VALUE_UNKNOWN   -1


#define WIRELESS_REGION_NAME_TAIWAN "TAIWAN"
#define WIRELESS_REGION_NAME_USA "USA"
#define WIRELESS_REGION_NAME_FRANCE "FRANCE"
#define WIRELESS_REGION_NAME_ISRAEL "ISRAEL"

#define WIRELESS_REGION_VALUE_TAIWAN 2 
#define WIRELESS_REGION_VALUE_USA  1
#define WIRELESS_REGION_VALUE_FRANCE  3	
#define WIRELESS_REGION_VALUE_ISRAEL  5

#define WIRELESS_REGION_VALUE_UNKNOWN  -1

/*
typedef struct
{
	int enable;
	char operationMode[MAX_WIRELESS_OPERATIONMODE_NAME_LEN];
	char macMode[MAX_WIRELESS_MACMODE_NAME_LEN];
	char bitRate[MAX_WIRELESS_BITRATE_NAME_LEN];
	char essid[MAX_WIRELESS_ESSID_NAME_LEN];
	char region[MAX_WIRELESS_REGION_NAME_LEN];
	int channelNum;
	char MACAddress[MAC_ADDRESS_LEN];
	WirelessEncrypt wirelessEncrypt;
}WirelessConfig;

typedef struct
{
	unsigned char MACAddress[MAC_ADDRESS_LEN];
}WireConfig;
*/

#define MAX_DDNS_SERVER_NAME_LEN 256
#define MAX_DDNS_DOMAIN_NAME_LEN 256
#define MAX_DDNS_USERNAME_LEN 256
#define MAX_DDNS_PASSWORD_LEN 256
typedef struct
{
	int enable;
	char server[MAX_DDNS_SERVER_NAME_LEN];
	char domain[MAX_DDNS_DOMAIN_NAME_LEN];
	char userName[MAX_DDNS_USERNAME_LEN];
	char password[MAX_DDNS_PASSWORD_LEN];
	int freshInterval;
}DDNSConfig;

typedef struct
{
	int enable;
}UPNPConfig;

typedef enum
{
	P2P_TYPE_NOTDEFINED = 0,
	P2P_TYPE_DANALE = 1,
	P2P_TYPE_ANKO = 2,
	P2P_TYPE_GOOLINK = 3,
	P2P_TYPE_YUECAM = 4,
	P2P_TYPE_QQCONNECT = 5,
    P2P_TYPE_TUTK = 6,
    P2P_TYPE_EYEPLUS = 7
}P2pType;

typedef struct
{
	int enable;
	int p2ptype;	//
}P2PConfig;


#define ADSL_NAME_MAX_LEN 32
#define ADSL_PASSWORD_MAX_LEN 32

typedef struct
{
	int  enable;
	char userName[ADSL_NAME_MAX_LEN];
	char password[ADSL_PASSWORD_MAX_LEN];
}ADSLConfigNew;

typedef struct
{
	unsigned char   MACAddress[MAC_ADDRESS_LEN];	
	short			dhcpEnable;
	short			onvifAllnetEnable;//是否启用ONVIF全网通
	char  IPAddress[MAX_IP_NAME_LEN];
	char  netMask[MAX_IP_NAME_LEN];
	char  gateWay[MAX_IP_NAME_LEN];
	char  DNS1[MAX_IP_NAME_LEN];
	char  DNS2[MAX_IP_NAME_LEN];
}LANConfig;

typedef struct
{
	int enable;
	char address[MAX_IP_NAME_LEN];
	int interval;
	int maxFail;
}WIFIPingWatchConfig;

typedef struct
{
	int enable;
	char version[MAX_WIFI_VERSION_LEN];
	int	dhcpEnable;
	char IPAddress[MAX_IP_NAME_LEN];
	char netMask[MAX_IP_NAME_LEN];
	char gateWay[MAX_IP_NAME_LEN];	
	char operationMode[MAX_WIRELESS_OPERATIONMODE_NAME_LEN];
	char macMode[MAX_WIRELESS_MACMODE_NAME_LEN];
	char bitRate[MAX_WIRELESS_BITRATE_NAME_LEN];
	char essid[MAX_WIRELESS_ESSID_NAME_LEN];
	char region[MAX_WIRELESS_REGION_NAME_LEN];
	int channelNum;
	char MACAddress[MAC_ADDRESS_LEN];
	WirelessEncrypt wirelessEncrypt;
	WIFIPingWatchConfig pingWatchCfg;	
}WIFIConfig;

typedef struct
{
	int enable;
	char version[MAX_WIFI_VERSION_LEN];
	char IPAddress[MAX_IP_NAME_LEN];
	char netMask[MAX_IP_NAME_LEN];
	char macMode[MAX_WIRELESS_MACMODE_NAME_LEN];
	char bitRate[MAX_WIRELESS_BITRATE_NAME_LEN];
	char essid[MAX_WIRELESS_ESSID_NAME_LEN];
	char region[MAX_WIRELESS_REGION_NAME_LEN];
	int channelNum;
	char MACAddress[MAC_ADDRESS_LEN];
	WirelessEncrypt wirelessEncrypt;
}WIFIApConfig;


#define MAX_DIALNUMBER_LEN  32
#define MAX_APN_STRING		256
#define MAX_CENTER_NUMBER   256 

#if 0

#define DIAL_OPTION_MANUAL		0
#define DIAL_OPTION_CALL		1
#define DIAL_OPTION_AUTO		2

#else  //not same as PVT

#define DIAL_OPTION_AUTO		0
#define DIAL_OPTION_CALL		1

#endif

typedef struct
{
	int enable;
	int type;
	char dialNumber[MAX_DIALNUMBER_LEN];
	char apnString[MAX_APN_STRING];
	char userName[ADSL_NAME_MAX_LEN];
	char password[ADSL_PASSWORD_MAX_LEN];
	int  dialOption; //0: manual dial; 1: call dial; 2: auto
	char centerNumber[MAX_CENTER_NUMBER];
	char SMSC[MAX_CENTER_NUMBER];
}G3Config;

#define MAX_VPN_SERVER_NAME_LEN 256
#define MAX_VPN_USERNAME_LEN 256
#define MAX_VPN_PASSWORD_LEN 256
typedef struct
{
	int enable;
	char vpnServerIp[MAX_VPN_SERVER_NAME_LEN];
	char userName[MAX_VPN_USERNAME_LEN];
	char password[MAX_VPN_PASSWORD_LEN];
	int mtu;
}PPTPConfig;

typedef struct
{
	LANConfig		lanCfg;
	WIFIConfig		wifiCfg;
	ADSLConfigNew	adslCfg;
	DDNSConfig		ddnsCfg;
	UPNPConfig		upnpCfg;
	G3Config		g3Cfg;
	PPTPConfig      pptpCfg; 
	P2PConfig		p2pCfg;
	WIFIApConfig	wifiApCfg;
}NetworkConfigNew;

typedef struct
{
	AlarmConfig  		alarmCfg;
	SystemConfig		systemCfg;
	MediaConfig 		mediaCfg;
	ServerConfig  		serverCfg;
	MediaStreamConfig 	mediaStreamCfg;
	PlatformConfig		platformCfg;
	RecordConfig  		recordCfg;
//	NetworkConfig 		networkCfg;
	NetworkConfigNew	networkCfgNew;
	GB28181Config		gb28181Cfg;
}GlobalConfig;

typedef struct
{
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
}SYSTEM_TIME;

#define MAX_ALARM_DATA 220//128 //32->128 modified by 20090403

typedef struct 
{
	SYSTEM_TIME	alarmtime;
	AjAlarmCode 		alarmcode;
	int 		alarmflag;
	int			alarmlevel;
	char 		alarmdata[MAX_ALARM_DATA];
} ALARM_MSG_DATA;

typedef struct
{
	char username[ACCOUNT_NAME_MAX_LEN];
	char password[ACCOUNT_PASSWORD_MAX_LEN];
}LOGIN_MSG_DATA;

typedef struct
{
	char username[ACCOUNT_NAME_MAX_LEN];
	char password[ACCOUNT_PASSWORD_MAX_LEN];
	char vendorid[ACCOUNT_PASSWORD_MAX_LEN];
	int	 authmethod;
}LOGIN_MSG_DATA_EX;

typedef struct
{
	char filePath[256];
}CONFIG_UPDATE_MSG_DATA;

typedef struct
{
	char filePath[256];
	unsigned int nPhyAddr;
	unsigned int nFileLen;
}APPBIN_UPDATE_MSG_DATA;

typedef struct
{
	char path[256];
	char fileName[256];
}FIRMWARE_FTP_UPDATE_MSG_DATA;

typedef struct
{
	char fileName[256];
}RECORD_REMOVE_MSG_DATA;

typedef struct
{
	int mounted;
	int total;
	int used;
	int free;
	int percent;
}SDCARD_INFO_DATA;

typedef struct
{
	int mounted;
	int total;
	int used;
	int free;
	int percent;
}USB_INFO_DATA;

typedef struct
{
	int mounted;
	int total;
	int used;
	int free;
	int percent;
}NETWORK_INFO_DATA;

typedef struct
{
	SDCARD_INFO_DATA  sd1;
	SDCARD_INFO_DATA  sd2;
	USB_INFO_DATA     usb;
	NETWORK_INFO_DATA     network;
}STORAGE_INFO_DATA;

typedef struct 
{
	char kernelVersion[256];
	char fsVersion[256];
}SYSTEM_VERSION_DATA;

typedef struct
{
	struct tm time;
	int tz;
	int manual;	//手动时不考虑夏令时。自动校时考虑夏令时
}TIMESET_MSG_DATA;

typedef struct
{
	int doBridge;

	char wireMac[256];	

	char ipType[256];
	char ip[256];
	char gateway[256];
	char netmask[256];
	char dns1[256];
	int  dns1Exsit;
	char dns2[256];
	int dns2Exsit;

	int isWirelessUp;
	//20130626
	char wirelessIp[256];
	char wirelessGateway[256];
	char wirelessNetmask[256];
	//20130626 end
	char wirelessMac[256];
	char essid[256];	
	char operationMode[256];
	char bitRate[256];
	char freq[256];
	char accessPoint[256];
	char encryptType[256];

	unsigned char linkquality;
	//unsigned char signallevel;
	//unsigned char noise;
	int signallevel;
	int noise;

	int cloudLogined;
	char cloudId[128];
	int cloudEnable;
	int cloudType;	
	//add end
}NETWORK_STATUS_DATA;

#define MAX_WIFI_AP_CNT  50 

#define WIFI_AUTH_OPEN   		0
#define WIFI_AUTH_SHARED   		1
#define WIFI_AUTH_WPAPSK   		2
#define WIFI_AUTH_WPA2PSK   	3
#define WIFI_AUTH_UNSPPORT   	4

#define WIFI_ENCRYP_NONE  		0
#define WIFI_ENCRYP_WEP   		1
#define WIFI_ENCRYP_TKIP  		2
#define WIFI_ENCRYP_AES   		3
#define WIFI_ENCRYP_UNSPPORT 	4

typedef struct
{
	char ssid[128];
	char wirelessMode[64];
	int authMode;
	int encryType; 
	int quality;
	int signalLevel;
	int noiseLevel;
#if PLATFORM_MSTAR
	int channelnum;
#endif
}WIFI_AP_INFO;

typedef struct
{
	int apCnt;
	WIFI_AP_INFO  apInfos[MAX_WIFI_AP_CNT];	
}WIFI_AP_SCAN;


typedef enum
{
	SHOWDATA_PRINTF,
	SHOWDATA_DEBUG,
	SHOWDATA_NONE,
}ShowDataType;


#endif

