/*#define NDEBUG*/
#include "tinymem.h"

#undef TM_PRINT
#ifdef  TM_PRINT
#define tm_debug(...)       do{printf("[DEBUG](%s,%u):", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n");}while(0)
#define DBGprintf(...)      printf(__VA_ARGS__)
#else
#define tm_debug(...)       do{}while(0)
#define DBGprintf(...)      do{}while(0)
#endif

#define TESTprint(...)      printf(__VA_ARGS__)

#ifdef TM_TEST
#define TM_TOOLS
#endif

#ifndef TM_TOOLS
#define assert(ignore)((void) 0)
#endif

/*---------------------------------------------------------------------------*/
/*      Check pre-compiler errors                                            */

#if     (UINT_MAX == UINT16_MAX)
#define INTSIZE     2
#elif   (UINT_MAX == UINT32_MAX)
#define INTSIZE     4
#elif   (UINT_MAX == UINT64_MAX)
#define INTSIZE     8
#endif

#ifndef TM_BLOCK_SIZE       // user didn't set block size, use default
    #define TM_BLOCK_TYPE           free_block
    #define TM_BLOCK_SIZE           (TM_INDEX_SIZE * 2)
#elif   (TM_BLOCK_SIZE == 2)
    #define TM_BLOCK_TYPE           uint16_t
#elif   (TM_BLOCK_SIZE == 4)
    #define TM_BLOCK_TYPE           uint32_t
#elif   (TM_BLOCK_SIZE == 8)
    typedef struct {uint32_t values[2];} TM_BLOCK_TYPE;
#elif   (TM_BLOCK_SIZE == 16)
    typedef struct {uint32_t values[4];} TM_BLOCK_TYPE;
#endif

#if     (TM_BLOCK_SIZE < 2 * TM_INDEX_SIZE)
#error  "block size must be 2 times bigger than index size"
#endif

#define TM_POOL_BLOCKS          (TM_POOL_SIZE / TM_BLOCK_SIZE)  // total blocks available

#if     (TM_POOL_BLOCKS < 256)
    typedef uint8_t         tm_blocks_t;
#else
    typedef uint16_t        tm_blocks_t;
#endif

#if TM_POOL_SIZE % (TM_BLOCK_SIZE)
#error "Invalid pool size, must be divisible by free_block"
#endif

#if (TM_POOL_INDEXES % (8 * INTSIZE))
#error "Invalid pool ptrs size, must be divisible by int"
#endif

/*---------------------------------------------------------------------------*/
/**
 * \brief           poolptr is used by Pool to track memory location and size
 */
TM_H_ATTPACKPRE typedef struct {
    tm_blocks_t loc;
    tm_index_t next;
} TM_H_ATTPACKSUF poolptr;

/*---------------------------------------------------------------------------*/
/**
 * \brief           free_block is stored INSIDE of freed memory as a linked list
 *                  of all freed data
 */
// TODO: packed!
TM_H_ATTPACKPRE typedef struct {
    tm_index_t prev;
    tm_index_t next;
} TM_H_ATTPACKSUF free_block;


#define CEILING(x, y)           (((x) % (y)) ? (x)/(y) + 1 : (x)/(y))
#define BOOL(value)             ((value) ? 1: 0)

// Used in defrag to subtract from clocks_left
#define INDEX_REMOVE_CLOCKS     (10)
#define FREED_REMOVE_CLOCKS     (8)
#define SPLIT_CLOCKS            (20)

#define MAX_BIT_INDEXES     (TM_POOL_INDEXES / (8 * sizeof(int)))          // for filled/points
#define MAXUINT             ((unsigned int) 0xFFFFFFFFFFFFFFFF)
#define INTBITS             (sizeof(int) * 8)                               // bits in an integer
// Masks of an integer
#define LOWER_MASK          (((unsigned int)0xFFFFFFFFFFFFFFFF) >> (sizeof(int) * 8 / 2))
#define UPPER_MASK          (LOWER_MASK << (sizeof(int) * 8 / 2))

