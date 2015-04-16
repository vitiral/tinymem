#include "tm_defrag.h"

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


/*---------------------------------------------------------------------------*/
/**     Defragmentation Functions                                            */

bool Pool_defrag_full(Pool *pool){
    tm_index_t prev_index, index, i;
    tm_index_t len = 0;
    Pool_status_set(pool, TM_DEFRAG_IP);
    Pool_status_clear(pool, TM_DEFRAG | TM_DEFRAG_FULL);

    pool->used_bytes = 1;
    pool->used_pointers = 1;

    // this function completely deallocates -- there are no freed values or anything
    // else in the upool
    Pool_upool_clear(pool);

    // Move used indexes into upool andand sort them
    for(index=0; index<TM_MAX_POOL_PTRS; index++){
        if(Pool_filled_bool(pool, index)){
            Pool_upool_set_index(pool, len, index);
            len++;
        }
    }
    if(! len){
        pool->heap = 1;
        return false;
    }

    heap_sort(pool, (tm_index_t *)pool->upool, len);  // kind of a pun, sorting the heap... haha

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

    prev_index = index;
    // rest of memory is packeduse2
    for(i=1; i<len; i++){
        index = Pool_upool_get_index(pool, i);
        memmove(
            Pool_void(pool, prev_index) + Pool_sizeof(pool, prev_index),
            Pool_void(pool, index),
            Pool_sizeof(pool, index)
        );
        Pool_location_set(pool, index, Pool_location(pool, prev_index) + Pool_sizeof(pool, prev_index));
        pool->used_bytes += Pool_sizeof(pool, index);
        pool->used_pointers++;
        prev_index = index;
    }

    // heap can now move left
    pool->heap = Pool_location(pool, index) + Pool_sizeof(pool, index);
    Pool_status_clear(pool, TM_DEFRAG_IP);
    return true;
}
