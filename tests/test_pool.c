#include "minunit.h"
#include "tm_pool.h"
#include "tm_freed.h"

#define TABLE_STANDIN NULL


char *test_tm_pool_new(){
    tm_index i;
    Pool *pool;

    pool = Pool_new();
    mu_assert(pool, "Pool was malloced");
    mu_assert(pool->heap == 1, "heap NULL");
    mu_assert(pool->stack == TM_POOL_SIZE, "new stack");
    mu_assert(Pool_heap_left(pool) == TM_POOL_SIZE - 1, "pool left");

    mu_assert(pool->filled[0] == 0, "filled NULL");
    mu_assert(pool->filled[TM_MAX_FILLED_PTRS - 1] == 0x00, "filled end");

    mu_assert(pool->points[0] == 1, "points NULL");
    /*mu_assert(pool->points[TM_MAX_FILLED_PTRS - 1] == 0xFC, "points end");*/

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
    Pool_del(pool);
    return NULL;
}


char *test_tm_pool_alloc(){
    uint8_t i, n;
    uint16_t heap = 1;
    Pool *pool;
    tm_index index;
    tm_index indexes[10];
    pool = Pool_new();
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
        mu_assert(pool->pointers[i + 2].ptr == heap, "alloc pointer indexes");
        heap += 8;
        mu_assert(pool->heap == heap, "alloc heap indexes");
        mu_assert(i + 2 == indexes[i], "alloc indexes");
        for(n=0; n<8; n++) Pool_uint8_p(pool, indexes[i])[n] = n * 10;
    }
    mu_assert(*Pool_uint32_p(pool, index) == 42, "alloc ptr is same");
    for(i=0; i<10; i++){
        for(n=0; n<8; n++){
            mu_assert(Pool_uint8_p(pool, indexes[i])[n] == n * 10, "alloc values stay consistent");
        }
    }
    Pool_del(pool);
    return NULL;
}


char *test_tm_pool_defrag_full(){
    uint8_t i, j;
    tm_index c;
    tm_index data[201];
    tm_size used = 0;
    const tm_size calculated_use = 40200;
    Pool *pool = Pool_new();
    mu_assert(pool->stack == TM_POOL_SIZE, "new stack");
    mu_assert(pool, "fdefrag new");
    // allocate a bunch of data and initialize it
    c = 0;
    for(i=1; i<201; i++){
        used += i*2;
        mu_assert(pool->stack == TM_POOL_SIZE, "using stack");
        data[i] = Pool_alloc(pool, i * 2);
        mu_assert(data[i], "fdefrag alloc");
        for(j=0;j<i;j++){
            Pool_uint16_p(pool, data[i])[j] = c;
            c++;
        }
    }
    mu_assert(pool->used_pointers == 1 + 200, "fdefrag used pointers");
    mu_assert(pool->used_bytes == 1 + calculated_use, "fdefrag used bytes");
    mu_assert(Pool_heap_left(pool) == TM_POOL_SIZE - 1 - calculated_use, "fdefrag heap left");
    mu_assert(Pool_available(pool) == TM_POOL_SIZE - 1 - calculated_use, "fdefrag available 1");
    // free odd elements
    for(i=1; i<201; i+=2){
        Pool_free(pool, data[i]);
    }
    mu_assert(Pool_heap_left(pool) == TM_POOL_SIZE - 1 - calculated_use, "fdefrag heap left 2");
    mu_assert(Pool_available(pool) == TM_POOL_SIZE - 1 - 20200, "fdefrag available 2");

    Pool_defrag_full(pool);

    mu_assert(Pool_heap_left(pool) == TM_POOL_SIZE - 20200 - 1, "fdefrag heap left 3");
    mu_assert(Pool_available(pool) == TM_POOL_SIZE - 20200 - 1, "defrag available 3");
    mu_assert(pool->used_pointers == 101, "defrag ptrs used 3");
    c = 0;
    for(i=1; i<201; i++){
        if(! i%2){
            mu_assert(Pool_sizeof(pool, i) == i*2, "defrag size");
            for(j=0;j<i;j++){
                mu_assert(Pool_uint16_p(pool, data[i])[j] == c, "defrag data inconcistency");
                c++;
            }
        }
        else c+=i;  // odds were deleted
    }
    // reallocate
    for(i=1; i<100; i+=2){
        j = Pool_alloc(pool, i*2);
        mu_assert(j == i, "defrag reget indexes");
    }
    Pool_del(pool);
    return NULL;
}

