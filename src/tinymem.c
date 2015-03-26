
#include "tinymem.h"
#include "tm_pool.h"
#include "tm_freed.h"

Pool pool = Pool_declare();

void tm_init(){
    Pool_freed_reset(&pool);
}

tm_index tmalloc(tm_size size){
    return Pool_alloc(&pool, size);
}

void tmfree(tm_index index){
    return Pool_free(&pool, index);
}

inline void*  tm_void(tm_index index){
    return Pool_void(&pool, index);
}
