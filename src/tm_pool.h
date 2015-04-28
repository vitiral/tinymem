#ifndef __tinymem_pool_h
#define __tinymem_pool_h
#include "tm_types.h"


//#if (TM_POOL_SIZE % sizeof(free_block))
//#error "Invalid pool size, must be divisible by free_block"
//#endif

//#if (TM_MAX_POOL_PTRS % (8 * sizeof(int)))
//#error "Invalid pool ptrs size, must be divisible by int"
//#endif

#define CEILING(x, y)           (((x) % (y)) ? (x)/(y) + 1 : (x)/(y))

#define TM_POOL_BLOCKS          (TM_POOL_SIZE / sizeof(free_block))
#define TM_MAX_BIT_INDEXES      (TM_MAX_POOL_PTRS / (8 * sizeof(int)))
#define MAXUINT                 ((unsigned int) 0xFFFFFFFFFFFFFFFF)
#define INTBITS                 (sizeof(int) * 8)

#define TM_FREED_BINS           (12)
#define TM_ALIGN_BYTES          sizeof(free_block)
#define TM_ALIGN(size)          (((size) % TM_ALIGN_BYTES) ? \
    ((size) + TM_ALIGN_BYTES - ((size) % TM_ALIGN_BYTES)): (size))
#define TM_ALIGN_BLOCKS(size)   CEILING(size, TM_ALIGN_BYTES)


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

/*---------------------------------------------------------------------------*/
/**
 * \brief           Pool object to track all memory usage
 *                  This is the main object used by tinymem to do memory
 *                  management
 */
typedef struct {
    free_block      pool[TM_POOL_BLOCKS];           //!< Actual memory pool (very large)
    unsigned int    filled[TM_MAX_BIT_INDEXES];     //!< bit array of filled pointers (only used, not freed)
    unsigned int    points[TM_MAX_BIT_INDEXES];     //!< bit array of used pointers (both used and freed)
    poolptr         pointers[TM_MAX_POOL_PTRS];     //!< This is the index lookup location
    tm_index_t      freed[TM_FREED_BINS];           //!< binned storage of all freed indexes
    tm_blocks_t     filled_blocks;                //!< total amount of data allocated
    tm_blocks_t     freed_blocks;                 //!< total amount of data freed
    tm_index_t      ptrs_filled;                    //!< total amount of pointers allocated
    tm_index_t      ptrs_freed;                     //!< total amount of pointers freed
    tm_index_t      find_index;                     //!< speed up find index
    tm_index_t      last_index;                     //!< required to allocate off heap
    uint8_t         status;                         //!< status byte. Access with Pool_status macros
    uint8_t         find_index_bit;                 //!< speed up find index
} Pool;


/*---------------------------------------------------------------------------*/
/**
 * \brief           Internal use only. Declares Pool with as many initial
 *                  values as possible
 */
#define Pool_init()  ((Pool) {                                  \
    .filled = {1},                      /*NULL is taken*/       \
    .points = {1},                      /*NULL is taken*/       \
    .pointers = {{0, 0}},               /*heap = 0*/            \
    .freed = {0},                                               \
    .filled_blocks = {0},                                       \
    .freed_blocks = {0},                                        \
    .ptrs_filled = 1,                    /*NULL is "filled"*/   \
    .ptrs_freed = 0,                                            \
    .find_index = 0,                                            \
    .last_index = 0,                                            \
    .status = 0,                                                \
    .find_index_bit = 1,                /*index 0 is invalid*/  \
})

/*---------------------------------------------------------------------------*/
/**
 * \brief           Access Pool Characteristics
 *                  Pool_filled* does operations on Pool's `filled` array
 *                  Pool_points* does operations on Pool's `points` array
 */
#define Pool_available_blocks(p)     (TM_POOL_BLOCKS - (p)->filled_blocks)
#define Pool_available(p)            (Pool_available_blocks(p) * TM_ALIGN_BYTES)
#define Pool_pointers_left(p)        (TM_MAX_POOL_PTRS - ((p)->ptrs_filled + (p)->ptrs_filled))
#define Pool_heap_left(p)            (TM_POOL_SIZE - Pool_heap(p))
#define Pool_loc(p, index)           ((p)->pointers[index].loc)
#define Pool_heap(p)                 ((p)->pointers[0].loc)
#define Pool_next(p, index)          ((p)->pointers[index].next)
#define Pool_loc_void_p(p, loc)      ((void*)(p)->pool + (loc))