#define FREED_BINS          (12)                                // bins to store freed values

// data is aligned on blocks
#define ALIGN_BLOCKS(size)  CEILING(size, TM_BLOCK_SIZE)           // get block value that can encompase size
#define ALIGN_BYTES(size)   (ALIGN_BLOCKS(size) * TM_BLOCK_SIZE)   // get value in bytes

#define CPU_CLOCKS_PER_US       ((CPU_CLOCKS_PER_SEC) / 1000000)
#define CPU_CLOCKS_PER_CLOCK    (CPU_CLOCKS_PER_SEC / CLOCKS_PER_SEC)

#ifndef TM_THREAD_TIME_US
#define TM_THREAD_TIME_US      2
#endif

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
    .defrag_prev=0,                                             \
})

/*---------------------------------------------------------------------------*/
/**
 * \brief           Pool object to track all memory usage
 *                  This is the main object used by tinymem to do memory
 *                  management
 */
typedef struct {
    TM_BLOCK_TYPE   pool[TM_POOL_BLOCKS];           //!< Actual memory pool (very large)
    unsigned int    filled[MAX_BIT_INDEXES];     //!< bit array of filled pointers (only used, not freed)
    unsigned int    points[MAX_BIT_INDEXES];     //!< bit array of used pointers (both used and freed)
    poolptr         pointers[TM_POOL_INDEXES];     //!< This is the index lookup location
    tm_index_t      freed[FREED_BINS];           //!< binned storage of all freed indexes
    tm_blocks_t     filled_blocks;                //!< total amount of data allocated
    tm_blocks_t     freed_blocks;                 //!< total amount of data freed
    tm_index_t      ptrs_filled;                    //!< total amount of pointers allocated
    tm_index_t      ptrs_freed;                     //!< total amount of pointers freed
    tm_index_t      find_index;                     //!< speed up find index
    tm_index_t      first_index;                    //!< required to start defrag
    tm_index_t      last_index;                     //!< required to allocate off heap
    uint8_t         status;                         //!< status byte. Access with Pool_status macros
    tm_index_t      defrag_index;                   //!< used during defrag
    tm_index_t      defrag_prev;                    //!< used during defrag
} Pool;

Pool tm_pool = tm_init();  // holds all allocations, deallocates and pretty much everything else


/*---------------------------------------------------------------------------*/
/*      Local Functions Declarations                                         */

inline bool     tm_defrag();
tm_index_t      find_index();
uint8_t         freed_bin(const tm_blocks_t blocks);
uint8_t         freed_bin_get(const tm_blocks_t blocks);
inline void     freed_remove(const tm_index_t index);
inline void     freed_insert(const tm_index_t index);
tm_index_t      freed_get(const tm_blocks_t size);

void index_extend(const tm_index_t index, const tm_blocks_t blocks, const bool filled);
void index_remove(const tm_index_t index, const tm_index_t prev_index, const bool defrag);
inline void index_join(const tm_index_t index, const tm_index_t with_index, int32_t *clocks_left);
bool index_split(const tm_index_t index, const tm_blocks_t blocks, tm_index_t new_index);
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
#define BLOCKS_LEFT                 (TM_POOL_BLOCKS - tm_pool.filled_blocks)
#define BYTES_LEFT                  (BLOCKS_LEFT * TM_BLOCK_SIZE)
#define PTRS_USED                   (tm_pool.ptrs_filled + tm_pool.ptrs_freed)
#define PTRS_LEFT                   (TM_POOL_INDEXES - tm_pool.ptrs_filled) // ptrs potentially left
#define PTRS_AVAILABLE              (TM_POOL_INDEXES - PTRS_USED) // ptrs available for immediate use
#define HEAP_LEFT                   (TM_POOL_BLOCKS - HEAP)
#define HEAP_LEFT_BYTES             (HEAP_LEFT * TM_BLOCK_SIZE)

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
#define LOC_VOID(loc)               ((void*)(tm_pool.pool + (loc)))

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

