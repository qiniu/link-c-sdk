/* config file parser */
/* Greg Kennedy 2012 */

#ifndef CFG_STRUCT_H_
#define CFG_STRUCT_H_

#define CFG_MAX_LINE 1024
#define CFG_MEMBER( mem ) (void *)&gIpc.config.mem 
#define ARRSZ( arr ) sizeof(arr)/sizeof(arr[0])
typedef struct {
    char **save;
    void *item; 
    int isStr;
} CfgItem;

typedef struct {
    unsigned char logOutput;
    unsigned char logVerbose;
    unsigned char logPrintTime;
    int timeStampPrintInterval;
    unsigned char heartBeatInterval;
    char *logFile;
    char *defaultLogFile;
    int tokenUploadInterval;
    int tokenRetryCount;
    char *bucketName;
    char *defaultBucketName;
    char *ak;
    char *sk;
    unsigned char movingDetection;
    int configUpdateInterval;
    unsigned char multiChannel;
    unsigned char openCache;
    int cacheSize;
    unsigned char updateFrom;
    char *url;
    char *defaultUrl;
    char *tokenUrl;
    unsigned char simpleSshEnable;
    unsigned char useLocalToken;
    char *serverIp;
    int serverPort;
    char *renameTokenUrl;
    char *h264_file;
    char *aac_file;
    char *ota_url;
    char *appName;
    int ota_check_interval;
    int ota_enable;
    char *audioType;
    
    /* mqtt */
    char *mqtt_server;
    char *mqtt_topic;
    char *mqtt_user;
    char *mqtt_passwd;
    char *client_id;
    int mqtt_port;

    char *otaMode;
    char *mqttOtaTopic;
    char *mqttOutTopic;
    char *mqttInTopic;
} Config;

struct cfg_struct;

/* Create a cfg_struct */
struct cfg_struct * cfg_init();

/* Free a cfg_struct */
void cfg_free(struct cfg_struct *);


/* Load into cfg from a file */
int cfg_load(struct cfg_struct *, const char *);

/* Save complete cfg to file */
int cfg_save(struct cfg_struct *, const char *);


/* Get value from cfg_struct by key */
char * cfg_get(struct cfg_struct *, const char *);

/* Set key,value in cfg_struct */
void cfg_set(struct cfg_struct *, const char *, const char *);

/* Delete key (+value) from cfg_struct */
void cfg_delete(struct cfg_struct *, const char *);
void cfg_dump(struct cfg_struct *cfg);

void InitConfig();
void LoadConfig();
void UpdateConfig();


#endif
