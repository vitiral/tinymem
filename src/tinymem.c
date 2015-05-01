#include "tinymem.h"

#define DBGprintf(...)      printf(__VA_ARGS__)

/*#define tm_debug(...)       do{}while(0)*/
/*#define DBGprintf(...)      do{}while(0)*/

typedef uint16_t        tm_blocks_t;

/*---------------------------------------------------------------------------*/
/**
 * \brief           poolptr is used by Pool to track memory location and size
 */
// TODO: packed!
typedef struct {
    tm_blocks_t loc;
    tm_index_t next;
} poolptr;

/*---------------------------------------------------------------------------*/
/**
 * \brief           free_block is stored INSIDE of freed memory as a linked list
 *                  of all freed data
 */
// TODO: packed!
typedef struct {
    tm_index_t prev;
    tm_index_t next;
} free_block;

#if TM_POOL_SIZE % (4)
#error "Invalid pool size, must be divisible by free_block"
#endif

/*#if (TM_MAX_POOL_PTRS % (8 * sizeof(int)))*/
#if (TM_MAX_POOL_PTRS % (8 * 4))
#error "Invalid pool ptrs size, must be divisible by int"
#endif


#define CEILING(x, y)           (((x) % (y)) ? (x)/(y) + 1 : (x)/(y))

#define POOL_BLOCKS         (TM_POOL_SIZE / sizeof(free_block))             // total blocks available
#define MAX_BIT_INDEXES     (TM_MAX_POOL_PTRS / (8 * sizeof(int)))          // for filled/points
#define MAXUINT             ((unsigned int) 0xFFFFFFFFFFFFFFFF)
#define INTBITS             (sizeof(int) * 8)                               // bits in an integer
// Masks of an integer
#define LOWER_MASK          (((unsigned int)0xFFFFFFFFFFFFFFFF) >> (sizeof(int) * 8 / 2))
#define UPPER_MASK          (LOWER_MASK << (sizeof(int) * 8 / 2))

#define FREED_BINS          (12)                                // bins to store freed values

// data is aligned on blocks
#define BLOCK_SIZE          (sizeof(free_block))                // size of a block in bytes
#define ALIGN_BLOCKS(size)  CEILING(size, BLOCK_SIZE)           // get block value that can encompase size
#define ALIGN_BYTES(size)   (ALIGN_BLOCKS(size) * BLOCK_SIZE)   // get value in bytes


/*---------------------------------------------------------------------------*/
/**
 * \brief           Initialize (reset) the pool
 */
#define tm_init()  ((Pool) {                                    \
    .filled = {1},                      /*NULL is taken*/       \
    .points = {1},                      /*NULL is taken*/       \
    .pointers = {{0, 0}},               /*heap = 0*/            \
    .freed = {0},                                               \
    .filled_blocks = 0,                                         \
    .freed_blocks = 0,                                          \
    .ptrs_filled = 1,                    /*NULL is "filled"*/   \
    .ptrs_freed = 0,                                            \
    .find_index = 0,                                            \
    .first_index = 0,                                           \
    .last_index = 0,                                            \
    .status = 0,                                                \
    .defrag_index=0,                                            \
})

/*---------------------------------------------------------------------------*/
/**
 * \brief           Pool object to track all memory usage
 *                  This is the main object used by tinymem to do memory
 *                  management
 */
typedef struct {
    free_block      pool[POOL_BLOCKS];           //!< Actual memory pool (very large)
    unsigned int    filled[MAX_BIT_INDEXES];     //!< bit array of filled pointers (only used, not freed)
    unsigned int    points[MAX_BIT_INDEXES];     //!< bit array of used pointers (both used and freed)
    poolptr         pointers[TM_MAX_POOL_PTRS];     //!< This is the index lookup location
    tm_index_t      freed[FREED_BINS];           //!< binned storage of all freed indexes
    tm_blocks_t     filled_blocks;                //!< total amount of data allocated
    tm_blocks_t     freed_blocks;                 //!< total amount of data freed
    tm_index_t      ptrs_filled;                    //!< total amount of pointers allocated
    tm_index_t      ptrs_freed;                     //!< total amount of pointers freed
    tm_index_t      find_index;                     //!< speed up find index
    tm_index_t      first_index;                    //!< required to start defrag
    tm_index_t      last_index;                     //!< required to allocate off heap
    uint8_t         status;                         //!< status byte. Access with Pool_status macros
    tm_index_t      defrag_index;                //!< used during defrag
} Pool;

Pool tm_pool = tm_init();


/*---------------------------------------------------------------------------*/
/*      Local Functions                                                      */

tm_index_t      find_index();
uint8_t         freed_bin(const tm_blocks_t blocks);
uint8_t         freed_bin_get(const tm_blocks_t blocks);
inline void     freed_remove(const tm_index_t index);
inline void     freed_insert(const tm_index_t index);
tm_index_t      freed_get(const tm_blocks_t size);

void index_extend(const tm_index_t index, const tm_blocks_t blocks, const bool filled);
void index_remove(const tm_index_t index, const tm_index_t prev_index);
void index_join(const tm_index_t index, const tm_index_t with_index);
bool index_split(const tm_index_t index, const tm_blocks_t blocks);
#define free_p(index)  ((free_block *)tm_void_p(index))

// For testing
void                freed_print();
void                freed_full_print(bool full);
tm_index_t          freed_count_print(tm_size_t *size, bool pnt);
inline void         index_print(tm_index_t index);

