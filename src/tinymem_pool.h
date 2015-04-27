#ifndef __tinymem_pool_h
#define __tinymem_pool_h
#include "tinymem_types.h"


//#if (TM_POOL_SIZE % sizeof(free_block))
//#error "Invalid pool size, must be divisible by free_block"
//#endif

//#if (TM_MAX_POOL_PTRS % (8 * sizeof(int)))
//#error "Invalid pool ptrs size, must be divisible by int"
//#endif

#define TM_POOL_BLOCKS          (TM_POOL_SIZE / sizeof(free_block))
#define TM_MAX_BIT_INDEXES      (TM_MAX_POOL_PTRS / (8 * sizeof(int)))
#define MAXUINT                 ((unsigned int) 0xFFFFFFFFFFFFFFFF)
#define INTBITS                 (sizeof(int) * 8)

#define TM_FREED_BINS           (12)
#define TM_ALIGN_BYTES          sizeof(free_block)

/*---------------------------------------------------------------------------*/
/**
 * \brief           poolptr is used by Pool to track memory location and size
 */
// TODO: packed!
typedef struct {
    tm_size_t loc;
    tm_index_t next;
} poolptr;
#define NULL_poolptr            ((poolptr){.size=0, .ptr=0})

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
    tm_size_t       filled_bytes;                   //!< total amount of data allocated
    tm_size_t       freed_bytes;                    //!< total amount of data freed
    tm_index_t      ptrs_filled;                     //!< total amount of pointers allocated
    tm_index_t      ptrs_freed;                      //!< total amount of pointers freed
    tm_index_t      find_index;                     //!< speed up find index
    uint8_t         status;                         //!< status byte. Access with Pool_status macros
    uint8_t         find_index_bit;                 //!< speed up find index
} Pool;


/*---------------------------------------------------------------------------*/
/**
 * \brief           Internal use only. Declares Pool with as many initial
 *                  values as possible
 */
#define Pool_declare()  ((Pool) {                               \
    .filled = {0},                                              \
    .points = {1},                      /*NULL is taken*/       \
    .pointers = {{0, 0}},               /*heap = 0*/            \
    .freed = {0},                                               \
    .filled_bytes = {0},                                        \
    .freed_bytes = {0},                                         \
    .ptrs_filled = 1,                    /*NULL is "filled"*/    \
    .ptrs_freed = 0,                                             \
    .find_index = 0,                                            \
    .status = 0,                                                \
    .find_index_bit = 1,                /*index 0 is invalid*/  \
})

/*---------------------------------------------------------------------------*/
/**
 * \brief           Pool available memory (both heap and inside freed)
 *                  This is how much space is left to allocate after a full
 *                  defragmentation
 * \return tm_size_t  available space in bytes
 */
#define Pool_available(p)            (TM_POOL_SIZE - (p)->used_bytes)

/*---------------------------------------------------------------------------*/
/**
 * \brief           Pool available pointers
 *                  This is how many more objects can be allocated
 * \return tm_size_t  available pointer spaces
 */
#define Pool_pointers_left(p)        (TM_MAX_POOL_PTRS - ((p)->ptrs_filled + (p)->ptrs_filled))

/*---------------------------------------------------------------------------*/
/**
 * \brief           Memory remaining on the heap
 * \return tm_size_t  Number of bytes remaining on the heap
 */
#define Pool_heap_left(p)            (TM_POOL_SIZE - Pool_heap(p))

/*---------------------------------------------------------------------------*/
/**
 * \brief           location of index
 * \return tm_size_t  location in relation to pool.pool
 */
#define Pool_loc(p, index)              ((p)->pointers[index].loc)

/*---------------------------------------------------------------------------*/
/**
 * \brief           Get the sizeof data at index in bytes
 * \return tm_size_t  the sizeof the data pointed to by index
 */
#define Pool_sizeof(p, index)        (Pool_loc((p)->indexes[index].next) - Pool_loc(p, index))

/*---------------------------------------------------------------------------*/
/**
 * \brief           Get the status bit (0 or 1) of name
 * \return uint8_t  status bit
 */
#define Pool_status(p, name)         (((p)->status) & (name))

/*---------------------------------------------------------------------------*/
/**
 * \brief           Set the pool status of name to 1
 */
#define Pool_status_set(p, name)     ((p)->status |= (name))

/*---------------------------------------------------------------------------*/
/**
 * \brief           Set the pool status of name to 0
 */
#define Pool_status_clear(p, name)   ((p)->status &= ~(name))

/*---------------------------------------------------------------------------*/
/**
 * \brief           Convinience functions
 *                  Pool_filled* does operations on Pool's `filled` array
 *                  Pool_points* does operations on Pool's `points` array
 */
#define BITARRAY_INDEX(index)        (index / 8)
#define BITARRAY_BIT(index)          (1 << (index % 8))
#define Pool_filled_bool(p, index)   ((p)->filled[BITARRAY_INDEX(index)] &   BITARRAY_BIT(index))
#define Pool_filled_set(p, index)    ((p)->filled[BITARRAY_INDEX(index)] |=  BITARRAY_BIT(index))
#define Pool_filled_clear(p, index)  ((p)->filled[BITARRAY_INDEX(index)] &= ~BITARRAY_BIT(index))
#define Pool_points_bool(p, index)   ((p)->points[BITARRAY_INDEX(index)] &   BITARRAY_BIT(index))
#define Pool_points_set(p, index)    ((p)->points[BITARRAY_INDEX(index)] |=  BITARRAY_BIT(index))
#define Pool_points_clear(p, index)  ((p)->points[BITARRAY_INDEX(index)] &= ~BITARRAY_BIT(index))
#define Pool_heap(p)                 ((p)->pointers[0].loc)

/*---------------------------------------------------------------------------*/
/**
 * \brief           move memory from location at index_from to location at
 *                  index_to
 */
#define Pool_memmove(pool, index_to, index_from)  memmove(              \
            Pool_void_p(pool, index_to),                                  \
            Pool_void_p(pool, index_from),                                \
            Pool_sizeof(pool, index_from)                               \
        )

/*---------------------------------------------------------------------------*/
/**
 * \brief           cast a void pointer of location
 */
#define Pool_loc_void_p(p, loc)           ((void*)(p)->pool + (loc))

/*---------------------------------------------------------------------------*/
/**
 * \brief           initialize (or reset) a pool
 */
void            Pool_init(Pool *pool);

/*---------------------------------------------------------------------------*/
/**
 * \brief           allocate memory from pool
 * \param pool      pointer to Pool struct
 * \param size      size of pointer to allocate
 * \return          tm_index_t corresponding to memory location
 *                  On error or if not enough memory, return value == 0
 */
tm_index_t        Pool_alloc(Pool *pool, tm_size_t size);

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
tm_index_t        Pool_realloc(Pool *pool, tm_index_t index, tm_size_t size);

/*---------------------------------------------------------------------------*/
/**
 * \brief           free allocated memory from pool
 * \param pool      pointer to Pool struct
 * \param index     tm_index_t to free
 */
void            Pool_free(Pool *pool, tm_index_t index);

/*---------------------------------------------------------------------------*/
/**
 * \brief           cast a void pointer from index
 * \param pool      pointer to Pool struct
 * \param index     tm_index_t to get pointer to
 * \return          void* pointer to actual data
 */
inline void*    Pool_void_p(Pool *pool, tm_index_t index);

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

#endif
/** @} */
