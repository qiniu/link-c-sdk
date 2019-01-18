#include <string.h>
#include "CuTest.h"
#include "../../libtsuploader/uploader.h"

void test_api_setup(CuTest *tc)
{

}

void test_api_cleanup(CuTest *tc)
{

}

void test_api_verify(CuTest *tc)
{
        char* ak_true = "LEKGY02T3LOt2LFiAYn75hWml8U_orS3V83ekdtr";
        char* sk_true = "gyt4BSk7CaMxxx-tdN__eYgLCNbRtfIkYsdx0gXr";
        char* token_true = "LEKGY02T3LOt2LFiAYn75hWml8U_orS3V83ekdtr:TNOr0hwecMIZMn_3URlmO6ibHsM=:eyJkZWFkbGluZSI6MTU0Nzc5MSwicmFuZG9tIjoxNTQ3NzkxNDU2LCJzdGF0ZW1lbnQiOnsiYWN0aW9uIjoibGlua2luZzp0dXRrIn19";

        CuAssertIntEquals(tc, LINK_TRUE, LinkVerify(ak_true, strlen(ak_true), sk_true, strlen(sk_true), token_true, strlen(token_true)));

        char* ak_false = "XEKGY02T3LOt2LFiAYn75hWml8U_orS3V83ekdtr";
        char* sk_false = "xyt4BSk7CaMxxx-tdN__eYgLCNbRtfIkYsdx0gXr";
        char* token_false = "XEKGY02T3LOt2LFiAYn75hWml8U_orS3V83ekdtr:TNOr0hwecMIZMn_3URlmO6ibHsM=:eyJkZWFkbGluZSI6MTU0Nzc5MSwicmFuZG9tIjoxNTQ3NzkxNDU2LCJzdGF0ZW1lbnQiOnsiYWN0aW9uIjoibGlua2luZzp0dXRrIn1X";

        CuAssertIntEquals(tc, LINK_FALSE, LinkVerify(ak_false, strlen(ak_false), sk_false, strlen(sk_false), token_false, strlen(token_false)));
}


CuSuite *test_link_api(CuTest *tc)
{
        CuSuite* suite = CuSuiteNew();

        SUITE_ADD_TEST(suite, test_api_setup);
        SUITE_ADD_TEST(suite, test_api_verify);
        SUITE_ADD_TEST(suite, test_api_cleanup);

        return suite;
}