inline void tm_reset(){
    tm_pool = tm_init();
}

/*---------------------------------------------------------------------------*/
inline tm_size_t tm_sizeof(const tm_index_t index){
    return BLOCKS(index) * TM_BLOCK_SIZE;
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
            if(!index_split(index, size, 0)){
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
    if(!PTRS_LEFT) return 0;
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
        index_join(index, new_index, NULL);
    }
    prev_size = BLOCKS(index);
    if(size == BLOCKS(index)) return index;
    if(size < prev_size){  // shrink data
        if(!index_split(index, size, 0)) return 0;
        return index;
    } else{  // grow data
        new_index = tm_alloc(size * TM_BLOCK_SIZE);
        if(!new_index) return 0;
        MEM_MOVE(new_index, index);
        tm_free(index);
        return new_index;
    }
}

/*---------------------------------------------------------------------------*/
void            tm_free(const tm_index_t index){
    if(!index) return;      // ISO requires free(NULL) be a NO-OP
    assert(LOCATION(index) < HEAP);
    assert(index < TM_POOL_INDEXES);
    assert(FILLED(index));
    FILLED_CLEAR(index);
    tm_pool.filled_blocks -= BLOCKS(index);
    tm_pool.freed_blocks += BLOCKS(index);
    tm_pool.ptrs_filled--;
    tm_pool.ptrs_freed++;
    freed_insert(index);
    // Join all the way up if next index is free
    if(!FILLED(NEXT(index))){
        index_join(index, NEXT(index), NULL);
    }
}

/*---------------------------------------------------------------------------*/
bool            tm_valid(const tm_index_t index){
    if(index >= TM_POOL_INDEXES)               return false;
    if(LOCATION(index) >= TM_POOL_BLOCKS)          return false;
    if((!POINTS(index)) || (!FILLED(index)))    return false;
    return true;
}

/*---------------------------------------------------------------------------*/
inline bool     tm_check(const tm_index_t index, const tm_size_t size){
    if(!tm_valid(index))         return false;
    if(tm_sizeof(index) != size) return false;
    return true;
}

/*---------------------------------------------------------------------------*/
inline bool     tm_thread(){
    // TODO: set defrag under certain user defined conditions (small heap with
    //  memory available)
    if(STATUS(TM_ANY_DEFRAG)){
        return tm_defrag();
    }
    return 0;   // no operations pending
}

