#ifndef _____AJ_MEDIA_CFG_H____
#define _____AJ_MEDIA_CFG_H____

#include <time.h>

#if defined (__cplusplus)
extern "C" {
#endif


//move there by XXX 20111220
typedef struct
{
	int hour;
	int minute;
	int sec;
}DayTime;

typedef struct
{
	DayTime startTime;
	DayTime endTime;
}DayTimeSpan;


typedef	enum
{
	CONFIG_LANGUAGE_CN = 0,
	CONFIG_LANGUAGE_HK,
	CONFIG_LANGUAGE_TW,
	CONFIG_LANGUAGE_EN
}CONFIG_LANGUAGE;


typedef enum
{
	TITLE_ADD_NOTHING = 0,
	TITLE_ADD_RESOLUTION,
	TITLE_ADD_BITRATE,
	TITLE_ADD_RESOLUTION_AND_BITRATE
}titleFormatEn;

typedef enum
{
	POSITION_TYPE_BY_FOUR_CORNER = 0,//�Ľ�
	POSITION_TYPE_BY_SCALE = 1,	//�������(0-100)
	POSITION_TYPE_DISABLE = 2,
}Positiontype;

typedef enum
{
	TYPE_TYPE_BY_TEXT = 0,//�ı�
	TYPE_TYPE_BY_BMP = 1,	//BMPͼƬ
}Titletype;


typedef	enum
{
	OSD_POINT_LEFT_TOP = 0,
	OSD_POINT_LEFT_BOTTOM,
	OSD_POINT_RIGHT_TOP,
	OSD_POINT_RIGHT_BOTTOM,
	OSD_POINT_HIDE, 
}PositionByCornerEnum;

typedef struct
{
	unsigned short xScale;//X����λ��,������������ı���(0-100)
	unsigned short yScale;//Y����λ��,������������ı���(0-100)
}PositionByScaleStruct;

typedef struct
{
	Positiontype type;
	union
	{
		PositionByCornerEnum value1;	
		PositionByScaleStruct value2;
	};
}osdPointEn;

typedef struct tag_encode_resolution
{
	char *res_name;	//�ֱ��ʡ����ķֱ���ѡ��ʱ������ֵ��Ҫ���ݱ��ṹ��Ӧ����
	char *codec_name;	//�����ʽ����ǰֻ��H.264
	int  stream_type; //0: main, 1: sub, 2: third
	int  def_bitrate; //kbps
	int  min_bitrate; //kbps
	int  max_bitrate; //kbps
	int  def_framerate;	//Ĭ��֡��
	int  min_framerate;	//��С֡��
	int  max_framerate;	//ʵ�����֡��(ʵ����Ч��֡�ʿ�ѡֵֻ������2��֮��)
	int  dual_stream; //if stream_type == 0, dual_stream == 0, means disable sub stream
	int  def_config;  //if def_config = 1, use for default config of video encode
	int  max_display_framerate;	//��ʾ���֡��(֡�ʿ�ѡֵֻ������2��֮��)
}RESOLUTION_ENTRY;

typedef struct tag_audio_mode
{
	char *codec_name;		//���뷽ʽ
	int  channels;			//ͨ����
	int  bitspersample;	//������
	int  samplerate;		//������
	int  bitrate;			//������
	int  def_config;		//�Ƿ�Ĭ������
}AUDIO_CODEC_ENTRY;

typedef struct
{
	int lbr_enable;	
	int lbr_style;		//������ģʽ:	0: ����֡��,�Զ�����	1: ��Ƶ��������,�Զ���֡
	int lbr_bitratemode;//���ʿ���:	0: �Զ� 1:�ֶ�
	int lbr_bitrate;		//������Ŀ��ֵ
	int lbr_motionlevel;	//�˶�����:	0: ��ֹ 1:�˶�����С 2:�˶����ȴ�
	int lbr_noicelevel;	//��㼶��:	0: �� 1:�� 2:��
}LbrControl;


typedef enum
{
	PRODUCT_TYPE_BASE = 0,
	PRODUCT_TYPE_HC100A,	//3518e+9712
	PRODUCT_TYPE_HC130A,	//3518e+0130
	PRODUCT_TYPE_HC100B,	//3518e+1004
	PRODUCT_TYPE_HW100A,	//3518e+9712+WIFI
	PRODUCT_TYPE_HC130B,	//3518c+0130 128M
	PRODUCT_TYPE_HW130B,//3518c+0130+WIFI 128M
	PRODUCT_TYPE_HC200A,	//3516c+imx222 128M
	PRODUCT_TYPE_HW130C,//3518c+0130 ����
	PRODUCT_TYPE_HW130D,	//3518c+0130 128M
	PRODUCT_TYPE_HC100C,	//3518e+1041
	PRODUCT_TYPE_HC200B,
	PRODUCT_TYPE_HC200C,
	PRODUCT_TYPE_HC_SD,	//ģ��������룬��934SD��ͬ
	PRODUCT_TYPE_HC200D,

	
	PRODUCT_TYPE_HC200C2 = 0x0101,	//3516cv300+imx323

	PRODUCT_TYPE_T934HD = 0x1001,		//3518c+9712 256M
	PRODUCT_TYPE_T9343G,				//3518c+0130 256M
	PRODUCT_TYPE_T934SD,				//3518c sd 256M

	PRODUCT_TYPE_HC400L = 0x1101,//3516d+ov4689
	PRODUCT_TYPE_HE1601,			 //3516A sd 256M	
	PRODUCT_TYPE_HC200F,				//3516d+IMX290
	PRODUCT_TYPE_HC500F,			//3516D+326
	PRODUCT_TYPE_HC500L,
	PRODUCT_TYPE_HY200F,			//3516d+IMX290,��ƶ���

	PRODUCT_TYPE_HC800L = 0x1181,//3516AV200+IMX274

	
	PRODUCT_TYPE_HE200A = 0x1201,//3518ev200+2035
	PRODUCT_TYPE_HC130E = 0x1202,//3518ev200+sc1135
	PRODUCT_TYPE_HF130E = 0x1203, //ͬ�ϣ����ڶ���

	PRODUCT_TYPE_TC130A = 0x2001,		//dm365+ar0130
	PRODUCT_TYPE_TC200A,				//dm365+imx222

	PRODUCT_TYPE_HT100A = 0x9001,		//3518c+9712 128M
	PRODUCT_TYPE_HT100W,		//3518c+9712 128M

	PRODUCT_TYPE_AC400A = 0xa001,		//S2L66+ov4689
	PRODUCT_TYPE_AC500A,				//S2L66+ov5658
	PRODUCT_TYPE_AC400L,				//S2Lm+ov4689
	PRODUCT_TYPE_AC100L,				//S2Lm+ar0141
	PRODUCT_TYPE_AC400F,				//S2Lm+ov4689 FULL FUNCTION
	PRODUCT_TYPE_AC500F,				//S2Lm+ov5658
	PRODUCT_TYPE_AC200F,				//S2Lm+imx291
	PRODUCT_TYPE_AC200L,				//S2L33m+imx291, no IO INPUT and OUTPUT

//MSTAR 316D
	PRODUCT_TYPE_MC200A = 0xb001,		//ov2710
	PRODUCT_TYPE_MY200A,
	PRODUCT_TYPE_MC200D,					//ar0237
	PRODUCT_TYPE_MY200D,
	PRODUCT_TYPE_MC200T,					//ar0237 or imx323

	PRODUCT_TYPE_MC200C,	//				Imx323
	PRODUCT_TYPE_MY200C,
	
//MSTAR 313e
	PRODUCT_TYPE_MC200E = 0xb011, //313e+2135
	PRODUCT_TYPE_MC200C2,			//313e+323
	PRODUCT_TYPE_MY200E,			//313e+2135
	PRODUCT_TYPE_MC200P,			//313E+PS5230
	PRODUCT_TYPE_MH130E,  //313e+2135
	PRODUCT_TYPE_MY200P,           //313e+ps5230
	
	PRODUCT_TYPE_TMA01, 				//313E����Ƶ
	PRODUCT_TYPE_MC200E2,              //313e+2235
	PRODUCT_TYPE_MC200G,           //313E+BG0816
	PRODUCT_TYPE_MC200J,            //313e+imx307
	PRODUCT_TYPE_MC200E3,              //313E+SC2232
	PRODUCT_TYPE_MW200E,               //313E+SC2232 ���ζ���

	PRODUCT_TYPE_MC200J2,
	PRODUCT_TYPE_FB200E,			  //313e+2235��������
	PRODUCT_TYPE_MC200E8,			   //313E+2310
	

	
//MSTAR 316dm
	PRODUCT_TYPE_MC400L = 0xb101,	//316DM+ov4689
	PRODUCT_TYPE_MC400L2,			//316DM+sc4236
	PRODUCT_TYPE_MT200E,			//316DM + sc2235
	PRODUCT_TYPE_MW400L,             //316DM+OV4689, ���ڶ���
	PRODUCT_TYPE_MS400L,			//MC400L ����ʶ��
	PRODUCT_TYPE_MJ400L,			//316DM+OV4689, ���޶���

	PRODUCT_TYPE_FH200S=0xb201,	//PRODUCT_TYPE_MC200E�����
	PRODUCT_TYPE_MHC30B2,			//PRODUCT_TYPE_MC200E2�����
	PRODUCT_TYPE_MHC30D2,			//PRODUCT_TYPE_MC200J�����
	PRODUCT_TYPE_MHC31C4,			//PRODUCT_TYPE_MC400L�����
	PRODUCT_TYPE_HB942_20C,		//MC200E2: HB-IPC942-20C
	PRODUCT_TYPE_HB942_20D,		//MT200E: HB-IPC942-20D
	PRODUCT_TYPE_HB942_20X, 	//MC200J2: HB-IPC942-20x
	PRODUCT_TYPE_HMC30H2,	//MC200J2
	PRODUCT_TYPE_HB500N,			//PRODUCT_TYPE_MC200J�����
	
	PRODUCT_TYPE_SIMULATOR = 0xffff,
}AjProductType;

typedef enum
{
	SENSOR_TYPE_OV2710 = 101,
	SENSOR_TYPE_OV4689,
	
	SENSOR_TYPE_SC2135 = 201,
	SENSOR_TYPE_SC2235,
	SENSOR_TYPE_SC2232,	
	SENSOR_TYPE_SC2310, 
	SENSOR_TYPE_SC4236,
	
	SENSOR_TYPE_IMX323 = 301,
	SENSOR_TYPE_IMX307,
	SENSOR_TYPE_IMX307_2,
	
	SENSOR_TYPE_AR0237 = 401,
	SENSOR_TYPE_BG0806,
	SENSOR_TYPE_PS5230,		
}AjSensorType;

typedef enum
{
	DSP_TYPE_TI_DM368 = 900,
		
	DSP_TYPE_HI3518CV100 = 1001,
	DSP_TYPE_HI3516CV100,
	DSP_TYPE_HI3518EV100,
	DSP_TYPE_HI3518EV200,
	DSP_TYPE_HI3516CV300,
	DSP_TYPE_HI3516AV100,
	DSP_TYPE_HI3516DV100,
	DSP_TYPE_HI3516AV200,
	DSP_TYPE_AMBAR_S2L66 = 2001,
	DSP_TYPE_AMBAR_S2L55M,
	DSP_TYPE_AMBAR_S2L33M,

	DSP_TYPE_SSTAR_316D = 3001,
	DSP_TYPE_SSTAR_313E,
	DSP_TYPE_SSTAR_316DM,	
}AjDspType;


#define PRODUCT_TYPE_HC200E PRODUCT_TYPE_HE200A


typedef enum
{
	AJ_OVERLAY_STYLE_BLACK_WHITE = 0,		//���ְ׵�
	AJ_OVERLAY_STYLE_WHITE_BLACK = 1,		//���ֺڵ�
	AJ_OVERLAY_STYLE_TRANSPARENT_BLACKWHITE = 2,	//͸�����������ְ׿�
	AJ_OVERLAY_STYLE_TRANSPARENT_WHITEBLACK = 3,	//͸�����������ֺڿ�
}AjOsdOverlayStyle;

typedef enum {
	ALARM_CODE_BEGIN=0,
	ALARM_CODE_LINKDOWN=1,
	ALARM_CODE_LINKUP,
	ALARM_CODE_USB_PLUG,
	ALARM_CODE_USB_UNPLUG,
	ALARM_CODE_SD0_PLUG,
	ALARM_CODE_SD0_UNPLUG,
	ALARM_CODE_SD1_PLUG,
	ALARM_CODE_SD1_UNPLUG,
	ALARM_CODE_USB_FREESPACE_LOW,
	ALARM_CODE_SD0_FREESPACE_LOW,
	ALARM_CODE_SD1_FREESPACE_LOW,
	ALARM_CODE_VIDEO_LOST,
	ALARM_CODE_VIDEO_COVERD,
	ALARM_CODE_MOTION_DETECT,
	ALARM_CODE_GPIO3_HIGH2LOW,	//�������ڸ澯�����жϡ�IO����ʹ��ALARM_CODE_IO_ALARM��ALARM_CODE_IO_ALARM_FINISH
	ALARM_CODE_GPIO3_LOW2HIGH,	//�������ڸ澯�����жϡ�IO����ʹ��ALARM_CODE_IO_ALARM��ALARM_CODE_IO_ALARM_FINISH
	ALARM_CODE_STORAGE_FREESPACE_LOW, 
	ALARM_CODE_RECORD_START,
	ALARM_CODE_RECORD_FINISHED,	
	ALARM_CODE_RECORD_FAILED,	
	ALARM_CODE_GPS_INFO,			
	ALARM_CODE_EMERGENCY_CALL,
	ALARM_CODE_JPEG_CAPTURED,	
	ALARM_CODE_RS485_DATA,		
	ALARM_CODE_SAME_IP,			
	ALARM_CODE_HW130_PIR,
	ALARM_CODE_LPR,	//����ʶ��
	ALARM_CODE_AUDIO_BABYCRY,//Ӥ�����
	ALARM_CODE_AUDIO_LSA,//�߷ֱ�����

	ALARM_CODE_VIDEO_FORMAT_CHANGED,	//��ʽ/�ֱ��ʸ��ģ�����֪ͨ�ͻ����������ý�����

	ALARM_CODE_VIDEO_GATE,//����Χ��

	ALARM_CODE_RESET_TO_FACTORY,//�ָ�����֪ͨ
	ALARM_CODE_MOTION_DETECT_DISAPPEAR,  //�ƶ����澯����
	
	ALARM_CODE_IO_ALARM,	//IO���뱨��,����һֱ���µ�����£���һֱ�澯
	ALARM_CODE_IO_ALARM_FINISH,	//IO���뱨������
	ALARM_CODE_VIDEO_PD,	//���μ��
	ALARM_CODE_VIDEO_PD_FINISH, //���θ澯���ȥ��
	ALARM_CODE_VIDEO_GATE_FINISH,
	
	ALARM_CODE_END
}AjAlarmCode;

typedef struct
{
	int year;
	int month;
	int day;
	int wday;
	int hour;
	int minute;
	int second;
}ALARM_TIME;

#define MAX_ALARM_DATA_LEN 128

typedef struct 
{
	ALARM_TIME	time;
	AjAlarmCode code;
	int 		flag;
	int			level;
	char 		data[MAX_ALARM_DATA_LEN];
} ALARM_ENTRY;

typedef enum
{
	AUDIO_PLAY_READY,	//Ԥ��OK��δ����
	AUDIO_PLAY_RESERVE,//���ڲ��ŷ�����Ƶ
	AUDIO_PLAY_FILE,	//���ڲ����ļ�
}AudioPlayStatus;


typedef enum _AudioChannel_e{
	AudioChannel_Mono	= 1,
	AudioChannel_Stereo = 2
}AudioChannel_e;

typedef enum _AudioType_e{
	AudioType_PCMU,
	AudioType_AAC_LC,
	AudioType_PCMA,
	AudioType_PCM,
	AudioType_OPUS,
	AudioType_MP3,
}AudioType_e;

typedef enum _AU_SampleRate_e{
	AU_SampleRate_8000HZ	= 8000,
	AU_SampleRate_16000HZ	= 16000,
	AU_SampleRate_32000HZ	= 32000,
	AU_SampleRate_44100HZ	= 44100,
	AU_SampleRate_48000HZ	= 48000
}AU_SampleRate_e;

typedef struct
{
	int bBlocked;	
	int bRun;
}PcmPlayParam;

//������Ƶ����ͷ(�¼�����PCM����)
#define AJ_RA_MAGIC 0xEEbbAAdd
typedef struct
{
	unsigned int magic; //AJ_RA_MAGIC
	unsigned short audiotype;
	unsigned short samplerate;
	unsigned short channels;
	unsigned short reserve;
}RaDataHeader;


#if defined (__cplusplus)
}
#endif

#endif

