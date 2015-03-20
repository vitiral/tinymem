#include "tm_freed.h"

uint16_t LIA_new(Pool *pool){
    uint8_t i;
    LinkedIndexArray *a;
    tm_index index = Pool_ualloc(pool);
    if(not index) return 0;
    a = Pool_LIA(pool, index);
    a->prev = 0;
    for(i=0; i<TM_FREED_BINSIZE; i++){
        a->indexes[i] = 0;
    }
    return index;
}


uint16_t LIA_del(Pool *pool, tm_index index){
    Pool_ufree(pool, index);
}


bool LIA_append(Pool *pool, tm_index index, tm_index value){
    uint8_t i, index2;
    LinkedIndexArray *a = Pool_LIA(pool, index);
    LinkedIndexArray *a2;
    for(i=0; i<TM_FREED_BINSIZE; i++){
        if(not a->indexes[i]){
            a->indexes[i] = value;
            return true;
        }
    }
    index2 = LIA_new(pool);
    if(not index) return false;
    a2 = Pool_LIA(pool, index2);
    a2->prev = index2;
    a2->indexes[0] = value;
    return true;
}

tm_index LIA_pop(Pool *pool, *LinkedIndexArray* l){

}
