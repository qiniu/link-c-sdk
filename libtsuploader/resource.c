#include "resource.h"
#include "base.h"

typedef struct _ResourceMgr
{
        LinkCircleQueue * pQueue_;
        pthread_t mgrThreadId_;
        int nQuit_;
        int nIsStarted_;
}ResourceMgr;

static ResourceMgr manager;

static void * recycle(void *_pOpaque)
{
        LinkUploaderStatInfo info = {0};
        manager.pQueue_->GetStatInfo(manager.pQueue_, &info);
        while(!manager.nQuit_ && info.nLen_ == 0) {
                LinkAsyncInterface *pAsync = NULL;
                int ret = manager.pQueue_->PopWithTimeout(manager.pQueue_, (char *)(&pAsync), sizeof(LinkAsyncInterface *), 24 * 60 * 60);
                LinkUploaderStatInfo info;
                manager.pQueue_->GetStatInfo(manager.pQueue_, &info);
                LinkLogDebug("thread queue:%d", info.nLen_);
                if (ret == LINK_TIMEOUT) {
                        continue;
                }
                if (ret == sizeof(LinkTsUploader *)) {
                        LinkLogInfo("pop from mgr:%p\n", pAsync);
                        if (pAsync == NULL) {
                                LinkLogWarn("NULL function");
                        } else {
                                LinkAsynFunction func = pAsync->function;
                                func(pAsync);
                        }
                }
                manager.pQueue_->GetStatInfo(manager.pQueue_, &info);
        }

	return NULL;
}

int LinkPushFunction(void *_pAsyncInterface)
{
        if (!manager.nIsStarted_) {
                return -1;
        }
        return manager.pQueue_->Push(manager.pQueue_, (char *)(&_pAsyncInterface), sizeof(LinkAsyncInterface *));
}

int LinkStartMgr()
{
        if (manager.nIsStarted_) {
                return LINK_SUCCESS;
        }
        
        int ret = LinkNewCircleQueue(&manager.pQueue_, 1, TSQ_FIX_LENGTH, sizeof(void *), 100);
        if (ret != 0){
                return ret;
        }
        
        ret = pthread_create(&manager.mgrThreadId_, NULL, recycle, NULL);
        if (ret != 0) {
                manager.nIsStarted_ = 0;
                return LINK_THREAD_ERROR;
        }
        manager.nIsStarted_ = 1;
        
        return LINK_SUCCESS;
}

void LinkStopMgr()
{
        manager.nQuit_ = 1;
        if (manager.nIsStarted_) {
                LinkPushFunction(NULL);
                pthread_join(manager.mgrThreadId_, NULL);
                manager.nIsStarted_ = 0;
                if (manager.pQueue_) {
                        LinkDestroyQueue(&manager.pQueue_);
                }
        }
        return;
}