tm_index_t          freed_count(tm_size_t *size);
tm_index_t          freed_count_bin(uint8_t bin, tm_size_t *size, bool pnt);
bool                freed_isvalid();
bool                freed_isin(const tm_index_t index);
bool                pool_isvalid();
void                fill_index(tm_index_t index);
bool                check_index(tm_index_t index);

/*---------------------------------------------------------------------------*/
/**
 * \brief           Access Pool Characteristics
 */
#define BLOCKS_LEFT                 (POOL_BLOCKS - tm_pool.filled_blocks)
#define BYTES_LEFT                  (BLOCKS_LEFT * BLOCK_SIZE)
#define PTRS_USED                   (tm_pool.ptrs_filled + tm_pool.ptrs_freed)
#define PTRS_LEFT                   (TM_MAX_POOL_PTRS - PTRS_USED)
#define HEAP_LEFT                   (POOL_BLOCKS - HEAP)
#define HEAP_LEFT_BYTES             (HEAP_LEFT * BLOCK_SIZE)

/**
 * \brief           Get, set or clear the status bit (0 or 1) of name
 */
#define STATUS(name)                ((tm_pool.status) & (name))
#define STATUS_SET(name)            (tm_pool.status |= (name))
#define STATUS_CLEAR(name)          (tm_pool.status &= ~(name))

/*---------------------------------------------------------------------------*/
/**
 * \brief           Access index characteristics
 */
#define LOCATION(index)             (tm_pool.pointers[index].loc)
#define HEAP                        (tm_pool.pointers[0].loc)
#define NEXT(index)                 (tm_pool.pointers[index].next)
#define FREE_NEXT(index)            ((free_p(index))->next)
#define FREE_PREV(index)            ((free_p(index))->prev)
#define BLOCKS(index)               ((tm_blocks_t) (LOCATION(tm_pool.pointers[index].next) - \
                                        LOCATION(index)))       // sizeof index in blocks
#define LOC_VOID(loc)               ((void*)tm_pool.pool + ((loc) * BLOCK_SIZE))

/*---------------------------------------------------------------------------*/
/**
 * \brief           Move memory from one index to another
 */
#define MEM_MOVE(index_to, index_from)  memmove(                \
            tm_void_p(index_to),                                    \
            tm_void_p(index_from),                                  \
            tm_sizeof(index_from)                                   \
        )

/**
 *                  FILLED* does operations on Pool's `filled` bit array
 *                  POINTS* does operations on Pool's `points` bit array
 */
#define BITARRAY_INDEX(index)       ((index) / (sizeof(int) * 8))
#define BITARRAY_BIT(index)         (1 << ((index) % (sizeof(int) * 8)))
#define FILLED(index)               (tm_pool.filled[BITARRAY_INDEX(index)] &   BITARRAY_BIT(index))
#define FILLED_SET(index)           (tm_pool.filled[BITARRAY_INDEX(index)] |=  BITARRAY_BIT(index))
#define FILLED_CLEAR(index)         (tm_pool.filled[BITARRAY_INDEX(index)] &= ~BITARRAY_BIT(index))
#define POINTS(index)               (tm_pool.points[BITARRAY_INDEX(index)] &   BITARRAY_BIT(index))
#define POINTS_SET(index)           (tm_pool.points[BITARRAY_INDEX(index)] |=  BITARRAY_BIT(index))
#define POINTS_CLEAR(index)         (tm_pool.points[BITARRAY_INDEX(index)] &= ~BITARRAY_BIT(index))


/*---------------------------------------------------------------------------*/
/*      Global Function Definitions                                          */

inline tm_size_t tm_sizeof(const tm_index_t index){
    return BLOCKS(index) * BLOCK_SIZE;
}

/*---------------------------------------------------------------------------*/
inline void tm_reset(){
    tm_pool = tm_init();
}

/*---------------------------------------------------------------------------*/
void *          tm_void_p(const tm_index_t index){
    // Note: index 0 has location == heap (it is where Pool_heap is stored)
    if(LOCATION(index) >= HEAP) return NULL;
    return tm_pool.pool + LOCATION(index);
}

/*---------------------------------------------------------------------------*/
tm_index_t      tm_alloc(tm_size_t size){
    tm_index_t index;
    size = ALIGN_BLOCKS(size);  // convert from bytes to blocks
    if(BLOCKS_LEFT < size) return 0;
    index = freed_get(size);
    if(index){
        if(BLOCKS(index) != size){ // Split the index if it is too big
            if(!index_split(index, size)){
                // Split can fail if there are not enough pointers
                tm_free(index);
                STATUS_SET(TM_DEFRAG_FAST);  // need more indexes
                return 0;
            }
        }
        return index;
    }
    if(HEAP_LEFT < size){
        STATUS_SET(TM_DEFRAG_FAST);  // need less fragmentation
        return 0;
    }
    index = find_index();
    if(!index){
        STATUS_SET(TM_DEFRAG_FAST);  // need more indexes
        return 0;
    }
    index_extend(index, size, true);  // extend index onto heap
    return index;
}


