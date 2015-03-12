
#ifndef __memptr_h
#define __memptr_h
#include "stdint.h"
#define TM_INDEX_LEN (8)
#define TM_INDEX_MAX (2 ^ TM_INDEX_LEN - 2)

typedef tm_index uint8_t;


/* tm_ptr structure
 * Main access to memory table pointers.
 */
typedef struct {
    tm_index table;
    tm_index index;
} tm_ptr;

#endif
