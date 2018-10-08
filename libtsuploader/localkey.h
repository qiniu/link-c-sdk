#ifndef __TS_LOCAL_TEST__
#define __TS_LOCAL_TEST__

int LinkGetUploadToken(OUT char *pBuf, IN int nBufLen, IN char *pUrl);
void LinkSetAk(IN char *pAk);
void LinkSetSk(IN char *pSk);
void LinkSetBucketName(IN char *_pName);
void LinkSetCallbackUrl(IN char *pUrl);
void LinkSetDeleteAfterDays(IN int days);


#endif
