#ifndef __tinymem_platform_h
#define __tinymem_platform_h

/*---------------------------------------------------------------------------*/
/**
 * \brief           Time module
 *                  This file must include the standard c time module,
 *                  or it must define the following standard C symbols:
 *                      clock()         -- returns total clock cycles
 *                      CLOCKS_PER_SEC  -- macro for number of clocks/second
 *                          returned by clock()
 */
#include "time.h"

#define CPU_CLOCKS_PER_SEC      (700000000uL)     // Defined at 700MHz for general

/*---------------------------------------------------------------------------*/
/**
 * \brief           Max time allowed per run of the thread (in microseconds)
*/
#define TM_THREAD_TIME_US      2

/*---------------------------------------------------------------------------*/
/**
 * \brief           Which threaded implementation?
 *                  Options:
 *                      0   -- No threading
 *                      >=1 -- Simple threading. Simply make calls to tm_thread
 *                                  in your main loop
 */
#define TM_THREADED     1
/*---------------------------------------------------------------------------*/
/**
 * \brief           number of memory pools to create
 *                  TODO: not yet used
 */
#define TM_NUM_POOLS            (1)
/*---------------------------------------------------------------------------*/
/**
 * \brief           size of memory pool
 *                  This is the maximum amount of memory that can be
 *                  allocated in a memory pool
 */
#define TM_POOL_SIZE            ((uint16_t)(0xFFFFFFFF - 1))
/*---------------------------------------------------------------------------*/
/**
 * \brief           Maximum number of pointers that can be allocated
 *
 *                  This is the maximum number of pointers that a system can
 *                  allocate. For instance, if TM_MAX_POOL_PTRS == 3 and you
 *                  allocated an integer, a 1000 character array and
 *                  a 30 byte struct, then you would be unable to allocate
 *                  any more data
 *
 *                  On memory constrained systems this number should be low,
 *                  possibly 20-25 or less
 *
 *                  This value is highly dependent on implementation details,
 *                  however, it is advisable to leave a large headroom
 *
 *                  Each pool_ptr uses 50 bits
 */
#define TM_MAX_POOL_PTRS        (2048)
/*---------------------------------------------------------------------------*/
/**
 * \brief           number of freed bins
 *                  Each bin takes up sizeof(tm_index) bytes (normally 2)
 *                  Increasing the number of bins increases the speed by
 *                  which freed values can be found, especially on systems
 *                  that are using many different sizes of data
 *
 *                  For memory constrained systems (<5kB of RAM), this value
 *                  should == 1
 */
#define TM_FREED_BINS           (16)
/*---------------------------------------------------------------------------*/
/**
 * \brief           Size that each bin is stored in upool
 *
 *                  It is rare that this value should be changed. It is the
 *                  number of elements in the LinkedIndexArray that pool.freed
 *                  uses
 *
 *                  TODO:
 *                  For memory constrained systems, TM_FREED_BINS should == 1
 *                  and this value is irrelevant
 */
#define TM_FREED_BINSIZE        (14)


#endif
