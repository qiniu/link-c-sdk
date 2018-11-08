#include "picuploader.h"
#include "servertime.h"
#include "localkey.h"
#include <unistd.h>
#include "picuploader.h"

static void *gSvaeWhenAsync; //may race risk. but for test is enough
enum LinkGetPictureSyncMode gSyncMode = LinkGetPictureModeSync;

static enum LinkGetPictureSyncMode getPicCallback (void *pOpaque, void *pSvaeWhenAsync, OUT char **pBuf, OUT int *pBufSize, OUT enum LinkPicUploadType *pType) {
#ifdef __APPLE__
        const char *file = "../../../tests/material/3c.jpg";
#else
        const char *file = "../../tests/material/3c.jpg";
#endif
        int n = strlen(file)+1;
        char * pFile = (char *)malloc(n);
        memcpy(pFile, file, n);
        *pBuf = pFile;
        *pType = LinkPicUploadTypeFile;
        gSvaeWhenAsync = pSvaeWhenAsync;
        return gSyncMode;
}

static int getPictureFreeCallback (char *pBuf, int nNameBufSize) {
        fprintf(stderr, "free data\n");
        free(pBuf);
        return 0;
}

static char gtestToken[1024] = {0};
static int getUploadParamCallback(IN void *pOpaque, IN OUT LinkUploadParam *pParam) {
        if (pParam->nTokenBufLen < strlen(gtestToken)) {
                return LINK_BUFFER_IS_SMALL;
        }
        memcpy(pParam->pTokenBuf, gtestToken, strlen(gtestToken));
        return LINK_SUCCESS;
}

void justTestSyncUploadPicture(char *pTokenUrl) {
        LinkUploadZone upzone = LINK_ZONE_UNKNOWN;
        int ret = LinkGetUploadToken(gtestToken, sizeof(gtestToken), &upzone, pTokenUrl);
        assert(ret == LINK_SUCCESS);
        ret = LinkInitTime();
        assert(ret == LINK_SUCCESS);
        fprintf(stderr, "token:%s\n", gtestToken);
        
        LinkPicUploadFullArg arg;
        arg.getPicCallback = getPicCallback;
        arg.getUploadParamCallback = getUploadParamCallback;
        arg.getPictureFreeCallback = getPictureFreeCallback;
        arg.pGetPicCallbackOpaque = NULL;
        arg.pGetUploadParamCallbackOpaque = NULL;
        arg.uploadZone = upzone;
        PictureUploader *pPicUploader;
        ret = LinkNewPictureUploader(&pPicUploader, &arg);
        assert(ret == LINK_SUCCESS);
        
        int64_t ts = LinkGetCurrentNanosecond() / 1000000;
        while(1) {
                
                LinkSendGetPictureSingalToPictureUploader(pPicUploader, "pic1", 4, ts);
                ts += 4990;
                sleep(5);
        }
        
}

void justTestAsyncUploadPicture(char *pTokenUrl) {
        gSyncMode = LinkGetPictureModeAsync;
        LinkUploadZone upzone = LINK_ZONE_UNKNOWN;
        int ret = LinkGetUploadToken(gtestToken, sizeof(gtestToken), &upzone, pTokenUrl);
        assert(ret == LINK_SUCCESS);
        ret = LinkInitTime();
        assert(ret == LINK_SUCCESS);
        fprintf(stderr, "token:%s\n", gtestToken);
        
        LinkPicUploadFullArg arg;
        arg.getPicCallback = getPicCallback;
        arg.getUploadParamCallback = getUploadParamCallback;
        arg.getPictureFreeCallback = getPictureFreeCallback;
        arg.pGetPicCallbackOpaque = NULL;
        arg.pGetUploadParamCallbackOpaque = NULL;
        arg.uploadZone = upzone;
        PictureUploader *pPicUploader;
        ret = LinkNewPictureUploader(&pPicUploader, &arg);
        assert(ret == LINK_SUCCESS);
        
        int64_t ts = LinkGetCurrentNanosecond() / 1000000;
        while(1) {
                
                LinkSendGetPictureSingalToPictureUploader(pPicUploader, "pic1", 4, ts);
                
                sleep(1);
                
                const char *file = "../../tests/material/3c.jpg";
                int n = strlen(file)+1;
                char * pFile = (char *)malloc(n);
                memcpy(pFile, file, n);
                fprintf(stderr, "send upload signal\n");
                LinkSendUploadPictureToPictureUploader(pPicUploader, gSvaeWhenAsync, pFile, n, LinkPicUploadTypeFile);
                
                sleep(4);
                ts += 4990;
        }
        
}
