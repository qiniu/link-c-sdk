/* config file parser */
/* greg kennedy 2012 */

#include "cfg.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "dbg.h"
#include "main.h"
#include "mymalloc.h"

/* Configuration list structures */
struct cfg_node
{
  char *key;
  char *value;

  struct cfg_node *next;
};

struct cfg_struct
{
  struct cfg_node *head;
};

/* Helper functions */
/*  A malloc() wrapper which handles null return values */
static void *cfg_malloc(unsigned int size)
{
  void *temp = malloc(size);
  if (temp == NULL)
  {
    fprintf(stderr,"CFG_PARSE ERROR: MALLOC(%u) returned NULL (errno=%d)\n",size,errno);
    exit(EXIT_FAILURE);
  }
  return temp;
}

/* Returns a duplicate of input str, without leading / trailing whitespace
    Input str *MUST* be null-terminated, or disaster will result */
static char *cfg_trim(const char *str)
{
  char *tstr = NULL;
  char *temp = (char *)str;

  int temp_len;

  /* advance start pointer to first non-whitespace char */
  while (*temp == ' ' || *temp == '\t' || *temp == '\n')
    temp ++;

  /* calculate length of output string, minus whitespace */
  temp_len = strlen(temp);
  while (temp_len > 0 && (temp[temp_len-1] == ' ' || temp[temp_len-1] == '\t' || temp[temp_len-1] == '\n'))
    temp_len --;

  /* copy portion of string to new string */
  tstr = (char *)malloc(temp_len + 1);
  tstr[temp_len] = '\0';
  memcpy(tstr,temp,temp_len);

  return tstr;
}

/* Load into cfg from a file.  Maximum line size is CFG_MAX_LINE-1 bytes... */
int cfg_load(struct cfg_struct *cfg, const char *filename)
{
  char buffer[CFG_MAX_LINE], *delim;
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) return -1;

  while (!feof(fp))
  {
    if (fgets(buffer,CFG_MAX_LINE,fp) != NULL)
    {
      /* locate first # sign and terminate string there (comment) */
      delim = strchr(buffer, '#');
      if (delim != NULL) *delim = '\0';

      /* locate first = sign and prepare to split */
      delim = strchr(buffer, '=');
      if (delim != NULL)
      {
        *delim = '\0';
        delim ++;

        cfg_set(cfg,buffer,delim);
      }
    }
  }

  fclose(fp);
  return 0;
}

/* Save complete cfg to file */
int cfg_save(struct cfg_struct *cfg, const char *filename)
{
  struct cfg_node *temp = cfg->head;

  FILE *fp = fopen(filename, "w");
  if (fp == NULL) return -1;

  while (temp != NULL)
  {
    if (fprintf(fp,"%s=%s\n",temp->key,temp->value) < 0) { 
      fclose(fp);
      return -2;
    }
    temp = temp->next;
  }
  fclose(fp);
  return 0;
}

/* Get option from cfg_struct */
char * cfg_get(struct cfg_struct *cfg, const char *key)
{
  struct cfg_node *temp = cfg->head;

  char *tkey = cfg_trim(key);

  while (temp != NULL)
  {
    if (strcmp(tkey, temp->key) == 0)
    {
      free(tkey);
      return temp->value;
    }
    temp = temp->next;
  }

  free(tkey);
  return NULL;
}

/* Set option in cfg_struct */
void cfg_set(struct cfg_struct *cfg, const char *key, const char *value)
{
  char *tkey, *tvalue;

  struct cfg_node *temp = cfg->head;

  /* Trim key. */
  tkey = cfg_trim(key);

  /* Exclude empty key */
  if (strcmp(tkey,"") == 0) { free(tkey); return; }

  /* Trim value. */
  tvalue = cfg_trim(value);

  /* Depending on implementation, you may wish to treat blank value
     as a "delete" operation */
  /* if (strcmp(tvalue,"") == 0) { free(tvalue); free(tkey); cfg_delete(cfg,key); return; } */

  /* search list for existing key */
  while (temp != NULL)
  {
    if (strcmp(tkey, temp->key) == 0)
    {
      /* found a match: no longer need temp key */
      free(tkey);

      /* update value */
      free(temp->value);
      temp->value = tvalue;
      return;
    }
    temp = temp->next;
  }

  /* not found: create new element */
  temp = (struct cfg_node *)cfg_malloc(sizeof(struct cfg_node));

  /* assign key, value */
  temp->key = tkey;
  temp->value = tvalue;

  /* prepend */
  temp->next = cfg->head;
  cfg->head = temp;
}

