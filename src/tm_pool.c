#include "tm_pool.h"


/*---------------------------------------------------------------------------*/
/*      Local Declarations                                                   */

static Pool tm_pool = tm_init();

tm_index_t      find_index();
uint8_t         freed_bin(const tm_blocks_t size);
inline void     freed_remove(const tm_index_t index);
inline void     freed_insert(const tm_index_t index);
tm_index_t      freed_get(const tm_blocks_t size);

void index_join(const tm_index_t index, const tm_index_t with_index);
bool index_split(const tm_index_t index, const tm_blocks_t blocks);
void index_remove(const tm_index_t index, const tm_index_t prev_index);
void index_extend(const tm_index_t index, const tm_blocks_t blocks, const bool filled);
#define free_p(index)  ((free_block *)tm_void_p(index))

/*---------------------------------------------------------------------------*/
/**
 * \brief           Access Pool Characteristics
 */
#define BLOCKS_LEFT                 (TM_POOL_BLOCKS - tm_pool.filled_blocks)
#define PTRS_LEFT                   (TM_MAX_POOL_PTRS - (tm_pool.ptrs_filled + tm_pool.ptrs_filled))
#define HEAP_LEFT                   (TM_POOL_SIZE - HEAP)
#define LOCATION(index)             (tm_pool.pointers[index].loc)
#define HEAP                        (tm_pool.pointers[0].loc)
#define NEXT(index)                 (tm_pool.pointers[index].next)
#define LOC_VOID(loc)               ((void*)tm_pool.pool + (loc))

/**
 *                  FILLED* does operations on Pool's `filled` array
 *                  POINTS* does operations on Pool's `points` array
 */
#define BITARRAY_INDEX(index)        (index / 8)
#define BITARRAY_BIT(index)          (1 << (index % 8))
#define FILLED(index)               (tm_pool.filled[BITARRAY_INDEX(index)] &   BITARRAY_BIT(index))
#define FILLED_SET(index)           (tm_pool.filled[BITARRAY_INDEX(index)] |=  BITARRAY_BIT(index))
#define FILLED_CLEAR(index)         (tm_pool.filled[BITARRAY_INDEX(index)] &= ~BITARRAY_BIT(index))
#define POINTS(index)               (tm_pool.points[BITARRAY_INDEX(index)] &   BITARRAY_BIT(index))
#define POINTS_SET(index)           (tm_pool.points[BITARRAY_INDEX(index)] |=  BITARRAY_BIT(index))
#define POINTS_CLEAR(index)         (tm_pool.points[BITARRAY_INDEX(index)] &= ~BITARRAY_BIT(index))

#define tm_memmove(index_to, index_from)  memmove(                \
            tm_void_p(index_to),                                    \
            tm_void_p(index_from),                                  \
            tm_sizeof(index_from)                                   \
        )

/**
 * \brief           Get, set or clear the status bit (0 or 1) of name
 * \return uint8_t  status bit
 */
#define STATUS(name)                ((tm_pool.status) & (name))
#define STATUS_SET(name)            (tm_pool.status |= (name))
#define STATUS_CLEAR(name)          (tm_pool.status &= ~(name))

/*---------------------------------------------------------------------------*/
/*      Global Functions                                                     */

inline void tm_reset(){
    tm_pool = tm_init();
}


void *          tm_void_p(const tm_index_t index){
    // Note: index 0 has location == heap (it is where Pool_heap is stored)
    if(LOCATION(index) >= HEAP) return NULL;
    return tm_pool.pool + LOCATION(index);
}


/*---------------------------------------------------------------------------*/
tm_index_t      tm_alloc(tm_size_t size){
    tm_index_t index;
    size = TM_ALIGN_BLOCKS(size);
    if(BLOCKS_LEFT < size) return 0;
    index = freed_get(size);
    if(index){
        if(TM_BLOCKS(index) != size) index_split(index, size);
        return index;
    }
    if(HEAP_LEFT < size){
        STATUS(TM_DEFRAG_FAST);  // need less fragmentation
        return 0;
    }
    index = find_index();
    if(!index){
        STATUS(TM_DEFRAG_FAST);  // need more indexes
        return 0;
    }
    index_extend(index, size, true);  // extend index onto heap
    return index;
}


/*---------------------------------------------------------------------------*/
tm_index_t      tm_realloc(tm_index_t index, tm_size_t size){
    tm_index_t new_index;
    tm_blocks_t prev_size;
    size = TM_ALIGN_BLOCKS(size);
    tm_debug("used start=%u", tm_pool.filled_blocks);

    if(!index) return tm_alloc(size);
    if(!FILLED(index)) return 0;
    if(!size){
        tm_free(index);
        return 0;
    }
    new_index = NEXT(index);
    if(!FILLED(new_index)){
        // If next index is free, always join it first
        index_join(index, new_index);
    }
    tm_debug("used combined=%u", tm_pool.filled_blocks);
    tm_debug("    blocks=%u", TM_BLOCKS(index));
    prev_size = TM_BLOCKS(index);
    if(size == TM_BLOCKS(index)) return index;
    if(size < prev_size){  // shrink data
        tm_debug("splitting");
        if(!index_split(index, size)) return 0;
        tm_debug("used=%u", tm_pool.filled_blocks);
        tm_debug("    blocks=%u", TM_BLOCKS(index));
        return index;
    } else{  // grow data
        new_index = tm_alloc(size * TM_ALIGN_BYTES);
        if(!new_index) return 0;
        tm_memmove(new_index, index);
        tm_free(index);
        return new_index;
    }
}

