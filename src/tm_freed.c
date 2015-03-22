#include "tm_freed.h"

uint16_t LIA_new(Pool *pool){
    uint8_t i;
    LinkedIndexArray *a;
    tm_index uindex = Pool_ualloc(pool, sizeof(LinkedIndexArray));
    if(not uindex) return 0;
    a = Pool_LIA(pool, uindex);
    a->prev = 0;
    for(i=0; i<TM_FREED_BINSIZE; i++){
        a->indexes[i] = 0;
    }
    return uindex;
}


void LIA_del(Pool *pool, tm_index uindex){
    Pool_LIA(pool, uindex)->prev = 0;
    Pool_ufree(pool, uindex);
}


bool LIA_append(Pool *pool, tm_index uindex, tm_index value){
    uint8_t i, index2;
    LinkedIndexArray *a = Pool_LIA(pool, uindex);
    LinkedIndexArray *a2;
    for(i=0; i<TM_FREED_BINSIZE; i++){
        if(not a->indexes[i]){
            a->indexes[i] = value;
            return true;
        }
    }
    index2 = LIA_new(pool);
    if(not uindex) return false;
    a2 = Pool_LIA(pool, index2);
    a2->prev = index2;
    a2->indexes[0] = value;
    return true;
}

tm_index LIA_pop(Pool *pool, tm_index uindex){

}
