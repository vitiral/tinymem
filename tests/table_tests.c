#include "minunit.h"
#include "tm_pool.h"

#define TABLE_STANDIN NULL



char *test_tm_pool_new(){
    tm_index i;
    tm_index size = 100;
    Pool *pool;

    pool = Pool_new(size);
    mu_assert(TM_MAX_FILLED_PTRS == 254 / 8 + 1, "MAX_POOL_PTRS");
    mu_assert(pool, "Pool was malloced");
    mu_assert(pool->heap == 1, "heap NULL");
    mu_assert(pool->size == size, "pool size");
    mu_assert(Pool_heap_left(pool) == size - 1, "pool left");

    mu_assert(pool->filled[0] == 0, "filled NULL");
    mu_assert(pool->filled[TM_MAX_FILLED_PTRS - 1] == 0x00, "filled end");

    mu_assert(pool->points[0] == 1, "points NULL");
    /*printf("%u == %u\n", pool->points[TM_MAX_FILLED_PTRS - 1], 0xFC);*/
    mu_assert(pool->points[TM_MAX_FILLED_PTRS - 1] == 0xFC, "points end");

    mu_assert(pool->pointers[0].size == 1, "pool ptrs NULL size");
    mu_assert(pool->pointers[0].ptr == 0, "pool ptrs NULL index");

    for(i=1; i<TM_MAX_FILLED_PTRS - 1; i++){
        mu_assert(pool->filled[i] == 0, "filled");
        mu_assert(pool->points[i] == 0, "points");
    }
    for(i=1; i<TM_MAX_POOL_PTRS; i++){
        mu_assert(pool->pointers[i].size == 0, "pool ptrs size");
        mu_assert(pool->pointers[i].ptr == 0, "ptr");
    }
    /*Pool_delete(pool);*/
    return NULL;
}


char *test_tm_pool_alloc(){
    uint8_t i, n;
    uint16_t heap = 1;
    Pool *pool;
    uint8_t size = 100;
    tm_index index;
    tm_index indexes[10];
    pool = Pool_new(size);
    mu_assert(pool, "Pool_alloc was malloced");
    index = Pool_alloc(pool, sizeof(uint32_t));
    mu_assert(index == 1, "alloc index first");
    mu_assert(pool->pointers[1].ptr == heap, "alloc pointer first");
    heap += sizeof(uint32_t);
    mu_assert(pool->heap == heap, "alloc heap first");
    *Pool_uint32_p(pool, index) = 42;
    mu_assert(*Pool_uint32_p(pool, index) == 42, "alloc setting ptr");
    for(i=0; i<10; i++){
        indexes[i] = Pool_alloc(pool, 8);
        mu_assert(indexes[i], "alloc indexes got NULL");
        printf("%u == %u\n", pool->pointers[i + 2].ptr, heap);
        mu_assert(pool->pointers[i + 2].ptr == heap, "alloc pointer indexes");
        heap += 8;
        mu_assert(pool->heap == heap, "alloc heap indexes");
        printf("%u == %u\n", i+2, indexes[i]);
        mu_assert(i + 2 == indexes[i], "alloc indexes");
        for(n=0; n<8; n++) Pool_uint8_p(pool, indexes[i])[n] = n * 10;
    }
    printf("%u == %u\n", *Pool_uint32_p(pool, index), 42);
    mu_assert(*Pool_uint32_p(pool, index) == 42, "alloc ptr is same");
    for(i=0; i<10; i++){
        for(n=0; n<8; n++){
            mu_assert(Pool_uint8_p(pool, indexes[i])[n] == n * 10, "alloc values stay consistent");
        }
    }
    return NULL;
}


char *test_tm_pool_defrag_full(){
    uint8_t i;
    tm_index c;
    tm_index data[201];
    Pool *pool = Pool_new(200*200*2);
    // allocate a bunch of data and initialize it
    c = 0;
    for(i=1; i<201; i++){
        data[i] = Pool_alloc(pool, i * 2);
        mu_assert(data[i], "fdefrag alloc");
        for(c;c<i;c++){
            *Pool_uint16_p(pool, data[i]) = c;
        }
    }
    /*mu_assert(Pool, */
    // free every other element
    for(i=1; i<201; i+=2){
        Pool_free(pool, data[i]);
    }
}


char *all_tests(){
    mu_suite_start();

    mu_run_test(test_tm_pool_new);
    mu_run_test(test_tm_pool_alloc);
    return NULL;
}

RUN_TESTS(all_tests);
