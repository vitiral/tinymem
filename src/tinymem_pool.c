#include "tinymem_pool.h"

/*---------------------------------------------------------------------------*/
/*      Local Declarations                                                   */
tm_index_t Pool_find_index(Pool *pool);

/*---------------------------------------------------------------------------*/
void Pool_init(Pool *pool){
    *pool = Pool_declare();
}


/*---------------------------------------------------------------------------*/
void *Pool_void_p(Pool *pool, tm_index_t index){
    // Note: index 0 has location == heap (it is where Pool_heap is stored)
    if(Pool_loc(pool, index) >= Pool_heap(pool)) return NULL;
    return pool->pool + Pool_loc(pool, index);
}

/*---------------------------------------------------------------------------*/
tm_index_t Pool_alloc(Pool *pool, tm_size_t size){
    tm_index_t index;
    size += size % TM_ALIGN_BYTES;
    //TODO: find index from freed
    if(Pool_heap_left(pool) < size) return 0;
    index = Pool_find_index(pool);
    if(!index) return 0;
    Pool_filled_set(pool, index);
    Pool_points_set(pool, index);
    pool->pointers[index] = (poolptr) {.loc = Pool_heap(pool), .next = 0};
    Pool_heap(pool) += size;
    pool->filled_bytes += size;
    pool->ptrs_filled++;
    return index;
}

/*---------------------------------------------------------------------------*/
tm_index_t Pool_find_index(Pool *pool){
    unsigned int bit;
    if(!Pool_pointers_left(pool)) return 0;

    while(1){
        for(; pool->find_index < TM_MAX_BIT_INDEXES; pool->find_index++){
            if(pool->points[pool->find_index] != MAXUINT){
                // there is an empty value
                bit = 1 << pool->find_index_bit;
                for(; pool->find_index_bit < INTBITS; pool->find_index_bit++){
                    if(!(pool->points[pool->find_index_bit] & bit)){
                        pool->find_index_bit++;
                        return pool->find_index * INTBITS + (pool->find_index_bit - 1);
                    }
                    bit = bit << 1;
                }
            }
            pool->find_index_bit = 0;
        }
        pool->find_index = 0;
        pool->find_index_bit = 1;   // index 0 is invalid
    }
}
