#include "tinymem.h"
#include "minunit.h"

#define TABLE_STANDIN NULL

char *all_tests(){
    char *out;
    uint32_t defrags, fills, frees, purges;   // record how many operations have been done
    mu_suite_start();

#ifdef TM_TESTS
    /*mu_run_test(test_tm_pool_new);*/
    /*mu_run_test(test_tm_pool_alloc);*/
    /*mu_run_test(test_tm_free_basic);*/
    /*mu_run_test(test_tm_pool_realloc);*/
    /*mu_run_test(test_tinymem);*/
    mu_test(test_tinymem(
        //  Times                   Indexes                 pool size
            10,                     8000,                   TM_POOL_SIZE,
        //Small size                Large size              Index max skip
            128,                    2048,                   20,
        //  Free distribution       Size distribution       purge distribution
            100,                      20,                     2,
        //  threaded
            false,
        //  return values
            &defrags, &fills, &frees, &purges
    ));
    printf("COMPLETE tinymem_test: fills=%u, frees=%u, defrags=%u, purges=%u\n",
               fills, frees, defrags, purges);
#endif

    return NULL;
}

RUN_TESTS(all_tests);
