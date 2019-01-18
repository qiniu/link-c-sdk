#include "base.h"
#include "CuTest.h"
#include "log.h"
#include "link_config.h"

extern CuSuite *test_link_queue();
extern CuSuite *test_link_api();

static const struct testlist {
    const char *testname;
    CuSuite *(*func)();
} tests[] = {
    {"test_link_queue", test_link_queue},
    {"test_link_api", test_link_api},
    {"LastTest", NULL}
};

int run_all_tests(int argc, char *argv[])
{
    int i;
    int exit_code;
    int list_provided = 0;
    CuSuite* suite = NULL;
    int j;
    int found;
    CuSuite *st = NULL;
    CuString *output = NULL;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v")) {
            continue;
        }
        if (!strcmp(argv[i], "-l")) {
            for (i = 0; tests[i].func != NULL; i++) {
                printf("%s\n", tests[i].testname);
            }
            exit(0);
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "invalid option: `%s'\n", argv[i]);
            exit(1);
        }
        list_provided = 1;
    }

    suite = CuSuiteNew();

    if (!list_provided) {
        /* add everything */
        for (i = 0; tests[i].func != NULL; i++) {
            st = tests[i].func();
            CuSuiteAddSuite(suite, st);
            CuSuiteFree(st);
        }
    } else {
        /* add only the tests listed */
        for (i = 1; i < argc; i++) {
            found = 0;
            if (argv[i][0] == '-') {
                continue;
            }
            for (j = 0; tests[j].func != NULL; j++) {
                if (!strcmp(argv[i], tests[j].testname)) {
                    CuSuiteAddSuite(suite, tests[j].func());
                    found = 1;
                }
            }
            if (!found) {
                fprintf(stderr, "invalid test name: `%s'\n", argv[i]);
                exit(1);
            }
        }
    }

    output = CuStringNew();
    CuSuiteRun(suite);
    CuSuiteSummary(suite, output);
    CuSuiteDetails(suite, output);
    printf("%s\n", output->buffer);

    exit_code = suite->failCount > 0 ? 1 : 0;

    CuSuiteFreeDeep(suite);
    CuStringFree(output);

    return exit_code;
}

int main(int argc, char *argv[])
{
    int exit_code = -1;

    if (LinkInit() != LINK_SUCCESS) {
        exit(1);
    }

    exit_code = run_all_tests(argc, argv);

    //deinitialize last
    LinkCleanup();

    return exit_code;
}
