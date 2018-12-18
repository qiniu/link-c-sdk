#include "CuTest.h"
#include "../../libtsuploader/queue.h"

void test_queue_setup(CuTest *tc)
{

}

void test_queue_cleanup(CuTest *tc)
{

}

void test_queue_create_destroy(CuTest *tc)
{
    LinkQueue *pQueue = NULL;
    /* test function LinkNewCircleQueue */
    CuAssertIntEquals(tc, LinkNewCircleQueue(NULL, sizeof(int), 100, LQP_NONE), LINK_QUEUE_ERROR);
    CuAssertIntEquals(tc, LinkNewCircleQueue(&pQueue, 0, 100, LQP_NONE), LINK_QUEUE_ERROR);
    CuAssertIntEquals(tc, LinkNewCircleQueue(&pQueue, sizeof(int), 0, LQP_NONE), LINK_QUEUE_ERROR);

    CuAssertIntEquals(tc, LinkNewCircleQueue(&pQueue, sizeof(int), 100, LQP_NONE), LINK_QUEUE_SUCCESS);
    CuAssertPtrNotNull(tc, pQueue);

    /* test function LinkDestroyQueue */
    CuAssertIntEquals(tc, LinkDestroyQueue(NULL), LINK_QUEUE_ERROR);
    CuAssertIntEquals(tc, LinkDestroyQueue(&pQueue), LINK_QUEUE_SUCCESS);
    CuAssertPtrEquals(tc, pQueue, NULL);
    CuAssertIntEquals(tc, LinkDestroyQueue(&pQueue), LINK_QUEUE_ERROR);
    CuAssertPtrEquals(tc, pQueue, NULL);
}

void test_queue_fix_length(CuTest *tc)
{
    int i, ret, nBuf;
    char * pBuf = &nBuf;
    size_t len = 0;
    LinkQueue *pQueue = NULL;
    CuAssertIntEquals(tc, LinkNewCircleQueue(&pQueue, sizeof(int), 100, LQP_NONE), LINK_QUEUE_SUCCESS);
    CuAssertPtrNotNull(tc, pQueue);

    /* test function pushQueueNormal popQueueNormal */
    CuAssertIntEquals(tc, pQueue->Push(pQueue, NULL, 0), LINK_QUEUE_ERROR);
    CuAssertIntEquals(tc, pQueue->Push(NULL, NULL, sizeof(int)), LINK_QUEUE_ERROR);

    CuAssertIntEquals(tc, pQueue->Pop(NULL, pBuf, len, 0), LINK_QUEUE_ERROR);

    /* test fix length queue normal push and pop */
    for (i = 0; i < 100; i++) {
        CuAssertIntEquals(tc, pQueue->Push(pQueue, (char* )&i, sizeof(int)), sizeof(int));
    }

    for (i = 0; i < 50; i++) {
        CuAssertIntEquals(tc, pQueue->Pop(pQueue, pBuf, sizeof(int), 0), sizeof(int));
        CuAssertIntEquals(tc, *(int* )pBuf, i);
    }
    for (i = 100; i < 150; i++) {
        CuAssertIntEquals(tc, pQueue->Push(pQueue, (char* )&i, sizeof(int)), sizeof(int));
    }

    for (i = 50; i < 150; i++) {
        CuAssertIntEquals(tc, pQueue->Pop(pQueue, pBuf, sizeof(int), 0), sizeof(int));
        CuAssertIntEquals(tc, *(int* )pBuf, i);
    }
    LinkDestroyQueue(&pQueue);
}

