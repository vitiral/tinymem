#ifndef __mempool_h_
#define __mempool_h_
#include <stdlib.h>
#include "tm_types.h"
#define TM_POOL_SIZE            ((uint16_t)(0xFFFFFFFF - 1))
#define TM_FREED_BINS           (16)
#define TM_FREED_BINSIZE        (14)

#define TM_MAX_POOL_PTRS        (254)
#define TM_MAX_FILLED_PTRS      (TM_MAX_POOL_PTRS / 8 + (TM_MAX_POOL_PTRS % 8 ? 1:0))
#define TM_MAX_FILLED_INT       (TM_MAX_FILLED_PTRS / sizeof(int) + \
                                    ((TM_MAX_FILLED_PTRS % sizeof(int)) ? 1:0))
#define TM_UPOOL_SIZE           (TM_MAX_POOL_PTRS * sizeof(tm_index))
#define INTBITS                 (sizeof(int) * 8)
#define MAXUINT                 ((unsigned int) 0xFFFFFFFFFFFFFFFF)
#define NULL_poolptr            ((poolptr){.size=0, .ptr=0})
#define TM_LAST_USED            ((uint8_t) (~(0x00FF >> (TM_MAX_POOL_PTRS % 8))))

#define TM_UPOOL_ERROR          (TM_UPOOL_SIZE + 1)

typedef struct {
    tm_size size;
    tm_size ptr;
} poolptr;

/* Ptrpool
 * Contains the Ptr and Type information of every pool. Main lookup
 * for malloc and freeavai
 */
typedef struct {
    tm_size heap;                       // location of completely free memory
    tm_size stack;                      // used for tempalloc and tempfree, similar to standard stack
    tm_size used_bytes;                 // total amount of data in use out of size
    tm_size used_pointers;              // total amount of pointers used
    tm_index filled_index;              // faster lookup of full pointers for defragmentation
    tm_index points_index;              // faster lookup for unused pointers for allocation
    tm_size uheap;                      // heap of the upool
    tm_size ustack;                     // stack of the upool
    uint8_t filled[TM_MAX_FILLED_PTRS]; // array of bit data for fast lookup of data to move
    uint8_t points[TM_MAX_FILLED_PTRS]; // array of bit data for fast lookup of unused pointers
    poolptr pointers[TM_MAX_POOL_PTRS]; // size and location of data in pool
    uint8_t  upool[TM_UPOOL_SIZE];      // extra space for processing
    tm_index freed[TM_FREED_BINS];      // binned storage of all freed indexes
    uint8_t pool[TM_POOL_SIZE];         // pool location in memory
} Pool;

#define Pool_available(pool)            (TM_POOL_SIZE - (pool)->used_bytes)
#define Pool_pointers_left(pool)        (TM_MAX_POOL_PTRS - (pool)->used_poin%ters)
#define Pool_heap_left(pool)            (pool->stack - pool->heap)
#define Pool_filled_index(index)        (index / 8)
#define Pool_filled_bit(index)          (1 << (index % 8))
#define Pool_filled_bool(pool, index)   ((pool)->filled[Pool_filled_index(index)] & Pool_filled_bit(index))
#define Pool_filled_set(pool, index)    ((pool)->filled[Pool_filled_index(index)] |=  Pool_filled_bit(index))
#define Pool_filled_clear(pool, index)  ((pool)->filled[Pool_filled_index(index)] &= ~Pool_filled_bit(index))
#define Pool_points_bool(pool, index)   ((pool)->points[Pool_filled_index(index)] & Pool_filled_bit(index))
#define Pool_points_set(pool, index)    ((pool)->points[Pool_filled_index(index)] |=  Pool_filled_bit(index))
#define Pool_points_clear(pool, index)  ((pool)->points[Pool_filled_index(index)] &= ~Pool_filled_bit(index))
#define Pool_sizeof(pool, index)        ((pool)->pointers[index].size) // get size of data at index

#define Pool_location(pool, index)              ((pool)->pointers[index].ptr)  // location of pointer inside pool
#define Pool_location_set(pool, index, loc)     (Pool_location(pool, index) = loc)
#define Pool_location_void(pool, loc)           ((void*)(pool)->pool + (loc))    // pointer of location

Pool*           Pool_new();
#define Pool_del(pool)  (free(pool))
void*           Pool_void(Pool *pool, tm_index index);
tm_index        Pool_alloc(Pool *pool, tm_size size);
void            Pool_free(Pool *pool, tm_size size);

/* upool allocation and freeing. Used for internal methods */
tm_index Pool_ualloc(Pool *pool, tm_size size);
void Pool_ufree(Pool *pool, tm_index index);
void *Pool_uvoid(Pool *pool, tm_index index);
#define Pool_upool_get_index(pool, index)  (((tm_index *)((pool)->upool))[index])
#define Pool_upool_set_index(pool, index, value)  (((tm_index *)((pool)->upool))[index] = value)
#define Pool_uheap_left(pool)           (pool->ustack - pool->uheap)
#define Pool_ustack_used(pool)          (TM_UPOOL_SIZE - pool->ustack)

/* Special methods for freed arrays */
void Pool_upool_clear(Pool *pool);
void Pool_freed_reset(Pool *pool);

/* Data types */
#define Pool_uint8_p(pool, index)       ((uint8_t *)Pool_void(pool, index))
#define Pool_uint16_p(pool, index)      ((uint16_t *)Pool_void(pool, index))
#define Pool_uint32_p(pool, index)      ((uint32_t *)Pool_void(pool, index))

#endif
