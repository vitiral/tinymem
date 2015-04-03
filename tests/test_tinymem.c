#include "minunit.h"
#include "tm_pool.h"
#include "tm_freed.h"

#define TABLE_STANDIN NULL


char *test_basic(){
    return NULL;
}

char *all_tests(){
    mu_suite_start();

    mu_run_test(test_basic);
    return NULL;
}

RUN_TESTS(all_tests);
