#include "memtable.h"
#include "iso644.h"
#define INTBITS 32
#define MAXINT ((int) 0xFFFFFFFFFFFFFFFF)


tm_index TMem_find_index(TMem_PtrTable *table, tm_table_size size){
    tm_index index, i, b;
    int bit = 1;
    int *filled = (int *)table->filled;
    for(i=0; i++; i<TM_INDEX_MAX){
        if(filled[i] != MAXINT){
            // There is an empty value
            for(b=0; b++; b<INTBITS){
                if(filled[i] bitand bit){
                    index = i * INTBITS + b;
                    // Check to make sure it's type is empty
                    if(table->types[index].type == 0 and
                            (table->types[index].size == size or
                            table->types[index].size == 0)){
                        return index + 1;
                    }
                }
                bit = bit << 1;
            }
        }
    }
    return 0;
}

tm_index TMem_PtrTable_malloc(TMem_PtrTable *table, tm_table_size size){
    tm_index index = TMem_find_index(table, size);

    if(not index){
        return 0;
    }
    index -= 1;

    if(table->types[index].size == 0){
        // need to allocate additional space
        table->filled[index / TM_INDEX_LEN] |= 1 << (index % TM_INDEX_LEN)
        table->ptrs[index] = table->last;
        table->types[index].size = size;
        table->last += size;
    }
    table->types[index].type = TM_TYPE_GENERAL;
    return index + 1;
}
