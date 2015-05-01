#include "tinymem.h"
#include "minunit.h"

#define TABLE_STANDIN NULL

char *all_tests(){
    char *out;
    mu_suite_start();

#ifdef TM_TESTS
    /*mu_run_test(test_tm_pool_new);*/
    /*mu_run_test(test_tm_pool_alloc);*/
    /*mu_run_test(test_tm_free_basic);*/
    /*mu_run_test(test_tm_pool_realloc);*/
    mu_run_test(test_tinymem);
#endif

    return NULL;
}

RUN_TESTS(all_tests);
