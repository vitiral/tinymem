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
#define BLOCKS_LEFT                 (TM_POOL_BLOCKS - tm_pool.filled_blocks)
#define PTRS_LEFT                   (TM_MAX_POOL_PTRS - (tm_pool.ptrs_filled + tm_pool.ptrs_filled))
#define HEAP_LEFT                   (TM_POOL_SIZE - HEAP)
#define LOCATION(index)             (tm_pool.pointers[index].loc)
#define HEAP                        (tm_pool.pointers[0].loc)
#define NEXT(index)                 (tm_pool.pointers[index].next)
#define LOC_VOID(loc)               ((void*)tm_pool.pool + (loc))

#define BITARRAY_INDEX(index)        (index / 8)
#define BITARRAY_BIT(index)          (1 << (index % 8))
#define FILLED(index)               (tm_pool.filled[BITARRAY_INDEX(index)] &   BITARRAY_BIT(index))
#define FILLED_SET(index)           (tm_pool.filled[BITARRAY_INDEX(index)] |=  BITARRAY_BIT(index))
#define FILLED_CLEAR(index)         (tm_pool.filled[BITARRAY_INDEX(index)] &= ~BITARRAY_BIT(index))
#define POINTS(index)               (tm_pool.points[BITARRAY_INDEX(index)] &   BITARRAY_BIT(index))
#define POINTS_SET(index)           (tm_pool.points[BITARRAY_INDEX(index)] |=  BITARRAY_BIT(index))
#define POINTS_CLEAR(index)         (tm_pool.points[BITARRAY_INDEX(index)] &= ~BITARRAY_BIT(index))

#define Pool_memmove(index_to, index_from)  memmove(                \
            tm_void_p(index_to),                                    \
            tm_void_p(index_from),                                  \
            tm_sizeof(index_from)                                   \
        )


/*---------------------------------------------------------------------------*/
/**
 * \brief           Get, set or clear the status bit (0 or 1) of name
 * \return uint8_t  status bit
 */
#define STATUS(name)                ((tm_pool.status) & (name))
#define STATUS_SET(name)            (tm_pool.status |= (name))
#define STATUS_CLEAR(name)          (tm_pool.status &= ~(name))


/*---------------------------------------------------------------------------*/
/**
 * \brief           Get the sizeof data at index in bytes
 * \return tm_size_t  the sizeof the data pointed to by index
 */
#define tm_sizeof(index)            (BLOCKS(index) * TM_ALIGN_BYTES)
#define BLOCKS(index)               (LOCATION(tm_pool.pointers[index].next) - \
                                        LOCATION(index))

/*---------------------------------------------------------------------------*/
/**
 * \brief           allocate memory from pool
 * \param pool      pointer to Pool struct
 * \param size      size of pointer to allocate
 * \return          tm_index_t corresponding to memory location
 *                  On error or if not enough memory, return value == 0
 */
tm_index_t          Pool_alloc(tm_size_t size);

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
tm_index_t          Pool_realloc(tm_index_t index, tm_size_t size);

/*---------------------------------------------------------------------------*/
/**
 * \brief           free allocated memory from pool
 * \param pool      pointer to Pool struct
 * \param index     tm_index_t to free
 */
void                Pool_free(const tm_index_t index);


/*---------------------------------------------------------------------------*/
/**
 * \brief           cast a void pointer from index
 * \param pool      pointer to Pool struct
 * \param index     tm_index_t to get pointer to
 * \return          void* pointer to actual data
 */
inline void*        tm_void_p(const tm_index_t index);

/*---------------------------------------------------------------------------*/
/**
 * \brief           Various data type casts
 */
#define Pool_char_p(pool, index)        ((char *)tm_void_p(pool, index))
#define Pool_int8_p(pool, index)        ((int8_t *)tm_void_p(pool, index))
#define Pool_uint8_p(pool, index)       ((uint8_t *)tm_void_p(pool, index))
#define Pool_int16_p(pool, index)       ((int16_t *)tm_void_p(pool, index))
#define Pool_uint16_p(pool, index)      ((uint16_t *)tm_void_p(pool, index))
#define Pool_int32_p(pool, index)       ((int32_t *)tm_void_p(pool, index))
#define Pool_uint32_p(pool, index)      ((uint32_t *)tm_void_p(pool, index))

/*---------------------------------------------------------------------------*/
/*      For Debug and Test                                                   */
tm_index_t          Pool_freed_count(tm_size_t *size);
tm_index_t          Pool_freed_print();

#endif
/** @} */
