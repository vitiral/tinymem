#ifndef __tinymem_types_h
#define __tinymem_types_h

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
//#undef NDEBUG
#include "tinymem_platform.h"

//#ifndef NDEBUG
//#define tm_debug(...)      do{printf("[DEBUG](%s,%u):", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n");}while(0)
//#endif


/*---------------------------------------------------------------------------*/
/**
 * \brief           status bitcodes
 */
#define TM_DEFRAG_FULL      (1<<0)  // a full defrag has been requested
#define TM_DEFRAG_FAST      (1<<1)  // a fast defrag has been requested
#define TM_DEFRAG_FULL_IP   (1<<2)  // A defrag is in progress
#define TM_DEFRAG_FAST_IP   (1<<3)  // A defrag is in progress
#define TM_MOVING           (1<<4)  // the memory manager is currently moving a block
#define TM_ERROR            (1<<7)  // a memory manager internal error occurred
#define TM_DEFRAG_IP        (TM_DEFRAG_FULL_IP | TM_DEFRAG_FAST_IP)             // defrag is in progress
#define TM_ANY_DEFRAG       (TM_DEFRAG_FULL | TM_DEFRAG_FAST | TM_DEFRAG_IP)    // some defrag has been requested


typedef uint16_t tm_index_t;
typedef uint16_t tm_size_t;
typedef uint8_t tm_status_type;

#endif
/** @} */
