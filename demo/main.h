// Last Update:2018-11-20 15:52:41
/**
 * @file ipc_test.h
 * @brief 
 * @author liyq
 * @version 0.1.00
 * @date 2018-08-27
 */

#ifndef IPC_TEST_H
#define IPC_TEST_H

#include "tools/queue.h"
#include "uploader.h"
#include "dev_core.h"
#include "cfg.h"

#define IPC_TRACE_TIMESTAMP 1
#define TIMESTAMP_REPORT_INTERVAL 20
#define TOKEN_RETRY_COUNT 1000
#define G711_TIMESTAMP_INTERVAL 40
#define FRAME_DATA_LEN 1024
#define STREAM_CACHE_SIZE 75
#define TOKEN_LENGTH 1024
#define DEV_ID_LEN 128


typedef enum {
    UPDATE_FROM_SOCKET,
    UPDATE_FROM_FILE,
} UpdateType;

typedef struct {
    char *data;
    unsigned char isKey;
    double timeStamp;
    int len;
    int64_t nCurSysTime;
} Frame;

typedef struct {
    Queue *videoCache;
    Queue *audioCache;
    Queue *jpegQ;
    char devId[DEV_ID_LEN];
    char token[TOKEN_LENGTH];
    LinkTsMuxUploader *uploader;
    void *pOpaque;
} Stream;

typedef struct {
    struct cfg_struct *cfg;
    Config config;
    unsigned char audioType;
    char devId[DEV_ID_LEN];
    CoreDevice *dev;
    unsigned char detectMoving;
    char *version;
    Stream stream[STREAM_MAX];
    int running;
} App;

extern App gIpc;
extern int UploadFile2Kodo( char *filename, char *localFile );

#endif  /*IPC_TEST_H*/
