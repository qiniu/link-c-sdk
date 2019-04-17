#ifndef _____FUNCTION_LIST_H____
#define _____FUNCTION_LIST_H____

//for system control config
#define MAX_SYSTEM_CONTROL_STRING_LEN	2048

#define FUNCTION_SUPPORT_STORAGE  		 "storage_support"

#define FUNCTION_WIRELESS_STATION			"wireless_station"
#define FUNCTION_WIFI_AP 					"wifi_ap"
#define FUNCTION_WIFI_AP_STATION_SAMETIME		"ap_station"

#define FUNCTION_NETWORK_STORAGE		"network_storage"
#define FUNCTION_FTPEMAIL_STORAGE			"ftpemail_aj"
#define FUNCTION_EMAIL_SSL			"ssl_email"
#define FUNCTION_SCHEDULE_RECORD		"schedule_record"
#define FUNCTION_PICTURE_CAPTURE		"picture_capture"
#define FUNCTION_PTZ_CONTROL			"ptz_control"
#define FUNCTION_GPIO_INPUT				"gpio_input"
#define FUNCTION_GPIO_OUTPUT			"gpio_output"
#define FUNCTION_PTZ_ACTION			"ptz_action"
#define FUNCTION_AUDIOPLAY_ACTION	"audio_action"
#define FUNCTION_RA_ANSWER	"ra_answer"
#define FUNCTION_RA_PCM	"ra_pcm"	
#define FUNCTION_RA_MP3STREAM	"ra_mp3"	
#define FUNCTION_UPNP	"enable_upnp"	


#define FUNCTION_THREE_VIDEO			"three_video"	//�Ƿ�֧��VIDEO��������
#define FUNCTION_YUV_VIDEO				"yuv_video"		//�Ƿ�֧��YUV VIDEO ����̶�ȡ
#define FUNCTION_ONLY_AUDIO				"only_audio"	//�Ƿ��������Ƶ����
#define FUNCTION_VIDEO_FORBIT			"forbid_video"	//�Ƿ�ɽ�����Ƶ��
#define FUNCTION_VIDEO_ROI				"video_roi"		//�Ƿ��������Ƶ��Ȥ����
#define FUNCTION_LED_TYPE				"ledtype_set" 	//�Ƿ�����ð׹�/�����
#define FUNCTION_VIDEO_ENCODE_MODE		"vencodemode_set" //�Ƿ��������Ƶ����ģʽ(ͼ��ģʽ)
#define FUNCTION_USEROSD		"userosd_set" //�Ƿ�������Զ���OSD
#define FUNCTION_TITLE_BMP		"bmplogo_set" //�Ƿ������BMP LOGOͼƬ
#define FUNCTION_TITLE_COLOR	"titlecolor_set" //�Ƿ������title��ɫͼƬ
#define FUNCTION_TITLE_TRANSPARENT	"alpha_set" //�Ƿ������͸����

#define FUNCTION_FRONT_REPLAY			"front_replay"
#define FUNCTION_REPLAY_BYTIME		"replay_bytime"
#define FUNCTION_MEDIA_CAPABILITY		"media_capabiltiy"
#define FUNCTION_IRCUT_SETTING			"ircut_setting"
#define FUNCTION_IRCUT_LED_DELAY		"ircut_leddelay"
#define FUNCTION_PROFLE_SETTING		"profile_setting"
#define FUNCTION_WDR_SETTING			"wdr_setting"
#define FUNCTION_VIDEO_MASK			"video_mask"
#define FUNCTION_SYSTEM_MAINTAIN		"system_maitain"
#define FUNCTION_LINKDOWN_RECORD		"linkdown_record"
#define FUNCTION_PPTP					"pptp_support"

#define FUNCTION_AUDIO					"audio_support"

#define FUCNTION_3G_EVDO				"evdo_support"
#define FUCNTION_3G_WCDMA				"wcdma_support"
#define FUCNTION_3G_TDSCDMA			"tdscdma_support"


