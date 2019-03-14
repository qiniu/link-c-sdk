#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include "libmqtt/qnlinking_mqtt.h"
#include "flag/flag.h"





int static test_qnlinking(const char * pDak, int nDakLen, const char * pDsk, int nDskLen, int IsLoop)
{
        time_t t;
        struct tm *tm_now;
        char test_buf[128] = {0};
        QnlinkingMQTT_Init(pDak, nDakLen, pDsk, nDskLen);

        if(IsLoop == 0) {
                for(;;) {
                        t = time(NULL);
                        tm_now = localtime(&t);
                        strftime(test_buf, 64, "[%Y-%m-%d %H:%M:%S]\n", tm_now);
                        QnlinkingMQTT_SendLog(1, test_buf);
                        printf("%s Link status: %s", test_buf, QnlinkingMQTT_Status());
                        memset(test_buf, 0, sizeof(test_buf));
                        sleep(1);
                }
        } else {
                int i;
                for(i = 0; i < IsLoop; i++) {
                        t = time(NULL);
                        tm_now = localtime(&t);
                        strftime(test_buf, 64, "[%Y-%m-%d %H:%M:%S]\n", tm_now);
                        QnlinkingMQTT_SendLog(1, test_buf);
                        memset(test_buf, 0, sizeof(test_buf));
                        sleep(1);
                }
        }
        QnlinkingMQTT_Cleanup();
        sleep(3);

        return EXIT_SUCCESS;
}


typedef struct {
        bool IsTESTQnlinkingMQTT;
        int nLoopTime;
        const char *pDak;
        const char *pDsk;
} CmdArg;

int main(int argc, const char * argv[])
{
        int ret = EXIT_FAILURE;
        CmdArg cmdArg = {false, 0, NULL, NULL};


        flag_bool(&cmdArg.IsTESTQnlinkingMQTT, "qnlink", "test qnlinking mqtt");
        flag_str(&cmdArg.pDak, "dak", "device ak");
        flag_str(&cmdArg.pDsk, "dsk", "device sk");
        flag_int(&cmdArg.nLoopTime, "loop", "loop time");
        flag_parse(argc, argv, "test libmqtt");
        if (!cmdArg.pDak) {
                cmdArg.pDak = getenv("LINK_TEST_DAK");
                if (!cmdArg.pDak) {
                        printf("No DAK specified.\n");
                        exit(EXIT_FAILURE);
                }
        }
        if (!cmdArg.pDsk) {
                cmdArg.pDsk = getenv("LINK_TEST_DSK");
                if (!cmdArg.pDsk) {
                        printf("No DSK specified.\n");
                        exit(EXIT_FAILURE);
                }
        }

        if (cmdArg.IsTESTQnlinkingMQTT) {
                ret = test_qnlinking(cmdArg.pDak, strlen(cmdArg.pDak),
                                cmdArg.pDsk,strlen(cmdArg.pDsk), cmdArg.nLoopTime);
        } else {
                printf("NO test to run.\n");
                ret = EXIT_FAILURE;
        }

        return ret;
}
