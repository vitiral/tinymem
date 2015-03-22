#ifndef __tm_freed_h
#define __tm_freed_h
#include "tm_pool.h"
#include "tm_types.h"

typedef struct {
    tm_index prev;
    tm_index indexes[TM_FREED_BINSIZE];
} LinkedIndexArray;

tm_index    LIA_new(Pool *pool);
void        LIA_del(Pool *pool, tm_index uindex);
bool        LIA_append(Pool *pool, tm_index *last, tm_index value);
tm_index    LIA_pop(Pool *pool, tm_index *last, tm_size size);


#define Pool_LIA(pool, index)                               ((LinkedIndexArray *) Pool_uvoid(pool, index))


#endif
