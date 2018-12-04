#include "tsuploaderapi.h"
#include "tsmuxuploader.h"
#include <assert.h>
#include "log.h"
#include <pthread.h>
#include "servertime.h"
#ifndef USE_OWN_TSMUX
#include <libavformat/avformat.h>
#endif
#include "fixjson.h"
#include "segmentmgr.h"
#include "httptools.h"

static int volatile nProcStatus = 0;

int LinkInit()
{
        if (nProcStatus) {
                return LINK_SUCCESS;
        }
#ifndef USE_OWN_TSMUX
    #if LIBAVFORMAT_VERSION_MAJOR < 58
        av_register_all();
    #endif
#endif
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
        LinkLogDebug("main thread id:%ld", (long)pthread_self());
        
        return LINK_SUCCESS;

}

int LinkCreateAndStartAVUploader(LinkTsMuxUploader **_pTsMuxUploader, LinkMediaArg *_pAvArg, LinkUserUploadArg *_pUserUploadArg)
{
        if (
            _pUserUploadArg->pDeviceId_ == NULL || _pUserUploadArg->nDeviceIdLen_ == 0 ||
            _pTsMuxUploader == NULL || _pAvArg == NULL || _pUserUploadArg == NULL) {
                LinkLogError("token or deviceid or argument is null");
                return LINK_ARG_ERROR;
        }

        LinkTsMuxUploader *pTsMuxUploader;
        int ret = LinkNewTsMuxUploader(&pTsMuxUploader, _pAvArg, _pUserUploadArg);
        if (ret != 0) {
                LinkLogError("NewTsMuxUploader fail");
                return ret;
        }
        
        ret = LinkTsMuxUploaderStart(pTsMuxUploader);
        if (ret != 0){
                LinkDestroyTsMuxUploader(&pTsMuxUploader);
                LinkLogError("UploadStart fail:%d", ret);
                return ret;
        }
        *_pTsMuxUploader = pTsMuxUploader;

        return LINK_SUCCESS;
}
// LinkMediaArg *_pAvArg, IN LinkPicUploadArg *_pPicArg
int LinkNewUploader(LinkTsMuxUploader **_pTsMuxUploader, LinkUploadArg *_pUserUploadArg)
{
        if ( _pUserUploadArg == NULL ||_pUserUploadArg->pDeviceId_ == NULL || _pUserUploadArg->nDeviceIdLen_ == 0
            || _pUserUploadArg->nDeviceAkLen == 0 || _pUserUploadArg->pDeviceAk == NULL ||
            _pUserUploadArg->pDeviceSk == NULL || _pUserUploadArg->nDeviceSkLen == 0) {
                LinkLogError("token or deviceid or argument is null");
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
        userUploadArg.nAppLen = _pUserUploadArg->nAppLen;
        userUploadArg.pApp = _pUserUploadArg->pApp;
        userUploadArg.pDeviceId_ = _pUserUploadArg->pDeviceId_;
        userUploadArg.nDeviceIdLen_ = _pUserUploadArg->nDeviceIdLen_;
        userUploadArg.pConfigRequestUrl = _pUserUploadArg->pConfigRequestUrl;
        userUploadArg.nConfigRequestUrlLen = _pUserUploadArg->nConfigRequestUrlLen;
        userUploadArg.pDeviceAk = _pUserUploadArg->pDeviceAk;
        userUploadArg.nDeviceAkLen = _pUserUploadArg->nDeviceAkLen;
        userUploadArg.pDeviceSk = _pUserUploadArg->pDeviceSk;
        userUploadArg.nDeviceSkLen = _pUserUploadArg->nDeviceSkLen;
        
        LinkTsMuxUploader *pTsMuxUploader;
        int ret = LinkNewTsMuxUploaderWillPicAndSeg(&pTsMuxUploader, &avArg, &userUploadArg, &picArg);
        if (ret != 0) {
                LinkLogError("LinkNewTsMuxUploaderWillPicAndSeg fail");
                return ret;
        }
        
        ret = LinkTsMuxUploaderStart(pTsMuxUploader);
        if (ret != 0){
                LinkDestroyTsMuxUploader(&pTsMuxUploader);
                LinkLogError("UploadStart fail:%d", ret);
                return ret;
        }
        *_pTsMuxUploader = pTsMuxUploader;
        
        return LINK_SUCCESS;
}

int LinkPushVideo(LinkTsMuxUploader *_pTsMuxUploader, char * _pData, int _nDataLen, int64_t _nTimestamp, int _nIsKeyFrame, int _nIsSegStart)
{
        if (_pTsMuxUploader == NULL || _pData == NULL || _nDataLen == 0) {
                return LINK_ARG_ERROR;
        }
        int ret = 0;
        ret = _pTsMuxUploader->PushVideo(_pTsMuxUploader, _pData, _nDataLen, _nTimestamp, _nIsKeyFrame, _nIsSegStart);
        return ret;
}

int LinkPushAudio(LinkTsMuxUploader *_pTsMuxUploader, char * _pData, int _nDataLen, int64_t _nTimestamp)
{
        if (_pTsMuxUploader == NULL || _pData == NULL || _nDataLen == 0) {
                return LINK_ARG_ERROR;
        }
        int ret = 0;
        ret = _pTsMuxUploader->PushAudio(_pTsMuxUploader, _pData, _nDataLen, _nTimestamp);
        return ret;
}

void LinkSetUploadBufferSize(LinkTsMuxUploader *_pTsMuxUploader, int _nSize)
{
        if (_pTsMuxUploader == NULL || _nSize < 0) {
                LinkLogError("wrong arg.%p %d", _pTsMuxUploader, _nSize);
                return;
        }
        _pTsMuxUploader->SetUploaderBufferSize(_pTsMuxUploader, _nSize);
}

int LinkGetUploadBufferUsedSize(LinkTsMuxUploader *_pTsMuxUploader)
{
        return _pTsMuxUploader->GetUploaderBufferUsedSize(_pTsMuxUploader);
}

void LinkUpdateNewSegmentInterval(LinkTsMuxUploader *_pTsMuxUploader, int _nIntervalSecond)
{
        if (_pTsMuxUploader == NULL || _nIntervalSecond < 0) {
                LinkLogError("wrong arg.%p %d", _pTsMuxUploader, _nIntervalSecond);
                return;
        }
        _pTsMuxUploader->SetUpdateSegmentInterval(_pTsMuxUploader, _nIntervalSecond);
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
        LinkUninitSegmentMgr();
        LinkStopMgr();
        
        return;
}

struct HttpToken {
        char * pData;
        int nDataLen;
        int nDeadline;
        int nHttpRet; //use with curl not ghttp
        LinkUploadZone *pZone;
};

size_t writeData(void *pTokenStr, size_t size,  size_t nmemb,  void *pUserData) {
        struct HttpToken *pToken = (struct HttpToken *)pUserData;
        
        int len = pToken->nDataLen;
        int ret = LinkGetJsonStringByKey((const char *)pTokenStr, "\"token\"", pToken->pData, &len);
        if (ret != LINK_SUCCESS) {
                pToken->nHttpRet = ret;
                return 0;
        }
        pToken->nDataLen = len;
        
        int nDeleteAfterDays = 0;
        ret = LinkGetPolicyFromUptoken(pToken->pData, &nDeleteAfterDays, &pToken->nDeadline);
        if (ret != LINK_SUCCESS || pToken->nDeadline < 1543397800) {
                pToken->nHttpRet = LINK_JSON_FORMAT;
                return 0;
        }
        
        char zone[10] = {0};
        len = sizeof(zone) - 1;
        ret = LinkGetJsonStringByKey((const char *)pTokenStr, "\"zone\"", zone, &len);
        if (ret == LINK_SUCCESS && pToken->pZone != NULL) {
                if (strcmp(zone, "z1") == 0) {
                        *pToken->pZone = LINK_ZONE_HUABEI;
                } else if(strcmp(zone, "z2") == 0) {
                        *pToken->pZone = LINK_ZONE_HUANAN;
                } else if(strcmp(zone, "na0.") == 0) {
                        *pToken->pZone = LINK_ZONE_BEIMEI;
                } else if(strcmp(zone, "as0") == 0) {
                        *pToken->pZone = LINK_ZONE_DONGNANYA;
                } else {
                        *pToken->pZone = LINK_ZONE_HUADONG;
                }
        }
        return size * nmemb;
}

int LinkGetUploadToken(char *pBuf, int nBufLen, LinkUploadZone *pZone, int *pDeadline, const char *pUrl)
{
        
        if (pUrl == NULL || pBuf == NULL || nBufLen <= 10)
                return LINK_ARG_ERROR;
        char httpResp[1024+256]={0};
        int nHttpRespLen = sizeof(httpResp);
        int nRespLen = 0;
        int ret = LinkSimpleHttpGet(pUrl, httpResp, nHttpRespLen, &nRespLen);
        
        if (ret != LINK_SUCCESS) {
                if (ret == LINK_BUFFER_IS_SMALL) {
                        LinkLogError("buffer is small:%d %d", sizeof(httpResp), nRespLen);
                }
                return ret;
        }
        
        memset(pBuf, 0, nBufLen);

        struct HttpToken token;
        token.pData = pBuf;
        token.nDataLen = nBufLen;
        token.nDeadline = 0;
        token.nHttpRet = 0;
        token.pZone = pZone;
        
        if (writeData(httpResp, nRespLen,  1,  &token) == 0){
                LinkLogError("maybe response format error:%s[%d]", httpResp, token.nHttpRet);
                return LINK_JSON_FORMAT;
        }
        token.pData[token.nDataLen] = 0;
        if (pDeadline) {
                *pDeadline = token.nDeadline;
        }
        
        return LINK_SUCCESS;
}
