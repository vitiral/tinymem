#ifndef __memtable_h
#define __memtable_h
#include "memptr.h"

#define TM_TYPE_EMPTY   0
#define TM_TYPE_GENERAL 1

typedef tm_table_size uint16_t;
typedef struct {
    unsigned int type   :2;
    unsigned int size   :14;
} tm_type;

#define MAX_TABLE_PTRS 256

/* PtrTable
 * Contains the Ptr and Type information of every table. Main lookup
 * for malloc and free
 *
 * Structure:
 * filled:      [11111100011110000011]  # bool indicating whether pointer in use
 * ptrs:        [*************00000**]  # 0 = NULL, not in use. * = some pointer. Pointers at the end are some form of BigMemTable
 * types:       [                    ]  # 0 = NULL, not in use. * = some pointer. Pointers at the end are some form of BigMemTable
 */
typedef struct {
    uint8_t *start;
    uint8_t *last;
    uint8_t *end;
    uint8_t filled[MAX_TABLE_PTRS];
    tm_type types[MAX_TABLE_PTRS];
    void *ptrs[MAX_TABLE_PTRS];
} TMem_PtrTable;


tm_index TMem_PtrTable_malloc(TMem_PtrTable *table, tm_table_size size);
void TMem_PtrTable_free(TMem_PtrTable *table, tm_table_size size);
