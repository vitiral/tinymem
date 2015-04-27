#include "tm_pool.h"

#define PRIME       (65599)


void        fill_index(Pool *pool, tm_index_t index){
    uint32_t value = index * PRIME;
    uint32_t *data = Pool_uint32_p(pool, index);
    assert(data);
    tm_blocks_t i;

    if(Pool_filled_bool(pool, index)) data[0] = value;
    for(i=1; i<Pool_blocks(pool, index); i++){
        data[i] = value;
    }
}


bool        check_index(Pool *pool, tm_index_t index){
    uint32_t value = index * PRIME;
    uint32_t *data = Pool_uint32_p(pool, index);
    assert(data);
    tm_blocks_t i;

    if(Pool_filled_bool(pool, index)){
        if(data[0] != value) return false;
    }
    for(i=1; i<Pool_blocks(pool, index); i++){
        if(data[i] != value) return false;
    }
    return true;
}

/*---------------------------------------------------------------------------*/
/**         Test free and alloc (automatically fills data)                   */

tm_index_t  Pool_talloc(Pool *pool, tm_size_t size){
    tm_index_t index = Pool_alloc(pool, size);
    if(!index) return 0;
    fill_index(pool, index);
    return index;
}


void        Pool_tfree(Pool *pool, tm_index_t index){
    Pool_free(pool, index);
    fill_index(pool, index);
}
