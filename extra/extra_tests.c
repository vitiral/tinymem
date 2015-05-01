
/**
 * These are some unit tests that were created early on.
 * They are now replaced by test_tinymem, but they may come in handy.
 * If it is possible they will be useful, copy paste them into the main file
 * and add them to the testing main.
 */

char *test_tm_pool_new(){
    tm_index_t i;
    tm_size_t size;
    tm_reset();

    mu_assert(HEAP == 0);
    mu_assert(HEAP_LEFT_BYTES == TM_POOL_SIZE);

    mu_assert(tm_pool.filled[0] == 1);
    mu_assert(tm_pool.points[0] == 1);
    for(i=1; i<MAX_BIT_INDEXES; i++){
        mu_assert(tm_pool.filled[i] == 0);
        mu_assert(tm_pool.points[i] == 0);
    }

    for(i=0; i<TM_POOL_INDEXES; i++){
        mu_assert(tm_pool.pointers[i].loc == 0);
        mu_assert(tm_pool.pointers[i].next == 0);
    }

    for(i=0; i<FREED_BINS; i++){
        mu_assert(tm_pool.freed[i] == 0);
    }
    mu_assert(freed_count(&size) == 0);
    mu_assert(size == 0);
    return NULL;
}

char *test_tm_pool_alloc(){
    tm_size_t size;
    uint8_t i;
    uint16_t heap = 0, ptrs=1;
    tm_reset();

    mu_assert(freed_count(&size) == 0);
    mu_assert(size == 0);

    tm_index_t index;
    tm_index_t indexes[10];

    index = talloc(sizeof(uint32_t));
    mu_assert(index == 1);
    mu_assert(tm_pool.pointers[1].loc == heap);
    heap += ALIGN_BLOCKS(sizeof(uint32_t)); ptrs++;
    mu_assert(HEAP == heap);
    mu_assert(tm_pool.ptrs_filled == ptrs);

    mu_assert(check_index(index));
    for(i=0; i<10; i++){
        indexes[i] = talloc(8);
        mu_assert(indexes[i]);
        mu_assert(tm_pool.pointers[i + 2].loc == heap);
        heap += ALIGN_BLOCKS(8); ptrs++;
        mu_assert(HEAP == heap);
        mu_assert(tm_pool.ptrs_filled == ptrs);
        mu_assert(i + 2 == indexes[i]);
    }
    mu_assert(check_index(index));
    for(i=0; i<10; i++) mu_assert(check_index(indexes[i]));
    mu_assert(pool_isvalid());
    return NULL;
}

char *test_tm_free_basic(){
    int8_t i, j;
    tm_index_t filled_ptrs;
    tm_index_t filled_blocks;
    tm_size_t heap;
    tm_index_t indexes[100];

    // allocate a bunch of memory, then free chunks of it.
    // Then allocate it again, making sure the heap doesn't change
    filled_ptrs = tm_pool.ptrs_filled;
    filled_blocks = tm_pool.filled_blocks;
    for(i=0; i<100; i++){
        indexes[i] = talloc(i+1);
        mu_assert(indexes[i]);
        filled_ptrs++;
        filled_blocks += ALIGN_BLOCKS(i+1);
        mu_assert(tm_sizeof(indexes[i]) == ALIGN_BYTES(i+1));
        mu_assert(filled_ptrs == tm_pool.ptrs_filled);
        mu_assert(filled_blocks == tm_pool.filled_blocks);
        mu_assert(pool_isvalid());
    }
    for(i=0; i<100; i++) mu_assert(tm_sizeof(indexes[i]) == ALIGN_BYTES(i+1));

    heap = filled_blocks;
    for(i=0; i<100; i++) mu_assert(check_index(indexes[i]));
    for(i=2; i<100; i+=2){ // free the even ones
        tm_free(indexes[i]);
        filled_ptrs--;
        filled_blocks -= ALIGN_BLOCKS(i+1);
        mu_assert(filled_ptrs == tm_pool.ptrs_filled);
        mu_assert(filled_blocks == tm_pool.filled_blocks);
        mu_assert(pool_isvalid());
        mu_assert(heap == HEAP);
    }
    for(i=0; i<100; i++) mu_assert(check_index(indexes[i]));
    mu_assert(pool_isvalid());
    return NULL;
}

char *test_tm_pool_realloc(){
    tm_index_t index, prev_index, other_index, index2;
    uint8_t i, n;
    uint16_t used = 0;
    uint16_t used_ptrs = 1;
    tm_size_t size;

    tm_reset();

    // allocate data
    index = talloc(40);
    mu_assert(index);
    used+=ALIGN_BLOCKS(40); used_ptrs++;
    mu_assert(used == tm_pool.filled_blocks);
    mu_assert(used_ptrs == tm_pool.ptrs_filled);
    mu_assert(pool_isvalid());

    // shrink data
    prev_index = index;
    index = tm_realloc(index, 32);
    mu_assert(index == prev_index);
    used-=ALIGN_BLOCKS(8);  // more free memory
    mu_assert(tm_sizeof(index) == 32);
    mu_assert(used == tm_pool.filled_blocks);
    mu_assert(used_ptrs == tm_pool.ptrs_filled);
    mu_assert(1 == tm_pool.ptrs_freed);
    mu_assert(1 == freed_count(&size));
    mu_assert(8 == size);
    mu_assert(ALIGN_BLOCKS(8) == tm_pool.freed_blocks);
    mu_assert(pool_isvalid());

    // grow data
    index2 = tm_alloc(4);       // force heap allocation
    used += ALIGN_BLOCKS(4); used_ptrs++;
    mu_assert(used == tm_pool.filled_blocks);
    mu_assert(used_ptrs == tm_pool.ptrs_filled);
    mu_assert(tm_sizeof(index) == 32);

    prev_index = index;

    index = tm_realloc(index, 60);
    mu_assert(index);
    mu_assert(index != prev_index);
    used += ALIGN_BLOCKS(60) - ALIGN_BLOCKS(32);
    mu_assert(used == tm_pool.filled_blocks);
    mu_assert(used_ptrs == tm_pool.ptrs_filled);
    mu_assert(pool_isvalid());
    return NULL;
}