void test_queue_var_length(CuTest *tc)
{
    int i, ret, nBuf;
    char * pBuf = &nBuf;
    LinkQueue *pQueue = NULL;
    CuAssertIntEquals(tc, LinkNewCircleQueue(&pQueue, sizeof(int), 1, LQP_VARIABLE_LENGTH), LINK_QUEUE_SUCCESS);
    CuAssertPtrNotNull(tc, pQueue);

    /* test variable length queue normal push and pop 1 */

    for (i = 0; i < 10000; i++) {
        CuAssertIntEquals(tc, pQueue->Push(pQueue, (char* )&i, sizeof(int)), sizeof(int));
    }

    for (i = 0; i < 10000; i++) {
        CuAssertIntEquals(tc, pQueue->Pop(pQueue, pBuf, sizeof(int), 0), sizeof(int));
        CuAssertIntEquals(tc, *(int* )pBuf, i);
    }

    /* test variable length queue normal push and pop 2 */
    for (i = 0; i < 60; i++) {
        CuAssertIntEquals(tc, pQueue->Push(pQueue, (char* )&i, sizeof(int)), sizeof(int));
    }

    for (i = 0; i < 30; i++) {
        CuAssertIntEquals(tc, pQueue->Pop(pQueue, pBuf, sizeof(int), 0), sizeof(int));
        CuAssertIntEquals(tc, *(int* )pBuf, i);
    }

    for (i = 60; i < 1000; i++) {
        CuAssertIntEquals(tc, pQueue->Push(pQueue, (char* )&i, sizeof(int)), sizeof(int));
    }

    for (i = 30; i < 1000; i++) {
        CuAssertIntEquals(tc, pQueue->Pop(pQueue, pBuf, sizeof(int), 0), sizeof(int));
        CuAssertIntEquals(tc, *(int* )pBuf, i);
    }

    LinkDestroyQueue(&pQueue);

}

void test_queue_append_mode(CuTest *tc)
{
    int ret, i;
    int nTestSize = 10000;
    char * pBuf;
    size_t len;
    LinkQueue *pQueue = NULL;
    CuAssertIntEquals(tc, LinkNewCircleQueue(&pQueue, sizeof(int), 1, LQP_BYTE_APPEND), LINK_QUEUE_SUCCESS);
    CuAssertPtrNotNull(tc, pQueue);

    /* test function pushQueueByteAppend popQueueByteAppend */
    CuAssertIntEquals(tc, pQueue->Push(pQueue, NULL, 0), LINK_QUEUE_ERROR);
    CuAssertIntEquals(tc, pQueue->Push(NULL, NULL, sizeof(int)), LINK_QUEUE_ERROR);

    CuAssertIntEquals(tc, pQueue->Pop(NULL, NULL, 0, 0), LINK_QUEUE_ERROR);

    /* test function byte append push and pop */
    for (i = 0; i < nTestSize; i++) {
        CuAssertIntEquals(tc, pQueue->Push(pQueue, (char* )&i, sizeof(int)), sizeof(int));
    }

    pBuf = (char*) malloc(nTestSize * sizeof(int));
    CuAssertPtrNotNull(tc, pBuf);
    CuAssertIntEquals(tc, pQueue->Pop(pQueue, pBuf, 100, 0), LINK_QUEUE_NO_SPACE);
    CuAssertIntEquals(tc, pQueue->Pop(pQueue, pBuf, nTestSize * sizeof(int), 0), nTestSize * sizeof(int));
    int * pTmp = (int*) pBuf;
    for (i = 0; i < nTestSize; i++) {
        CuAssertIntEquals(tc, *pTmp, i);
        pTmp++;
    }
    free(pBuf);
    LinkDestroyQueue(&pQueue);
}

