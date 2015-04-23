#include "stdlib.h"
#include "tm_defrag.h"

/*---------------------------------------------------------------------------*/
/**
 * A Note to Developers
 *
 * This module is designed to be "threaded" -- which just means that
 * Pool_defrag_full (and other defrag methods) do not block for very long --
 * the reference implementation is 5us or less
 *
 * However, many operations inside the defrag methods require much more than 5us.
 * Even searching through * the index array for filled indexes will take more
 * than 5us in most cases
 *
 * Therefore, these functions were designed to be completely re-entrant. But
 * this had to be done without the use of traditional threading
 * (microcontrollers don't have that ability!)
 *
 * This threading programming style may not be familiar to you. If not I would
 * highly recommend reading the documentation and source code for protothreads,
 * to get you started check out this link:
 *      http://dunkels.com/adam/pt/about.html
 *
 * To put it simply, all functions are defined so that they can return, and
 * upon being called again they arrive in the place they were before, with
 * the local variables set to whatever they should be set to.
 *
 * In addition, the rest of the system uses the current progress of the defrag
 * method to mark it's own progress. The progress of defrag can be retrieved
 * with the "TM_DEFRAG_loc" macro. These two macros are used to record
 * the progress and what the system is allowed to do
 *      TM_DEFRAG_CAN_ALLOC             : before this, no allocation can happen
 *      TM_DEFRAG_CAN_ALLOC_FREESPACE   : after this, allocation can happen from
 *                                        inside the defragmentation space
 */

/*---------------------------------------------------------------------------*/
/**     Local Functions                                                      */
void Pool_append_index_during_defrag(Pool *pool, tm_index_t index);
void Pool_load_freed_after_defrag(Pool *pool);

int8_t heap_sort(Pool *pool, tm_index_t *a, int16_t count, int32_t *clocks_left);
int8_t Pool_filled_sort(Pool *pool, int32_t *clocks_left);


#define TM_DEFRAG_internal      ((pool)->freed[1])

/*---------------------------------------------------------------------------*/
/**     Defragmentation Functions                                            */

int8_t Pool_defrag_full(Pool *pool){
    return Pool_defrag_full_wtime(pool, TM_THREAD_TIME_US);
}


/**
 *  Notes:
 *      Returns 0 when done, 1 when not done (in threaded mode)
 *      maxtime is in microseconds
 */
/*---------------------------------------------------------------------------*/
/**
 * \brief           run a full defragmentation run
 * \param pool      pointer to Pool struct
 * \param maxtime   maximum time to run per call
 * \return          0: when done
 *                  1: when need to call again
 *                  *: error
 */