/*---------------------------------------------------------------------------*/
inline bool         tm_defrag(){
#ifndef NDEBUG
    tm_index_t i = 0;
    tm_blocks_t used = tm_pool.filled_blocks;
    tm_blocks_t available = BLOCKS_LEFT, heap = HEAP_LEFT, freed = tm_pool.freed_blocks;
#endif
    int32_t clocks_left = CPU_CLOCKS_PER_US * TM_THREAD_TIME_US;
    tm_blocks_t blocks;
    tm_blocks_t location;
    if(!STATUS(TM_DEFRAG_IP)){
        tm_pool.defrag_index = tm_pool.first_index;
        tm_pool.defrag_prev = 0;
        STATUS_CLEAR(TM_ANY_DEFRAG);
        STATUS_SET(TM_DEFRAG_FULL_IP);
    }
    if(!tm_pool.defrag_index) goto done;
    while(NEXT(tm_pool.defrag_index)){
        if(!FILLED(tm_pool.defrag_index)){
            clocks_left -= 30 + INDEX_REMOVE_CLOCKS + SPLIT_CLOCKS;
            if(!FILLED(NEXT(tm_pool.defrag_index))){
                index_join(tm_pool.defrag_index, NEXT(tm_pool.defrag_index), &clocks_left);
                if(clocks_left < 0) return 1;
            }
            if(!NEXT(tm_pool.defrag_index)) break;

            /*DBGprintf("### Defrag: loop=%-11u", i); index_print(tm_pool.defrag_index);*/
            assert(FILLED(NEXT(tm_pool.defrag_index)));
            blocks = BLOCKS(NEXT(tm_pool.defrag_index));        // store size of actual data
            location = LOCATION(NEXT(tm_pool.defrag_index));    // location of actual data
            clocks_left -= CEILING(blocks * TM_BLOCK_SIZE, sizeof(int));

            // Make index "filled", we will split it up later
            freed_remove(tm_pool.defrag_index);         // 7 clocks
            FILLED_SET(tm_pool.defrag_index);           // 2 clocks
            tm_pool.ptrs_filled++, tm_pool.filled_blocks+=BLOCKS(tm_pool.defrag_index);
            tm_pool.ptrs_freed--, tm_pool.freed_blocks-=BLOCKS(tm_pool.defrag_index);

            // Do an odd join, where the locations are just equal
            LOCATION(NEXT(tm_pool.defrag_index)) = LOCATION(tm_pool.defrag_index);

            // Now remove the index. Note that the size is == 0
            //      Also note that even though it was removed, it's NEXT and LOCATION
            //      are still valid (not changed in remove index)
            index_remove(tm_pool.defrag_index, tm_pool.defrag_prev, true);
            tm_pool.defrag_prev = NEXT(tm_pool.defrag_index);  // defrag_index was removed

            assert(LOCATION(tm_pool.defrag_prev) < TM_POOL_BLOCKS);
            assert(location < TM_POOL_BLOCKS);
            memmove(LOC_VOID(LOCATION(tm_pool.defrag_prev)),
                    LOC_VOID(location), ((tm_size_t)blocks) * TM_BLOCK_SIZE);
            if(!FILLED(NEXT(tm_pool.defrag_prev))){
                index_join(tm_pool.defrag_prev, NEXT(tm_pool.defrag_prev), &clocks_left);
            }
            assert(FILLED(NEXT(tm_pool.defrag_prev)));  // it will never "join up"
            if(!index_split(tm_pool.defrag_prev, blocks, tm_pool.defrag_index)){
                assert(0);
            } // note: tm_pool.defrag_index is now invalid (split used it)
            assert(BLOCKS(tm_pool.defrag_prev) == blocks);

            tm_pool.defrag_index = NEXT(tm_pool.defrag_prev);

            assert(!FILLED(tm_pool.defrag_index));

        } else{
            clocks_left -= 10;
            tm_pool.defrag_prev = tm_pool.defrag_index;
            tm_pool.defrag_index = NEXT(tm_pool.defrag_index);
        }
        assert(tm_pool.defrag_prev != tm_pool.defrag_index);
        assert((i++, used == tm_pool.filled_blocks));
        assert(available == BLOCKS_LEFT);
        /*if(clocks_left < -200) printf("clocks very low=%i\n", clocks_left);*/
        if(clocks_left < 0) return 1;
    }
done:
    if(!FILLED(tm_pool.defrag_index)){
        index_remove(tm_pool.defrag_index, tm_pool.defrag_prev, true);
    }
    STATUS_CLEAR(TM_DEFRAG_IP);
    STATUS_SET(TM_DEFRAG_FULL_DONE);
    tm_debug("filled end=%lu, total=%lu, operate=%lu, isavail=%lu",
            tm_pool.filled_blocks, TM_POOL_BLOCKS, TM_POOL_BLOCKS - tm_pool.filled_blocks,
            BLOCKS_LEFT);
    DBGprintf("## Defrag done: Heap left: start=%u, end=%lu, recovered=%lu, ",
            heap, HEAP_LEFT, HEAP_LEFT - heap);
    DBGprintf("wasfree=%u was_avail=%lu isavail=%lu,  \n", freed, available, BLOCKS_LEFT);
    assert(HEAP_LEFT - heap == freed);
    assert(tm_pool.freed_blocks == 0);
    assert(HEAP_LEFT == BLOCKS_LEFT);

    tm_pool.defrag_index = 0;
    tm_pool.defrag_prev = 0;
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
    if(!PTRS_AVAILABLE) return 0;
    for(loop=0; loop<2; loop++){
        for(; tm_pool.find_index < MAX_BIT_INDEXES; tm_pool.find_index++){
            bits = tm_pool.points[tm_pool.find_index];
            if(bits != MAXUINT){
                bit = 0;
                if((bits & LOWER_MASK) == LOWER_MASK){
                    // bits are in the second half
                    bit += sizeof(int)  * 8 / 2;
                    bits = bits >> (sizeof(int) * 8 / 2);
                }
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
                }
                assert(0);
found:
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
#ifdef TM_TESTS  // processor intensive
    /*assert(freed_isin(index));*/
#endif
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
#if 0
        assert(!FILLED(index));
        tm_pool.freed_blocks += blocks;
        tm_pool.ptrs_freed++;
        freed_insert(index);
#endif
    }
}


void index_remove(const tm_index_t index, const tm_index_t prev_index, bool defrag){
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
    assert(index);
    switch(((FILLED(prev_index) ? 1:0) << 1) + (FILLED(index) ? 1:0)){
        case 0b00:  // merging two free values
            // TODO: this causes failure, find out why
            freed_remove(index);
            tm_pool.ptrs_freed--;  // no change in blocks, both are free
            // if index is last value, freed_blocks will be reduced
            if(!NEXT(index)) tm_pool.freed_blocks -= BLOCKS(index);
            break;
        case 0b10:  // growing prev_index "up"
            freed_remove(index);
            tm_pool.freed_blocks -= BLOCKS(index); tm_pool.ptrs_freed--;
            // grow prev_index, unless index is last value
            if(NEXT(index)) tm_pool.filled_blocks += BLOCKS(index);
            break;
        case 0b11:  // combining two filled indexes, used ONLY in defrag
            assert(defrag);  // defrag is using
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
    // Check for defragmentation settings
    if(!defrag){
        if(index == tm_pool.defrag_index){
            assert(prev_index == tm_pool.defrag_prev);
            tm_pool.defrag_index = NEXT(index);  // index is gone, defrag should do next index
        } else if(index == tm_pool.defrag_prev){
            tm_pool.defrag_prev = prev_index;  // index is gone, joined with prev_index
        }
    }

}

inline void index_join(tm_index_t index, tm_index_t with_index, int32_t *clocks_left){
    // join index with_index. with_index will be removed
    do{
        if(clocks_left) *clocks_left -= 8;
        assert(!FILLED(with_index));
        assert(LOCATION(index) <= LOCATION(with_index));
        if(!FILLED(index)){
            if(clocks_left) *clocks_left -= FREED_REMOVE_CLOCKS;
            freed_remove(index); // index has to be rebinned, remove before changing size
        }
        // Remove and combine the index
        if(clocks_left) *clocks_left -= INDEX_REMOVE_CLOCKS;
        index_remove(with_index, index, BOOL(clocks_left));
        if(!FILLED(index)) freed_insert(index); // rebin the index
        with_index = NEXT(index);
    }while(!FILLED(with_index));
}

bool index_split(const tm_index_t index, const tm_blocks_t blocks, tm_index_t new_index){
    assert(blocks < BLOCKS(index));
    if(!FILLED(NEXT(index))){
        new_index = NEXT(index);
        // If next index is free, always join it first. This also frees up new_index to
        // use however we want!
        index_join(index, new_index, NULL);
    } else if(new_index){  // an empty index has been given to us
        // pass
    }else{
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


#ifdef TM_TOOLS
/*###########################################################################*/
/**
 * +------------------------------------------------------------+
 * | XXXX XXXX XXXX     XXXXX XXX   XXXX XXXXX XXX  XX  X  XXXX |
 * | X    X  X X  X       X   X     X      X    X   XX  X  X    |
 * | XXX  X  X XXXX       X   XXX   XXXX   X    X   X X X  X XX |
 * | X    X  X X X        X   X        X   X    X   X  XX  X  X |
 * | X    XXXX X  X       X   XXX   XXXX   X   XXX  X   X  XXXX |
 * +------------------------------------------------------------+
 */

#define TESTassert(value)   do{if(!(value)){TESTprint("[FAIL](%s,%u): \"%s\"\n", __FILE__, __LINE__, __func__); return false;}}while(0)


#define PRIME       (65599)
bool testing = false;

void                pool_print(){
    DBGprintf("## Pool (status=%x):\n", tm_pool.status);
    DBGprintf("    mem blocks: heap=  %-7u     filled=%-7u  freed=%-7u     total=%-7u\n",
            HEAP, tm_pool.filled_blocks, tm_pool.freed_blocks, TM_POOL_BLOCKS);
    DBGprintf("    avail ptrs: filled=  %-7u   freed= %-7u   used=%-7u,    total= %-7u\n",
            tm_pool.ptrs_filled, tm_pool.ptrs_freed, PTRS_USED, TM_POOL_INDEXES);
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
    assert(*size==tm_pool.freed_blocks * TM_BLOCK_SIZE);
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
        /*tm_debug("loc=%u, blocks=%u", LOCATION(index), TM_POOL_BLOCKS);*/
        assert(LOCATION(index) < TM_POOL_BLOCKS);
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
        TESTassert(findex != FREE_NEXT(findex));
        if(findex==index) return true;
        findex = FREE_NEXT(findex);
    }
    return false;
}


/**
 * \brief       Massive check of the entire pool to verify that it is "sane"
 *
 *              This single function will find 95% of errors with this library. The memory
 *              manager stores data about itself, and this makes sure that all data is alligned
 *              the way that it should be, that the freed array makes sense, etc.
 *
 * \return      true if valid, false otherwise
 */
bool                pool_isvalid(){
    tm_blocks_t filled = 0, freed=0;
    tm_index_t ptrs_filled = 1, ptrs_freed = 0;
    tm_index_t index;
    bool flast = false, ffirst = false;  // found first/last
    bool freed_first[FREED_BINS] = {0};  // found freed first bin
    bool freed_last[FREED_BINS] = {0};   // found freed last bin
    uint8_t bin;

    TESTassert(HEAP <= TM_POOL_BLOCKS); TESTassert(BLOCKS_LEFT <= TM_POOL_BLOCKS);
    TESTassert(PTRS_LEFT < TM_POOL_INDEXES);

    if(!freed_isvalid()){TESTprint("[ERROR] general freed check failed"); return false;}

    // Do a complete check on ALL indexes
    for(index=1; index<TM_POOL_INDEXES; index++){
        if((!POINTS(index)) && FILLED(index)){
            TESTprint("[ERROR] index=%u is filled but doesn't point", index);
            index_print(index);
            return false;
        }
        if(POINTS(index)){  // only check indexes that point to something
            TESTassert(NEXT(index) < TM_POOL_INDEXES);
            if(!NEXT(index)){  // This should be the last index
                if(flast || (tm_pool.last_index != index)){  // only 1 last index
                    TESTprint("last index error");
                    index_print(index);
                    return 0;
                }
                flast = true;
            } if(tm_pool.first_index == index){
                TESTassert(!ffirst); ffirst = true;  // only 1 first index
            }
            if(FILLED(index))   {filled+=BLOCKS(index); ptrs_filled++;}  // keep track of count
            else{
                freed+=BLOCKS(index); ptrs_freed++;                     // keep track of count
                TESTassert(FREE_NEXT(index) < TM_POOL_INDEXES); TESTassert(FREE_PREV(index) < TM_POOL_INDEXES);
                if(!freed_isin(index)){
                    TESTprint("[ERROR] index is freed but isn't in freed array:"); index_print(index);
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
                    TESTassert(!freed_last[bin]); freed_last[bin] = true;
                } else if(FREE_PREV(FREE_NEXT(index)) != index){
                    DBGprintf("[ERROR] free array is corrupted:"); index_print(index); return false;
                }
            }
        } else{
            TESTassert(tm_pool.last_index != index); TESTassert(tm_pool.first_index != index);
        }
    }
    // Make sure we found the first and last index (or no indexes exist)
    if(PTRS_USED > 1)   TESTassert(flast && ffirst);
    else                TESTassert(!(tm_pool.last_index || tm_pool.first_index));

    // Make sure we found all the freed values
    for(bin=0; bin<FREED_BINS; bin++){
        if(tm_pool.freed[bin]){
            TESTassert(freed_first[bin] && freed_last[bin]);
        }
        else TESTassert(!(freed_first[bin] || freed_last[bin]));
    }

    // check that we have proper count of filled and freed
    TESTassert((filled == tm_pool.filled_blocks) && (freed == tm_pool.freed_blocks));
    TESTassert((ptrs_filled == tm_pool.ptrs_filled) && (ptrs_freed == tm_pool.ptrs_freed));

    // Now count filled and freed by going down the index linked list
    filled=0, freed=0, ptrs_freed=0, ptrs_filled=1;
    index = tm_pool.first_index;
    while(index){
        if(FILLED(index))   {filled+=BLOCKS(index); ptrs_filled++;}
        else                {freed+=BLOCKS(index); ptrs_freed++;}
        index = NEXT(index);
    }
    TESTassert((filled == tm_pool.filled_blocks) && (freed == tm_pool.freed_blocks));
    TESTassert((ptrs_filled == tm_pool.ptrs_filled) && (ptrs_freed == tm_pool.ptrs_freed));

    if(testing){
        // if testing assume that all filled indexes should have correct "filled" data
        for(index=1; index<TM_POOL_INDEXES; index++){
            if(FILLED(index)) TESTassert(check_index(index));
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

tm_index_t  talloc(tm_size_t size, bool threaded){
    tm_index_t index = tm_alloc(size);
    uint64_t start;
    if((!threaded) && STATUS(TM_ANY_DEFRAG)){
        assert(!index);
        while(1){
            start = clock();
            if(!tm_thread()) break;
            start = ((clock() - start) * 1000000) / CLOCKS_PER_SEC;
#ifndef TM_PRINT
            assert(start < 20);
#endif
        }

        while(tm_thread())
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
#endif

#ifdef TM_TESTS
/*---------------------------------------------------------------------------*/
/*      Tests                                                                */
#define mu_assert(test) if (!(test)) {TESTprint("MU ASSERT FAILED(%s,%u): \"%s\"\n", __FILE__, __LINE__, __func__); return "FAILED\n";}


#define TEST_INDEXES        8000
#define TEST_TIMES          10
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
    bool threaded = false;
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
                    /*DBGprintf("freeing i=%u,l=%u:", i, loop); index_print(indexes[i].index);*/
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
                if((size <= BYTES_LEFT) && PTRS_LEFT){
                    /*DBGprintf("filling i=%u,l=%u,size=%u, bleft=%u, pavail=%u, pleft=%u:\n",*/
                            /*i, loop, size, BYTES_LEFT, PTRS_AVAILABLE, PTRS_LEFT);*/
                    if(i==61 && loop==1){
                        /*__asm__("int $3");*/
                    }
                    indexes[i].index = talloc(size, threaded);
                    indexes[i].blocks = BLOCKS(indexes[i].index);
                    mu_assert(indexes[i].index);
                    used+=ALIGN_BLOCKS(size); mu_assert(used == tm_pool.filled_blocks);
                    mu_assert(ALIGN_BLOCKS(size) == BLOCKS(indexes[i].index));
                    fills++; acted = true;
                }
            }
            if(STATUS(TM_DEFRAG_FULL_DONE)){
                STATUS_CLEAR(TM_DEFRAG_FULL_DONE);
                DBGprintf("!! Defrag has been done\n"); defrags++;
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
            }
            purges++;
            acted=true;
            mu_assert(pool_isvalid());
        }
    }
    DBGprintf("\n");
    TESTprint("COMPLETE tinymem_test: loops=%u, fills=%u, frees=%u, defrags=%u, purges=%u\n",
               loop, fills, frees, defrags, purges);
    return NULL;
}
#endif
