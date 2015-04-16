#include <stdlib.h>
#include "dbg.h"
#include "tm_pool.h"
#include "tm_freed.h"



/*---------------------------------------------------------------------------*/
/* Pool Methods                                                              */

void Pool_init(Pool *pool){
    *pool = Pool_declare();
    Pool_upool_clear(pool);
}

Pool *Pool_new(){
    // Malloc space we need
    Pool *pool = malloc(sizeof(Pool));
    if(!pool) return NULL;
    Pool_init(pool);
    return pool;
}


void Pool_upool_clear(Pool *pool){
    pool->uheap = 0;
    pool->ustack = TM_UPOOL_SIZE;
    Pool_freed_reset(pool);
}


void Pool_freed_reset(Pool *pool){
    tm_index i;
    for(i=0; i<TM_FREED_BINS; i++){
        pool->freed[i] = TM_UPOOL_ERROR;
    }
    // there are no freed values anymore, only filled ones
    for(i=0; i<TM_MAX_FILLED_PTRS; i++){
        pool->points[i] = pool->filled[i];
    }
    pool->points[0]     |= 1;  // NULL needs to be taken
    pool->points[i-1]   |= TM_LAST_USED;  // prevent use of values that don't exist
}

inline void *Pool_void(Pool *pool, tm_index index){
    // get a void pointer to data from pool index.
    if(!index) return NULL;
    return Pool_location_void(pool, Pool_location(pool, index));
}

tm_index Pool_find_index(Pool *pool){
    tm_index index, i, b;
    unsigned int bit;
    unsigned int *points = (unsigned int *)pool->points;
    for(i=0; i<TM_MAX_FILLED_INT; i++){
        if(points[i] != MAXUINT){
            // There is an empty value
            bit = 1;
            for(b=0; b<INTBITS; b++){
                if(! (points[i] & bit)){
                    index = i * INTBITS + b;
                    return index;
                }
                bit = bit << 1;
            }
        }
    }
    return 0;
}


tm_index Pool_alloc(Pool *pool, tm_index size){
    tm_index index = Pool_freed_getsize(pool, size);
    if(index){
        if(!Pool_points_bool(pool, index)){
            tmdebug("ERROR pointer should point, but be unfilled");
            return 0;
        }
        if(Pool_filled_bool(pool, index)){
            tmdebug("ERROR data in freed array should not be filled!");
            return 0;
        }
        if(Pool_sizeof(pool, index) != size){
            tmdebug("ERROR value is wrong size!");
            return 0;
        }
        Pool_filled_set(pool, index);
        pool->used_bytes += Pool_sizeof(pool, index);
        pool->used_pointers++;
        return index;
    }
    if(size > Pool_available(pool)) return 0;
    if(size > Pool_heap_left(pool)){
        Pool_status_set(pool, TM_DEFRAG);
        // TODO: this is the "simplest" implementation.
        //      Threading is going to be main one long term
        Pool_defrag_full(pool);
        if(size > Pool_heap_left(pool)){
            Pool_status_set(pool, TM_ERROR);
            return 0;
        }
    }
    // find an unused index
    if(!Pool_pointers_left(pool)){
        return 0;
    }
    index = Pool_find_index(pool);
    if(!index){
        Pool_status_set(pool, TM_DEFRAG);
        Pool_defrag_full(pool);  // TODO: simple implemntation
        index = Pool_find_index(pool);
        if((!Pool_pointers_left(pool)) || (!index)){
            Pool_status_set(pool, TM_ERROR);
            return 0;
        }
    }
    Pool_filled_set(pool, index);
    Pool_points_set(pool, index);
    pool->pointers[index] = (poolptr) {.size = size, .ptr = pool->heap};
    pool->heap += size;
    pool->used_bytes += size;
    pool->used_pointers++;
    return index;
}


tm_index Pool_realloc(Pool *pool, tm_index index, tm_size size){
    tm_index new_index;
    tm_size prev_size;

    if(!index) return Pool_alloc(pool, size);
    if(!Pool_filled_bool(pool, index)) return 0;
    if(!size){
        Pool_free(pool, index);
        return 0;
    }
    prev_size = Pool_sizeof(pool, index);
    if(size == prev_size) return index;
    if(size < prev_size){  // shrink data
        new_index = Pool_find_index(pool);
        if(!new_index) return 0;

        // set the original index to a smaller footprint
        pool->pointers[index].size = size,

        // update new index for freed data
        Pool_points_set(pool, new_index);
        pool->pointers[new_index] = (poolptr) {.size = prev_size - size,
                                                .ptr = Pool_location(pool, index) + size};

        // mark changes
        pool->used_bytes -= prev_size - size;
        return index;
    } else{  // grow data
        new_index = Pool_alloc(pool, size);
        if(!new_index) return 0;
        Pool_memmove(pool, new_index, index);
        Pool_free(pool, index);
        return new_index;
    }
}


void Pool_free(Pool *pool, tm_index index){
    if(index > TM_MAX_POOL_PTRS || index == 0 || !Pool_filled_bool(pool, index)){
        return;
    }
    Pool_filled_clear(pool, index);
    pool->used_bytes -= Pool_sizeof(pool, index);
    pool->used_pointers--;
    if(!Pool_freed_append(pool, index)){
        tmdebug("requesting defrag!");
        Pool_status_set(pool, TM_DEFRAG);
        Pool_defrag_full(pool);  // TODO: simple implementation
    }
}


/*---------------------------------------------------------------------------*/
/* upool allocation and freeing. (internal use)                              */

tm_index Pool_ualloc(Pool *pool, tm_size size){
    // The upool ASSUMES that all blocks are the same size. Make sure this is always true.
    tm_index location;

    if(Pool_ustack_used(pool)) {
        // free pointers available
        location = Pool_upool_get_index(pool, pool->ustack / 2);
        pool->ustack += 2;
        return location;
    }
    if(size > Pool_uheap_left(pool)) return TM_UPOOL_ERROR;
    pool->uheap += size;
    return pool->uheap - size;
}


bool Pool_ufree(Pool *pool, tm_index location){
    if(Pool_uheap_left(pool) < 2){
        return false;
    }
    pool->ustack-=2;
    Pool_upool_set_index(pool, pool->ustack / 2, location);
    return true;
}


inline void *Pool_uvoid(Pool *pool, tm_index location){
    if(location >= TM_UPOOL_ERROR) return NULL;
    return pool->upool + location;
}