#define BITARRAY_INDEX(index)        (index / 8)
#define BITARRAY_BIT(index)          (1 << (index % 8))
#define Pool_filled_bool(p, index)   ((p)->filled[BITARRAY_INDEX(index)] &   BITARRAY_BIT(index))
#define Pool_filled_set(p, index)    ((p)->filled[BITARRAY_INDEX(index)] |=  BITARRAY_BIT(index))
#define Pool_filled_clear(p, index)  ((p)->filled[BITARRAY_INDEX(index)] &= ~BITARRAY_BIT(index))
#define Pool_points_bool(p, index)   ((p)->points[BITARRAY_INDEX(index)] &   BITARRAY_BIT(index))
#define Pool_points_set(p, index)    ((p)->points[BITARRAY_INDEX(index)] |=  BITARRAY_BIT(index))
#define Pool_points_clear(p, index)  ((p)->points[BITARRAY_INDEX(index)] &= ~BITARRAY_BIT(index))

#define Pool_bytes_free(p, size)   do{                  \
        (p)->freed_blocks += size;                       \
        (p)->filled_blocks -= size;                      \
    }while(0)

#define Pool_bytes_fill(p, size)   do{                  \
        (p)->freed_blocks -= size;                       \
        (p)->filled_blocks += size;                      \
    }while(0)

#define Pool_memmove(pool, index_to, index_from)  memmove(              \
            Pool_void_p(pool, index_to),                                  \
            Pool_void_p(pool, index_from),                                \
            Pool_sizeof(pool, index_from)                               \
        )


/*---------------------------------------------------------------------------*/
/**
 * \brief           Get, set or clear the status bit (0 or 1) of name
 * \return uint8_t  status bit
 */
#define Pool_status(p, name)         (((p)->status) & (name))
#define Pool_status_set(p, name)     ((p)->status |= (name))
#define Pool_status_clear(p, name)   ((p)->status &= ~(name))


/*---------------------------------------------------------------------------*/
/**
 * \brief           Get the sizeof data at index in bytes
 * \return tm_size_t  the sizeof the data pointed to by index
 */
#define Pool_sizeof(p, index)        (Pool_blocks(p, index) * TM_ALIGN_BYTES)
#define Pool_blocks(p, index)        (Pool_loc(p, (p)->pointers[index].next) - \
                                      Pool_loc(p, index))

/*---------------------------------------------------------------------------*/
/**
 * \brief           allocate memory from pool
 * \param pool      pointer to Pool struct
 * \param size      size of pointer to allocate
 * \return          tm_index_t corresponding to memory location
 *                  On error or if not enough memory, return value == 0
 */
tm_index_t          Pool_alloc(Pool *pool, tm_size_t size);

/*---------------------------------------------------------------------------*/
/**
 * \brief           changes the size of memory in the pool
 *                  See standard documentation on realloc for more info
 * \param pool      pointer to Pool struct
 * \param index     tm_index_t to realloc
 *                  If 0: acts as tm_alloc(size)
 * \param size      new requested size of index
 *                  If 0: acts as tm_free(index)
 * \return          index with new size of memory.
 *                  If this index has changed, the previous index is been freed
 *                  If return value == 0, then no change has been done
 *                  (or index has been freed if size=0)
 */
tm_index_t          Pool_realloc(Pool *pool, tm_index_t index, tm_size_t size);

/*---------------------------------------------------------------------------*/
/**
 * \brief           free allocated memory from pool
 * \param pool      pointer to Pool struct
 * \param index     tm_index_t to free
 */
void                Pool_free(Pool *pool, const tm_index_t index);


/*---------------------------------------------------------------------------*/
/**
 * \brief           cast a void pointer from index
 * \param pool      pointer to Pool struct
 * \param index     tm_index_t to get pointer to
 * \return          void* pointer to actual data
 */
inline void*        Pool_void_p(const Pool *pool, const tm_index_t index);

/*---------------------------------------------------------------------------*/
/**
 * \brief           Various data type casts
 */
#define Pool_char_p(pool, index)        ((char *)Pool_void_p(pool, index))
#define Pool_int8_p(pool, index)        ((int8_t *)Pool_void_p(pool, index))
#define Pool_uint8_p(pool, index)       ((uint8_t *)Pool_void_p(pool, index))
#define Pool_int16_p(pool, index)       ((int16_t *)Pool_void_p(pool, index))
#define Pool_uint16_p(pool, index)      ((uint16_t *)Pool_void_p(pool, index))
#define Pool_int32_p(pool, index)       ((int32_t *)Pool_void_p(pool, index))
#define Pool_uint32_p(pool, index)      ((uint32_t *)Pool_void_p(pool, index))

/*---------------------------------------------------------------------------*/
/*      For Debug and Test                                                   */
tm_index_t          Pool_freed_count(Pool *pool, tm_size_t *size);
tm_index_t          Pool_freed_print(Pool *pool);

#endif
/** @} */
