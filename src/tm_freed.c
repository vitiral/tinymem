#include <stdio.h>
#include "tm_freed.h"

/* Freed Array methods for Pool
 */

#define HASH_PRIME 1677619


// Designed to create an ultra fast hash with < 256 bins
// see scripts/hash.py for more details and tests (hash4)
uint8_t freed_hash(tm_index value){
    uint32_t h = value * HASH_PRIME;
    return ((h>>16) ^ (h & 0xFFFF)) % TM_FREED_BINS;
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

bool Pool_freed_isvalid(Pool *pool){
    tm_index bin, i;
    tm_index index;
    for(bin=0; bin<TM_FREED_BINS; bin++){
        LinkedIndexArray *a = Pool_LIA(pool, pool->freed[bin]);
        if(!a) continue;
        for(i=0; i<=TM_FREED_BINSIZE; i++){
            // tmdebug("f i=%u, a=0x%x, a_prev=%u", i, a, a->prev);
            // check that data is valid
            if(i==TM_FREED_BINSIZE || a->indexes[i] == 0){
                // reached end of array, load and check prev if it exists
                if(a->prev >= TM_UPOOL_ERROR){
                    break;
                }
                LinkedIndexArray *a = Pool_LIA(pool, a->prev);
                i=0;
            }
            index = a->indexes[i];
            if(bin!=freed_hash(Pool_sizeof(pool, index))){
                tmdebug("error: invalid hash. index=%u", index);
                return false;
            }
            if(Pool_filled_bool(pool, index)){
                tmdebug("error: is filled. index=%u", index);
                return false;
            }
            if(!Pool_points_bool(pool, index)){
                tmdebug("error: does not point index=%u", index);
                return false;
            }
        }
    }
    return true;
}

/* Linked Index Array Methods
 */

uint16_t LIA_new(Pool *pool){
    uint8_t i;
    LinkedIndexArray *a;
    tm_index uindex = Pool_ualloc(pool, sizeof(LinkedIndexArray));
    if(! uindex) return 0;
    a = Pool_LIA(pool, uindex);
    a->prev = TM_UPOOL_ERROR + 1;
    for(i=0; i<TM_FREED_BINSIZE; i++){
        a->indexes[i] = 0;
    }
    return uindex;
}


bool LIA_del(Pool *pool, tm_index uindex){
    Pool_LIA(pool, uindex)->prev = TM_UPOOL_ERROR;
    if(!Pool_ufree(pool, uindex)){
        return false;
    }
    return true;
}


bool LIA_append(Pool *pool, tm_index *last, tm_index value){
    // Appends a value onto the end of the LIA. If it overflows,
    // moves the *last index to true last if it changes
    // If this returns false it is time to full defrag
    // because free values will be lost otherwise!
    uint8_t i;
    tm_index uindex;
    if(Pool_status(pool, TM_ANY_DEFRAG)){
        // TODO: for threading some very specific things need to happen here.
        //      Everything is fine for simple
        return false;
    }
    LinkedIndexArray *a = Pool_LIA(pool, *last);
    if(a){  // if *last is ERROR/NULL, just create a new array
        for(i=0; i<TM_FREED_BINSIZE; i++){
            if(! a->indexes[i]){
                a->indexes[i] = value;
                if(i<TM_FREED_BINSIZE - 1) a->indexes[i+1] = 0;
                return true;
            }
        }
    }
    // data does not fit in this array, must allocate a new one
    uindex = LIA_new(pool);
    if(uindex >= TM_UPOOL_ERROR){
        Pool_status_set(pool, TM_DEFRAG);
        Pool_defrag_full(pool);  // TODO: simple implementation
        return true;             // Also for simple only. It was freed because it was defraged
    }
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
    if(Pool_status(pool, TM_ANY_DEFRAG)){
        return 0;
    }
    uint8_t i, j;
    tm_index final_last_i;
    tm_index index = 0;
    tm_index temp;
    tm_index uindex = *last;
    LinkedIndexArray *a = Pool_LIA(pool, uindex);

    if(! a) return 0;
    while(true){
        for(i=0; i<TM_FREED_BINSIZE; i++){
            if(Pool_sizeof(pool, a->indexes[i]) == size){
                index = a->indexes[i];
                goto found;
            }
        }
        if(! index){
            // index wasn't found, try previous array
            if(uindex == *last) final_last_i = i;  // will be used later
            uindex = a->prev;
            a = Pool_LIA(pool, uindex);
            if(! a){
                return 0;
            }
        }
    }
found:
    if(uindex == *last){
        // the index was found in the "last" array
        // we need to find it's final index location
        for(j=i; (j<TM_FREED_BINSIZE) && (a->indexes[j]); j++);
        final_last_i = j - 1;
    }

    // "pop" the very last index value
    temp = Pool_LIA(pool, *last)->indexes[final_last_i];
    Pool_LIA(pool, *last)->indexes[final_last_i] = 0;

    // Put the popped value into the one being removed
    a->indexes[i] = temp;

    // If the *last array is empty, delete it
    if(! final_last_i){
        final_last_i = *last;
        *last = Pool_LIA(pool, final_last_i)->prev;
        if(!LIA_del(pool, final_last_i)){
            Pool_status_set(pool, TM_DEFRAG);
            Pool_defrag_full(pool);  // TODO: simple implementation
            return 0;
        }
    }
    tmdebug("popping: %u", index);
    return index;
}
