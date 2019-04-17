// Last Update:2019-03-20 19:32:30
/**
 * @file testwss.c
 * @brief 
 * @author felix
 * @version 0.1.00
 * @date 2019-03-05
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "transport.h"
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/time.h>
#include <signal.h>

FRTMPHANDLER gHandler;
#define PUSH_TAG 1

typedef struct _Flv {
        FILE *pAFile;
        int aQuit;
        FILE *pVFile;
        int vQuit;
        pthread_t aThread;
        pthread_t vThread;
}Flv;

void printHex(unsigned char *p, int len) {
    int i = 0;
    if (p[0] != 0x08)
	return;
    for (i = 0; i < len; i++) {
	printf("%02x", p[i]);
    }
	printf("\n");
}
void freeCallback(void *pUser, char *pPushTag, int nTagLen) {
    return;
}

static uint64_t lws_time_in_microseconds(void)
{
        struct timeval tv;
        
        gettimeofday(&tv, NULL);
        return ((unsigned long long)tv.tv_sec * 1000000LL) + tv.tv_usec;
}

char gData[70][54] = { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaf, 0x01, 0x21, 0x20, 0x03, 0x40, 0x68, 0x1b, 0xb7, 0x78, 0x00, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x30, 0x00, 0x06, 0x00, 0x38, 0x00, 0x00, 0x00, 0x22}, };
int readNextAudioTag(int execTime) {
    char cf[]={0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaf, 0x00, 0x13, 0x90, 0x56, 0xe5, 0xa5, 0x48, 0x00, 0x00, 0x00, 0x00, 0x14};
        int ret = FRtmpPushTag(gHandler, cf, sizeof(cf) - 16);
        if (ret != 0) {
                fprintf(stderr, "FRtmpPushTag fail:%d\n", ret);
                return ret;
        }

    int t = 0, i = 0;
    for (t = 0; t < execTime; i++) {
	char *data = gData[i%70];
        data[3+1] = (t & 0xFF000000)>>24;
        data[3+2] = (t & 0x00FF0000)>>16;
        data[3+3] = (t & 0x0000FF00)>>8;
        data[3+4] = t & 0xFF;
        ret = FRtmpPushTag(gHandler, data, 54 - 16);
        if (ret != 0) {
                fprintf(stderr, "FRtmpPushTag fail:%d\n", ret);
                return ret;
        }
	usleep(23000);
	t += 23;
    }

    return 0;
}

void * readNextATag(void *pArg) {
        unsigned char buf[LWS_HEADER+4];
        Flv * pFlv = (Flv *)pArg;
        int ret = 0;
        ret = fread(buf, 1, 9+4, pFlv->pAFile);
        int ok = 1;
        if (ret <= 0) {
                if (feof(pFlv->pAFile)) {
                        fprintf(stderr, "read eof\n");
                } else {
                        fprintf(stderr, "read error\n");
                }
                ok = 0;
        }
        uint64_t startTime = lws_time_in_microseconds();
        while(ok) {
                
                ret = fread(buf, 1, 11, pFlv->pAFile);
                if (ret <= 0) {
                        if (feof(pFlv->pAFile)) {
                                fprintf(stderr, "auido read eof\n");
                        } else {
                                fprintf(stderr, "audio read error\n");
                        }
                        break;
                }

                unsigned int ts = (((unsigned int)(buf[4]))<<16) + (((unsigned int)(buf[5]))<<8) + (unsigned int)(buf[6]) + (((unsigned int)(buf[7]))<<24);
                int tagSize = buf[1] * 256 * 256 + buf[2] * 256 + buf[3];
                
                char *pTag = (char *)malloc(tagSize+11+LWS_HEADER+4);
                if (pTag == NULL) {
                        fprintf(stderr, "malloc error\n");
                        break;
                }
                
                memcpy(pTag+LWS_HEADER, buf, 11);
                
                ret = fread(pTag + LWS_HEADER + 11, 1, tagSize+4, pFlv->pAFile);
                if (ret <= 0) {
                        if (feof(pFlv->pAFile)) {
                                fprintf(stderr, "audio read eof\n");
                        } else {
                                fprintf(stderr, "audio read error\n");
                        }
                        break;
                }
                if (buf[0] != 0x08) {
                        free(pTag);
                        continue;
                }
                uint64_t diff = lws_time_in_microseconds() - startTime;
                int64_t stime = (int64_t)ts * 1000 - (int64_t)diff - (int64_t)1000;
                fprintf(stderr, "audio tag type:%d tag size:%d ts:%u sleep:%"PRId64"\n",buf[0], tagSize, ts, stime);
                if (ts > 0) {
                        if (stime > 0) {
                                usleep(stime);
                        }
                }
                //printHex(pTag+LWS_HEADER, tagSize+11+4);
#ifdef PUSH_TAG
                ret = FRtmpPushTag(gHandler, pTag, tagSize+11+4);
                if (ret != 0) {
                        fprintf(stderr, "FRtmpPushTag fail:%d\n", ret);
                        break;
                }
#endif
        }
        pFlv->aQuit = 1;
        return NULL;
}

void * readNextVTag(void *pArg) {
        unsigned char buf[LWS_HEADER+4];
        Flv * pFlv = (Flv *)pArg;
        int ret = 0;
        ret = fread(buf, 1, 9+4, pFlv->pVFile);
        int ok = 1;
        if (ret <= 0) {
                if (feof(pFlv->pVFile)) {
                        fprintf(stderr, "read eof\n");
                } else {
                        fprintf(stderr, "read error\n");
                }
                ok = 0;
        }
        uint64_t startTime = lws_time_in_microseconds();
        while(ok) {
                
                ret = fread(buf, 1, 11, pFlv->pVFile);
                if (ret <= 0) {
                        if (feof(pFlv->pVFile)) {
                                fprintf(stderr, "video read eof\n");
                        } else {
                                fprintf(stderr, "video read error\n");
                        }
                        break;
                }
                
                unsigned int ts = (((unsigned int)(buf[4]))<<16) + (((unsigned int)(buf[5]))<<8) + (unsigned int)(buf[6]) + (((unsigned int)(buf[7]))<<24);
                int tagSize = buf[1] * 256 * 256 + buf[2] * 256 + buf[3];
                
                char *pTag = (char *)malloc(tagSize+11+LWS_HEADER+4);
                if (pTag == NULL) {
                        fprintf(stderr, "malloc error\n");
                        break;
                }
                
                memcpy(pTag+LWS_HEADER, buf, 11);
                
                ret = fread(pTag + LWS_HEADER + 11, 1, tagSize+4, pFlv->pVFile);
                if (ret <= 0) {
                        if (feof(pFlv->pVFile)) {
                                fprintf(stderr, "video read eof\n");
                        } else {
                                fprintf(stderr, "video read error\n");
                        }
                        break;
                }
                if (buf[0] != 0x09) {
                        free(pTag);
                        continue;
                }
                uint64_t diff = lws_time_in_microseconds() - startTime;
                int64_t stime = (int64_t)ts * 1000 - (int64_t)diff - (int64_t)1000;
                fprintf(stderr, "video tag type:%d tag size:%d ts:%u sleep:%"PRId64"\n",buf[0], tagSize, ts, stime);
                if (ts > 0) {
                        if (stime > 0) {
                                usleep(stime);
                        }
                }
                //printHex(pTag+LWS_HEADER, tagSize+11+4);
#ifdef PUSH_TAG
                ret = FRtmpPushTag(gHandler, pTag, tagSize+11+4);
                if (ret != 0) {
                        fprintf(stderr, "FRtmpPushTag fail:%d\n", ret);
                        break;
                }
#endif
        }
        pFlv->vQuit = 1;
        return NULL;
}

int readNextTag(Flv *pFlv) {
        int ret = 0;
        ret = pthread_create(&pFlv->aThread, NULL, readNextATag, pFlv);
        if (ret < 0){
                pFlv->aQuit = 1;
        }
        ret = pthread_create(&pFlv->vThread, NULL, readNextVTag, pFlv);
        if (ret < 0){
                pFlv->vQuit = 1;
        }
        return 0;
}

const char *lws_cmdline_option1(int argc, const char **argv, const char *val)
{
        int n = (int)strlen(val), c = argc;

        while (--c > 0) {

                if (!strncmp(argv[c], val, n)) {
                        if (!*(argv[c] + n) && c < argc - 1) {
                                if (!argv[c + 1] || strlen(argv[c + 1]) > 1024)
                                        return NULL;
                                return argv[c + 1];
                        }

                        return argv[c] + n;
                }
        }

        return NULL;
}

int main(int argc, const char **argv) {
        signal(SIGPIPE, SIG_IGN);
        const char *p = NULL;
	int justAudio = 0, execTime = 600000, timeout = 10;
        Flv flv;
        memset(&flv, 0, sizeof(flv));
        
        if (lws_cmdline_option1(argc, argv, "-ja"))
		justAudio = 1;
        if ((p = lws_cmdline_option1(argc, argv, "-jt")))
	    execTime = atoi(p);
        if ((p = lws_cmdline_option1(argc, argv, "-to")))
                timeout = atoi(p);

        p = lws_cmdline_option1(argc, argv, "-f");
        if (p == NULL)
                p = "/mnt/d/video/a.flv";//"/Users/liuye/Downloads/output.flv";
        fprintf(stderr, "open file:%s\n", p);

	if (!justAudio) {
        	flv.pAFile = fopen(p, "r");
        	if (flv.pAFile == NULL) {
        	        fprintf(stderr, "read fail err:%s\n", strerror(errno));
        	        return -1;
        	}
                flv.pVFile = fopen(p, "r");
                if (flv.pVFile == NULL) {
                        fclose(flv.pAFile);
                        fprintf(stderr, "read fail err:%s\n", strerror(errno));
                        return -1;
                }
        }

        
        if (!(p = lws_cmdline_option1(argc, argv, "-ws")))
                //p = "ws://100.100.33.24:1977/live/testlmk.wsrtmp";
                p = "wss://wss-publish-test.cloudvdn.com/ly-live/lytest.wsrtmp";
        fprintf(stderr, "ws url:%s\n", p);
        //const char* pWsUrl = "ws://127.0.0.1:8082/recv";
        const char *pWsUrl = p;
        
        RtmpSettings settings;
        memset(&settings, 0, sizeof(settings));
        if (!(p = lws_cmdline_option1(argc, argv, "-rtmp")))
            p = "rtmp://wss-publish-test.cloudvdn.com/ly-live/lytest";
        fprintf(stderr, "rtmp url:%s\n", p);

        //settings.pRtmpUrl = "rtmp://127.0.0.1/live/t1";
        settings.pRtmpUrl = (char *)p;
        settings.nRtmpUrlLen =  strlen(settings.pRtmpUrl);
        
        if ((p = lws_cmdline_option1(argc, argv, "-ca"))) {
                settings.pCertFile = (char *)p;
                settings.nCertFileLen = strlen(p);
                fprintf(stderr, "ca file:%s\n", p);
        }
        
        fprintf(stderr, "main thread id:%ld\n", pthread_self());
        
	if (justAudio) {
	    settings.freeCb = freeCallback;
	}
        int ret = 0;
#ifdef PUSH_TAG
        ret = FRtmpWssInit(pWsUrl, strlen(pWsUrl), timeout, &settings, &gHandler);
        if (ret != 0) {
                fprintf(stderr, "FRtmpWssInit fail:%d\n", ret);
                return -1;
        }
#endif
	if (justAudio) {
                while(readNextAudioTag(execTime) == 0) {
                        usleep(10000);
                }
        	

	} else {
                readNextTag(&flv);
                while(!(flv.aQuit && flv.vQuit)) {
                        sleep(1);
                }
	}
        fprintf(stderr, "test end\n");
        FRtmpWssDestroy(&gHandler);
        return 0;
}
