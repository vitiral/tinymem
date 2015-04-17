/**
 * \defgroup tinymem-lib
 * @{
 */

/**
 * \file
 *      tm_defrag.h tinymem memory pool defragmentation functions
 *  \author
 *      2015 Garrett Berg, Garrett@cloudformdesign.com
 */
#ifndef __tm_defrag_h
#define __tm_defrag_h
#include "tm_pool.h"

// use the NULL index and other points for relevant data during defrag
#define TM_DEFRAG_len               ((pool)->uheap / 2)
#define TM_DEFRAG_index             ((pool)->pointers[0].ptr)
#define TM_DEFRAG_temp              ((pool)->pointers[0].size)

/*---------------------------------------------------------------------------*/
/**
 * \brief           perform a full (non-threaded) defragmentation of pool
 */
int8_t            Pool_defrag_full(Pool *pool);

void Pool_append_index_during_defrag(Pool *pool, tm_index_t index);

#endif
/** @} */
