#ifndef __LINK_BASE_H__
#define __LINK_BASE_H__

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <errno.h>
#include <inttypes.h>
#ifndef __APPLE__
#include <stdint.h>
#endif

#include "uploader.h"

#ifndef OUT
#define OUT
#endif
#ifndef IN
#define IN
#endif

#define LINK_MAX_APP_LEN 200
#define LINK_MAX_BUCKET_LEN 63
#define LINK_MAX_DEVICE_NAME_LEN 200
#define LINK_MAX_SESSION_ID_LEN 20


typedef struct {
	void *pOpaque;
        const char *pFilename;
        int nFilenameLen;
        const char *pBuf;
        int nBuflen;
}LinkPicture;


typedef struct {
        int64_t nTimestamp90Khz;
        int nOffset;
        int nLength;
}LinkKeyFrameMetaInfo;

typedef void (*LinkSetKeyframeMetaInfo)(void *pUserArg, LinkKeyFrameMetaInfo *pMetaInfo);

typedef struct {
        IN char *pTokenBuf;
        IN OUT int nTokenBufLen;
        IN char *pUpHost;
        IN OUT int nUpHostLen;
        IN char* pSegUrl;
        IN OUT int nSegUrlLen;

        char sessionId[LINK_MAX_SESSION_ID_LEN+1];
        char *pAk;
        int nAkLen;
        char *pSk;
        int nSkLen;
        char *pFilePrefix;
        int nFilePrefix;
}LinkUploadParam;

typedef enum {
        LINK_UPLOAD_CB_GETPARAM = 1,
        LINK_UPLOAD_CB_UPREMOTECONFIG = 2,
        LINK_UPLOAD_CB_GETFRAMEPARAM = 3,
        LINK_UPLOAD_CB_GETTSPARAM = 4,
} LinkUploadCbType;

typedef enum {
        LINK_PLAN_TYPE_NONE = -1,
        LINK_PLAN_TYPE_MOVE = 0,
        LINK_PLAN_TYPE_24   = 1,
        LINK_PLAN_TYPE_BAN  = 2,
} LinkPlanType;

typedef int (*LinkUploadParamCallback)(IN void *pOpaque, IN OUT LinkUploadParam *pParam, IN LinkUploadCbType cbtype);



typedef enum {
        LINK_UPLOAD_INIT,
        LINK_UPLOAD_FAIL,
        LINK_UPLOAD_OK
}LinkUploadState;


typedef struct _LinkUserUploadArg{
        const char *pDeviceName;
        int   nDeviceNameLen;
        const char * pConfigRequestUrl;
        size_t nConfigRequestUrlLen;
        const char *pDeviceAk;
        size_t nDeviceAkLen;
        const char *pDeviceSk;
        size_t nDeviceSkLen;
        size_t nMaxUploadThreadNum;
        UploadStatisticCallback pUploadStatisticCb;
        void *pUploadStatArg;
        int nTsMaxSize;
}LinkUserUploadArg;


typedef struct _LinkSession { // seg report info
        // session scope
        char sessionId[LINK_MAX_SESSION_ID_LEN+1];
        int64_t nSessionStartTime;
        int64_t nTsSequenceNumber;
        int64_t nSessionEndTime;
        int64_t nLastTsEndTime;
        int nSessionEndResonCode;
        int isNewSessionStarted;
        int coverStatus;
        int segHandle;
        int64_t nAccSessionDuration;
        int64_t nAccSessionAudioDuration; // aad
        int64_t nAccSessionVideoDuration; // avd
        int64_t nTotalSessionAudioDuration; // tad
        int64_t nTotalSessionVideoDuration; // tvd
        
        // report scope
        int64_t nAudioGapFromLastReport; // ad
        int64_t nVideoGapFromLastReport; // vd
        
        // current ts scope
        int64_t nTsStartTime;
        int64_t nTsDuration;
} LinkSession;

void LinkUpdateSessionId(LinkSession* pSession, int64_t nTsStartSystime);


/**************************/

#define LINK_STREAM_UPLOAD 1


int LinkIsProcStatusQuit();

#define LINK_DEFINE_HANDLE(object) typedef struct object##_T* object;

#endif
