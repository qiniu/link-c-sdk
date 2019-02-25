#include "tsuploader.h"
#include "tsmuxuploader.h"
#include <assert.h>
#include "log.h"
#include <pthread.h>
#include "servertime.h"
#include "segmentmgr.h"
#include "httptools.h"
#include "cJSON/cJSON.h"
#include <signal.h>
#include <libmqtt/linking-emitter-api.h>
#include <qupload.h>

extern const char *gVersionAgent;

static int volatile nProcStatus = 0;

void ghttpLogOutput(const char * errMsg) {
        LinkLogError("%s", errMsg);
}

int LinkInit()
{
        if (nProcStatus) {
                return LINK_SUCCESS;
        }
        LinkGhttpSetLog(ghttpLogOutput);
        
        signal(SIGPIPE, SIG_IGN);
        sigset_t signal_mask;
        sigemptyset(&signal_mask);
        sigaddset(&signal_mask, SIGPIPE);
        int rc = pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
        if (rc != 0) {
                LinkLogError("block sigpipe error");
        }
        
        LinkInitSn();
        
        setenv("TZ", "GMT-8", 1);

        int ret = 0;
        ret = LinkInitTime();
        if (ret != 0) {
                LinkLogError("InitUploader gettime from server fail:%d", ret);
                return LINK_HTTP_TIME;
        }
        
        ret = LinkStartMgr();
        if (ret != 0) {
                LinkLogError("StartMgr fail");
                return ret;
        }
        nProcStatus = 1;
        LinkLogInfo("main thread id:%ld(version:%s)", (long)pthread_self(), gVersionAgent);

        return LINK_SUCCESS;

}

int LinkCreateAndStartAVUploader(LinkTsMuxUploader **_pTsMuxUploader, LinkMediaArg *_pAvArg, LinkUserUploadArg *_pUserUploadArg)
{
        if (
            _pUserUploadArg->pDeviceName == NULL || _pUserUploadArg->nDeviceNameLen == 0 ||
            _pTsMuxUploader == NULL || _pAvArg == NULL || _pUserUploadArg == NULL) {
                LinkLogError("token or deviceName or argument is null");
                return LINK_ARG_ERROR;
        }

        LinkTsMuxUploader *pTsMuxUploader;
        int ret = LinkNewTsMuxUploader(&pTsMuxUploader, _pAvArg, _pUserUploadArg);
        if (ret != 0) {
                LinkLogError("NewTsMuxUploader fail");
                return ret;
        }
        
        *_pTsMuxUploader = pTsMuxUploader;

        return LINK_SUCCESS;
}
// LinkMediaArg *_pAvArg, IN LinkPicUploadArg *_pPicArg
int LinkNewUploader(LinkTsMuxUploader **_pTsMuxUploader, LinkUploadArg *_pUserUploadArg)
{
        if ( _pUserUploadArg == NULL
            || _pUserUploadArg->nDeviceAkLen == 0 || _pUserUploadArg->pDeviceAk == NULL ||
            _pUserUploadArg->pDeviceSk == NULL || _pUserUploadArg->nDeviceSkLen == 0) {
                LinkLogError("app or deviceName or ak or sk argument is null");
                return LINK_ARG_ERROR;
        }
        
        LinkMediaArg avArg;
        avArg.nAudioFormat = _pUserUploadArg->nAudioFormat;
        avArg.nChannels = _pUserUploadArg->nChannels;
        avArg.nSamplerate = _pUserUploadArg->nSampleRate;
        avArg.nVideoFormat = _pUserUploadArg->nVideoFormat;
        
        LinkPicUploadArg picArg;
        picArg.getPicCallback = _pUserUploadArg->getPictureCallback;
        picArg.pGetPicCallbackOpaque = _pUserUploadArg->pGetPictureCallbackUserData;
        
        LinkUserUploadArg userUploadArg;
        memset(&userUploadArg, 0, sizeof(userUploadArg));
        userUploadArg.pConfigRequestUrl = _pUserUploadArg->pConfigRequestUrl;
        userUploadArg.nConfigRequestUrlLen = _pUserUploadArg->nConfigRequestUrlLen;
        userUploadArg.pDeviceAk = _pUserUploadArg->pDeviceAk;
        userUploadArg.nDeviceAkLen = _pUserUploadArg->nDeviceAkLen;
        userUploadArg.pDeviceSk = _pUserUploadArg->pDeviceSk;
        userUploadArg.nDeviceSkLen = _pUserUploadArg->nDeviceSkLen;
        userUploadArg.pUploadStatisticCb = (UploadStatisticCallback)_pUserUploadArg->pUpStatCb;
        userUploadArg.pUploadStatArg = _pUserUploadArg->pUpStatCbUserArg;
        
        LinkTsMuxUploader *pTsMuxUploader;
        int ret = LinkNewTsMuxUploaderWillPicAndSeg(&pTsMuxUploader, &avArg, &userUploadArg, &picArg);
        if (ret != 0) {
                LinkLogError("LinkNewTsMuxUploaderWillPicAndSeg fail");
                return ret;
        }
        
        *_pTsMuxUploader = pTsMuxUploader;
        
        /* Initial link emitter service */
        LinkEmitter_Init(_pUserUploadArg->pDeviceAk, _pUserUploadArg->nDeviceAkLen,
                        _pUserUploadArg->pDeviceSk, _pUserUploadArg->nDeviceSkLen);
        LinkSetLogCallback(LinkEmitter_SendLog);
        return LINK_SUCCESS;
}

