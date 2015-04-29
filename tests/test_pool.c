#include "minunit.h"
#include "tinymem.h"
#include "time.h"

#define TABLE_STANDIN NULL

char *all_tests(){
    mu_suite_start();

    mu_run_test(test_tm_pool_new);
    mu_run_test(test_tm_pool_alloc);
    mu_run_test(test_tm_free_basic);
    mu_run_test(test_tm_pool_realloc);
    return NULL;
}

RUN_TESTS(all_tests);