char *test_upool_basic(){
    static int mchange = 32000 / 20;
    int value;
    int8_t i;
    int m = 0;
    tm_index index;
    tm_index indexes[20];
    Pool *pool = Pool_new();
    mu_assert(pool, "upool basic");
    for(i=0; i<20; i++){
        indexes[i] = Pool_ualloc(pool, sizeof(int));
        mu_assert(indexes[i] != TM_UPOOL_ERROR, "upool alloc");
        *(int *)Pool_uvoid(pool, indexes[i]) = m;
        mu_assert(*(int *)Pool_uvoid(pool, indexes[i]) == m, "upool sanity");
        m += mchange;
    }
    for(i=1; i<20; i+=2){
        Pool_ufree(pool, indexes[i]);
    }
    m-=mchange;
    for(i=19; i>=1; i-=2){
        index = Pool_ualloc(pool, sizeof(int));
        mu_assert(index == indexes[i], "upool realloc");
        mu_assert(*(int *)Pool_uvoid(pool, indexes[i]) == m, "upool basic value");
        *(int *)Pool_uvoid(pool, indexes[i]) = m;
        m-=mchange;
        value = *(int *)Pool_uvoid(pool, indexes[i-1]);
        mu_assert(value == m, "upool setting");
        m-=mchange;
    }
    m = 0;
    Pool_del(pool);
    return NULL;
}

char *test_tm_free_foundation(){
    int8_t i;
    tm_index result;
    tm_index lray = TM_UPOOL_ERROR;
    Pool *pool = Pool_new();
    // Some fake pointers
    pool->pointers[42] = (poolptr) {.size = 4, .ptr = 1};
    pool->pointers[37] = (poolptr) {.size = 6, .ptr = 1+4};
    pool->pointers[1] = (poolptr) {.size = 8, .ptr = 1+4+6};
    pool->pointers[2] = (poolptr) {.size = 5, .ptr = 1+4+6+8};
    mu_assert(pool, "free foundation -- pool new");
    // append index 42 (size 4)
    mu_assert(Pool_sizeof(pool, 37) == 6, "free f sanity");
    mu_assert(Pool_sizeof(pool, 42) == 4, "free f sanity");

    mu_assert(LIA_append(pool, &lray, 42), "free f -- appending");  // automatically creates new array
    mu_assert(pool->uheap == sizeof(LinkedIndexArray), "free f -- did create");
    result = LIA_pop(pool, &lray, 4);
    mu_assert(result == 42, "free f -- pop1");  // get value of size 4
    mu_assert(pool->uheap == sizeof(LinkedIndexArray), "free f -- heap doesn't change");
    mu_assert(Pool_ustack_used(pool) == 2, "free f -- freed is stored");
    mu_assert(Pool_upool_get_index(pool, pool->stack / 2) == 0, "free f -- freed index is stored");

    mu_assert(LIA_append(pool, &lray, 42), "free f sanity");
    mu_assert(LIA_append(pool, &lray, 37), "free f sanity");
    mu_assert(pool->uheap == sizeof(LinkedIndexArray), "free f -- did not create");
    mu_assert(Pool_ustack_used(pool) == 0, "free f -- removed from stack");
    mu_assert(LIA_pop(pool, &lray, 6) == 37, "free f -- pop2");
    result = LIA_pop(pool, &lray, 4);
    mu_assert(result == 42, "free f -- pop3");

    // append a whole bunch of values
    for(i=0; i<33; i++){
        mu_assert(LIA_append(pool, &lray, 42), "free f sanity");
        mu_assert(LIA_append(pool, &lray, 37), "free f sanity");
        mu_assert(LIA_append(pool, &lray, 1), "free f sanity");
        mu_assert(LIA_append(pool, &lray, 2), "free f sanity");
    }

    for(i=0; i<33; i++){
        mu_assert(LIA_pop(pool, &lray, 4) == 42, "free popping 42");
        mu_assert(LIA_pop(pool, &lray, 6) == 37, "free popping 37");
        mu_assert(LIA_pop(pool, &lray, 5) == 2, "free popping 2");
        mu_assert(LIA_pop(pool, &lray, 8) == 1, "free popping 1");
    }
    Pool_del(pool);
    return NULL;
}

