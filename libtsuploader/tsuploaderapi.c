#include "tsuploader.h"
#include "tsmuxuploader.h"
#include <assert.h>
#include "log.h"
#include <pthread.h>
#include "servertime.h"
#include "segmentmgr.h"
#include "httptools.h"
#include "cJSON/cJSON.h"

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


int LinkGetUploadToken(char *pTokenBuf, int nTokenBufLen, int *pDeadline, OUT char *pFnamePrefix, IN int nFnamePrefixLen,
                       const char *pUrl, const char *pReqToken, int nReqTokenLen)
{
        
        if (pUrl == NULL || pTokenBuf == NULL || pFnamePrefix == NULL || nTokenBufLen <= 10)
                return LINK_ARG_ERROR;
        char httpResp[1024+256]={0};
        int nHttpRespLen = sizeof(httpResp);
        int nRespLen = 0;
        int ret = LinkSimpleHttpGetWithToken(pUrl, httpResp, nHttpRespLen, &nRespLen, pReqToken, nReqTokenLen);
        
        if (ret != LINK_SUCCESS) {
                if (ret == LINK_BUFFER_IS_SMALL) {
                        LinkLogError("buffer is small:%d %d", sizeof(httpResp), nRespLen, nReqTokenLen);
                }
                return ret;
        }
        
        memset(pTokenBuf, 0, nTokenBufLen);

        assert(nRespLen <= sizeof(httpResp) - 1);
        httpResp[nRespLen] = 0;
        cJSON * pJsonRoot = cJSON_Parse(httpResp);
        if (pJsonRoot == NULL) {
                return LINK_JSON_FORMAT;
        }
        
        cJSON *pNode = cJSON_GetObjectItem(pJsonRoot, "ttl");
        if (pNode == NULL) {
                cJSON_Delete(pJsonRoot);
                return LINK_JSON_FORMAT;
        }
        *pDeadline = pNode->valueint;
        
        pNode = cJSON_GetObjectItem(pJsonRoot, "token");
        if (pNode == NULL) {
                cJSON_Delete(pJsonRoot);
                return LINK_JSON_FORMAT;
        }
        int nCpyLen = strlen(pNode->valuestring);
        if (nTokenBufLen < nCpyLen) {
                return LINK_BUFFER_IS_SMALL;
        }
        memcpy(pTokenBuf, pNode->valuestring, nCpyLen);
        pTokenBuf[nCpyLen] = 0;
        
        pNode = cJSON_GetObjectItem(pJsonRoot, "fnamePrefix");
        if (pNode == NULL) {
                cJSON_Delete(pJsonRoot);
                return LINK_JSON_FORMAT;
        }
        nCpyLen = strlen(pNode->valuestring);
        if (nFnamePrefixLen < nCpyLen) {
                cJSON_Delete(pJsonRoot);
                return LINK_BUFFER_IS_SMALL;
        }
        memcpy(pFnamePrefix, pNode->valuestring, nCpyLen);
        pFnamePrefix[nCpyLen] = 0;
        
        cJSON_Delete(pJsonRoot);
        return LINK_SUCCESS;
}
