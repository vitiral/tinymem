#ifndef __tm_freed_h
#define __tm_freed_h
#include "tm_pool.h"

/* Freed Array methods for Pool */
uint8_t     freed_hash(tm_index value);
bool        Pool_freed_append(Pool *pool, tm_index index);
tm_index    Pool_freed_getsize(Pool *pool, tm_size size);


/* Linked Index Array Methods */
typedef struct {
    tm_index prev;
    tm_index indexes[TM_FREED_BINSIZE];
} LinkedIndexArray;

tm_index    LIA_new(Pool *pool);
bool        LIA_del(Pool *pool, tm_index uindex);
bool        LIA_append(Pool *pool, tm_index *last, tm_index value);
tm_index    LIA_pop(Pool *pool, tm_index *last, tm_size size);


#define Pool_LIA(pool, ulocation)   ((LinkedIndexArray *) Pool_uvoid(pool, ulocation))


#endif