char *test_tm_free_basic(){
    int8_t i, j;
    Pool *pool = Pool_new();
    LinkedIndexArray *lia;
    tm_index used_ptrs;
    tm_index used_bytes;
    tm_size heap;
    tm_size temp;
    tm_index indexes[100];
    tm_index index;
    mu_assert(pool, "fbasic sanity");

    // allocate a bunch of memory, then free chunks of it.
    // Then allocate it again, making sure the heap doesn't change
    used_ptrs = pool->used_pointers;
    used_bytes = pool->used_bytes;
    for(i=0; i<100; i++){
        indexes[i] = Pool_alloc(pool, i+1);
        mu_assert(indexes[i], "fbasic alloc");
        used_ptrs++;
        used_bytes+=i+1;
        mu_assert(used_ptrs == pool->used_pointers, "used ptrs");
        mu_assert(used_bytes == pool->used_bytes, "used bytes");
        // TODO: load values
    }
    heap = 5050 + 1;
    mu_assert(pool->heap == heap, "fbasic heap1");
    j = 0;
    for(i=2; i<100; i+=2){ // free the even ones
        Pool_free(pool, indexes[i]);
        used_ptrs--;
        used_bytes-=i+1;
        mu_assert(used_ptrs == pool->used_pointers, "used ptrs");
        mu_assert(used_bytes == pool->used_bytes, "used bytes");
        j+=2;
    }
    temp=0;
    for(i=0; i<TM_FREED_BINS; i++){
        lia = Pool_LIA(pool, pool->freed[i]);
        if(!lia) continue;
        j = 0;
        while(lia->indexes[j]){
            j++;
        }
        temp+=j; // total number of freed elements
    }
    mu_assert(temp == 49, "fbasic total freed");

    mu_assert(pool->heap == heap, "fbasic heap2"); // heap doesn't change

    for(i=98; i>0; i-=2){   // allocate the even ones again (in reverse order)
        mu_assert(Pool_sizeof(pool, indexes[i]) == i+1, "fbasic size");
        mu_assert(freed_hash(Pool_sizeof(pool, indexes[i])) == freed_hash(i+1), "fbasic hash");
        indexes[i] = Pool_alloc(pool, i+1);
        mu_assert(indexes[i], "fbasic alloc2");
        mu_assert(pool->heap == heap, "fbasic heap continuous"); // heap doesn't change
        used_ptrs++;
        used_bytes+=i+1;
        mu_assert(used_ptrs == pool->used_pointers, "used ptrs");
        mu_assert(used_bytes == pool->used_bytes, "used bytes");
    }

    index = Pool_alloc(pool, 4);
    heap += 4;
    mu_assert(pool->heap == heap, "fbasic heap4"); // heap finally changes
    Pool_del(pool);
    return NULL;
}

char *all_tests(){
    mu_suite_start();

    mu_run_test(test_tm_pool_new);
    mu_run_test(test_tm_pool_alloc);
    mu_run_test(test_tm_pool_defrag_full);
    mu_run_test(test_upool_basic);
    mu_run_test(test_tm_free_foundation);
    mu_run_test(test_tm_free_basic);
    return NULL;
}

RUN_TESTS(all_tests);
