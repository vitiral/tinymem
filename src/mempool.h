#ifndef __mempool_h_
#define __mempool_h_
#include "memptr.h"


#define TM_MAX_POOL_PTRS        (254)
#define TM_MAX_FILLED_PTRS      (TM_MAX_POOL_PTRS / 8 + (TM_MAX_POOL_PTRS % 8 ? 1:0))
#define TM_MAX_FILLED_INT       (TM_MAX_FILLED_PTRS / sizeof(int) + \
                                    ((TM_MAX_FILLED_PTRS % sizeof(int)) ? 1:0))
#define TM_FREED_BINS           (12)
#define TM_FREED_BINSIZE        (18)
#define INTBITS                 (sizeof(int) * 8)
#define MAXUINT                 ((unsigned int) 0xFFFFFFFFFFFFFFFF)
#define NULL_poolptr            ((poolptr){.size=0, .ptr=0})

typedef struct {
    tm_size size;
    tm_size ptr;
} poolptr;

/* Ptrpool
 * Contains the Ptr and Type information of every pool. Main lookup
 * for malloc and free
 */
typedef struct {
    uint8_t *pool;                      // pool location in memory
    tm_size size;                       // size of pool
    tm_size heap;                       // location of completely free memory
    tm_size stack;                      // used for tempalloc and tempfree, similar to standard stack
    tm_index filled_index;              // faster lookup of full pointers for defragmentation
    tm_index used_index;                // faster lookup for unused pointers for allocation
    uint8_t filled[TM_MAX_FILLED_PTRS]; // array of bit data for fast lookup of data to move
    uint8_t used[TM_MAX_FILLED_PTRS];   // array of bit data for fast lookup of unused pointers
    poolptr pointers[TM_MAX_POOL_PTRS]; // size and location of data in pool
    tm_index freed[TM_FREED_BINS];      // binned storage of all freed indexes
} Pool;

#define Pool_left(pool)                 (pool->stack - pool->heap)
#define Pool_filled_index(index)        (index / 8)
#define Pool_filled_bit(index)          (1 << (index % 8))
#define Pool_filled_set(pool, index)    ((pool)->filled[Pool_filled_index(index)] |= Pool_filled_bit(index))
#define Pool_used_set(pool, index)      ((pool)->used[Pool_filled_index(index)]   |= Pool_filled_bit(index))
#define Pool_sizeof(pool, index)        ((pool)->pointers[index].size) // get size of data at index
#define Pool_location(pool, index)      ((pool)->pointers[index].ptr)  // location of pointer inside pool

void            Pool_delete(Pool *pool);
Pool*           Pool_new(tm_size size);
void*           Pool_void(Pool *pool, tm_index index);
tm_index        Pool_alloc(Pool *pool, tm_size size);
void            Pool_free(Pool *pool, tm_size size);

/* Data types */
#define Pool_uint8_p(pool, index)       ((uint8_t *)Pool_void(pool, index))
#define Pool_uint32_p(pool, index)      ((uint32_t *)Pool_void(pool, index))

#endif