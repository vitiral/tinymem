#ifndef __tm_tools_h
#define __tm_tools_h
#include "tm_pool.h"

void            fill_index(Pool *pool, tm_index_t index);
bool            check_index(Pool *pool, tm_index_t index);
tm_index_t      Pool_talloc(Pool *pool, tm_size_t size);
void            Pool_tfree(Pool *pool, tm_index_t index);


#endif
