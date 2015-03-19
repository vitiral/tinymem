#include <stdlib.h>
#include "tm_pool.h"
#include "dbg.h"

/* ##################################################
 * ###  Pool Methods
 */


Pool *Pool_new(tm_size size){
    tm_size i;
    // Malloc space we need
    Pool *pool = malloc(sizeof(Pool));
    check_mem(pool);
    *pool = (Pool) {
        .size = size,
        .heap = 1,              // 0 == NULL
        .stack = size,          // size - 1???
        .filled_index = 0,
        .used_index = 0
    };
    pool->pool = malloc(size);
    check_mem(pool->pool);
    for(i=0; i<TM_FREED_BINS; i++){
        pool->freed[i] = 0;
    }

    // Index 0 is NULL and taken
    pool->filled[0] = 0;        // it has no data in it to prevent deallocation from caring about it
    pool->used[0] = 1;          // but it is "used". This prevents it from being used by any user value
    pool->pointers[0] = (poolptr){.size=1, .ptr=0};
    *(pool->pool) = 0;             // This should NEVER change (for diagnostics)


    // default values for everything else
    for(i=1; i<TM_MAX_POOL_PTRS; i++){
        pool->pointers[i] = NULL_poolptr;
    }
    for(i=1; i<TM_MAX_FILLED_PTRS - 1; i++){
        pool->filled[i] = 0;
    }
    for(i=1; i<TM_MAX_FILLED_PTRS - 1; i++){
        pool->used[i] = 0;
    }
    pool->filled[TM_MAX_FILLED_PTRS - 1] = 0; // filled is 0 so deallocate ignores them
    // The last "used" values need to be 1 as they don't actually exist and we don't want to put data
    // there
    pool->used[TM_MAX_FILLED_PTRS - 1] = (uint8_t) (~(0x00FF >> (TM_MAX_POOL_PTRS % 8)));

    return pool;
error:
    /*Pool_delete(pool);*/
    return NULL;
}


void *Pool_void(Pool *pool, tm_index index){
    // get a void pointer to data from pool index.
    tm_size location;
    if(not index) return NULL;
    location = Pool_location(pool, index);
    if(not location) return NULL;
    return pool->pool + location;
}


tm_index Pool_find_index(Pool *pool){
    tm_index index, i, b;
    unsigned int bit;
    unsigned int *used = (unsigned int *)pool->used;
    printf("max_filled_int_index=%u\n", TM_MAX_FILLED_INT);
    printf("max_UINT=0x%X\n", MAXUINT);
    for(i=0; i<TM_MAX_FILLED_INT; i++){
        if(used[i] != MAXUINT){
            printf("i=%u\n", i);
            // There is an empty value
            bit = 1;
            for(b=0; b<INTBITS; b++){
                printf("bit=%u\n", bit);
                if(not (used[i] bitand bit)){
                    index = i * INTBITS + b;
                    printf("used=%u\n", used[i]);
                    printf("index=%u\n", index);
                    // Check values
                    if(((unsigned int *)(pool->filled))[i] bitand bit != 0) return 0;
                    if(Pool_sizeof(pool, index) != 0) return 0;
                    if(Pool_location(pool, index) != 0) return 0;
                    return index;
                }
                bit = bit << 1;
            }
        }
    }
    return 0;
}


tm_index Pool_alloc(Pool *pool, tm_index size){
    tm_index index;
    // TODO: Use freed first
    printf("ALLOC=%u\n", size);
    printf("stack=%u\n", pool->stack);
    printf("left=%u\n", Pool_left(pool));
    if(size > Pool_left(pool)) return 0;  // TODO: Deallocate here
    // find an unused index
    index = Pool_find_index(pool);
    if(not index) return 0;
    printf("index=%u\n", index);
    printf("filled[%u] |= 0x%X\n", Pool_filled_index(index), Pool_filled_bit(index));
    Pool_filled_set(pool, index);
    Pool_used_set(pool, index);
    pool->pointers[index] = (poolptr) {.size = size, .ptr = pool->heap};
    pool->heap += size;
    return index;
}