/*---------------------------------------------------------------------------*/
void            tm_free(const tm_index_t index){
    uint8_t bin;
    if(LOCATION(index) >= HEAP) return;
    if(index >= TM_MAX_POOL_PTRS || !FILLED(index)) return;
    FILLED_CLEAR(index);
    tm_pool.filled_blocks -= TM_BLOCKS(index);
    tm_pool.freed_blocks += TM_BLOCKS(index);
    tm_pool.ptrs_filled--;
    tm_pool.ptrs_freed++;

    // Note: index 0 == filled
    tm_debug("Freeing index=%u, next=%u, size=%u", index, NEXT(index), tm_sizeof(index));
    if(!FILLED(NEXT(index))){
        tm_debug("Next is free");
        // The next index is also free, remove from free bin and join
        freed_remove(NEXT(index));
        tm_pool.ptrs_freed--;
        NEXT(index) = NEXT(NEXT(index));
    }
    freed_insert(index);
}


/*###########################################################################*/
/*      Local Functions                                                      */

/*---------------------------------------------------------------------------*/
tm_index_t      find_index(){
    unsigned int bit;
    if(!PTRS_LEFT) return 0;
    while(1){
        for(; tm_pool.find_index < TM_MAX_BIT_INDEXES; tm_pool.find_index++){
            if(tm_pool.points[tm_pool.find_index] != MAXUINT){
                // there is an empty value
                bit = 1 << tm_pool.find_index_bit;
                for(; tm_pool.find_index_bit < INTBITS; tm_pool.find_index_bit++){
                    if(!(tm_pool.points[tm_pool.find_index_bit] & bit)){
                        tm_pool.find_index_bit++;
                        return tm_pool.find_index * INTBITS + (tm_pool.find_index_bit - 1);
                    }
                    bit = bit << 1;
                }
            }
            tm_pool.find_index_bit = 0;
        }
        tm_pool.find_index = 0;
        tm_pool.find_index_bit = 1;   // index 0 is invalid
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


inline void     freed_remove(const tm_index_t index){
    free_block *free = free_p(index);
    assert(free);
    if(free->prev){
        free_p(free->prev)->next = free->next;
    } else{ // free is first element in the array
        tm_pool.freed[freed_bin(TM_BLOCKS(index))] = free->next;
    }
    if(free->next) free_p(free->next)->prev = free->prev;
}


inline void     freed_insert(const tm_index_t index){
    uint8_t bin = freed_bin(TM_BLOCKS(index));
    *free_p(index) = (free_block){.next=tm_pool.freed[bin], .prev=0};
    if(tm_pool.freed[bin]){
        free_p(tm_pool.freed[bin])->prev = index;
    }
    tm_pool.freed[bin] = index;
}


tm_index_t      freed_get(const tm_blocks_t blocks){
    // Get an index from the freed array of the specified size. The
    //      index settings are automatically set to filled
    uint8_t bin;
    bin = freed_bin(blocks);
    tm_index_t index;
    for(; bin<TM_FREED_BINS; bin++){
        if(tm_pool.freed[bin]){
            index = tm_pool.freed[bin];
            assert(!FILLED(index));
            freed_remove(index);
            tm_pool.filled_blocks += TM_BLOCKS(index);
            tm_pool.freed_blocks -= TM_BLOCKS(index);
            tm_pool.ptrs_filled++;
            tm_pool.ptrs_freed--;
            FILLED_SET(index);
            return index;
        }
    }
    return 0;
}


/*---------------------------------------------------------------------------*/
/*          Index Operations (remove, join, etc)                             */

void index_join(const tm_index_t index, const tm_index_t with_index){
    // join index with_index. with_index will be removed
    // TODO: this should join all the way up -- as many free indexes as it finds
    assert(!FILLED(with_index));
    if(FILLED(index)){
        tm_pool.filled_blocks += TM_BLOCKS(with_index);
        tm_pool.freed_blocks -= TM_BLOCKS(with_index);
    }
    NEXT(index) = NEXT(with_index);
    index_remove(with_index, index);
    tm_pool.ptrs_freed--;
    if(!FILLED(index)){ // rebin the index, as it has grown
        freed_remove(index);
        freed_insert(index);
    }
}

bool index_split(const tm_index_t index, const tm_blocks_t blocks){
    // split an index so the index becomes size blocks. Automatically creates a new freed
    //      index to store the new free data
    //      returns true on success, false on failure
    assert(blocks < TM_BLOCKS(index));
    tm_index_t new_index = NEXT(index);
    if(!FILLED(new_index)){
        // If next index is free, always join it first. This also frees up new_index to
        // use however we want!
        index_join(index, new_index);
    } else{
        new_index = find_index();
        if(!new_index) return false;
    }

    // update new index for freed data
    POINTS_SET(new_index);
    assert(!FILLED(new_index));

    if(FILLED(index)){
        tm_pool.freed_blocks += TM_BLOCKS(index) - blocks;
        tm_pool.filled_blocks -= TM_BLOCKS(index) - blocks;
    }
    tm_pool.ptrs_freed++;
    tm_pool.pointers[new_index] = (poolptr) {.loc = LOCATION(index) + blocks,
                                           .next = NEXT(index)};
    NEXT(index) = new_index;

    // mark changes
    freed_insert(new_index);
    if(tm_pool.last_index == index) tm_pool.last_index = new_index;
    return true;
}

void index_remove(const tm_index_t index, const tm_index_t prev_index){
    // Completely remove the index. Used for combining indexes and when defragging
    // from end (to combine heap)
    if(!FILLED(index)){
        freed_remove(index);
        tm_pool.freed_blocks -= TM_BLOCKS(index);
        tm_pool.ptrs_freed--;
    } else{
        tm_pool.filled_blocks -= TM_BLOCKS(index);
        tm_pool.ptrs_filled--;
    }
    if(!NEXT(index)) { // this is the last index
        assert(tm_pool.last_index == index);
        tm_pool.last_index = prev_index;
        NEXT(prev_index) = 0;
        HEAP = LOCATION(index);
    }
    // TODO: if first index
    FILLED_CLEAR(index);
    POINTS_CLEAR(index);
}

void index_extend(const tm_index_t index, const tm_blocks_t blocks,
        const bool filled){
    // extend index onto the heap
    POINTS_SET(index);
    tm_pool.pointers[index] = (poolptr) {.loc = HEAP, .next = 0};
    HEAP += blocks;
    if(tm_pool.last_index) NEXT(tm_pool.last_index) = index;
    tm_pool.last_index = index;
    // TODO: if first

    if(filled){
        FILLED_SET(index);
        tm_pool.filled_blocks += blocks;
        tm_pool.ptrs_filled++;
    }
    else{
        assert(!FILLED(index));
        tm_pool.freed_blocks += blocks;
        tm_pool.ptrs_freed++;
    }

}

/*---------------------------------------------------------------------------*/
/*      For Debug and Test                                                   */
tm_index_t      freed_count_bin(uint8_t bin, tm_size_t *size, bool pnt){
    // Get the number and the size of the items in bin
    tm_index_t prev_index;
    tm_index_t index = tm_pool.freed[bin];
    tm_index_t count = 0;
    *size = 0;
    if(!index) return 0;
    free_block *free = free_p(index);
    assert(free);
    assert(free->prev == 0);
    while(1){
        assert(!FILLED(index));
        if(pnt) printf("        prev=%u, ind=%u, next=%u\n", free->prev, index, free->next);
        *size += tm_sizeof(index);
        count++;
        prev_index = index;
        index = free->next;
        if(!free->next) return count;
        free = free_p(index);
        assert(free);
        assert(free->prev == prev_index);
    }
}

tm_index_t      freed_count_print(tm_size_t *size, bool pnt){
    uint8_t bin;
    tm_size_t size_get;
    tm_index_t count = 0;
    *size = 0;
    for(bin=0; bin<TM_FREED_BINS; bin++){
        count += freed_count_bin(bin, &size_get, pnt);
        *size += size_get;
    }
    assert(count==tm_pool.ptrs_freed);
    tm_debug("%u==%u", *size, tm_pool.freed_blocks * TM_ALIGN_BYTES);
    assert(*size==tm_pool.freed_blocks * TM_ALIGN_BYTES);
    return count;
}

tm_index_t freed_count(tm_size_t *size){
    return freed_count_print(size, false);
}

bool            freed_isvalid(){
    tm_size_t size;
    tm_index_t count = freed_count(&size);
    size = TM_ALIGN_BLOCKS(size);
    if(!((count==tm_pool.ptrs_freed) && (size==tm_pool.freed_blocks))){
        tm_debug("freed: %u==%u", count, tm_pool.ptrs_freed);
        tm_debug("size:  %u==%u", size, tm_pool.freed_blocks);
        return false;
    }
    return true;
}


tm_index_t      freed_full_print(bool full){
    uint8_t bin;
    tm_size_t size = 0, size_get;
    tm_index_t count = 0, count_get;
    printf("## Freed Bins:\n");
    for(bin=0; bin<TM_FREED_BINS; bin++){
        count_get = freed_count_bin(bin, &size_get, full);
        if(count_get) printf("    bin %4u: size=%-8u count=%-8u\n", bin, size_get, count_get);
        count += count_get;
        size += size_get;
    }
    printf("TOTAL: size=%u, count=%u\n", size, count);
}


tm_index_t      freed_print(){
    freed_full_print(false);
}

