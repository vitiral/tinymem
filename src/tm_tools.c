#include "tm_pool.h"

#define PRIME       (65599)

#if 0

void        fill_index(tm_index_t index){
    uint32_t value = index * PRIME;
    uint32_t *data = Pool_uint32_p(index);
    assert(data);
    tm_blocks_t i;

    if(Pool_filled_bool(index)) data[0] = value;
    for(i=1; i<Pool_blocks(index); i++){
        data[i] = value;
    }
}


bool        check_index(tm_index_t index){
    uint32_t value = index * PRIME;
    uint32_t *data = Pool_uint32_p(index);
    assert(data);
    tm_blocks_t i;

    if(Pool_filled_bool(index)){
        if(data[0] != value) return false;
    }
    for(i=1; i<Pool_blocks(index); i++){
        if(data[i] != value) return false;
    }
    return true;
}

/*---------------------------------------------------------------------------*/
/**         Test free and alloc (automatically fills data)                   */

tm_index_t  Pool_talloc(tm_size_t size){
    tm_index_t index = Pool_alloc(size);
    if(!index) return 0;
    fill_index(index);
    return index;
}


void        Pool_tfree(tm_index_t index){
    Pool_free(index);
    fill_index(index);
}

#endif
