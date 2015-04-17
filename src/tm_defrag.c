#include "stdlib.h"
#include "tm_defrag.h"


/**     Local Functions                                                      */
#if TM_THREADED
void Pool_append_index_during_defrag(Pool *pool, tm_index_t index);
void Pool_load_freed_after_defrag(Pool *pool);
#endif

void heap_sort(Pool *pool, tm_index_t *a, int16_t count);
void Pool_filled_sort(Pool *pool);


/*---------------------------------------------------------------------------*/
/**     Defragmentation Functions                                            */

/**
 *  Notes:
 *      Returns 0 when done, 1 when not done (in threaded mode)
 */
int8_t Pool_defrag_full(Pool *pool){
    tm_index_t index;

    if(!Pool_status(pool, TM_DEFRAG_FULL_IP))   goto NOT_STARTED;
    else if(TM_DEFRAG_temp == 0)                    goto STARTED;
    else                                        goto THREAD_LOOP;

NOT_STARTED:
    Pool_status_clear(pool, TM_ERROR);
    Pool_status_set(pool, TM_DEFRAG_FULL_IP);
    Pool_status_clear(pool, TM_DEFRAG_FAST | TM_DEFRAG_FULL);

    Pool_filled_sort(pool);

    pool->used_bytes = 1;
    pool->used_pointers = 1;

    if(!TM_DEFRAG_len) return 0;  // there were no filled indexes, done

    // freed bins now help store newly freed data
    // for(index=0; index<TM_FREED_BINS; index++) pool->freed[index] = 0;

    TM_DEFRAG_temp = 0;  // indicate STARTED

STARTED:
    // we now have sorted indexes by location. We just need to
    // move all memory to the left
    // First memory can be moved to loc 1
    // TODO: use macro Pool_memmove(pool, to, from)
    index = Pool_upool_get_index(pool, 0);
    // memmove(to, from, size)
    memmove(Pool_location_void(pool, 1), Pool_void(pool, index), Pool_sizeof(pool, index));
    Pool_location_set(pool, index, 1);
    pool->used_bytes += Pool_sizeof(pool, index);
    pool->used_pointers++;

    TM_DEFRAG_index = index;
    // rest of memory is packed
    for(TM_DEFRAG_temp=1; TM_DEFRAG_temp<TM_DEFRAG_len; TM_DEFRAG_temp++){
THREAD_LOOP:
        index = Pool_upool_get_index(pool, TM_DEFRAG_temp);
        memmove(
            Pool_void(pool, TM_DEFRAG_index) + Pool_sizeof(pool, TM_DEFRAG_index),
            Pool_void(pool, index),
            Pool_sizeof(pool, index)
        );
        Pool_location_set(pool, index,
                          Pool_location(pool, TM_DEFRAG_index) +
                          Pool_sizeof(pool, TM_DEFRAG_index));
        pool->used_bytes += Pool_sizeof(pool, index);
        pool->used_pointers++;
        TM_DEFRAG_index = index;
    }

    // heap can now move left
    pool->heap = Pool_location(pool, index) + Pool_sizeof(pool, index);
    // reset p_* variables
    pool->pointers[0].ptr  = 0;
    pool->pointers[0].size = 1;
    Pool_status_clear(pool, TM_DEFRAG_FULL_IP);
    pool->uheap = 0;  // deallocate sorted filled indexes

#if TM_THREADED
    // Deal with freed values during defrag
    if(Pool_status(pool, TM_ERROR)){
        tmdebug("There was an error!");
        Pool_upool_clear(pool);  // freed values we have are invalid
        Pool_reload_free(pool);
    } else {
        Pool_load_freed_after_defrag(pool);
    }
    Pool_status_clear(pool, TM_ERROR);
#else  // simple
    Pool_upool_clear(pool);
#endif
    return 0;
}


/*---------------------------------------------------------------------------*/
/**     Sorting Functions (internal use)                                     */

