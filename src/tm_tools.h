#ifndef __tm_tools_h
#define __tm_tools_h
#include "tm_pool.h"

void            fill_index(tm_index_t index);
bool            check_index(tm_index_t index);
tm_index_t      Pool_talloc(tm_size_t size);
void            Pool_tfree(tm_index_t index);


#endif