/* Remove option in cfg_struct */
void cfg_delete(struct cfg_struct *cfg, const char *key)
{
  struct cfg_node *temp = cfg->head, *temp2 = NULL;

  char *tkey = cfg_trim(key);

  /* search list for existing key */
  while (temp != NULL)
  {
    if (strcmp(tkey, temp->key) == 0)
    {
      /* cleanup trimmed key */
      free(tkey);

      if (temp2 == NULL)
      {
        /* first element */
        cfg->head = temp->next;
      } else {
        /* splice out element */
        temp2->next = temp->next;
      }

      /* delete element */
      free(temp->value);
      free(temp->key);
      free(temp);

      return;
    }

    temp2 = temp;
    temp = temp->next;
  }

  /* not found */
  /* cleanup trimmed key */
  free(tkey);
}

/* Create a cfg_struct */
struct cfg_struct * cfg_init()
{
  struct cfg_struct *temp;
  temp = (struct cfg_struct *)cfg_malloc(sizeof(struct cfg_struct));
  temp->head = NULL;
  return temp;
}

/* Free a cfg_struct */
void cfg_free(struct cfg_struct *cfg)
{
  struct cfg_node *temp = NULL, *temp2;
  temp = cfg->head;
  while (temp != NULL)
  {
    temp2 = temp->next;
    free(temp->key);
    free(temp->value);
    free(temp);
    temp = temp2;
  }
  free (cfg);
}

void cfg_dump(struct cfg_struct *cfg)
{
    struct cfg_node *temp = cfg->head;


    while (temp != NULL)
    {
        printf("key = %s\n", temp->key );
        printf("value = %s\n", temp->value );
        temp = temp->next;
    }
}

void InitConfig()
{
    gIpc.cfg = cfg_init();

    printf("init config...\n");
    gIpc.config.logOutput = OUTPUT_CONSOLE;
    gIpc.config.logVerbose = 1;
    gIpc.config.logPrintTime = 1;
    gIpc.config.timeStampPrintInterval = TIMESTAMP_REPORT_INTERVAL;
    gIpc.config.heartBeatInterval = 5;
    gIpc.config.defaultLogFile = "/tmp/oem/tsupload.log";
    gIpc.config.tokenRetryCount = TOKEN_RETRY_COUNT;
    gIpc.config.defaultBucketName = "ipcamera";
    gIpc.config.bucketName = NULL;
    gIpc.config.movingDetection = 0;
    gIpc.config.configUpdateInterval = 10;
    gIpc.config.multiChannel = 0;
    gIpc.config.openCache = 1;
    gIpc.config.cacheSize = STREAM_CACHE_SIZE;
    gIpc.config.updateFrom = UPDATE_FROM_FILE;
    gIpc.config.defaultUrl = "http://www.qiniu.com";
    gIpc.config.tokenUrl = NULL;
    gIpc.config.simpleSshEnable = 1;
    gIpc.config.useLocalToken = 0;
    gIpc.config.ak = NULL;
    gIpc.config.sk = NULL;
    gIpc.config.serverIp = NULL;
    gIpc.config.renameTokenUrl = NULL;
    gIpc.config.ota_url = NULL;
    gIpc.config.ota_check_interval = 30;
    gIpc.config.ota_enable = 0;
    if ( gIpc.config.useLocalToken ) {
        gIpc.config.tokenUploadInterval = 5;
    } else {
        gIpc.config.tokenUploadInterval = 3540;
    }

}

void LoadConfig()
{
    if (cfg_load(gIpc.cfg,"/tmp/oem/app/ipc.conf") < 0) {
        fprintf(stderr,"Unable to load ipc.conf\n");
    }
}

int  CfgSaveItem( char *value, char **dest )
{
    char *p = (char *) malloc ( strlen(value)+1 );
    
    if ( !p && !dest ) {
        return -1;
    }
    memset( p, 0, strlen(value)+1 );
    memcpy( p, value, strlen(value) );
    if ( *dest ) {
        free( *dest );
    }

    *dest = p;

    return 0;
}

void CfgGetStrItem( char *item, char **out )
{
    char *p = NULL;

    p = cfg_get( gIpc.cfg, item );
    if ( p ) {
        if ( *out ) {
            if( strcmp( *out, p ) != 0 ) {
                printf("%s %s %d new %s : %s, old : %s \n", __FILE__, __FUNCTION__, __LINE__, item, p, *out );
                CfgSaveItem( p, out );
            }
        } else {
            printf("%s %s %d new %s : %s, old : %s \n", __FILE__, __FUNCTION__, __LINE__, item, p, *out );
            CfgSaveItem( p, out );
        }
    }
}

