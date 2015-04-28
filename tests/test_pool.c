#include "minunit.h"
#include "tm_pool.h"
#include "tm_tools.h"
#include "time.h"

#define TABLE_STANDIN NULL

#if 0












    // "null" pointer acts as alloc
    other_index = Pool_realloc(pool, 0, 100);
    mu_assert(other_index, "sanity");
    used+=100; used_ptrs++;
    mu_assert(used == pool.used_bytes, "used");
    mu_assert(used_ptrs == pool.used_pointers, "used ptrs");

    // size=0 frees data
    prev_index = Pool_realloc(pool, index, 0);
    mu_assert(!prev_index, "is null");
    mu_assert(!Pool_filled_bool(pool, index), "is freed");
    used -= 60; used_ptrs--;  // memory freed, pointer freed
    mu_assert(used == pool.used_bytes, "used");
    mu_assert(used_ptrs == pool.used_pointers, "used ptrs");

    Pool_del(pool);
    return NULL;
}


    int8_t i, j;
    heap = 5200;
    mu_assert(pool.heap == heap, "fbasic heap1");
    j = 0;
    for(i=2; i<100; i+=2){ // free the even ones
        Pool_free(pool, indexes[i]);
        filled_ptrs--;
        filled_bytes-= COMPUTE_SIZE(i+1);
        mu_assert(filled_ptrs == pool.ptrs_filled, "used ptrs");
        mu_assert(filled_bytes == pool.filled_bytes, "used bytes");
        j+=2;
        mu_assert(Pool_freed_isvalid(pool), "freed isvalid");
    }

    temp=0;
    for(i=0; i<TM_FREED_BINS; i++){
        lia = Pool_LIA(pool, pool.freed[i]);
        if(!lia) continue;
        j = 0;
        while(lia->indexes[j]){
            j++;
        }
        temp+=j; // total number of freed elements
    }
    mu_assert(temp == 49, "fbasic total freed");

    mu_assert(pool.heap == heap, "fbasic heap2"); // heap doesn't change

    for(i=98; i>0; i-=2){   // allocate the even ones again (in reverse order)
        mu_assert(Pool_sizeof(pool, indexes[i]) == COMPUTE_SIZE(i+1), "fbasic size");
        mu_assert(freed_hash(Pool_sizeof(pool, indexes[i])) == freed_hash(COMPUTE_SIZE(i+1)), "fbasic hash");
        indexes[i] = Pool_talloc(pool, i+1);
        mu_assert(indexes[i], "fbasic alloc2");
        mu_assert(pool.heap == heap, "fbasic heap continuous"); // heap doesn't change
        used_ptrs++;
        used_bytes+=COMPUTE_SIZE(i+1);
        mu_assert(used_ptrs == pool.used_pointers, "used ptrs");
        mu_assert(used_bytes == pool.used_bytes, "used bytes");
        mu_assert(Pool_freed_isvalid(pool), "freed isvalid");
    }

    index = Pool_talloc(pool, 4);
    heap += 4;
    mu_assert(pool.heap == heap, "fbasic heap4"); // heap finally changes
    mu_assert(Pool_freed_isvalid(pool), "freed isvalid");
    Pool_del(pool);
    return NULL;
}


char *test_tm_pool_realloc(){
    tm_index_t index, prev_index, other_index;
    uint8_t i, n;
    uint16_t used = 0;
    uint16_t used_ptrs = 1;
    Pool *pool;

    pool = Pool_new();
    mu_assert(pool, "Pool_talloc was malloced");

    // allocate data
    index = Pool_talloc(pool, 40);
    mu_assert(index, "sanity");
    used+=40; used_ptrs++;
    mu_assert(used == pool.used_bytes, "used");
    mu_assert(used_ptrs == pool.used_pointers, "used ptrs");

    // shrink data
    prev_index = index;
    index = Pool_realloc(pool, index, 33);
    mu_assert(index, "sanity");
    mu_assert(index == prev_index, "no change");
    used-=7;  // more free memory, free pointers don't "use pointers"
    mu_assert(used == pool.used_bytes, "used");
    mu_assert(used_ptrs == pool.used_pointers, "used ptrs");

    // grow data
    prev_index = index;
    index = Pool_realloc(pool, index, 60);
    mu_assert(index, "sanity");
    mu_assert(index != prev_index, "changed");
    used += 60 - 33;  // use more memory
    mu_assert(used == pool.used_bytes, "used");
    mu_assert(used_ptrs == pool.used_pointers, "used ptrs");

    // "null" pointer acts as alloc
    other_index = Pool_realloc(pool, 0, 100);
    mu_assert(other_index, "sanity");
    used+=100; used_ptrs++;
    mu_assert(used == pool.used_bytes, "used");
    mu_assert(used_ptrs == pool.used_pointers, "used ptrs");

    // size=0 frees data
    prev_index = Pool_realloc(pool, index, 0);
    mu_assert(!prev_index, "is null");
    mu_assert(!Pool_filled_bool(pool, index), "is freed");
    used -= 60; used_ptrs--;  // memory freed, pointer freed
    mu_assert(used == pool.used_bytes, "used");
    mu_assert(used_ptrs == pool.used_pointers, "used ptrs");

    Pool_del(pool);
    return NULL;
}


