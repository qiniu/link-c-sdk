#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include "libmqtt/qnlinking_mqtt.h"

#include "flag/flag.h"

typedef struct {
        bool IsTESTQnlinkingMQTT;
        int nLoopTime;
        const char *pDak;
        const char *pDsk;
} CmdArg;

int main(int argc, const char * argv[])
{
        CmdArg cmdArg = {false, 0, NULL, NULL};
        time_t t;
        struct tm *tm_now;
        char test_buf[128] = {0};

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

        printf("DAK:%s\nDSK:%s\n", cmdArg.pDak, cmdArg.pDsk);

        QnlinkingMQTT_Init(cmdArg.pDak, strlen(cmdArg.pDak), cmdArg.pDsk, strlen(cmdArg.pDsk));

        if(cmdArg.nLoopTime == 0) {
                for(;;) {
                        t = time(NULL);
                        tm_now = localtime(&t);
                        strftime(test_buf, 64, "[%Y-%m-%d %H:%M:%S]", tm_now);
                        QnlinkingMQTT_SendLog(1, test_buf);
                        memset(test_buf, 0, sizeof(test_buf));
                        sleep(1);
                }
        } else {
                int i;
                for(i = 0; i < cmdArg.nLoopTime; i++) {
                        t = time(NULL);
                        tm_now = localtime(&t);
                        strftime(test_buf, 64, "[%Y-%m-%d %H:%M:%S]", tm_now);
                        QnlinkingMQTT_SendLog(1, test_buf);
                        memset(test_buf, 0, sizeof(test_buf));
                        sleep(1);
                }
        }
        QnlinkingMQTT_Cleanup();
        sleep(3);
        return EXIT_SUCCESS;
}