void CfgGetIntItem( char *item, int *out )
{
    char *p = NULL;
    int val = 0;

    p = cfg_get( gIpc.cfg, item );
    if ( p ) {
        val = atoi( p );
        if ( out ) {
            if ( *out != val ) {
               *out = val; 
               printf("%s %s %d new value of %s is %d\n", __FILE__, __FUNCTION__, __LINE__, item, val );
            }
        } 
    }
}


static CfgItem cfg_items[] =
{
    { CFG_MEMBER(logFile), "LOG_FILE", 1 },
    { CFG_MEMBER(audioType), "AUDIO_ENCODE_TYPE", 1 },
    { CFG_MEMBER(h264_file), "H264_FILE", 1 },
    { CFG_MEMBER(aac_file), "AAC_FILE", 1 },
    { CFG_MEMBER(ak), "DAK", 1 },
    { CFG_MEMBER(sk), "DSK", 1 },
    { CFG_MEMBER(url), "NETWORK_CHECK_URL", 1 },
    { CFG_MEMBER(tokenUrl), "GET_CONFIG_URL", 1 },
    { CFG_MEMBER(serverIp), "SERVER_IP", 1 },
    { CFG_MEMBER(ota_url), "OTA_URL", 1 },
    { CFG_MEMBER(bucketName), "BUCKET_NAME", 1 },
    { CFG_MEMBER(appName), "APP_NAME", 1 },
    { CFG_MEMBER(movingDetection), "MOUTION_DETECTION", 0 },
    { CFG_MEMBER(openCache), "OPEN_CACHE", 0 },
    { CFG_MEMBER(multiChannel), "MULTI_CHANNEL", 0 },
    { CFG_MEMBER(useLocalToken), "USE_LOCAL_TOKEN", 0 },
    { CFG_MEMBER(serverPort), "SERVER_PORT", 0 },
    { CFG_MEMBER(simpleSshEnable), "SIMPLE_SSH", 0 },
    { CFG_MEMBER(ota_check_interval), "OTA_CHECK_INTERVAL", 0 },
    { CFG_MEMBER(ota_enable), "OTA_ENABLE", 0 },
};

void CfgGetItem()
{
    int i = 0;

    for ( i=0; i<ARRSZ(cfg_items); i++ ) {
        if ( cfg_items[i].isStr ) {
            CfgGetStrItem( cfg_items[i].item, cfg_items[i].save );
        } else {
            CfgGetIntItem( cfg_items[i].item, (int *)cfg_items[i].save );
        }
    }
}

void DumpConfig()
{
#if 0
    printf("log_output : %d\n", gIpc.config.logOutput );
    printf("h264_file : %s\n", gIpc.config.h264_file );
    printf("ota_enable : %d\n", gIpc.config.ota_enable );
#endif
}

void UpdateConfig()
{
    char *logOutput = NULL;
    static int last = 0;

    if ( cfg_load(gIpc.cfg,"/tmp/oem/app/ipc.conf") < 0) {
        fprintf(stderr,"Unable to load ipc.conf\n");
        return;
    }

    logOutput = cfg_get( gIpc.cfg, "LOG_OUTPUT" );
    if ( logOutput ) {
        if ( strcmp( logOutput, "socket") == 0 ) {
            gIpc.config.logOutput = OUTPUT_SOCKET;
        } else if ( strcmp(logOutput, "console" ) == 0 ) {
            gIpc.config.logOutput = OUTPUT_CONSOLE;
        } else if ( strcmp( logOutput, "mqtt") == 0  ) {
            gIpc.config.logOutput = OUTPUT_MQTT;
        } else if ( strcmp ( logOutput, "file") == 0 ) {
            gIpc.config.logOutput = OUTPUT_FILE;
        } else {
            gIpc.config.logOutput = OUTPUT_SOCKET;
        }
    }

    if ( last ) {
        if ( last != gIpc.config.logOutput ) {
            last = gIpc.config.logOutput;
            printf("%s %s %d reinit the logger, logOutput = %s\n", __FILE__, __FUNCTION__, __LINE__, logOutput );
            LoggerInit( gIpc.config.logPrintTime, gIpc.config.logOutput, gIpc.config.logFile, gIpc.config.logVerbose );
        }
    } else {
        last = gIpc.config.logOutput;
    }

    CfgGetItem();

    if ( gIpc.config.useLocalToken ) {
        gIpc.config.tokenUploadInterval = 5;
    } else {
        gIpc.config.tokenUploadInterval = 3540;
    }
    DumpConfig();
}

