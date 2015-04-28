#include "tm_pool.h"


/*---------------------------------------------------------------------------*/
/*      Local Declarations                                                   */
tm_index_t      Pool_find_index(Pool *pool);
uint8_t         freed_bin(const tm_blocks_t size);
inline void     Pool_freed_remove(Pool *pool, const tm_index_t index);
inline void     Pool_freed_insert(Pool *pool, const tm_index_t index);
tm_index_t      Pool_freed_get(Pool *pool, const tm_blocks_t size);
#define Pool_free_p(pool, index)  ((free_block *)Pool_void_p(pool, index))


/*---------------------------------------------------------------------------*/
void *          Pool_void_p(const Pool *pool, const tm_index_t index){
    // Note: index 0 has location == heap (it is where Pool_heap is stored)
    if(Pool_loc(pool, index) >= Pool_heap(pool)) return NULL;
    return pool->pool + Pool_loc(pool, index);
}

/*---------------------------------------------------------------------------*/
tm_index_t      Pool_alloc(Pool *pool, tm_size_t size){
    tm_index_t index;
    size = TM_ALIGN_BLOCKS(size);
    if(Pool_available_blocks(pool) < size) return 0;

    index = Pool_freed_get(pool, size);
    if(index){
        pool->filled_blocks += size;
        pool->freed_blocks -= size;
        pool->ptrs_filled++;
        pool->ptrs_freed--;
        Pool_filled_set(pool, index);
        assert(Pool_points_bool(pool, index));
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
    pool->filled_blocks += size;
    pool->ptrs_filled++;
    if(pool->last_index) pool->pointers[pool->last_index].next = index;
    pool->last_index = index;
    return index;
}

/*---------------------------------------------------------------------------*/
tm_index_t      Pool_realloc(Pool *pool, tm_index_t index, tm_size_t size){
    tm_index_t new_index;
    tm_blocks_t prev_size;
    size = TM_ALIGN_BLOCKS(size);

    if(!index) return Pool_alloc(pool, size);
    if(!Pool_filled_bool(pool, index)) return 0;
    if(!size){
        Pool_free(pool, index);
        return 0;
    }
    new_index = Pool_next(pool, index);
    if(!Pool_filled_bool(pool, new_index)){
        // If next index is free, always join it first
        Pool_freed_remove(pool, new_index);
        Pool_bytes_fill(pool, Pool_blocks(pool, new_index));
        pool->ptrs_freed--;
        Pool_next(pool, index) = Pool_next(pool, new_index);
        Pool_points_clear(pool, new_index);
    }
    prev_size = Pool_blocks(pool, index);
    if(size == Pool_blocks(pool, index)) return index;
    if(size < prev_size){  // shrink data
        new_index = Pool_find_index(pool);
        if(!new_index){
            Pool_status_set(pool, TM_DEFRAG_FAST);
            return 0;
        }

        // update new index for freed data
        Pool_points_set(pool, new_index);
        pool->pointers[new_index] = (poolptr) {.loc = Pool_loc(pool, index) + size,
                                               .next = Pool_next(pool, index)};
        Pool_next(pool, index) = new_index;

        // mark changes
        Pool_bytes_free(pool, prev_size - size);
        pool->ptrs_freed++;
        Pool_freed_insert(pool, new_index);
        return index;
    } else{  // grow data
        new_index = Pool_alloc(pool, size * TM_ALIGN_BYTES);
        if(!new_index) return 0;
        Pool_memmove(pool, new_index, index);
        Pool_free(pool, index);
        return new_index;
    }
}

/*---------------------------------------------------------------------------*/
void            Pool_free(Pool *pool, const tm_index_t index){
    uint8_t bin;
    if(Pool_loc(pool, index) >= Pool_heap(pool)) return;
    if(index >= TM_MAX_POOL_PTRS || !Pool_filled_bool(pool, index)) return;
    Pool_filled_clear(pool, index);
    pool->filled_blocks -= Pool_blocks(pool, index);
    pool->freed_blocks += Pool_blocks(pool, index);
    pool->ptrs_filled--;
    pool->ptrs_freed++;

    // Note: index 0 == filled
    tm_debug("Freeing index=%u, next=%u, size=%u", index, Pool_next(pool, index), Pool_sizeof(pool, index));
    if(!Pool_filled_bool(pool, Pool_next(pool, index))){
        tm_debug("Next is free");
        // The next index is also free, remove from free bin and join
        Pool_freed_remove(pool, Pool_next(pool, index));
        pool->ptrs_freed--;
        Pool_next(pool, index) = Pool_next(pool, Pool_next(pool, index));
    }
    Pool_freed_insert(pool, index);
}


/*###########################################################################*/
/*      Local Functions                                                      */

/*---------------------------------------------------------------------------*/
tm_index_t      Pool_find_index(Pool *pool){
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
/*      get the freed bin for blocks                                         */
uint8_t         freed_bin(const tm_blocks_t blocks){
    switch(blocks){
        case 1:                     return 0;
        case 2:                     return 1;
        case 3:                     return 2;
    }
    if(blocks < 64){
        if(blocks < 8)              return 3;
        else if(blocks < 16)        return 4;
        else if(blocks < 32)        return 5;
        else                        return 6;
    }
    else if(blocks < 1024){
        if(blocks < 128)            return 7;
        else if(blocks < 256)       return 8;
        else if(blocks < 512)       return 9;
        else                        return 10;
    }
    else                            return 11;
}


inline void     Pool_freed_remove(Pool *pool, const tm_index_t index){
    free_block *free = Pool_free_p(pool, index);
    assert(free);
    if(free->prev){
        Pool_free_p(pool, free->prev)->next = free->next;
    } else{ // free is first element in the array
        pool->freed[freed_bin(Pool_blocks(pool, index))] = free->next;
    }
    if(free->next) Pool_free_p(pool, free->next)->prev = free->prev;
}


inline void     Pool_freed_insert(Pool *pool, const tm_index_t index){
    uint8_t bin = freed_bin(Pool_blocks(pool, index));
    *Pool_free_p(pool, index) = (free_block){.next=pool->freed[bin], .prev=0};
    if(pool->freed[bin]){
        Pool_free_p(pool, pool->freed[bin])->prev = index;
    }
    pool->freed[bin] = index;
}


tm_index_t      Pool_freed_get(Pool *pool, const tm_blocks_t blocks){
    uint8_t bin;
    bin = freed_bin(blocks);
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
tm_index_t      Pool_freed_count_bin(Pool *pool, uint8_t bin, tm_size_t *size, bool pnt){
    // Get the number and the size of the items in bin
    tm_index_t prev_index;
    tm_index_t index = pool->freed[bin];
    tm_index_t count = 0;
    *size = 0;
    if(!index) return 0;
    free_block *free = Pool_free_p(pool, index);
    assert(free);
    assert(free->prev == 0);
    while(1){
        if(pnt) printf("        prev=%u, ind=%u, next=%u\n", free->prev, index, free->next);
        *size += Pool_sizeof(pool, index);
        count++;
        prev_index = index;
        index = free->next;
        if(!free->next) return count;
        free = Pool_free_p(pool, index);
        assert(free);
        assert(free->prev == prev_index);
    }
}

tm_index_t      Pool_freed_count_print(Pool *pool, tm_size_t *size, bool pnt){
    uint8_t bin;
    tm_size_t size_get;
    tm_index_t count = 0;
    *size = 0;
    for(bin=0; bin<TM_FREED_BINS; bin++){
        count += Pool_freed_count_bin(pool, bin, &size_get, pnt);
        *size += size_get;
    }
    assert(count==pool->ptrs_freed);
    assert(*size==pool->freed_blocks * TM_ALIGN_BYTES);
    return count;
}

tm_index_t Pool_freed_count(Pool *pool, tm_size_t *size){
    return Pool_freed_count_print(pool, size, false);
}

bool            Pool_freed_isvalid(Pool *pool){
    tm_size_t size;
    tm_index_t count = Pool_freed_count(pool, &size);
    size = TM_ALIGN_BLOCKS(size);
    if(!((count==pool->ptrs_freed) && (size==pool->freed_blocks))){
        tm_debug("freed: %u==%u", count, pool->ptrs_freed);
        tm_debug("size:  %u==%u", size, pool->freed_blocks);
        return false;
    }
    return true;
}


tm_index_t      Pool_freed_full_print(Pool *pool, bool full){
    uint8_t bin;
    tm_size_t size = 0, size_get;
    tm_index_t count = 0, count_get;
    printf("## Freed Bins:\n");
    for(bin=0; bin<TM_FREED_BINS; bin++){
        count_get = Pool_freed_count_bin(pool, bin, &size_get, full);
        if(count_get) printf("    bin %4u: size=%-8u count=%-8u\n", bin, size_get, count_get);
        count += count_get;
        size += size_get;
    }
    printf("TOTAL: size=%u, count=%u\n", size, count);
}


tm_index_t      Pool_freed_print(Pool *pool){
    Pool_freed_full_print(pool, false);
}