/*---------------------------------------------------------------------------*/
tm_index_t      tm_realloc(tm_index_t index, tm_size_t size){
    tm_index_t new_index;
    tm_blocks_t prev_size;
    size = ALIGN_BLOCKS(size);

    assert(0);  // not used currently
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
    prev_size = BLOCKS(index);
    if(size == BLOCKS(index)) return index;
    if(size < prev_size){  // shrink data
        if(!index_split(index, size)) return 0;
        return index;
    } else{  // grow data
        new_index = tm_alloc(size * BLOCK_SIZE);
        if(!new_index) return 0;
        MEM_MOVE(new_index, index);
        tm_free(index);
        return new_index;
    }
}

/*---------------------------------------------------------------------------*/
void            tm_free(const tm_index_t index){
    if(LOCATION(index) >= HEAP){assert(0); return;}
    if((index >= TM_MAX_POOL_PTRS) || (!FILLED(index))){assert(0); return;}
    FILLED_CLEAR(index);
    tm_pool.filled_blocks -= BLOCKS(index);
    tm_pool.freed_blocks += BLOCKS(index);
    tm_pool.ptrs_filled--;
    tm_pool.ptrs_freed++;

    freed_insert(index);
    // Join all the way up if next index is free
    if(!FILLED(NEXT(index))){
        index_join(index, NEXT(index));
    }
}

/*---------------------------------------------------------------------------*/
bool            tm_defrag(){
    // TODO: implement re-entrant threaded
    /*assert(0);  // defrag not ready*/
#ifndef NDEBUG
    tm_index_t i = 0;
    tm_blocks_t used = tm_pool.filled_blocks;
    tm_blocks_t available = BLOCKS_LEFT, heap = HEAP_LEFT, freed = tm_pool.freed_blocks;
#endif
    tm_blocks_t blocks;
    tm_blocks_t location;
    tm_index_t prev_index = 0;
    if(!STATUS(TM_DEFRAG_IP)){
        tm_pool.defrag_index = tm_pool.first_index;
        STATUS_CLEAR(TM_ANY_DEFRAG);
        STATUS_SET(TM_DEFRAG_FULL_IP);
    }
    if(!tm_pool.defrag_index) goto done;
    while(NEXT(tm_pool.defrag_index)){
        if(!FILLED(tm_pool.defrag_index)){
            if(!FILLED(NEXT(tm_pool.defrag_index))){
                /*DBGprintf("  joining above\n", tm_pool.defrag_index);*/
                index_join(tm_pool.defrag_index, NEXT(tm_pool.defrag_index));
            }
            if(!NEXT(tm_pool.defrag_index)) break;

            /*DBGprintf("### Defrag: loop=%-11u", i); index_print(tm_pool.defrag_index);*/
            assert(FILLED(NEXT(tm_pool.defrag_index)));
            blocks = BLOCKS(NEXT(tm_pool.defrag_index));        // store size of actual data
            location = LOCATION(NEXT(tm_pool.defrag_index));    // location of actual data

            // Make index "filled", we will split it up later
            freed_remove(tm_pool.defrag_index);
            FILLED_SET(tm_pool.defrag_index);
            tm_pool.ptrs_filled++, tm_pool.filled_blocks+=BLOCKS(tm_pool.defrag_index);
            tm_pool.ptrs_freed--, tm_pool.freed_blocks-=BLOCKS(tm_pool.defrag_index);

            // Do an odd join, where the locations are just equal
            /*DBGprintf("  New bef join  :           "); index_print(NEXT(tm_pool.defrag_index));*/
            LOCATION(NEXT(tm_pool.defrag_index)) = LOCATION(tm_pool.defrag_index);
            /*DBGprintf("  After odd join:           "); index_print(tm_pool.defrag_index);*/
            /*DBGprintf("  New after join:           "); index_print(NEXT(tm_pool.defrag_index));*/

            // Now remove the index. Note that the size is == 0
            //      Also note that even though it was removed, it's NEXT and LOCATION
            //      are still valid (not changed in remove index)
            index_remove(tm_pool.defrag_index, prev_index);
            prev_index = NEXT(tm_pool.defrag_index);  // defrag_index was removed

            assert(LOCATION(prev_index) < POOL_BLOCKS);
            assert(location < POOL_BLOCKS);
            /*tm_debug("location to=%u, from=%u, size=%u, end=%u, ", LOCATION(prev_index), location,*/
                    /*blocks, POOL_BLOCKS);*/
            memmove(LOC_VOID(LOCATION(prev_index)),
                    LOC_VOID(location), ((tm_size_t)blocks) * BLOCK_SIZE);
            /*DBGprintf("  New index, before split:  "); index_print(prev_index);*/
            if(!index_split(NEXT(tm_pool.defrag_index), blocks)){
                assert(0); exit(-1);
            }
            /*DBGprintf("  New index, after split:   "); index_print(prev_index);*/

            /*tm_debug("blocks=%u, freespace=%u, %u==%u", blocks, BLOCKS(NEXT(prev_index)),*/
                     /*used, tm_pool.filled_blocks);*/
            /*tm_debug("%u==%u", BLOCKS(prev_index), blocks);*/
            assert(BLOCKS(prev_index) == blocks);

            tm_pool.defrag_index = NEXT(prev_index);

            /*tm_debug("%u==%u", used, tm_pool.filled_blocks);*/
            assert(!FILLED(tm_pool.defrag_index));

        } else{
            prev_index = tm_pool.defrag_index;
            tm_pool.defrag_index = NEXT(tm_pool.defrag_index);
        }
        assert(prev_index != tm_pool.defrag_index);
        assert((i++, used == tm_pool.filled_blocks));
        assert(available == BLOCKS_LEFT);
        /*assert(pool_isvalid());*/
    }
done:
    if(!FILLED(tm_pool.defrag_index)){
        /*tm_debug("removing last index");*/
        /*DBGprintf("index       :"); index_print(tm_pool.defrag_index);*/
        /*DBGprintf("prev_index  :"); index_print(prev_index);*/
        index_remove(tm_pool.defrag_index, prev_index);
    }
    STATUS_CLEAR(TM_DEFRAG_IP);
    STATUS_SET(TM_DEFRAG_FULL_DONE);
    DBGprintf("## Defrag done: Heap left: start=%u, end=%lu, recovered=%lu, wasfree=%u was_avail=%lu isavail=%lu\n",
            heap, HEAP_LEFT, HEAP_LEFT - heap, freed, available, BLOCKS_LEFT);
    assert(HEAP_LEFT - heap == freed);
    assert(tm_pool.freed_blocks == 0);
    assert(HEAP_LEFT == BLOCKS_LEFT);
    assert(pool_isvalid());
    return 0;
}

