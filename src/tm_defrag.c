#include "stdlib.h"
#include "tm_defrag.h"


/**     Local Functions                                                      */
#if TM_THREADED
void Pool_append_index_during_defrag(Pool *pool, tm_index_t index);
void Pool_load_freed_after_defrag(Pool *pool);
#endif

int8_t heap_sort(Pool *pool, tm_index_t *a, int16_t count);
int8_t Pool_filled_sort(Pool *pool, int32_t *clocks_left);

#define TM_DEFRAG_CAN_ALLOC    100


/*---------------------------------------------------------------------------*/
/**     Defragmentation Functions                                            */

int8_t Pool_defrag_full(Pool *pool){
    return Pool_defrag_full_wtime(pool, TM_THREAD_TIME_US);
}


/**
 *  Notes:
 *      Returns 0 when done, 1 when not done (in threaded mode)
 *      maxtime is in 100's of nanoseconds
 */
int8_t Pool_defrag_full_wtime(Pool *pool, uint16_t maxtime){
    tm_index_t index;
#if TM_THREADED
    int32_t clocks_left = CPU_CLOCKS_PER_US * maxtime;
#endif

    if(!Pool_status(pool, TM_DEFRAG_FULL_IP)){
        goto NOT_STARTED;
    }
    index = TM_DEFRAG_loc;
    if(index < 30){
        goto SORTING;
    }
    switch(index){
    case 40:
        index = Pool_upool_get_index(pool, 0);
        goto MOVE_FIRST;
    case TM_DEFRAG_CAN_ALLOC:
        index = Pool_upool_get_index(pool, TM_DEFRAG_temp);
        goto MOVE_LOOP;
    default:
        assert(0);
        return 0;
    }

NOT_STARTED:
    Pool_status_clear(pool, TM_ERROR);
    Pool_status_set(pool, TM_DEFRAG_FULL_IP);
    Pool_status_clear(pool, TM_DEFRAG_FAST | TM_DEFRAG_FULL);

    TM_DEFRAG_temp = 0;     // initialize for sorting
    TM_DEFRAG_index = 0;    // indicate no valid size

    TM_DEFRAG_loc = 10;

SORTING:
    if(Pool_filled_sort(pool, &clocks_left)){
        return 1;
    }

    if(!TM_DEFRAG_len){
        Pool_status_clear(pool, TM_DEFRAG_FULL_IP);
        return 0;  // there were no filled indexes, done
    }

    TM_DEFRAG_loc = 40;
    index = Pool_upool_get_index(pool, 0);
#if TM_THREADED
    clocks_left -= 8 + Pool_sizeof(pool, index) / TM_WORD_SIZE;
    if(clocks_left <= 0) return 1;
#endif

MOVE_FIRST:
    // we now have sorted indexes by location. We just need to
    // move all memory to the left
    // First memory can be moved to loc 1
    memmove(Pool_location_void(pool, 1), Pool_void(pool, index), Pool_sizeof(pool, index));
    Pool_location_set(pool, index, 1);

    TM_DEFRAG_index = index;
    TM_DEFRAG_loc = TM_DEFRAG_CAN_ALLOC;
    // rest of memory is packed
    for(TM_DEFRAG_temp=1; TM_DEFRAG_temp<TM_DEFRAG_len; TM_DEFRAG_temp++){
        index = Pool_upool_get_index(pool, TM_DEFRAG_temp);
#if TM_THREADED
        // 13 is an estimate for how many clock cycles the other operations take
        clocks_left -= 13 + Pool_sizeof(pool, index) / TM_WORD_SIZE;
        if(clocks_left <= 0) return 1;
#endif
MOVE_LOOP:
        memmove(
            Pool_void(pool, TM_DEFRAG_index) + Pool_sizeof(pool, TM_DEFRAG_index),
            Pool_void(pool, index),
            Pool_sizeof(pool, index)
        );
        Pool_location_set(pool, index,
                          Pool_location(pool, TM_DEFRAG_index) +
                          Pool_sizeof(pool, TM_DEFRAG_index));
        TM_DEFRAG_index = index;
    }

    // heap can now move left
    pool->heap = Pool_location(pool, index) + Pool_sizeof(pool, index);
    // reset p_* variables
    pool->pointers[0].ptr  = 0;
    pool->pointers[0].size = 1;
    Pool_status_clear(pool, TM_DEFRAG_FULL_IP);
    pool->uheap = 0;  // deallocate sorted filled indexes
    Pool_freed_array_reset(pool);

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


int8_t heap_sort(Pool *pool, tm_index_t *a, int16_t count){
    switch(TM_DEFRAG_loc){
case 15:
    /* heapify */
    TM_DEFRAG_loc = 16;
    for (TM_DEFRAG_temp = (int16_t)(count-2)/2; TM_DEFRAG_itemp >= 0;
            TM_DEFRAG_temp = TM_DEFRAG_itemp - 1) {
case 16:
        siftDown(pool, a, TM_DEFRAG_itemp, count);
    }
    TM_DEFRAG_loc = 17;

    for (TM_DEFRAG_temp=(int16_t)(count-1); TM_DEFRAG_itemp > 0;
            TM_DEFRAG_temp = TM_DEFRAG_itemp - 1) {
case 17:
        SWAP(a[TM_DEFRAG_itemp], a[0]);
        siftDown(pool, a, 0, TM_DEFRAG_itemp);
    }
    return 0;
default:
    assert(0);
    return 0;
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


int8_t Pool_filled_sort(Pool *pool, int32_t *clocks_left){
#if TM_THREADED
    int32_t start;
#endif

    switch(TM_DEFRAG_loc){
case 10:
    // this function completely deallocates -- there are no freed values or anything
    // else in the upool
#if TM_THREADED
    start = clock();
#endif

    Pool_upool_clear(pool);
    Pool_freed_reset(pool);

#if TM_THREADED
    start = (clock() - start);
    *clocks_left -= start * CPU_CLOCKS_PER_CLOCK;
#endif

    TM_DEFRAG_loc = 11;
    // Move used indexes into upool and sort them
    for(TM_DEFRAG_temp=0; TM_DEFRAG_temp<TM_MAX_POOL_PTRS; TM_DEFRAG_temp++){
#if TM_THREADED
        *clocks_left -= 4;
        if(*clocks_left < 0) return 1;
#endif
case 11:
        if(Pool_filled_bool(pool, TM_DEFRAG_temp)){
            *clocks_left -= 8;
            Pool_upool_set_index(pool, TM_DEFRAG_len, TM_DEFRAG_temp);
            Pool_ualloc(pool, sizeof(tm_index_t));
        }
    }
    if(!TM_DEFRAG_len){
        pool->heap = 1;
        return 0;
    }

    TM_DEFRAG_loc = 15;
default:
    // kind of a pun, sorting the heap... haha
    return heap_sort(pool, (tm_index_t *)pool->upool, TM_DEFRAG_len);
    }
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

inline tm_size_t Pool_space_free_in_defrag(Pool *pool){
    if(TM_DEFRAG_loc < TM_DEFRAG_CAN_ALLOC) return 0;
    return (Pool_location(pool, Pool_upool_get_index(pool, TM_DEFRAG_temp)) -
     (Pool_location(pool, TM_DEFRAG_index) + Pool_sizeof(pool, TM_DEFRAG_index)));
}

/**         Local                                                             */
void Pool_load_freed_after_defrag(Pool *pool){
    // move ustack to local as Pool_freed_append uses it
    tm_index_t ustack = pool->ustack;
    pool->ustack = TM_UPOOL_SIZE;
    while(ustack != TM_UPOOL_SIZE){
        if(!Pool_freed_append(pool, *(tm_index_t *)Pool_uvoid(
                pool, ustack))){
            // unlikely
            Pool_status_set(pool, TM_DEFRAG_FULL);
            return;
        }
        ustack += sizeof(tm_index_t);
    }
}

#endif
