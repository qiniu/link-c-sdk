#include "picuploader.h"
#include "servertime.h"
#include "security.h"
#include <unistd.h>
#include "picuploader.h"

static char *gpFilename; //may race risk. but for test is enough
static int gnPfilenameLen = 0;
extern char *gpPictureBuf;
extern int gnPictureBufLen;
static int asyncType = 1;
static PictureUploader *pPicUploader = NULL;

static void getPicCallback (void *pOpaque,  const char *pFileName, int nFilenameLen) {
        if (gpFilename == NULL) {
                gnPfilenameLen = nFilenameLen * 2;
                gpFilename = (char *)malloc(gnPfilenameLen);
                memcpy(gpFilename, pFileName, nFilenameLen);
                gpFilename[nFilenameLen] = 0;
        } else {
                if (gnPfilenameLen < nFilenameLen) {
                        free(gpFilename);
                }
                gnPfilenameLen = nFilenameLen * 2;
                gpFilename = (char *)malloc(gnPfilenameLen);
                memcpy(gpFilename, pFileName, nFilenameLen);
                gpFilename[nFilenameLen] = 0;
        }
        printf("in testpic getPicCallback:%s\n", gpFilename);
        if (!asyncType) {
                if(gpPictureBuf != NULL && pPicUploader != NULL)
                        LinkSendUploadPictureToPictureUploader(pPicUploader, pFileName, nFilenameLen, gpPictureBuf, gnPfilenameLen);
                else
                        fprintf(stderr, "gpPictureBuf is NULL\n");
        }
        return;
}

static char gtestToken[1024] = {0};
static char gfnamePrefix[256] = {0};
static int getUploadParamCallback(IN void *pOpaque, IN OUT LinkUploadParam *pParam, LinkUploadCbType cbtype) {
        if (pParam->nTokenBufLen < strlen(gtestToken)) {
                return LINK_BUFFER_IS_SMALL;
        }
        if (pParam->pTypeBuf != NULL)
                memcpy(pParam->pTokenBuf, gtestToken, strlen(gtestToken));
        if (pParam->pDeviceName != NULL)
                memcpy(pParam->pDeviceName, "dname1", 6);
        return LINK_SUCCESS;
}

void justTestSyncUploadPicture(const char *pTokenUrl) {
        asyncType = 0;
        int nDeadline = 0;
        int ret = LinkGetUploadToken(gtestToken, sizeof(gtestToken), &nDeadline, gfnamePrefix, sizeof(gfnamePrefix), pTokenUrl, NULL, 0);
        assert(ret == LINK_SUCCESS);
        ret = LinkInitTime();
        assert(ret == LINK_SUCCESS);
        fprintf(stderr, "token:%s\n", gtestToken);
        
        LinkPicUploadFullArg arg;
        arg.getPicCallback = getPicCallback;
        arg.getUploadParamCallback = getUploadParamCallback;
        arg.pGetPicCallbackOpaque = NULL;
        arg.pGetUploadParamCallbackOpaque = NULL;
        
        ret = LinkNewPictureUploader(&pPicUploader, &arg);
        assert(ret == LINK_SUCCESS);
        
        int64_t ts = LinkGetCurrentNanosecond() / 1000000;
        while(1) {
                
                LinkSendItIsTimeToCaptureSignal(pPicUploader, ts);
                ts += 4990;
                sleep(5);
        }
        
}

void justTestAsyncUploadPicture(const char *pTokenUrl) {
        int nDeadline;
        int ret = LinkGetUploadToken(gtestToken, sizeof(gtestToken), &nDeadline, gfnamePrefix, sizeof(gfnamePrefix), pTokenUrl, NULL, 0);
        assert(ret == LINK_SUCCESS);
        ret = LinkInitTime();
        assert(ret == LINK_SUCCESS);
        fprintf(stderr, "token:%s\n", gtestToken);
        
        LinkPicUploadFullArg arg;
        arg.getPicCallback = getPicCallback;
        arg.getUploadParamCallback = getUploadParamCallback;
        arg.pGetPicCallbackOpaque = NULL;
        arg.pGetUploadParamCallbackOpaque = NULL;
        
        ret = LinkNewPictureUploader(&pPicUploader, &arg);
        assert(ret == LINK_SUCCESS);
        
        int64_t ts = LinkGetCurrentNanosecond() / 1000000;
        while(1) {
                
                LinkSendItIsTimeToCaptureSignal(pPicUploader, ts);
                
                sleep(1);
                
                const char *file = "../../tests/material/3c.jpg";
                int n = strlen(file)+1;
                char * pFile = (char *)malloc(n);
                memcpy(pFile, file, n);
                fprintf(stderr, "send upload signal\n");
                if (gpPictureBuf != NULL)
                        LinkSendUploadPictureToPictureUploader(pPicUploader, gpFilename, strlen(gpFilename), gpPictureBuf, gnPfilenameLen);
                else
                        fprintf(stderr, "gpPictureBuf is NULL\n");
                
                sleep(4);
                ts += 4990;
        }
        
}