char *test_tm_pool_defrag_full(){
    uint8_t i, j;
    tm_index_t c;
    tm_index_t data[201];
    tm_size_t used = 0;
    const tm_size_t calculated_use = 40400;
    Pool *pool = Pool_new();
    mu_assert(pool.stack == TM_POOL_SIZE, "new stack");
    mu_assert(pool, "fdefrag new");
    // allocate a bunch of data and initialize it
    c = 0;
    for(i=1; i<201; i++){
        used += COMPUTE_SIZE(i*2);
        mu_assert(pool.stack == TM_POOL_SIZE, "using stack");
        data[i] = Pool_talloc(pool, i * 2);
        mu_assert(data[i], "fdefrag alloc");
        for(j=0;j<i;j++){
            Pool_uint16_p(pool, data[i])[j] = c;
            c++;
        }
    }
    mu_assert(pool.used_pointers == 1 + 200, "fdefrag used pointers");
    mu_assert(pool.used_bytes == calculated_use, "fdefrag used bytes");
    mu_assert(Pool_heap_left(pool) == TM_POOL_SIZE - calculated_use, "fdefrag heap left");
    mu_assert(Pool_available(pool) == TM_POOL_SIZE - calculated_use, "fdefrag available 1");
    // free odd elements
    for(i=1; i<201; i+=2){
        Pool_free(pool, data[i]);
    }
    mu_assert(Pool_heap_left(pool) == TM_POOL_SIZE - calculated_use, "fdefrag heap left 2");
    mu_assert(Pool_available(pool) == TM_POOL_SIZE - 20200, "fdefrag available 2");

    while(Pool_defrag_full(pool));

    mu_assert(Pool_heap_left(pool) == TM_POOL_SIZE - 20200, "fdefrag heap left 3");
    mu_assert(Pool_available(pool) == TM_POOL_SIZE - 20200, "defrag available 3");
    mu_assert(pool.used_pointers == 101, "defrag ptrs used 3");
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
        j = Pool_talloc(pool, i*2);
        mu_assert(j == i, "defrag reget indexes");
    }
    Pool_del(pool);
    return NULL;
}