int8_t Pool_defrag_full_wtime(Pool *pool, uint16_t maxtime){
    tm_index_t index;
    int32_t clocks_left = CPU_CLOCKS_PER_US * maxtime;

    // Select the "location" to goto in the function. Remember that this
    // function returns before it is complete, storing it's location in
    // TM_DEFRAG_loc
    if(!Pool_status(pool, TM_DEFRAG_FULL_IP)){
        goto NOT_STARTED;
    }
    index = TM_DEFRAG_loc;
    if(index < 30){
        goto SORTING;
    }
    switch(index){
    case 40:
        // The index has to be loaded because it is a local variable,
        // so it was lost from the previous call
        index = Pool_upool_get_index(pool, 0);
        goto MOVE_FIRST;
    case TM_DEFRAG_CAN_ALLOC_FREESPACE:
        // Again, the index has to be reloaded because it is a local
        // variable and was lost
        index = Pool_upool_get_index(pool, TM_DEFRAG_temp);
        goto MOVE_LOOP;
    default:
        assert(0);
        return 0;
    }

NOT_STARTED:
    // Start the thread, mark the location, get things rolling!
    Pool_status_clear(pool, TM_ERROR);
    Pool_status_set(pool, TM_DEFRAG_FULL_IP);
    Pool_status_clear(pool, TM_DEFRAG_FAST | TM_DEFRAG_FULL);

    TM_DEFRAG_temp = 0;     // initialize for sorting
    TM_DEFRAG_index = 0;    // indicate no valid size

    TM_DEFRAG_loc = 10;

SORTING:
    // The Pool_filled_sort function handles it's own re-entrancy
    // Like this function, it returns 0 when it is complete
    if(Pool_filled_sort(pool, &clocks_left)){
        return 1;
    }

    if(!TM_DEFRAG_len){
        Pool_status_clear(pool, TM_DEFRAG_FULL_IP);
        return 0;  // there were no filled indexes, done
    }

    TM_DEFRAG_loc = 40;
    index = Pool_upool_get_index(pool, 0);
    clocks_left -= 8 + Pool_sizeof(pool, index) / TM_ALIGN_BYTES;
    if(clocks_left <= 0) return 1;

MOVE_FIRST:
    // we now have sorted indexes by location. We just need to
    // move all memory to the left
    // First memory can be moved to loc 1
    memmove(Pool_location_void(pool, 0), Pool_void(pool, index), Pool_sizeof(pool, index));
    Pool_location_set(pool, index, 0);

    // Set everything up for the loop and run it
    TM_DEFRAG_index = index;
    TM_DEFRAG_loc = TM_DEFRAG_CAN_ALLOC_FREESPACE;
    for(TM_DEFRAG_temp=1; TM_DEFRAG_temp<TM_DEFRAG_len; TM_DEFRAG_temp++){
        index = Pool_upool_get_index(pool, TM_DEFRAG_temp);
        // 13 is an estimate for how many clock cycles the other operations take
        clocks_left -= 13 + Pool_sizeof(pool, index) / TM_ALIGN_BYTES;
        if(clocks_left <= 0) return 1;

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

    // reset p_* variables (they were used as temporary variables)
    pool->pointers[0].ptr  = 0;
    pool->pointers[0].size = 1;
    Pool_status_clear(pool, TM_DEFRAG_FULL_IP);
    pool->uheap = 0;  // deallocate sorted filled indexes
    Pool_freed_array_reset(pool);  // The array was used for temporary variables

    // TODO: do one more return before this?

    // Deal with values that were freed during defrag
    if(Pool_status(pool, TM_ERROR)){
        tmdebug("There was an error!");
        Pool_upool_clear(pool);  // freed values we have are invalid
        Pool_reload_free(pool);
    } else {
        Pool_load_freed_after_defrag(pool);
    }
    Pool_status_clear(pool, TM_ERROR);
    return 0;
}


/*---------------------------------------------------------------------------*/
/**     Sorting Functions (internal use)                                     */

#define IS_LESS(pool, v1, v2)  (Pool_location(pool, v1) < Pool_location(pool, v2))
void siftDown(Pool *pool, tm_index_t *a, int16_t start, int16_t count);
#define SWAP(r,s)  do{tm_index_t t=r; r=s; s=t;} while(0)


/*---------------------------------------------------------------------------*/
/**
 * \brief           Standard heap sort made to be re-entrant
 *                  http://rosettacode.org/wiki/Sorting_algorithms/Heapsort#C
 */
int8_t heap_sort(Pool *pool, tm_index_t *a, int16_t count, int32_t *clocks_left){
    int32_t start = clock();

    // TODO: this uses clock() because I do not know a way to predict how
    //      long a heap sort operation will take. Is this possible?

    switch(TM_DEFRAG_loc){
case TM_DEFRAG_CAN_ALLOC:
    /* heapify */
    for (TM_DEFRAG_temp = (int16_t)(count-2)/2, TM_DEFRAG_loc = TM_DEFRAG_CAN_ALLOC + 1;
            TM_DEFRAG_itemp >= 0;
            TM_DEFRAG_temp = TM_DEFRAG_itemp - 1) {
        if(*clocks_left < (clock() - start) * CPU_CLOCKS_PER_CLOCK) return 1;
case TM_DEFRAG_CAN_ALLOC + 1:
        siftDown(pool, a, TM_DEFRAG_itemp, count);
    }

    for (TM_DEFRAG_temp=(int16_t)(count-1), TM_DEFRAG_loc = TM_DEFRAG_CAN_ALLOC + 2;
            TM_DEFRAG_itemp > 0;
            TM_DEFRAG_temp = TM_DEFRAG_itemp - 1) {
        if(*clocks_left < (clock() - start) * CPU_CLOCKS_PER_CLOCK) return 1;
case TM_DEFRAG_CAN_ALLOC + 2:
        SWAP(a[TM_DEFRAG_itemp], a[0]);
        siftDown(pool, a, 0, TM_DEFRAG_itemp);
    }
    *clocks_left -= (clock() - start) * CPU_CLOCKS_PER_CLOCK;
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


/*---------------------------------------------------------------------------*/
/**
 * \brief           Sort all filled indexes in the pool (re-entrant)
 *                  - move indexes from filled->upool   (allocation disabled )
 *                  - sort indexes                      (heap only allocation)
 */
int8_t Pool_filled_sort(Pool *pool, int32_t *clocks_left){
    int32_t start;

    switch(TM_DEFRAG_loc){
case 10:
    // this function completely deallocates -- there are no freed values or anything
    // else in the upool
    start = clock();

    Pool_upool_clear(pool);
    Pool_freed_reset(pool);

    start = (clock() - start);
    *clocks_left -= start * CPU_CLOCKS_PER_CLOCK;

    TM_DEFRAG_loc = 11;
    // Move used indexes into upool and sort them
    for(TM_DEFRAG_temp=0; TM_DEFRAG_temp<TM_MAX_POOL_PTRS; TM_DEFRAG_temp++){
        *clocks_left -= 4;
        if(*clocks_left < 0) return 1;

case 11:
        if(Pool_filled_bool(pool, TM_DEFRAG_temp)){
            *clocks_left -= 8;
            Pool_upool_set_index(pool, TM_DEFRAG_len, TM_DEFRAG_temp);
            Pool_ualloc(pool, sizeof(tm_index_t));
        }
    }
    if(!TM_DEFRAG_len){
        pool->heap = 0;
        return 0;
    }
    // Use index instead of length, as the uheap can change during sorting
    TM_DEFRAG_index = TM_DEFRAG_len;
    TM_DEFRAG_loc = TM_DEFRAG_CAN_ALLOC;        // Allow allocation (on heap)

default:
    return heap_sort(pool, (tm_index_t *)pool->upool, TM_DEFRAG_index, clocks_left);
    }
}


/*---------------------------------------------------------------------------*/
/**     Threading Helper Functions                                           */
/**       -  Global                                                          */

/*---------------------------------------------------------------------------*/
/**
 * \brief           Mark a freed value as freed while a defrag is happening
 *
 *                  This function is used by other files to indicate
 *                  that a value has been freed while a defrag is in progress.
 *
 *                  Because a defrag clears the upool and freed arrays,
 *                  the normal methods for indicating that values have been
 *                  freed must be disabled.
 *
 *                  This function can fail. When it fails, it sets TM_ERROR
 *                  If this happens, Pool_reload_freed is called at the end
 *                  of defrag
 *
 *                  Other functions actually set the bits and decrement
 *                  counters, etc
 */
void Pool_mark_freed_during_defrag(Pool *pool, tm_index_t index){
    if(Pool_uheap_left(pool) < 2){
        Pool_status_set(pool, TM_ERROR);
        return;
    }
    pool->ustack -= sizeof(tm_index_t);
    *(tm_index_t *)Pool_uvoid(pool, pool->ustack) = index;
}

/**
 * \brief           This appends the index onto the current defrag route.
 *
 *                  The index MUST have been allocated off of the heap,
 *                  therefore it is already sorted.
 *
 *                  The defragmentation routine will then defragment the
 *                  index with the rest of the data
 */
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

/**
 * \brief           Get the amount of data free inside the defrag process
 *
 *                  When a defrag is in progress the data looks like this:
 *
 *                  [...X X X X*- - - - - - -$X X X - - X X X X X ...]
 *
 *                  where * is the current location of the defrag method,
 *                  and $ is the location of the next data to be moved
 *
 *                  As you can see, there is empty space between * and $
 *
 *                  This method allows you to detect how much, and then
 *                  use it.
 */
inline tm_size_t Pool_space_free_in_defrag(Pool *pool){
    if(TM_DEFRAG_loc < TM_DEFRAG_CAN_ALLOC_FREESPACE) return 0;
    return (Pool_location(pool, Pool_upool_get_index(pool, TM_DEFRAG_temp)) -
     (Pool_location(pool, TM_DEFRAG_index) + Pool_sizeof(pool, TM_DEFRAG_index)));
}

/**         Local                                                             */
/**
 * \brief           After a defrag, the freed values must be loaded from the
 *                  uheap stack
 */
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
