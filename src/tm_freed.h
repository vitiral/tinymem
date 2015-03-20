#ifndef __tm_freed_h
#define __tm_freed_h
#include "tm_pool.h"
#include "tm_types.h"

#define TM_FREED_BINS           (16)
#define TM_FREED_BINSIZE        (14)

typedef struct {
    tm_index prev;
    tm_index indexes[TM_FREED_BINSIZE];
} LinkedIndexArray;

tm_index    LIA_new(Pool *pool);
bool        LIA_append(Pool *pool, tm_index index);
tm_index    LIA_pop(Pool *pool, tm_index index);


#define LIA_del(Pool *pool, *LinkedIndexArray* l)           Pool_ufree(pool, index)
#define Pool_LIA(pool, index)                               ((LinkedIndexArray *) Pool_uvoid(pool, index))

#endif