char *test_tm_threaded(){
    uint8_t i, i2, j, n;
    tm_index_t index;
    tm_size_t size;
    uint16_t c, c2;
    tm_index_t data[201];
    tm_size_t used = 0;
    tm_size_t stack;
    tm_size_t heap;
    const tm_size_t calculated_use = 40200;

    Pool *pool = Pool_new();
    mu_assert(pool, "fdefrag new");


    // allocate a bunch of data
    c = 0;
    for(i=1; i<201; i++){
        data[i] = Pool_talloc(pool, i * 2);
        mu_assert(data[i], "alloc");
        for(j=0;j<i;j++){  // fill with data
            Pool_uint16_p(pool, data[i])[j] = c;
            c++;
        }
    }
    tmdebug("data[1] size=%u", Pool_sizeof(pool, data[1]));

    // free odd elements
    for(i=1; i<201; i+=2) Pool_free(pool, data[i]);
    mu_assert(!Pool_status(pool, TM_ANY_DEFRAG), "no defrag");

    // Run the defragmenter a few times to get some empty space
    Pool_defrag_full_wtime(pool, 0);
    mu_assert(Pool_space_free_in_defrag(pool) == 0, "space free first");
    // First move.
    // - index is size 4 (data[2])
    // - to the left of it is free space size=4 (data[1])
    // - to the right of it is free space size=8 (data[3])
    // Free space should be 4 + 8 = 12
    size = 12;
    while(TM_DEFRAG_loc < 40) Pool_defrag_full_wtime(pool, 0);
    Pool_defrag_full_wtime(pool, 0);

    mu_assert(Pool_space_free_in_defrag(pool) == size, "space free second");
    for(i=5; i<55; i+=2){
        Pool_defrag_full_wtime(pool, 0);
        size += COMPUTE_SIZE(i * 2);
        mu_assert(Pool_space_free_in_defrag(pool) == size, "space free");
    }

    mu_assert(Pool_status(pool, TM_DEFRAG_FULL_IP), "defrag ip");
    // we now have some free data we can allocate from inside the defragger
    c = 0;
    i2 = i;
    heap = pool.heap;
    for(i=1; i<35; i+=2){
        size = Pool_space_free_in_defrag(pool);
        data[i] = Pool_talloc(pool, i * 2);
        mu_assert(data[i], "alloc during defrag");
        mu_assert(heap == pool.heap, "not allocated off heap");
        size -= COMPUTE_SIZE(i*2);
        mu_assert(Pool_space_free_in_defrag(pool) == size, "free space 1");
        for(j=0;j<i;j++){
            Pool_uint16_p(pool, data[i])[j] = c;
            c++;
        }
        if((i>10) && i%3){  // throw in some random defrags
            Pool_defrag_full_wtime(pool, 0);
             size += COMPUTE_SIZE(i2 * 2);
             mu_assert(Pool_space_free_in_defrag(pool) == size, "space free 2");
            i2 += 2;
        }
    }

    // Verify all that data
    c = 0;
    c2 = 0;
    tmdebug("free in defrag=%u, size=%u", Pool_space_free_in_defrag(pool), size);
    for(i=1; i<201; i++){
        mu_assert(Pool_sizeof(pool, i) == COMPUTE_SIZE(i*2), "defrag size");
        if(!(i%2)){     // even
            for(j=0;j<i;j++, c++) mu_assert(Pool_uint16_p(pool, data[i])[j] == c, "defrag data inconcistency");
        }
        else{           // odd
            c+=i;
            if(i>=35) continue;
            for(j=0;j<i;j++, c2++) mu_assert(Pool_uint16_p(pool, data[i])[j] == c2, "defrag data inconcistency 2");
        }
    }

    stack = pool.ustack;
    // Awesome, now let's free a whole bunch of stuff and verify it works
    for(i=150; i<180; i+=2){
        Pool_free(pool, data[i]);
        mu_assert(!Pool_filled_bool(pool, data[i]), "is freed");
        stack -= sizeof(tm_index_t);
        mu_assert(pool.ustack == stack, "free appended");
    }

    // Complete the defrag
    while(Pool_defrag_full(pool));

    // re-allocate, verifying that it doesn't come off the heap
    heap = pool.heap;
    for(i=150; i<180; i+=2){
        mu_assert(Pool_points_bool(pool, data[i]), "does point");
        mu_assert(!Pool_filled_bool(pool, data[i]), "is still freed");
        data[i] = Pool_talloc(pool, i * 2);
        mu_assert(data[i], "alloc");
        mu_assert(heap == pool.heap, "heap not changed");
        for(j=0;j<i;j++) Pool_uint16_p(pool, data[i])[j] = 0xF5F5;
    }

    c=0, c2=0;
    for(i=1; i<201; i++){
        mu_assert(Pool_sizeof(pool, i) == COMPUTE_SIZE(i*2), "defrag size");
        if(!(i%2)){     // even
            if((i>=150) && (i<180)){
                mu_assert(Pool_uint16_p(pool, data[i])[j] == 0xF5F5, "new alloc");
                c+=i;
                continue;
            }
            for(j=0;j<i;j++, c++) mu_assert(Pool_uint16_p(pool, data[i])[j] == c, "defrag data inconcistency 3");
        }
        else{           // odd
            c+=i;
            if(i>=35) continue;
            for(j=0;j<i;j++, c2++) mu_assert(Pool_uint16_p(pool, data[i])[j] == c2, "defrag data inconcistency 3");
        }
    }

    return NULL;
}

char *test_tm_threaded_time(){
    uint8_t i;
    tm_index_t data[201];
    clock_t start;
    Pool *pool = Pool_new();
    mu_assert(pool, "fdefrag new");

    // allocate a bunch of data, free every other
    for(i=1; i<201; i++){
        data[i] = Pool_talloc(pool, i * 2);
        mu_assert(data[i], "alloc");
    }
    for(i=1; i<201; i++){
        if(i%2) Pool_free(pool, data[i]);
    }

    // during allocation make sure that nothing takes more than US + 1
    i = true;
    while(i){
        start = clock();
        i = Pool_defrag_full(pool);
        start = (clock() - start) * 1000000 / (CLOCKS_PER_SEC);
        tmdebug("time=%u", start);
        /*mu_assert(start < TM_THREAD_TIME_US + 1, "thread time");*/
    }

    tmdebug("clocks/us=%u", CPU_CLOCKS_PER_US);

    return NULL;
}

#endif

char *all_tests(){
    mu_suite_start();

    mu_run_test(test_tm_pool_new);
    mu_run_test(test_tm_pool_alloc);
    mu_run_test(test_tm_free_basic);
#if 0
    mu_run_test(test_tm_pool_realloc);
    mu_run_test(test_tm_pool_defrag_full);

    mu_run_test(test_tm_threaded);
    mu_run_test(test_tm_threaded_time);
#endif
    return NULL;
}

RUN_TESTS(all_tests);