#define FUNCTION_LANGUAGE_ZH_CN		"zh_cn"
#define FUNCTION_LANGUAGE_ZH_TW		"zh_tw"
#define FUNCTION_LANGUAGE_ZH_HK		"zh_hk"
#define FUNCTION_LANGUAGE_EN_US		"en_us"
#define FUNCTION_LANGUAGE_RU_PY		"ru_py"
#define FUNCTION_LANGUAGE_TR_TR		"tr-tr"
#define FUNCTION_LANGUAGE_KO_KO		"ko-ko"
#define FUNCTION_LANGUAGE_CZ_CZEKH	"cz-czekh"


#define FUNCTION_SEARCH_WIFIAP		 	"SEARCH_WIFIAP"
#define FUNCTION_LONG_TITLE		 	"LONG_TITLE"
#define FUNCTION_TIMEZONE_HALFHOUR	"timezone_halfhour"
#define FUNCTION_P2P_CFG				"p2p_cfg_support"

#define FUNCTION_SUPPORT_RECORD_AJV  "ajv_support"
#define FUNCTION_MD_18X22				"md_18x22"
#define FUNCTION_ONLY_18X22			"only_18x22"
#define FUNCTION_AMBAR_ENCMODE		"encmode_setting"
#define FUNCTION_SUPPORT_LBR			"lbr_support"
#define FUNCTION_SUPPORT_28181			"gb28181"
#define FUNCTION_SUPPORT_IPVS			"ipvs"
#define FUNCTION_SUPPORT_RTMP	"rtmp"
#define FUNCTION_HIK_CONFIG		"hikconfig"	
#define FUNCTION_COMM_ONVIF_ENABLE		"commenable"	

#define FUCNTION_VOIP			"voip"
#define FUNCTION_ROTATE		"rotate_enable"
#define FUNCTION_AUDIO_AMPLIFY		"audio_amplify"
#define FUNCTION_ALOWIP_SETTING		"ipaddrlimit"
#define FUNCTION_OVERLAYFPS_SETTING		"overlayfps"//����֡��
#define FUNCTION_GAIN_SETTING		"gainsetting"//�ֶ�����/�Զ�����
#define FUNCTION_ALARMCLOCK_SETTING		"alarmclock"
#define FUNCTION_ALARM_VIDEOGATE		"VideoGate"	//����Χ��
#define FUNCTION_ISP_MODE		"ispmode"
#define FUNCTION_ALARM_PD		"VideoPD"	//���μ��
#define FUNCTION_OSD_ANYPOSITION		"OSD_ANYPOS"	//OSD����λ��
#define FUNCTION_VIDEO_CROP		"video_crop"	//�ü�
#define FUNCTION_VIDEO_FORCT_ANTIFLICKER		"antiflicker"	//ǿ�ƿ���


#define FUNCTION_VIDEOMASK_ONESET	"ONEVIDEOMASK"		//��������ʹ��ͬһ����˽�ڵ�������Ҫ�ֿ�����
#define FUNCTION_MULTICAST	"multicast"	
#define FUNCTION_VIDEOSHUTTER	"VideoShutter"	

#define FUNCTION_MAC_MODIFY	"ModifyMac"	
#define FUNCTION_AUDIO_ALARM	"AudioAlarm"	


//��˼����֧�ֿ�̬��ȥ���ܣ�������Χ0-255
#define FUNCTION_HISCON_ENCMODE		"hisconenc"	

#define FUNCTION_P2P_CONFIG	"p2p_config"	
#define FUNCTION_P2P_DANALE	"p2p_danale"	
#define FUNCTION_P2P_ANKO		"p2p_anko"	
#define FUNCTION_P2P_GOOLINK	"p2p_goolink"	
#define FUNCTION_P2P_ISMART	"p2p_ismart"	
#define FUNCTION_P2P_QQ		"p2p_qq"
#define FUNCTION_P2P_EYEPLUS		"p2p_eyeplus"

#define FUNCTION_PRE_RECORD   "pre_record"

#endif       