/*###########################################################################*/
/*      Local Functions                                                      */

/*---------------------------------------------------------------------------*/
tm_index_t      find_index(){
    uint8_t loop;
    unsigned int bits;
    uint8_t bit;
    uint8_t i;
    if(!PTRS_LEFT) return 0;
    for(loop=0; loop<2; loop++){
        for(; tm_pool.find_index < MAX_BIT_INDEXES; tm_pool.find_index++){
            bits = tm_pool.points[tm_pool.find_index];
            if(bits != MAXUINT){
                bit = 0;
                /*tm_debug("bits=%x", bits);*/
                /*tm_debug("lower mask=%x", LOWER_MASK);*/
                /*tm_debug("operation=%x", bits & LOWER_MASK);*/
                if((bits & LOWER_MASK) == LOWER_MASK){
                    // bits are in the second half
                    bit += sizeof(int)  * 8 / 2;
                    bits = bits >> (sizeof(int) * 8 / 2);
                    /*tm_debug("ubits=%x", bits);*/
                }
                /*tm_debug("for bits=%x", bits);*/
                for(i=0; i < sizeof(int) * 2; i++){
                    switch(bits & 0xF){
                        case 0b0000: case 0b0010: case 0b0100: case 0b0110:
                        case 0b1000: case 0b1010: case 0b1100: case 0b1110:
                            goto found;
                        case 0b0001: case 0b0101: case 0b1001: case 0b1101:
                            bit += 1;
                            goto found;
                        case 0b0011: case 0b1011:
                            bit += 2;
                            goto found;
                        case 0b0111:
                            bit += 3;
                            goto found;
                    }
                    bit += 4;
                    bits = bits >> 4;
                    /*tm_debug("breaking bits=%x", bits);*/
                }
                assert(0);
found:
                /*tm_debug("bit=%u, true_bits=%x", bit, tm_pool.points[tm_pool.find_index]);*/
                /*tm_debug("index=%u", tm_pool.find_index * INTBITS + bit);*/
                assert(!POINTS(tm_pool.find_index * INTBITS + bit));
                assert(!FILLED(tm_pool.find_index * INTBITS + bit));
                return tm_pool.find_index * INTBITS + bit;
            }
        }
        tm_pool.find_index = 0;
    }
    assert(0);
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

uint8_t         freed_bin_get(const tm_blocks_t blocks){
    // Used for getting the bin to return. Makes certain that the size
    // value is >= blocks

    // If it aligns perfectly with power of 2, we can fit it
    switch(blocks){
        case 1:                     return 0;
        case 2:                     return 1;
        case 3:                     return 2;
        case 4:                     return 3;
        case 8:                     return 4;
        case 16:                    return 5;
        case 32:                    return 6;
        case 64:                    return 7;
        case 128:                   return 8;
        case 256:                   return 9;
        case 512:                   return 10;
        case 1024:                  return 11;
    }

    // Else, we only know that the bin above it will fit it
    //      User has to make sure bin != 12, if it does it has to check the size
    return freed_bin(blocks) + 1;
}


inline void     freed_remove(const tm_index_t index){
    // remove the index from the freed array. This doesn't do anything else
    //      It is very important that this is called BEFORE any changes
    //      to the index's size
    assert(!FILLED(index));
    assert(freed_isin(index));
    if(FREE_PREV(index)){
        // if previous exists, move it's next as index's next
        assert(FREE_NEXT(FREE_PREV(index)) == index);
        FREE_NEXT(FREE_PREV(index)) = FREE_NEXT(index);
    } else{ // free is first element in the bin
        assert(tm_pool.freed[freed_bin(BLOCKS(index))] == index);
        tm_pool.freed[freed_bin(BLOCKS(index))] = FREE_NEXT(index);
    }
    if(FREE_NEXT(index)) FREE_PREV(FREE_NEXT(index)) = FREE_PREV(index);
}


inline void     freed_insert(const tm_index_t index){
    // Insert the index onto the correct freed bin
    //      (inserts at position == 0)
    // Does not do ANY other record keeping (no adding ptrs, blocks, etc)
    uint8_t bin = freed_bin(BLOCKS(index));
    assert(!FILLED(index));
    *free_p(index) = (free_block){.next=tm_pool.freed[bin], .prev=0};
    if(tm_pool.freed[bin]){
        // If a previous index exists, update it's previous value to be index
        FREE_PREV(tm_pool.freed[bin]) = index;
    }
    tm_pool.freed[bin] = index;
}


tm_index_t      freed_get(const tm_blocks_t blocks){
    // Get an index from the freed array of the specified size. The
    //      index settings are automatically set to filled
    tm_index_t index;
    uint8_t bin = freed_bin_get(blocks);
    if(bin == FREED_BINS){  // size is off the binning charts
        index = tm_pool.freed[FREED_BINS-1];
        while(index){
            if(BLOCKS(index) >= blocks) goto found;
            index = FREE_NEXT(index);
        }
        // no need to return here: bin == FREED_BINS
    }
    for(; bin<FREED_BINS; bin++){
        index = tm_pool.freed[bin];
        if(index){
found:
            assert(POINTS(index)); assert(!FILLED(index));
            freed_remove(index);
            FILLED_SET(index);
            // Mark the index as filled. It is already on the indexes list
            tm_pool.filled_blocks += BLOCKS(index);
            tm_pool.freed_blocks -= BLOCKS(index);
            tm_pool.ptrs_filled++;
            tm_pool.ptrs_freed--;
            return index;
        }
    }
    return 0;
}


/*---------------------------------------------------------------------------*/
/*          Index Operations (remove, join, etc)                             */

void index_extend(const tm_index_t index, const tm_blocks_t blocks,
        const bool filled){
    // extend index onto the heap
    assert(!POINTS(index));
    assert(!FILLED(index));
    POINTS_SET(index);
    tm_pool.pointers[index] = (poolptr) {.loc = HEAP, .next = 0};
    HEAP += blocks;
    if(tm_pool.last_index) NEXT(tm_pool.last_index) = index;
    tm_pool.last_index = index;
    if(!tm_pool.first_index) tm_pool.first_index = index;
    if(filled){
        FILLED_SET(index);
        tm_pool.filled_blocks += blocks;
        tm_pool.ptrs_filled++;
    }
    else{
        assert(0);      // not used currently
        assert(!FILLED(index));
        tm_pool.freed_blocks += blocks;
        tm_pool.ptrs_freed++;
        freed_insert(index);
    }
}


void index_remove(const tm_index_t index, const tm_index_t prev_index){
    // Completely remove the index. Used for combining indexes and when defragging
    //      from end (to combine heap).
    //      This function also combines the indexes (NEXT(prev_index) = NEXT(index))
    // Note: NEXT(index) and LOCATION(index) **must not change**. They are used by other code
    //
    //  Update information
    // possibilities:
    //      both are free:              this happens when joining free indexes
    //      prev filled, index free:    this happens for realloc and all the time
    //      prev free, index filled:    this should only happen during defrag
    //      both filled:                this should never happen
    //
    //
    //
    switch(((FILLED(prev_index) ? 1:0) << 1) + (FILLED(index) ? 1:0)){
        case 0b00:  // merging two free values
            // TODO: this causes failure, find out why
            /*assert(freed_isin(prev_index) && freed_isin(index));*/
            freed_remove(index);
            tm_pool.ptrs_freed--;  // no change in blocks, both are free
            // if index is last value, freed_blocks will be reduced
            if(!NEXT(index)) tm_pool.freed_blocks -= BLOCKS(index);
            break;
        case 0b10:  // growing prev_index "up"
            assert(freed_isin(index));
            freed_remove(index);
            tm_pool.freed_blocks -= BLOCKS(index); tm_pool.ptrs_freed--;
            // grow prev_index, unless index is last value
            if(NEXT(index)) tm_pool.filled_blocks += BLOCKS(index);
            break;
        case 0b11:  // combining two filled indexes, used ONLY in defrag
            assert(STATUS(TM_DEFRAG_IP));  // TODO: have a better way to check when threading
            tm_pool.ptrs_filled--;
            break;
        default:
            assert(0);
    }

    if(index == tm_pool.first_index) tm_pool.first_index = NEXT(index);
    // Combine indexes (or heap)
    if(NEXT(index)) {
        NEXT(prev_index) = NEXT(index);
    } else{ // this is the last index, move the heap
        assert(tm_pool.last_index == index);
        tm_pool.last_index = prev_index;
        if(prev_index)  NEXT(prev_index) = 0;
        else            tm_pool.first_index = 0;  // prev_index == 0
        HEAP = LOCATION(index);
    }
    FILLED_CLEAR(index);
    POINTS_CLEAR(index);
}

void index_join(tm_index_t index, tm_index_t with_index){
    // join index with_index. with_index will be removed
    do{
        assert(!FILLED(with_index));
        assert(LOCATION(index) <= LOCATION(with_index));
        if(!FILLED(index)) freed_remove(index); // index has to be rebinned, remove before changing size
        // Remove and combine the index
        index_remove(with_index, index);
        if(!FILLED(index)) freed_insert(index); // rebin the index
        with_index = NEXT(index);
    }while(!FILLED(with_index));
}

bool index_split(const tm_index_t index, const tm_blocks_t blocks){
    assert(blocks < BLOCKS(index));
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
    assert(!POINTS(new_index));
    assert(!FILLED(new_index));
    POINTS_SET(new_index);

    if(FILLED(index)){ // there will be some newly freed data
        tm_pool.freed_blocks += BLOCKS(index) - blocks;
        tm_pool.filled_blocks -= BLOCKS(index) - blocks;
    }

    tm_pool.ptrs_freed++;
    tm_pool.pointers[new_index] = (poolptr) {.loc = LOCATION(index) + blocks,
                                             .next = NEXT(index)};
    NEXT(index) = new_index;

    // mark changes
    freed_insert(new_index);
    if(tm_pool.last_index == index){
        tm_pool.last_index = new_index;
    }
    else{
        assert(NEXT(index));
        assert(NEXT(new_index));
    }
    return true;
}


/*---------------------------------------------------------------------------*/
/*      For Debug and Test                                                   */

#define PRIME       (65599)
bool testing = false;

void                pool_print(){
    DBGprintf("## Pool (status=%x):\n", tm_pool.status);
    DBGprintf("    mem blocks: heap=  %-7u     filled=%-7u  freed=%-7u     total=%-7u\n",
            HEAP, tm_pool.filled_blocks, tm_pool.freed_blocks, POOL_BLOCKS);
    DBGprintf("    avail ptrs: filled=  %-7u   freed= %-7u   used=%-7u,    total= %-7u\n",
            tm_pool.ptrs_filled, tm_pool.ptrs_freed, PTRS_USED, TM_MAX_POOL_PTRS);
    DBGprintf("    indexes   : first=%u, last=%u\n", tm_pool.first_index, tm_pool.last_index);
}

void                freed_print(){
    freed_full_print(false);
}

void            freed_full_print(bool full){
    uint8_t bin;
    tm_size_t size = 0, size_get;
    tm_index_t count = 0, count_get;
    DBGprintf("## Freed Bins:\n");
    for(bin=0; bin<FREED_BINS; bin++){
        count_get = freed_count_bin(bin, &size_get, full);
        if(count_get) DBGprintf("    bin %4u: size=%-8u count=%-8u\n", bin, size_get, count_get);
        count += count_get;
        size += size_get;
    }
    DBGprintf("TOTAL: size=%u, count=%u\n", size, count);
}

tm_index_t      freed_count_print(tm_size_t *size, bool pnt){
    uint8_t bin;
    tm_size_t size_get;
    tm_index_t count = 0;
    *size = 0;
    for(bin=0; bin<FREED_BINS; bin++){
        count += freed_count_bin(bin, &size_get, pnt);
        *size += size_get;
    }
    assert(count==tm_pool.ptrs_freed);
    assert(*size==tm_pool.freed_blocks * BLOCK_SIZE);
    return count;
}

inline void         index_print(tm_index_t index){
    DBGprintf("index %-5u(%u,%u):bl=%-4u, l=%-5u, n=%-5u, f/l=%u,%u", index,
           !!POINTS(index), !!FILLED(index),
           BLOCKS(index), LOCATION(index), NEXT(index),
           tm_pool.first_index == index, tm_pool.last_index == index);
    if(FILLED(index))       DBGprintf("\n");
    else                    DBGprintf(" free:b=%u p=%u n=%u\n", freed_bin(BLOCKS(index)),
                                   FREE_PREV(index), FREE_NEXT(index));
}

tm_index_t      freed_count(tm_size_t *size){
    return freed_count_print(size, false);
}

tm_index_t      freed_count_bin(uint8_t bin, tm_size_t *size, bool pnt){
    // Get the number and the size of the items in bin
    tm_index_t index = tm_pool.freed[bin];
    tm_index_t count = 0;
    *size = 0;
    if(!index) return 0;
    assert(FREE_PREV(index)== 0);
    while(index){
        /*tm_debug("loc=%u, blocks=%u", LOCATION(index), POOL_BLOCKS);*/
        assert(LOCATION(index) < POOL_BLOCKS);
        assert(POINTS(index));
        assert(!FILLED(index));
        if(pnt) DBGprintf("        prev=%u, ind=%u, next=%u\n", FREE_PREV(index), index, FREE_NEXT(index));
        *size += tm_sizeof(index);
        count++;
        if(FREE_NEXT(index)) assert(index == FREE_PREV(FREE_NEXT(index)));
        index = FREE_NEXT(index);
    }
    return count;
}

bool            freed_isvalid(){
    tm_size_t size;
    tm_index_t count = freed_count(&size);
    size = ALIGN_BLOCKS(size);
    if(!((count==tm_pool.ptrs_freed) && (size==tm_pool.freed_blocks))){
        tm_debug("freed: %u==%u", count, tm_pool.ptrs_freed);
        tm_debug("size:  %u==%u", size, tm_pool.freed_blocks);
        return false;
    }
    return true;
}

bool            freed_isin(const tm_index_t index){
    tm_index_t findex = tm_pool.freed[freed_bin(BLOCKS(index))];
    while(findex){
        assert(findex != FREE_NEXT(findex));
        if(findex==index) return true;
        findex = FREE_NEXT(findex);
    }
    return false;
}

bool                pool_isvalid(){
    // Check pretty much every aspect of the pool to make sure it is completely valid
    tm_blocks_t filled = 0, freed=0;
    tm_index_t ptrs_filled = 1, ptrs_freed = 0;
    tm_index_t index;
    bool flast = false, ffirst = false;  // found first/last
    bool freed_first[FREED_BINS] = {0};  // found freed first bin
    bool freed_last[FREED_BINS] = {0};   // found freed last bin
    uint8_t bin;

    assert(HEAP <= POOL_BLOCKS); assert(BLOCKS_LEFT <= POOL_BLOCKS);
    assert(PTRS_LEFT < TM_MAX_POOL_PTRS);

    if(!freed_isvalid()){tm_debug("[ERROR] general freed check failed"); return false;}

    // Do a complete check on ALL indexes
    for(index=1; index<TM_MAX_POOL_PTRS; index++){
        if((!POINTS(index)) && FILLED(index)){
            tm_debug("[ERROR] index=%u is filled but doesn't point", index);
            index_print(index);
            return false;
        }
        if(POINTS(index)){  // only check indexes that point to something
            assert(NEXT(index) < TM_MAX_POOL_PTRS);
            if(!NEXT(index)){  // This should be the last index
                if(flast || (tm_pool.last_index != index)){  // only 1 last index
                    tm_debug("last index error");
                    index_print(index);
                    return 0;
                }
                flast = true;
            } if(tm_pool.first_index == index){
                assert(!ffirst); ffirst = true;  // only 1 first index
            }
            if(FILLED(index))   {filled+=BLOCKS(index); ptrs_filled++;}  // keep track of count
            else{
                freed+=BLOCKS(index); ptrs_freed++;                     // keep track of count
                assert(FREE_NEXT(index) < TM_MAX_POOL_PTRS); assert(FREE_PREV(index) < TM_MAX_POOL_PTRS);
                if(!freed_isin(index)){
                    tm_debug("[ERROR] index is freed but isn't in freed array:"); index_print(index);
                    return false;
                }
                // Make sure the freed arrays have one first and one last
                bin = freed_bin(BLOCKS(index));
                if(!FREE_PREV(index)){  // index should be beginning of freed array
                    if((tm_pool.freed[bin] != index) || freed_first[bin]){
                        DBGprintf("[ERROR] index has no prev but isn't first bin %u:", bin);
                        index_print(index); return false;
                    }
                    freed_first[bin] = true;
                } else if(FREE_NEXT(FREE_PREV(index)) != index){
                    DBGprintf("[ERROR] free array is corrupted: "); index_print(index); return false;
                }
                if(!FREE_NEXT(index)){
                    assert(!freed_last[bin]); freed_last[bin] = true;
                } else if(FREE_PREV(FREE_NEXT(index)) != index){
                    DBGprintf("[ERROR] free array is corrupted:"); index_print(index); return false;
                }
            }
        } else{
            assert(tm_pool.last_index != index); assert(tm_pool.first_index != index);
        }
    }
    // Make sure we found the first and last index (or no indexes exist)
    if(PTRS_USED > 1)   assert(flast && ffirst);
    else                assert(!(tm_pool.last_index || tm_pool.first_index));

    // Make sure we found all the freed values
    for(bin=0; bin<FREED_BINS; bin++){
        if(tm_pool.freed[bin]){
            if(!(freed_first[bin] && freed_last[bin])){
                tm_debug("did not find first of bin=%u", bin);
                return 0;
            }
        }
        else assert(!(freed_first[bin] || freed_last[bin]));
    }

    // check that we have proper count of filled and freed
    if((filled != tm_pool.filled_blocks) || (freed != tm_pool.freed_blocks)){
        tm_debug("filled and/or freed blocks don't match count filled=%u, freed=%u", filled, freed);
        pool_print();
        return 0;
    } if((ptrs_filled != tm_pool.ptrs_filled) || (ptrs_freed != tm_pool.ptrs_freed)){
        tm_debug("filled and/or freed ptrs don't match count filled_p=%u, freed_p=%u", ptrs_filled, ptrs_freed);
        pool_print();
        return 0;
    }

    // Now count filled and freed by going down the index linked list
    filled=0, freed=0, ptrs_freed=0, ptrs_filled=1;
    index = tm_pool.first_index;
    while(index){
        if(FILLED(index))   {filled+=BLOCKS(index); ptrs_filled++;}
        else                {freed+=BLOCKS(index); ptrs_freed++;}
        index = NEXT(index);
    }
    if((filled != tm_pool.filled_blocks) || (freed != tm_pool.freed_blocks)){
        tm_debug("2nd test: filled and/or freed blocks don't match count: filled=%u, freed=%u", filled, freed);
        pool_print();
        return 0;
    } if((ptrs_filled != tm_pool.ptrs_filled) || (ptrs_freed != tm_pool.ptrs_freed)){
        tm_debug("2nd test: filled and/or freed ptrs don't match count: filled_p=%u, freed_p=%u", ptrs_filled, ptrs_freed);
        pool_print();
        return 0;
    }

    if(testing){
        // if testing assume that all filled indexes should have correct "filled" data
        for(index=1; index<TM_MAX_POOL_PTRS; index++){
            if(FILLED(index)){
                if(!check_index(index)){
                    DBGprintf("[ERROR] index failed check:"); index_print(index); return 0;
                }
            }
        }
    }
    return true;
}

void        fill_index(tm_index_t index){
    uint32_t value = index * PRIME;
    uint32_t *data = tm_uint32_p(index);
    /*index_print(index);*/
    assert(data);
    tm_blocks_t i;

    if(FILLED(index)) data[0] = value;
    for(i=1; i<BLOCKS(index); i++){
        data[i] = value;
    }
    assert(check_index(index));
}

bool        check_index(tm_index_t index){
    uint32_t value = index * PRIME;
    uint32_t *data = tm_uint32_p(index);
    tm_blocks_t i;
    if(!POINTS(index))  return false;
    if(!data)           return false;

    if(FILLED(index)){
        if(data[0] != value) return false;
    }
    for(i=1; i<BLOCKS(index); i++){
        if(data[i] != value) return false;
    }
    return true;
}

/*---------------------------------------------------------------------------*/
/**         Test free and alloc (automatically fills data)                   */

tm_index_t  talloc(tm_size_t size){
    tm_index_t index = tm_alloc(size);
    if(STATUS(TM_ANY_DEFRAG)){
        while(tm_defrag());
        assert(BLOCKS_LEFT >= ALIGN_BLOCKS(size));
        index = tm_alloc(size);
    }
    if(!index) return 0;
    fill_index(index);
    return index;
}


void        tfree(tm_index_t index){
    tm_free(index);
    /*fill_index(index);*/
}

/*---------------------------------------------------------------------------*/
/*      Tests                                                                */
#define mu_assert(test) if (!(test)) {assert(test); return "FAILED\n";}

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

    for(i=0; i<TM_MAX_POOL_PTRS; i++){
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

#define TEST_INDEXES        2000
#define TEST_TIMES          20
#define LARGE_SIZE          2048
#define SMALL_SIZE          128
#define SIZE_DISTRIBUTION   20
#define PURGE_DISTRIBUTION  2
#define FREE_AMOUNT         2
#define MAX_SKIP            20


/**
 * Use the pseudo random number generator rand() to randomly allocate and deallocate
 * a whole bunch of data, then use pool_isvalid() to make sure everything is still
 * valid.
 *
 * This test randomly allocates and frees thousands of times and defragments tens of times.
 */

typedef struct {
    tm_index_t index;
    tm_size_t  blocks;
} mem_allocated;

char *test_tinymem(){
    tm_debug("Starting test tinymem");
    testing = true;
    srand(777);
    uint32_t i, j, loop;
    uint32_t defrags = 0, fills=0, frees=0, purges=0;   // record how many operations have been done
    mem_allocated indexes[TEST_INDEXES] = {0};
    tm_size_t used = 0, size;
    uint8_t mod = 125;
    bool acted = true;
    tm_pool = tm_init();
    mu_assert(pool_isvalid());
    for(loop=1; loop<TEST_TIMES; loop++){
        if(acted) tm_debug("loop=%u", loop);
        acted = false;
        for(i=rand() % MAX_SKIP; i<TEST_INDEXES; i+=rand() % MAX_SKIP){
            if(indexes[i].index){
                // index is filled, free it sometimes
                /*if(!(rand() % FREE_AMOUNT)){*/
                if(1){
                    /*DBGprintf("freeing i=%u,l=%u:", i, loop); index_print(indexes[i]);*/
                    mu_assert(BLOCKS(indexes[i].index) == indexes[i].blocks)
                    used -= BLOCKS(indexes[i].index);
                    tfree(indexes[i].index);
                    indexes[i].blocks = 0;
                    mu_assert(used == tm_pool.filled_blocks);
                    frees++;
                    acted = true;
                }
            } else{
                // index is free. decide whether to allocate large or small
                size = (rand() % SIZE_DISTRIBUTION) ? (rand() % SMALL_SIZE) : (rand() % LARGE_SIZE);
                size++;
                if(size <= BYTES_LEFT){
                    /*DBGprintf("filling i=%u,l=%u:\n", i, loop);*/
                    if(i==61 && loop==1){
                        /*__asm__("int $3");*/
                    }
                    indexes[i].index = talloc(size);
                    indexes[i].blocks = BLOCKS(indexes[i].index);
                    mu_assert(indexes[i].index);
                    used+=ALIGN_BLOCKS(size); mu_assert(used == tm_pool.filled_blocks);
                    mu_assert(ALIGN_BLOCKS(size) == BLOCKS(indexes[i].index));
                    fills++; acted = true;
                }
            }
            if(STATUS(TM_DEFRAG_FULL_DONE)){
                STATUS_CLEAR(TM_DEFRAG_FULL_DONE);
                DBGprintf("!! Defrag has been done"); defrags++;
            }

            if(acted){
                DBGprintf("checking i=%u,l=%u,ffdp=(%u,%u,%u,%u):",
                          i, loop, fills, frees, defrags, purges);
                index_print(indexes[i].index);
                if(!FILLED(indexes[i].index)){
                    indexes[i].index = 0;
                    assert(indexes[i].blocks == 0);
                }
                mu_assert(pool_isvalid());
                for(j=0; j<TEST_INDEXES; j++){
                    if(indexes[j].index){
                        mu_assert(BLOCKS(indexes[j].index) == indexes[j].blocks);
                        mu_assert(check_index(indexes[j].index));
                    }
                }
            }
        }
        if(!(rand() % PURGE_DISTRIBUTION)){
            // free tons of indexes
            for(i=0; i<TEST_INDEXES; i+=rand()%5){
                if(indexes[i].index){
                    used -= BLOCKS(indexes[i].index);
                    tfree(indexes[i].index);
                    indexes[i].index = 0;
                    indexes[i].blocks = 0;
                    frees++;
                }
                mu_assert(pool_isvalid());
            }
            purges++;
            acted=true;
            mu_assert(pool_isvalid());
        }
    }
    DBGprintf("\n");
    return NULL;
}
