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

/*---------------------------------------------------------------------------*/
/**
 * \brief           perform a full (non-threaded) defragmentation of pool
 */
int8_t            Pool_defrag_full(Pool *pool);


#endif
/** @} */
