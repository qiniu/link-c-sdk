#ifndef __LINK_RESOURCE_MGR_H__
#define __LINK_RESOURCE_MGR_H__

#include "tsuploader.h"
typedef int (*LinkAsynFunction)(void * pOpaque);
typedef struct _LinkAsyncInterface{
        LinkAsynFunction function;
}LinkAsyncInterface;


int LinkStartMgr();
void LinkStopMgr();
int LinkPushFunction(void *pAsyncInterface);

#endif
