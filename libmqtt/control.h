#ifndef __IO_CTRL__
#define __IO_CTRL__

#include "mqtt.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>

// AVIOCTRL Message Type
typedef enum
{
        LINKING_TYPE_IPCAM_START                                 = 0x01FF,
        LINKING_TYPE_IPCAM_STOP                                  = 0x02FF,
        LINKING_TYPE_IPCAM_AUDIOSTART                            = 0x0300,
        LINKING_TYPE_IPCAM_AUDIOSTOP                             = 0x0301,
        LINKING_TYPE_IPCAM_SPEAKERSTART                          = 0x0350,
        LINKING_TYPE_IPCAM_SPEAKERSTOP                           = 0x0351,

        LINKING_TYPE_IPCAM_SETSTREAMCTRL_REQ                     = 0x0320,
        LINKING_TYPE_IPCAM_SETSTREAMCTRL_RESP                    = 0x0321,
        LINKING_TYPE_IPCAM_GETSTREAMCTRL_REQ                     = 0x0322,
        LINKING_TYPE_IPCAM_GETSTREAMCTRL_RESP                    = 0x0323,

        LINKING_TYPE_IPCAM_SETMOTIONDETECT_REQ                    = 0x0324,
        LINKING_TYPE_IPCAM_SETMOTIONDETECT_RESP                   = 0x0325,
        LINKING_TYPE_IPCAM_GETMOTIONDETECT_REQ                    = 0x0326,
        LINKING_TYPE_IPCAM_GETMOTIONDETECT_RESP                   = 0x0327,

        LINKING_TYPE_IPCAM_GETSUPPORTSTREAM_REQ                   = 0x0328,       // Get Support Stream
        LINKING_TYPE_IPCAM_GETSUPPORTSTREAM_RESP                  = 0x0329,

        LINKING_TYPE_IPCAM_DEVINFO_REQ                            = 0x0330,
        LINKING_TYPE_IPCAM_DEVINFO_RESP                           = 0x0331,

        LINKING_TYPE_IPCAM_SETPASSWORD_REQ                        = 0x0332,
        LINKING_TYPE_IPCAM_SETPASSWORD_RESP                       = 0x0333,

        LINKING_TYPE_IPCAM_LISTWIFIAP_REQ                         = 0x0340,
        LINKING_TYPE_IPCAM_LISTWIFIAP_RESP                        = 0x0341,
        LINKING_TYPE_IPCAM_SETWIFI_REQ                            = 0x0342,
        LINKING_TYPE_IPCAM_SETWIFI_RESP                           = 0x0343,
        LINKING_TYPE_IPCAM_GETWIFI_REQ                            = 0x0344,
        LINKING_TYPE_IPCAM_GETWIFI_RESP                           = 0x0345,
        LINKING_TYPE_IPCAM_SETWIFI_REQ_2                          = 0x0346,
        LINKING_TYPE_IPCAM_GETWIFI_RESP_2                         = 0x0347,

        LINKING_TYPE_IPCAM_SETRECORD_REQ                          = 0x0310,
        LINKING_TYPE_IPCAM_SETRECORD_RESP                         = 0x0311,
        LINKING_TYPE_IPCAM_GETRECORD_REQ                          = 0x0312,
        LINKING_TYPE_IPCAM_GETRECORD_RESP                         = 0x0313,

        LINKING_TYPE_IPCAM_LISTEVENT_REQ                          = 0x0318,
        LINKING_TYPE_IPCAM_LISTEVENT_RESP                         = 0x0319,

        LINKING_TYPE_IPCAM_RECORD_PLAYCONTROL                      = 0x031A,
        LINKING_TYPE_IPCAM_RECORD_PLAYCONTROL_RESP                 = 0x031B,

        LINKING_TYPE_IPCAM_GETAUDIOOUTFORMAT_REQ                   = 0x032A,
        LINKING_TYPE_IPCAM_GETAUDIOOUTFORMAT_RESP                  = 0x032B,

        LINKING_TYPE_IPCAM_GET_EVENTCONFIG_REQ                     = 0x0400,       // Get Event Config Msg Request
        LINKING_TYPE_IPCAM_GET_EVENTCONFIG_RESP                    = 0x0401,       // Get Event Config Msg Response
        LINKING_TYPE_IPCAM_SET_EVENTCONFIG_REQ                     = 0x0402,       // Set Event Config Msg req
        LINKING_TYPE_IPCAM_SET_EVENTCONFIG_RESP                    = 0x0403,       // Set Event Config Msg resp

        LINKING_TYPE_IPCAM_SET_ENVIRONMENT_REQ                     = 0x0360,
        LINKING_TYPE_IPCAM_SET_ENVIRONMENT_RESP                    = 0x0361,
        LINKING_TYPE_IPCAM_GET_ENVIRONMENT_REQ                     = 0x0362,
        LINKING_TYPE_IPCAM_GET_ENVIRONMENT_RESP                    = 0x0363,

        LINKING_TYPE_IPCAM_SET_VIDEOMODE_REQ                       = 0x0370,       // Set Video Flip Mode
        LINKING_TYPE_IPCAM_SET_VIDEOMODE_RESP                      = 0x0371,
        LINKING_TYPE_IPCAM_GET_VIDEOMODE_REQ                       = 0x0372,       // Get Video Flip Mode
        LINKING_TYPE_IPCAM_GET_VIDEOMODE_RESP                      = 0x0373,

        LINKING_TYPE_IPCAM_FORMATEXTSTORAGE_REQ                    = 0x0380,       // Format external storage
        LINKING_TYPE_IPCAM_FORMATEXTSTORAGE_RESP                   = 0x0381,

        LINKING_TYPE_IPCAM_PTZ_COMMAND                             = 0x1001,       // P2P PTZ Command Msg

        LINKING_TYPE_IPCAM_EVENT_REPORT                            = 0x1FFF,       // Device Event Report Msg
        LINKING_TYPE_IPCAM_RECEIVE_FIRST_IFRAME                    = 0x1002,       // Send from client, used to talk to device that
                                                                                   // client had received the first I frame

        LINKING_TYPE_IPCAM_GET_FLOWINFO_REQ                        = 0x0390,
        LINKING_TYPE_IPCAM_GET_FLOWINFO_RESP                       = 0x0391,
        LINKING_TYPE_IPCAM_CURRENT_FLOWINFO                        = 0x0392,

        LINKING_TYPE_IPCAM_GET_TIMEZONE_REQ                        = 0x3A0,
        LINKING_TYPE_IPCAM_GET_TIMEZONE_RESP                       = 0x3A1,
        LINKING_TYPE_IPCAM_SET_TIMEZONE_REQ                        = 0x3B0,
        LINKING_TYPE_IPCAM_SET_TIMEZONE_RESP                       = 0x3B1,
        LINKING_TYPECMD_MAX
}ENUM_AVIOCTRL_MSGTYPE;

typedef enum
{
        LINKING_RESPONSE_SUCCESS = 0,
}ENUM_RESPONSE_CODE;
int LinkInitIOCtrl(const char *_pAppId, const char *_pEncodeDeviceName, void *_pInstance);

int LinkSendIOResponse(int nSession, unsigned int _nIOCtrlType, const char *_pIOCtrlData, int _nIOCtrlDataSize);

int LinkRecvIOCtrl(int nSession, unsigned int *_pIOCtrlType, char *_pIOCtrlData, int *_nIOCtrlMaxDataSize, unsigned int _nTimeout);

void LinkDinitIOCtrl(int nSession);
#endif
