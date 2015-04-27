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
//#include "time.h"

//#define CPU_CLOCKS_PER_SEC      (700000000uL)     // Defined at 700MHz for general

/*---------------------------------------------------------------------------*/
/**
 * \brief           Max time allowed per run of the thread (in microseconds)
*/
//#define TM_THREAD_TIME_US      5

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
#define TM_POOL_SIZE            (0xFFFF - (0xFFFF % 4))

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


#endif
