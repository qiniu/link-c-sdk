#include "CuTest.h"
#include "../../libtsuploader/queue.h"
#include "../../libtsuploader/base.h"

void test_queue_setup(CuTest *tc)
{

}

void test_queue_cleanup(CuTest *tc)
{

}

void test_queue_create_destroy(CuTest *tc)
{
        LinkCircleQueue *pQueue = NULL;
        /* test function LinkNewCircleQueue */
        CuAssertIntEquals(tc, LINK_ERROR, LinkNewCircleQueue(NULL, 1, TSQ_FIX_LENGTH, sizeof(int), 100));
        CuAssertIntEquals(tc, LINK_ERROR, LinkNewCircleQueue(&pQueue, 1, TSQ_FIX_LENGTH, 0, 100));
        CuAssertIntEquals(tc, LINK_ERROR, LinkNewCircleQueue(&pQueue, 1, TSQ_FIX_LENGTH, sizeof(int), 0));

        CuAssertIntEquals(tc, LINK_SUCCESS, LinkNewCircleQueue(&pQueue, 1, TSQ_FIX_LENGTH, sizeof(int), 100));
        CuAssertPtrNotNull(tc, pQueue);

        /* test function LinkDestroyQueue */
        CuAssertIntEquals(tc, LINK_ERROR, LinkDestroyQueue(NULL));
        CuAssertIntEquals(tc, LINK_SUCCESS, LinkDestroyQueue(&pQueue));
        CuAssertPtrEquals(tc, NULL, pQueue);
        CuAssertIntEquals(tc, LINK_ERROR, LinkDestroyQueue(&pQueue));
        CuAssertPtrEquals(tc, NULL, pQueue);
}

void test_queue_fix_length(CuTest *tc)
{
        int i, ret, nBuf;
        char * pBuf = (char*)&nBuf;
        size_t len = 0;
        LinkCircleQueue *pQueue = NULL;
        CuAssertIntEquals(tc, LINK_SUCCESS, LinkNewCircleQueue(&pQueue, 1, TSQ_FIX_LENGTH, sizeof(int), 100));
        CuAssertPtrNotNull(tc, pQueue);

        /* test function pushQueueNormal popQueueNormal */
        CuAssertIntEquals(tc, LINK_ERROR, pQueue->Push(pQueue, NULL, 0));
        CuAssertIntEquals(tc, LINK_ERROR, pQueue->Push(NULL, NULL, sizeof(int)));
        CuAssertIntEquals(tc, LINK_TIMEOUT, pQueue->PopWithTimeout(pQueue, pBuf, len, 0));

        /* test fix length queue normal push and pop */
        for (i = 0; i < 100; i++) {
                CuAssertIntEquals(tc, pQueue->Push(pQueue, (char* )&i, sizeof(int)), sizeof(int));
        }

        for (i = 0; i < 50; i++) {
                CuAssertIntEquals(tc, sizeof(int), pQueue->PopWithTimeout(pQueue, pBuf, sizeof(int), 24 * 60 * 60));
                CuAssertIntEquals(tc, i, *(int* )pBuf);
        }
        for (i = 100; i < 150; i++) {
                CuAssertIntEquals(tc, sizeof(int), pQueue->Push(pQueue, (char* )&i, sizeof(int)));
        }

        for (i = 50; i < 150; i++) {
                CuAssertIntEquals(tc, sizeof(int), pQueue->PopWithTimeout(pQueue, pBuf, sizeof(int), 24 * 60 * 60));
                CuAssertIntEquals(tc, i, *(int* )pBuf);
        }
        LinkDestroyQueue(&pQueue);
}

void test_queue_append_mode(CuTest *tc)
{
// TODO
}

CuSuite *test_link_queue(CuTest *tc)
{
        CuSuite* suite = CuSuiteNew();

        SUITE_ADD_TEST(suite, test_queue_setup);
        SUITE_ADD_TEST(suite, test_queue_create_destroy);
        SUITE_ADD_TEST(suite, test_queue_fix_length);
        SUITE_ADD_TEST(suite, test_queue_append_mode);
        SUITE_ADD_TEST(suite, test_queue_cleanup);

        return suite;
}
