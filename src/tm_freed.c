#include <stdio.h>
#include "tm_freed.h"

/* Freed Array methods for Pool
 */

#define HASH_PRIME 1677619


// Designed to create an ultra fast hash with < 256 bins
// see scripts/hash.py for more details and tests (hash4)
uint8_t freed_hash(tm_index value){
    uint32_t h = value * HASH_PRIME;
    return ((h>>16) xor (h & 0xFFFF)) % TM_FREED_BINS;
}


bool Pool_freed_append(Pool *pool, tm_index index){
    // Indicate that index was freed to freed arrays
    uint8_t findex = freed_hash(Pool_sizeof(pool, index));
    return LIA_append(pool, &(pool->freed[findex]), index);
}


tm_index Pool_freed_getsize(Pool *pool, tm_size size){
    // Indicate that index was freed to freed arrays
    uint8_t findex = freed_hash(size);
    return LIA_pop(pool, &(pool->freed[findex]), size);
}


/* Linked Index Array Methods
 */

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
    tm_index delete = false;
    tm_index uindex = *last;
    LinkedIndexArray *a = Pool_LIA(pool, uindex);

    if(not a) return 0;
    while(true){
        for(i=0; i<TM_FREED_BINSIZE; i++){
            printf("i=%u\n", i);
            if(Pool_sizeof(pool, a->indexes[i]) == size){
                index = a->indexes[i];
                goto found;
            }
        }
        if(not index){
            // index wasn't found, try previous array
            if(uindex == *last) final_last_i = i;  // will be used later
            uindex = a->prev;
            printf("going to prev: %u\n", uindex);
            a = Pool_LIA(pool, uindex);
            if(not a){
                printf("no prev, return 0\n");
                return 0;
            }
        }
    }
found:
    printf("found\n");
    if(uindex == *last){
        printf("islast\n");
        // the index was found in the "last" array
        // we need to find it's final index location
        for(j=i; (j<TM_FREED_BINSIZE) and (a->indexes[j]); j++);
        final_last_i = j - 1;
        printf("flast=%u\n", final_last_i);
        if(not final_last_i) delete = true;
    }
    j = final_last_i;

    // "pop" the very last index value
    final_last_i = Pool_LIA(pool, *last)->indexes[final_last_i];
    Pool_LIA(pool, *last)->indexes[j] = 0;

    // Put the popped value into the one being removed
    a->indexes[i] = j;

    // If the *last array is empty, delete it
    printf("delete=%u\n", delete);
    if(delete){
        printf("deleting\n");
        delete = *last;
        *last = Pool_LIA(pool, delete)->prev;
        LIA_del(pool, delete);
    }
    else{
        printf("not deleting\n");
    }

    return index;
}