#define IS_LESS(pool, v1, v2)  (Pool_location(pool, v1) < Pool_location(pool, v2))
void siftDown(Pool *pool, tm_index_t *a, int16_t start, int16_t count);
#define SWAP(r,s)  do{tm_index_t t=r; r=s; s=t;} while(0)


void heap_sort(Pool *pool, tm_index_t *a, int16_t count){
    int16_t start, end;

    /* heapify */
    for (start = (count-2)/2; start >=0; start--) {
        siftDown(pool, a, start, count);
    }

    for (end=count-1; end > 0; end--) {
        SWAP(a[end], a[0]);
        siftDown(pool, a, 0, end);
    }
}


void siftDown(Pool *pool, tm_index_t *a, int16_t start, int16_t end){
    int16_t root = start;

    while ( root*2+1 < end ) {
        int16_t child = 2*root + 1;
        if ((child + 1 < end) && IS_LESS(pool, a[child], a[child+1])) {
            child += 1;
        }
        if (IS_LESS(pool, a[root], a[child])) {
            SWAP(a[child], a[root]);
            root = child;
        }
        else
            return;
    }
}


void bubble_sort(Pool *pool, tm_index_t *array, tm_index_t len){
    // sort indexes by location
    tm_index_t i, j;
    tm_index_t swap;
    bool workdone = false;
    for (i = 0; i < len; i++){
        workdone = false;
        for (j = 0; j < len - 1; j++){
            if (Pool_location(pool, array[j]) < Pool_location(pool, array[j-1])){
                swap       = array[j];
                array[j]   = array[j-1];
                array[j-1] = swap;
                workdone = true;
            }
        }
        if(! workdone) break; // no values were moved, list is sorted
    }
}


void Pool_filled_sort(Pool *pool){
    tm_index_t index, len = 0;
    // this function completely deallocates -- there are no freed values or anything
    // else in the upool
    Pool_upool_clear(pool);
    Pool_freed_reset(pool);

    // Move used indexes into upool and sort them
    for(index=0; index<TM_MAX_POOL_PTRS; index++){
        if(Pool_filled_bool(pool, index)){
            Pool_upool_set_index(pool, len, index);
            len++;
        }
    }
    if(!len){
        pool->heap = 1;
        return;
    }

    // allocating from ualloc also tracks the length
    tm_assert(Pool_ualloc(pool, len * sizeof(tm_index_t)) < TM_UPOOL_ERROR, "ualloc");

    heap_sort(pool, (tm_index_t *)pool->upool, len);  // kind of a pun, sorting the heap... haha
}


#if TM_THREADED
/*---------------------------------------------------------------------------*/
/**     Threading Helper Functions                                           */

/**         Global                                                           */
void Pool_mark_freed_during_defrag(Pool *pool, tm_index_t index){
    if(Pool_uheap_left(pool) < 2){
        Pool_status_set(pool, TM_ERROR);
        return;
    }
    pool->ustack -= sizeof(tm_index_t);
    *(tm_index_t *)Pool_uvoid(pool, pool->ustack) = index;
}


/**         Local                                                             */
void Pool_append_index_during_defrag(Pool *pool, tm_index_t index){
    // Append an index so it will get defragmented as well
    if(Pool_ualloc(pool, sizeof(tm_index_t)) >= TM_UPOOL_ERROR){
        // Force allocate, desroying freed values
        Pool_status_set(pool, TM_ERROR);
        pool->ustack = TM_UPOOL_SIZE;
        tm_assert(Pool_ualloc(pool, sizeof(tm_index_t)) < TM_UPOOL_ERROR, "ualloc");
    }
    Pool_upool_set_index(pool, TM_DEFRAG_len - 2, index);
}

void Pool_load_freed_after_defrag(Pool *pool){
    while(pool->ustack != TM_UPOOL_SIZE){
        if(!Pool_freed_append(pool, *(tm_index_t *)Pool_uvoid(
                pool, pool->ustack))){
            // unlikely
            tmdebug("load_freed_after_defrag failed");
            Pool_status_set(pool, TM_DEFRAG_FULL);
            return;
        }
        pool->ustack += sizeof(tm_index_t);
    }
}

#endif
