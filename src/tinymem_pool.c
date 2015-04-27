#include "tinymem_pool.h"


/*---------------------------------------------------------------------------*/
/*      Local Declarations                                                   */
tm_index_t Pool_find_index(Pool *pool);
uint8_t freed_bin(tm_size_t size);
inline void Pool_freed_remove(Pool *pool, tm_index_t index);
tm_index_t Pool_freed_get(Pool *pool, tm_size_t size);

#define Pool_free_p(pool, index)  ((free_block *)Pool_void_p(pool, index))
#define Pool_freed_insert(pool, index, bin) do{                                     \
    *Pool_free_p(pool, index) = (free_block){.next=(pool)->freed[bin], .prev=0};    \
    Pool_free_p(pool, (pool)->freed[bin])->prev = index;                           \
    }while(0)



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
    if(Pool_available(pool) < size) return 0;

    index = Pool_freed_get(pool, size);
    if(index){
        pool->filled_bytes += size;
        pool->freed_bytes -= size;
        pool->ptrs_filled++;
        pool->ptrs_freed--;
        return index;
    }
    if(Pool_heap_left(pool) < size){
        Pool_status_set(pool, TM_DEFRAG_FAST);  // need less fragmentation
        return 0;
    }
    index = Pool_find_index(pool);
    if(!index){
        Pool_status_set(pool, TM_DEFRAG_FAST);  // need more indexes
        return 0;
    }
    Pool_filled_set(pool, index);
    Pool_points_set(pool, index);
    pool->pointers[index] = (poolptr) {.loc = Pool_heap(pool), .next = 0};
    Pool_heap(pool) += size;
    pool->filled_bytes += size;
    pool->ptrs_filled++;
    return index;
}

/*---------------------------------------------------------------------------*/
void Pool_free(Pool *pool, tm_index_t index){
    uint8_t bin;
    if(Pool_loc(pool, index) >= Pool_heap(pool)) return;
    if(index >= TM_MAX_POOL_PTRS || !Pool_filled_bool(pool, index)) return;
    Pool_filled_clear(pool, index);
    pool->filled_bytes -= Pool_sizeof(pool, index);
    pool->ptrs_filled--;
    pool->ptrs_freed++;

    // Note: index 0 == filled
    if(!Pool_filled_bool(pool, Pool_next(pool, index))){
        // The next index is also free, remove from free bin and join
        Pool_freed_remove(pool, Pool_next(pool, index));
        pool->ptrs_freed--;
        Pool_next(pool, index) = Pool_next(pool, Pool_next(pool, index));
    }
    bin = freed_bin(Pool_sizeof(pool, index));    // speeds up macro
    Pool_freed_insert(pool, index, bin);
}


/*###########################################################################*/
/*      Local Functions                                                      */

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

/*---------------------------------------------------------------------------*/
/*      get the freed bin for size                                           */
uint8_t freed_bin(tm_size_t size){
    assert(size % TM_ALIGN_BYTES == 0);
    switch(size){
        case TM_ALIGN_BYTES:                    return 0;
        case TM_ALIGN_BYTES * 2:                return 1;
        case TM_ALIGN_BYTES * 3:                return 2;
    }
    if(size < TM_ALIGN_BYTES * 64){
        if(size < TM_ALIGN_BYTES * 8)           return 3;
        else if(size < TM_ALIGN_BYTES * 16)     return 4;
        else if(size < TM_ALIGN_BYTES * 32)     return 5;
        else                                    return 6;
    }
    else if(size < TM_ALIGN_BYTES * 1024){
        if(size < TM_ALIGN_BYTES * 128)         return 7;
        else if(size < TM_ALIGN_BYTES * 256)    return 8;
        else if(size < TM_ALIGN_BYTES * 512)    return 9;
        else                                    return 10;
    }
    else                                        return 11;
}


inline void Pool_freed_remove(Pool *pool, tm_index_t index){
    free_block *free = Pool_free_p(pool, index);
    if(free->prev){
        Pool_free_p(pool, free->prev)->next = free->next;
    } else{ // free is first element in the array
        pool->freed[freed_bin(Pool_sizeof(pool, index))] = free->next;
    }
    if(free->next) Pool_free_p(pool, free->next)->prev = free->prev;
    Pool_points_clear(pool, index);
}


tm_index_t Pool_freed_get(Pool *pool, tm_size_t size){
    assert(size % TM_ALIGN_BYTES == 0);
    uint8_t bin = freed_bin(size);
    tm_index_t index;
    for(; bin<TM_FREED_BINS; bin++){
        if(pool->freed[bin]){
            index = pool->freed[bin];
            Pool_freed_remove(pool, index);
            return index;
        }
    }
    return 0;
}

/*---------------------------------------------------------------------------*/
/*      For Debug and Test                                                   */
tm_index_t Pool_freed_count_bin(Pool *pool, uint8_t bin, tm_size_t *size){
    // Get the number and the size of the items in bin
    tm_index_t index = pool->freed[bin];
    tm_index_t count = 0;
    *size = 0;
    if(!index) return 0;
    free_block *free = Pool_free_p(pool, index);
    assert(free);
    assert(free->prev == 0);
    while(1){
        *size += Pool_sizeof(pool, index);
        count++;
        index = free->next;
        if(!index) return count;
        free = Pool_free_p(pool, index);
        assert(free);
        assert(free->prev == index);
    }
}

tm_index_t Pool_freed_count(Pool *pool, tm_size_t *size){
    uint8_t bin;
    tm_size_t size_get;
    tm_index_t count = 0;
    *size = 0;
    for(bin=0; bin<TM_FREED_BINS; bin++){
        count += Pool_freed_count_bin(pool, bin, &size_get);
        *size += size_get;
    }
    return count;
}


tm_index_t Pool_freed_print(Pool *pool){
    uint8_t bin;
    tm_size_t size = 0, size_get;
    tm_index_t count = 0, count_get;
    printf("## Freed Bins:\n");
    for(bin=0; bin<TM_FREED_BINS; bin++){
        count_get = Pool_freed_count_bin(pool, bin, &size_get);
        printf("    bin %u: size=%u, count=%u\n", bin, size_get, count_get);
        count += count_get;
        size += size_get;
    }
    printf("TOTAL: size=%u, count=%u\n", size, count);
}
