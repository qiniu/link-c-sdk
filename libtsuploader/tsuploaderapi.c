#include "tsuploaderapi.h"
#include "tsmuxuploader.h"
#include <assert.h>
#include "log.h"
#include <pthread.h>
#include <curl/curl.h>
#include "servertime.h"
#ifndef USE_OWN_TSMUX
#include <libavformat/avformat.h>
#endif
#include <qiniu/io.h>
#include <qiniu/rs.h>
#include "fixjson.h"
#include "segmentmgr.h"

static int volatile nProcStatus = 0;

int LinkInitUploader()
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

        Qiniu_Global_Init(-1);

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
        if (_pUserUploadArg->pToken_ == NULL || _pUserUploadArg->nTokenLen_ == 0 ||
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
        if (_pUserUploadArg->nUploaderBufferSize != 0) {
                pTsMuxUploader->SetUploaderBufferSize(pTsMuxUploader, _pUserUploadArg->nUploaderBufferSize);
        }
        if (_pUserUploadArg->nNewSegmentInterval != 0) {
                pTsMuxUploader->SetNewSegmentInterval(pTsMuxUploader, _pUserUploadArg->nNewSegmentInterval);
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

int LinkCreateAndStartAll(LinkTsMuxUploader **_pTsMuxUploader, LinkMediaArg *_pAvArg,
                          LinkUserUploadArg *_pUserUploadArg, IN LinkPicUploadArg *_pPicArg,
                          IN SegmentUserArg *_pSegArg)
{
        if (_pUserUploadArg->pToken_ == NULL || _pUserUploadArg->nTokenLen_ == 0 ||
            _pUserUploadArg->pDeviceId_ == NULL || _pUserUploadArg->nDeviceIdLen_ == 0 ||
            _pTsMuxUploader == NULL || _pAvArg == NULL || _pUserUploadArg == NULL) {
                LinkLogError("token or deviceid or argument is null");
                return LINK_ARG_ERROR;
        }
        
        LinkTsMuxUploader *pTsMuxUploader;
        int ret = LinkNewTsMuxUploaderWillPicAndSeg(&pTsMuxUploader, _pAvArg, _pUserUploadArg, _pPicArg, _pSegArg);
        if (ret != 0) {
                LinkLogError("LinkNewTsMuxUploaderWillPicAndSeg fail");
                return ret;
        }
        if (_pUserUploadArg->nUploaderBufferSize != 0) {
                pTsMuxUploader->SetUploaderBufferSize(pTsMuxUploader, _pUserUploadArg->nUploaderBufferSize);
        }
        if (_pUserUploadArg->nNewSegmentInterval != 0) {
                pTsMuxUploader->SetNewSegmentInterval(pTsMuxUploader, _pUserUploadArg->nNewSegmentInterval);
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

int LinkUpdateToken(LinkTsMuxUploader *_pTsMuxUploader, const char * _pToken, int _nTokenLen)
{
        if (_pTsMuxUploader == NULL || _pToken == NULL || _nTokenLen == 0) {
                return LINK_ARG_ERROR;
        }
        return _pTsMuxUploader->SetToken(_pTsMuxUploader, _pToken, _nTokenLen);
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

void LinkSetNewSegmentInterval(LinkTsMuxUploader *_pTsMuxUploader, int _nIntervalSecond)
{
        if (_pTsMuxUploader == NULL || _nIntervalSecond < 0) {
                LinkLogError("wrong arg.%p %d", _pTsMuxUploader, _nIntervalSecond);
                return;
        }
        _pTsMuxUploader->SetNewSegmentInterval(_pTsMuxUploader, _nIntervalSecond);
}

void LinkDestroyAVUploader(LinkTsMuxUploader **pTsMuxUploader)
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

void LinkSetuploadZone(IN LinkTsMuxUploader *pTsMuxUploader, LinkUploadZone zone) {
        LinkTsMuxUploaderSetUploadZone(pTsMuxUploader, zone);
}

void LinkUninitUploader()
{
        if (nProcStatus != 1)
                return;
        nProcStatus = 2;
        LinkUninitSegmentMgr();
        LinkStopMgr();
        Qiniu_Global_Cleanup();
        
        return;
}

//---------test
static char gAk[65] = {0};
static char gSk[65] = {0};
static char gBucket[128] = {0};
static char gCallbackUrl[256] = {0};
static int nDeleteAfterDays = -1;
void LinkSetBucketName(char *_pName)
{
        int nLen = strlen(_pName);
        assert(nLen < sizeof(gBucket));
        strcpy(gBucket, _pName);
        gBucket[nLen] = 0;
        
        return;
}

void LinkSetAk(char *_pAk)
{
        int nLen = strlen(_pAk);
        assert(nLen < sizeof(gAk));
        strcpy(gAk, _pAk);
        gAk[nLen] = 0;
        
        return;
}

void LinkSetSk(char *_pSk)
{
        int nLen = strlen(_pSk);
        assert(nLen < sizeof(gSk));
        strcpy(gSk, _pSk);
        gSk[nLen] = 0;
        
        return;
}

void LinkSetCallbackUrl(char *pUrl)
{
        int nLen = strlen(pUrl);
        assert(nLen < sizeof(gCallbackUrl));
        strcpy(gCallbackUrl, pUrl);
        gCallbackUrl[nLen] = 0;
        return;
}

void LinkSetDeleteAfterDays(int nDays)
{
        nDeleteAfterDays = nDays;
}

struct CurlToken {
        char * pData;
        int nDataLen;
        int nCurlRet;
        LinkUploadZone *pZone;
};

size_t writeData(void *pTokenStr, size_t size,  size_t nmemb,  void *pUserData) {
        struct CurlToken *pToken = (struct CurlToken *)pUserData;
        
        int len = pToken->nDataLen;
        int ret = GetJsonContentByKey((const char *)pTokenStr, "\"token\"", pToken->pData, &len);
        if (ret != LINK_SUCCESS) {
                pToken->nCurlRet = ret;
                return 0;
        }
        
        char zone[10] = {0};
        len = sizeof(zone) - 1;
        ret = GetJsonContentByKey((const char *)pTokenStr, "\"zone\"", zone, &len);
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

int LinkGetUploadToken(char *pBuf, int nBufLen, LinkUploadZone *pZone, const char *pUrl)
{
#ifdef DISABLE_OPENSSL
        if (pUrl == NULL || pBuf == NULL || nBufLen <= 10)
                return LINK_ARG_ERROR;
        memset(pBuf, 0, nBufLen);
        CURL *curl;
        curl_global_init(CURL_GLOBAL_ALL);
        curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeData);
        curl_easy_setopt(curl, CURLOPT_URL, pUrl);
        
        struct CurlToken token;
        token.pData = pBuf;
        token.nDataLen = nBufLen;
        token.nCurlRet = 0;
        token.pZone = pZone;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &token);
        int ret =curl_easy_perform(curl);
        if (ret != 0) {
                curl_easy_cleanup(curl);
                return ret;
        }
        curl_easy_cleanup(curl);
        return token.nCurlRet;
#else
        if (gAk[0] == 0 || gSk[0] == 0 || gBucket[0] == 0)
                return -11;
        memset(pBuf, 0, nBufLen);
        Qiniu_Mac mac;
        mac.accessKey = gAk;
        mac.secretKey = gSk;
        
        Qiniu_RS_PutPolicy putPolicy;
        Qiniu_Zero(putPolicy);
        putPolicy.scope = gBucket;
        putPolicy.expires = 40;
        putPolicy.deleteAfterDays = 7;
        putPolicy.callbackBody = "{\"key\":\"$(key)\",\"hash\":\"$(etag)\",\"fsize\":$(fsize),\"bucket\":\"$(bucket)\",\"name\":\"$(x:name)\",\"duration\":\"$(avinfo.format.duration)\"}";
        putPolicy.callbackUrl = pUrl;
        putPolicy.callbackBodyType = "application/json";
        
        char *uptoken;
        uptoken = Qiniu_RS_PutPolicy_Token(&putPolicy, &mac);
        assert(nBufLen > strlen(uptoken));
        fprintf(stderr,"token from local:\n");
        strcpy(pBuf, uptoken);
        Qiniu_Free(uptoken);
        return LINK_SUCCESS;
#endif
}