int LinkPushVideo(LinkTsMuxUploader *_pTsMuxUploader, char * _pData, int _nDataLen, int64_t _nTimestamp, int _nIsKeyFrame, int _nIsSegStart, int64_t nFrameSysTime)
{
        if (_pTsMuxUploader == NULL || _pData == NULL || _nDataLen == 0) {
                return LINK_ARG_ERROR;
        }
        int ret = 0;
        ret = _pTsMuxUploader->PushVideo(_pTsMuxUploader, _pData, _nDataLen, _nTimestamp, _nIsKeyFrame, _nIsSegStart, nFrameSysTime);
        return ret;
}

int LinkPushAudio(LinkTsMuxUploader *_pTsMuxUploader, char * _pData, int _nDataLen, int64_t _nTimestamp, int64_t nFrameSysTime)
{
        if (_pTsMuxUploader == NULL || _pData == NULL || _nDataLen == 0) {
                return LINK_ARG_ERROR;
        }
        int ret = 0;
        ret = _pTsMuxUploader->PushAudio(_pTsMuxUploader, _pData, _nDataLen, _nTimestamp, nFrameSysTime);
        return ret;
}

int LinkGetUploadBufferUsedSize(LinkTsMuxUploader *_pTsMuxUploader)
{
        return _pTsMuxUploader->GetUploaderBufferUsedSize(_pTsMuxUploader);
}

void LinkFreeUploader(LinkTsMuxUploader **pTsMuxUploader)
{
        LinkDestroyTsMuxUploader(pTsMuxUploader);
}

int LinkIsProcStatusQuit()
{
        if (nProcStatus == 2) {
                return 1;
        }
        return 0;
}

void LinkCleanup()
{
        if (nProcStatus != 1)
                return;
        nProcStatus = 2;
        LinkStopMgr();
        LinkUninitSegmentMgr();

        /* stop emitter service */
        LinkEmitter_Cleanup();
        LinkSetLogCallback(NULL);
        return;
}


int LinkGetUploadToken(cJSON ** pJsonRoot,const char *pUrl, const char *pReqToken, int nReqTokenLen)
{
        
        char httpResp[2560]={0};
        int nHttpRespLen = sizeof(httpResp);
        int nRespLen = 0;
        int ret = LinkSimpleHttpGetWithToken(pUrl, httpResp, nHttpRespLen, &nRespLen, pReqToken, nReqTokenLen);
        
        if (ret != LINK_SUCCESS) {
                if (ret == LINK_BUFFER_IS_SMALL) {
                        LinkLogError("buffer is small:%d %d", sizeof(httpResp), nRespLen, nReqTokenLen);
                }
                return ret;
        }

        assert(nRespLen <= sizeof(httpResp) - 1);
        httpResp[nRespLen] = 0;
        cJSON * pJRoot = cJSON_Parse(httpResp);
        if (pJRoot == NULL) {
                return LINK_JSON_FORMAT;
        }
        *pJsonRoot = (void *)pJRoot;
        return LINK_SUCCESS;
}
