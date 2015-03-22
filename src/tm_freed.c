#include "tm_freed.h"

uint16_t LIA_new(Pool *pool){
    uint8_t i;
    LinkedIndexArray *a;
    tm_index uindex = Pool_ualloc(pool, sizeof(LinkedIndexArray));
    if(not uindex) return 0;
    a = Pool_LIA(pool, uindex);
    a->prev = TM_UPOOL_ERROR + 1;
    for(i=0; i<TM_FREED_BINSIZE; i++){
        a->indexes[i] = 0;
    }
    return uindex;
}


void LIA_del(Pool *pool, tm_index uindex){
    Pool_LIA(pool, uindex)->prev = TM_UPOOL_ERROR;
    Pool_ufree(pool, uindex);
}


bool LIA_append(Pool *pool, tm_index *last, tm_index value){
    // Appends a value onto the end of the LIA. If it overflows,
    // moves the *last index to true last if it changes
    // If this returns false it is time to full defrag
    // because free values will be lost otherwise!
    uint8_t i;
    tm_index uindex;
    LinkedIndexArray *a = Pool_LIA(pool, *last);
    if(a){  // if *last is ERROR/NULL, just create a new array
        for(i=0; i<TM_FREED_BINSIZE; i++){
            if(not a->indexes[i]){
                a->indexes[i] = value;
                if(i<TM_FREED_BINSIZE - 1) a->indexes[i+1] = 0;
                return true;
            }
        }
    }
    // data does not fit in this array, must allocate a new one
    uindex = LIA_new(pool);
    if(uindex >= TM_UPOOL_ERROR) return false;
    a = Pool_LIA(pool, uindex);
    a->prev = *last;
    a->indexes[0] = value;
    a->indexes[1] = 0;
    *last = uindex;
    return true;
}


tm_index LIA_pop(Pool *pool, tm_index *last, tm_size size){
    // Pops a value off of LIA that meets the size criteria
    // modifies *last if this action causes the last index to get deleted
    uint8_t i, j;
    tm_index final_last_i;
    tm_index index = 0;
    tm_index temp;
    tm_index uindex = *last;
    LinkedIndexArray *a = Pool_LIA(pool, uindex);

    if(not a) return 0;
    while(true){
        for(i=0; i<TM_FREED_BINSIZE; i++){
            if(Pool_sizeof(pool, a->indexes[i]) == size){
                index = a->indexes[i];
                goto found;
            }
        }
        if(not index){
            // index wasn't found, try previous array
            if(uindex == *last) final_last_i = i;  // will be used later
            uindex = a->prev;
            a = Pool_LIA(pool, uindex);
            if(not a) return 0;
        }
    }
found:
    if(uindex == *last){
        // the index was found in the "last" array
        // we need to find it's final index location
        for(j=i; (j<TM_FREED_BINSIZE) or (not a->indexes[j+1]); j++);
        final_last_i = j - 1;
    }
    j = final_last_i;

    // "pop" the very last index value
    final_last_i = Pool_LIA(pool, *last)->indexes[final_last_i];
    Pool_LIA(pool, *last)->indexes[j] = 0;

    // Put the popped value into the one being removed
    a->indexes[i] = j;

    // If the *last array is empty, delete it
    if(not j){
        temp = *last;
        *last = Pool_LIA(pool, temp)->prev;
        LIA_del(pool, temp);
    }

    return index;
}
