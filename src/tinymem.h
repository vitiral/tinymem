#ifndef __tinymem_h
#define __tinymem_h
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#undef NDEBUG
#include "tinymem_platform.h"
#include <assert.h>
#include <signal.h>


#ifndef NDEBUG
#define tm_debug(...)      do{printf("[DEBUG](%s,%u):", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n");}while(0)
#endif


/*---------------------------------------------------------------------------*/
/**
 * \brief           status bitcodes
 */
#define TM_DEFRAG_FULL      (1<<0)  // a full defrag has been requested
#define TM_DEFRAG_FAST      (1<<1)  // a fast defrag has been requested
#define TM_DEFRAG_FULL_IP   (1<<2)  // A defrag is in progress
#define TM_DEFRAG_FAST_IP   (1<<3)  // A defrag is in progress
#define TM_MOVING           (1<<4)  // the memory manager is currently moving a block
#define TM_DEFRAG_FULL_DONE (1<<5)  // this will be set after a full defrag has happend
#define TM_DEFRAG_FAST_DONE (1<<6)  // this will be set after a fast defrag has happened.
#define TM_ERROR            (1<<7)  // a memory manager internal error occurred
#define TM_DEFRAG_IP        (TM_DEFRAG_FULL_IP | TM_DEFRAG_FAST_IP)             // defrag is in progress
#define TM_ANY_DEFRAG       (TM_DEFRAG_FULL | TM_DEFRAG_FAST | TM_DEFRAG_IP)    // some defrag has been requested


typedef uint16_t        tm_index_t;
typedef uint32_t        tm_size_t;
typedef uint8_t         tm_status_type;


/*---------------------------------------------------------------------------*/
/**
 * \brief           Get the sizeof data at index in bytes
 * \return tm_size_t  the sizeof the data pointed to by index
 */
inline tm_size_t tm_sizeof(const tm_index_t index);


/*---------------------------------------------------------------------------*/
/**
 * \brief           allocate memory from pool
 * \param pool      pointer to Pool struct
 * \param size      size of pointer to allocate
 * \return          tm_index_t corresponding to memory location
 *                  On error or if not enough memory, return value == 0
 */
tm_index_t          tm_alloc(tm_size_t size);

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
tm_index_t          tm_realloc(tm_index_t index, tm_size_t size);

/*---------------------------------------------------------------------------*/
/**
 * \brief           free allocated memory from pool
 * \param pool      pointer to Pool struct
 * \param index     tm_index_t to free
 */
void                tm_free(const tm_index_t index);


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
#define tm_char_p(index)        ((char *)tm_void_p(index))
#define tm_int8_p(index)        ((int8_t *)tm_void_p(index))
#define tm_uint8_p(index)       ((uint8_t *)tm_void_p(index))
#define tm_int16_p(index)       ((int16_t *)tm_void_p(index))
#define tm_uint16_p(index)      ((uint16_t *)tm_void_p(index))
#define tm_int32_p(index)       ((int32_t *)tm_void_p(index))
#define tm_uint32_p(index)      ((uint32_t *)tm_void_p(index))

/*---------------------------------------------------------------------------*/
/*      For Debug and Test                                                   */
tm_index_t          tm_freed_count(tm_size_t *size);
tm_index_t          tm_freed_print();

char*               test_tm_pool_new();
char*               test_tm_pool_alloc();
char*               test_tm_free_basic();
char*               test_tm_pool_realloc();

#endif
/** @} */
