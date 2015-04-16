#include "stdio.h"
#include "tinymem.h"
#include "tm_pool.h"
#include "tm_freed.h"

Pool pool = Pool_declare();

void tm_init(){
    Pool_init(&pool);
}

inline tm_index_t tm_alloc(tm_size_t size){
    return Pool_alloc(&pool, size);
}

inline tm_index_t tm_realloc(tm_index_t index, tm_size_t size){
    return Pool_realloc(&pool, index, size);
}

inline void tm_free(tm_index_t index){
    return Pool_free(&pool, index);
}

inline tm_size_t tm_sizeof(tm_index_t index){
    return Pool_sizeof(&pool, index);
}

inline bool tm_valid(tm_index_t index){
    if(!index || index >= TM_POOL_SIZE) return false;
    return Pool_filled_bool(&pool, index);
}

inline uint8_t tm_status(tm_index_t poolid, uint8_t name){
    return Pool_status(&pool, name);
}

inline void*  tm_void(tm_index_t index){
    return Pool_void(&pool, index);
}

void tm_print_stats(){
    printf("  available=%u\n", Pool_available(&pool));
    printf("  heap left=%u\n", Pool_heap_left(&pool));
    printf("  ptrs left=%u\n", Pool_pointers_left(&pool));
    printf("  status   =%x\n", Pool_status(&pool, 0xFF));
}
