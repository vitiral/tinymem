#ifndef __tm_freed_h
#define __tm_freed_h
#include "tm_pool.h"

/* Freed Array methods for Pool */
uint8_t     freed_hash(tm_index_t value);
bool        Pool_freed_append(Pool *pool, tm_index_t index);
tm_index_t    Pool_freed_getsize(Pool *pool, tm_size_t size);


/* Linked Index Array Methods */
typedef struct {
    tm_index_t prev;
    tm_index_t indexes[TM_FREED_BINSIZE];
} LinkedIndexArray;

tm_index_t    LIA_new(Pool *pool);
bool        LIA_del(Pool *pool, tm_index_t uindex);
bool        LIA_append(Pool *pool, tm_index_t *last, tm_index_t value);
tm_index_t    LIA_pop(Pool *pool, tm_index_t *last, tm_size_t size);


#define Pool_LIA(pool, ulocation)   ((LinkedIndexArray *) Pool_uvoid(pool, ulocation))

/*---------------------------------------------------------------------------*/
/**
 * \brief           For debugging and testing
 */
bool LIA_valid(Pool *pool, tm_index_t uindex);
bool Pool_freed_isvalid(Pool *pool);

#endif
