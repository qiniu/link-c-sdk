#include "tsuploader.h"
#include "tsmuxuploader.h"
#include <assert.h>
#include "log.h"
#include <pthread.h>
#include "servertime.h"
#include "fixjson.h"
#include "segmentmgr.h"
#include "httptools.h"

static int volatile nProcStatus = 0;

int LinkInit()
{
        if (nProcStatus) {
                return LINK_SUCCESS;
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
        LinkLogDebug("main thread id:%ld", (long)pthread_self());
        
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
#ifdef LINK_USE_OLD_NAME
            ||_pUserUploadArg->pDeviceName == NULL || _pUserUploadArg->nDeviceNameLen == 0
#endif
            || _pUserUploadArg->nDeviceAkLen == 0 || _pUserUploadArg->pDeviceAk == NULL ||
            _pUserUploadArg->pDeviceSk == NULL || _pUserUploadArg->nDeviceSkLen == 0) {
                LinkLogError("app or deviceName or ak or sk argument is null");
                return LINK_ARG_ERROR;
        }
#ifdef LINK_USE_OLD_NAME
        if (_pUserUploadArg->nDeviceNameLen > LINK_MAX_DEVICE_NAME_LEN) {
                LinkLogError("app or deviceName is too long: %d", _pUserUploadArg->nDeviceNameLen);
                return LINK_ARG_ERROR;
        }
#endif
        
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
        userUploadArg.pDeviceName = _pUserUploadArg->pDeviceName;
        userUploadArg.nDeviceNameLen = _pUserUploadArg->nDeviceNameLen;
        userUploadArg.pConfigRequestUrl = _pUserUploadArg->pConfigRequestUrl;
        userUploadArg.nConfigRequestUrlLen = _pUserUploadArg->nConfigRequestUrlLen;
        userUploadArg.pDeviceAk = _pUserUploadArg->pDeviceAk;
        userUploadArg.nDeviceAkLen = _pUserUploadArg->nDeviceAkLen;
        userUploadArg.pDeviceSk = _pUserUploadArg->pDeviceSk;
        userUploadArg.nDeviceSkLen = _pUserUploadArg->nDeviceSkLen;
        userUploadArg.pUploadStatisticCb = (UploadStatisticCallback)_pUserUploadArg->reserved1;
        userUploadArg.pUploadStatArg = _pUserUploadArg->reserved2;
        userUploadArg.nMaxUploadThreadNum = _pUserUploadArg->nMaxUploadThreadNum;
        
        LinkTsMuxUploader *pTsMuxUploader;
        int ret = LinkNewTsMuxUploaderWillPicAndSeg(&pTsMuxUploader, &avArg, &userUploadArg, &picArg);
        if (ret != 0) {
                LinkLogError("LinkNewTsMuxUploaderWillPicAndSeg fail");
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
        char * pToken;
        int nTokenLen;
        char *pFnamePrefix;
        int nFnamePrefixLen;
        int nDeadline;
        int nHttpRet; //use with curl not ghttp
};

size_t writeData(void *pTokenStr, size_t size,  size_t nmemb,  void *pUserData) {
        struct HttpToken *pToken = (struct HttpToken *)pUserData;
        
        int len = pToken->nTokenLen;
        int ret = LinkGetJsonStringByKey((const char *)pTokenStr, "\"token\"", pToken->pToken, &len);
        if (ret != LINK_SUCCESS) {
                pToken->nHttpRet = ret;
                return 0;
        }
        pToken->nTokenLen = len;
        
        len = pToken->nFnamePrefixLen;
        ret = LinkGetJsonStringByKey((const char *)pTokenStr, "\"fnamePreifx\"", pToken->pFnamePrefix, &len);
        if (ret != LINK_SUCCESS) {
                pToken->nHttpRet = ret;
                return 0;
        }
        pToken->nFnamePrefixLen = len;
        
        int nDeadline = LinkGetJsonIntByKey((const char *)pTokenStr, "\"tts\"");
        
        int nDeleteAfterDays = 0;
        ret = LinkGetPolicyFromUptoken(pToken->pToken, &nDeleteAfterDays, &pToken->nDeadline);
        if (ret != LINK_SUCCESS || pToken->nDeadline < 1543397800) {
                pToken->nHttpRet = LINK_JSON_FORMAT;
                return 0;
        }
        
        if (nDeadline > 0)
                pToken->nDeadline = nDeadline;

        return size * nmemb;
}

int LinkGetUploadToken(char *pBuf, int nBufLen, int *pDeadline, OUT char *pFnamePrefix, IN int nFnamePrefixLen,
                       const char *pUrl, const char *pToken, int nTokenLen)
{
        
        if (pUrl == NULL || pBuf == NULL || nBufLen <= 10)
                return LINK_ARG_ERROR;
        char httpResp[1024+256]={0};
        int nHttpRespLen = sizeof(httpResp);
        int nRespLen = 0;
        int ret = LinkSimpleHttpGetWithToken(pUrl, httpResp, nHttpRespLen, &nRespLen, pToken, nTokenLen);
        
        if (ret != LINK_SUCCESS) {
                if (ret == LINK_BUFFER_IS_SMALL) {
                        LinkLogError("buffer is small:%d %d", sizeof(httpResp), nRespLen, nTokenLen);
                }
                return ret;
        }
        
        memset(pBuf, 0, nBufLen);

        struct HttpToken token;
        token.pToken = pBuf;
        token.nTokenLen = nBufLen;
        token.nDeadline = 0;
        token.nHttpRet = 0;
        token.nFnamePrefixLen = nFnamePrefixLen;
        token.pFnamePrefix = pFnamePrefix;
        
        if (writeData(httpResp, nRespLen,  1,  &token) == 0){
                LinkLogError("maybe response format error:%s[%d]", httpResp, token.nHttpRet);
                return LINK_JSON_FORMAT;
        }
        token.pToken[token.nTokenLen] = 0;
        if (pDeadline) {
                *pDeadline = token.nDeadline;
        }
        
        return LINK_SUCCESS;
}