void test_queue_info(CuTest *tc)
{
    int i, ret, nBuf;
    char * pBuf = &nBuf;
    LinkQueue *pQueue = NULL;
    LinkQueueInfo info = { 0 };

    /* test state nomal mode */
    CuAssertIntEquals(tc, LinkNewCircleQueue(&pQueue, sizeof(int), 100, LQP_NONE), LINK_QUEUE_SUCCESS);

    CuAssertIntEquals(tc, pQueue->GetInfo(NULL, NULL), LINK_QUEUE_ERROR);

    CuAssertIntEquals(tc, pQueue->GetInfo(pQueue, &info), LINK_QUEUE_SUCCESS);
    CuAssertIntEquals(tc, info.state & LQS_EMPTY, LQS_EMPTY);
    for (i = 0; i < 100; i++) {
        CuAssertIntEquals(tc, pQueue->Push(pQueue, (char* )&i, sizeof(int)), sizeof(int));
    }
    CuAssertIntEquals(tc, pQueue->GetInfo(pQueue, &info), LINK_QUEUE_SUCCESS);
    CuAssertIntEquals(tc, info.state, LQS_FULL);

    for (i = 0; i < 100; i++) {
        CuAssertIntEquals(tc, pQueue->Pop(pQueue, pBuf, sizeof(int), 0), sizeof(int));
    }

    CuAssertIntEquals(tc, pQueue->GetInfo(pQueue, &info), LINK_QUEUE_SUCCESS);
    CuAssertIntEquals(tc, info.state, LQS_EMPTY);

    LinkDestroyQueue(&pQueue);

    /* test state overwirte mode */
    CuAssertIntEquals(tc, LinkNewCircleQueue(&pQueue, sizeof(int), 100, LQP_OVERWRITEABLE), LINK_QUEUE_SUCCESS);

    CuAssertIntEquals(tc, pQueue->GetInfo(pQueue, &info), LINK_QUEUE_SUCCESS);
    CuAssertIntEquals(tc, info.state, LQS_EMPTY);
    for (i = 0; i < 101; i++) {
        if (i < 100) {
            CuAssertIntEquals(tc, pQueue->Push(pQueue, (char* )&i, sizeof(int)), sizeof(int));
        } else {
            CuAssertIntEquals(tc, pQueue->Push(pQueue, (char* )&i, sizeof(int)), LINK_QUEUE_OVERWRIT);
        }
    }
    CuAssertIntEquals(tc, pQueue->GetInfo(pQueue, &info), LINK_QUEUE_SUCCESS);
    CuAssertIntEquals(tc, info.state, LQS_FULL | LQS_OVERWIRTE);

    for (i = 0; i < 99; i++) {
        CuAssertIntEquals(tc, pQueue->Pop(pQueue, pBuf, sizeof(int), 0), sizeof(int));
    }

    CuAssertIntEquals(tc, pQueue->GetInfo(pQueue, &info), LINK_QUEUE_SUCCESS);
    CuAssertIntEquals(tc, info.state, LQS_OVERWIRTE);

    LinkDestroyQueue(&pQueue);
}

void test_queue_property(CuTest *tc)
{
    int i, ret, nBuf;
    char * pBuf = &nBuf;
    LinkQueue *pQueue = NULL;
    LinkQueueProperty property = LQP_NONE;

    CuAssertIntEquals(tc, LinkNewCircleQueue(&pQueue, sizeof(int), 100, LQP_NONE), LINK_QUEUE_SUCCESS);

    CuAssertIntEquals(tc, pQueue->SetProperty(NULL, NULL), LINK_QUEUE_ERROR);

    property = LQP_NO_PUSH;
    CuAssertIntEquals(tc, pQueue->SetProperty(pQueue, &property), LINK_QUEUE_SUCCESS);
    CuAssertIntEquals(tc, pQueue->Push(pQueue, (char*) &i, sizeof(int)), LINK_QUEUE_NO_PUSH);

    property = LQP_NO_POP;
    CuAssertIntEquals(tc, pQueue->SetProperty(pQueue, &property), LINK_QUEUE_SUCCESS);
    CuAssertIntEquals(tc, pQueue->Pop(pQueue, pBuf, sizeof(int), 0), 0);

    LinkDestroyQueue(&pQueue);
}

CuSuite *test_link_queue(CuTest *tc)
{
    CuSuite* suite = CuSuiteNew();

    SUITE_ADD_TEST(suite, test_queue_setup);
    SUITE_ADD_TEST(suite, test_queue_create_destroy);
    SUITE_ADD_TEST(suite, test_queue_fix_length);
    SUITE_ADD_TEST(suite, test_queue_var_length);
    SUITE_ADD_TEST(suite, test_queue_append_mode);
    SUITE_ADD_TEST(suite, test_queue_info);
    SUITE_ADD_TEST(suite, test_queue_property);
    SUITE_ADD_TEST(suite, test_queue_cleanup);

    return suite;
}
