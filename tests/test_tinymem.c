#include "minunit.h"
#include "tinymem_platform.h"
#include "tinymem.h"
#include "tm_types.h"

/*#define tmdebug(...)*/

#define TABLE_STANDIN NULL

#define HASH_PRIME 1677619
#define MAXPTRS TM_MAX_POOL_PTRS - 1

bool alloc_index(tm_index *indexes, tm_index index, tm_size size);


uint8_t index_hash(tm_index value, tm_size i){
    uint32_t h = (value + i) * HASH_PRIME;
    return ((h>>16) ^ (h & 0xFFFF));
}

bool check_sizes(tm_index *indexes, tm_index len, tm_size maxlen){
    tm_index i;
    for(i=0; i<len; i++){
        if(indexes[i]){
            if(!tm_valid(indexes[i])){
                tmdebug("CS:Fake Valid data");
                return false;
            }
            if(tm_sizeof(indexes[i]) != (i % maxlen + 1)){
                tmdebug("CS:Index wrong size. i=%u, expected=%u, size=%u",
                        i, i % maxlen + 1, tm_sizeof(indexes[i]));
                return false;
            }
        }
    }
    return true;
}

bool check_indexes(tm_index *indexes, tm_index len){
    // All indexes where index!=0 should be equal to their hash
    tm_index i, j;
    tm_size size;
    uint8_t *data;
    for(i=0; i<len; i++){
        if(!indexes[i]) continue;
        if(!tm_valid(indexes[i])){
            tmdebug("Fake Valid data");
            return 0;
        }
        size = tm_sizeof(indexes[i]);
        data = tm_uint8_p(indexes[i]);
        for(j=0; j<size; j++){
            if(data[j] != index_hash(i, j)){
                tmdebug("Index check failed");
                return false;
            }
        }
    }
    return true;
}

bool fill_indexes(tm_index *indexes, tm_index len, tm_size maxlen){
    // Allocate indexex of size index % mod + 1
    tm_index i;
    tm_index filled = 0;
    for(i=0; i<len; i++){
        if(!indexes[i]){
            if(!alloc_index(indexes, i, i % maxlen + 1)){
                tmdebug("Failed to fill index %u\n", i);
                tm_print_stats();
                return false;
            }
            if(!check_indexes(indexes, len)){
                tmdebug("check failed after alloc");
                return false;
            }
            if(!check_sizes(indexes, len, maxlen)){
                tmdebug("check sizes failed after alloc");
                return false;
            }
            filled++;
        }
    }
    return true;
}

bool alloc_index(tm_index *indexes, tm_index index, tm_size size){
    tm_size i;
    uint8_t *data;
    indexes[index] = tm_alloc(size);
    if(!indexes[index]) return false;
    if(tm_sizeof(indexes[index]) != size){
        tmdebug("alloc size wrong size: %u != %u", tm_sizeof(indexes[index]), size);
        return false;
    }
    data = tm_uint8_p(indexes[index]);
    for(i=0; i<size; i++){
        data[i] = index_hash(index, i);
    }
    return true;
}

bool free_index(tm_index *indexes, tm_index index){
    tm_free(indexes[index]);
    indexes[index] = 0;
    return true;
}

char *test_basic(){
    // first allocate and free to the full extent possible without requiring a defrag
    const tm_size maxlen = (TM_POOL_SIZE / TM_MAX_POOL_PTRS) * 2;
    tm_size i;
    tm_index indexes[MAXPTRS] = {0};
    mu_assert(fill_indexes(indexes, MAXPTRS, maxlen), "filled");

    // Deallocate every other one (backwards) and then refill it
    for(i=MAXPTRS-1; i<MAXPTRS; i-=2){
        free_index(indexes, i);
        mu_assert(!tm_status(0, TM_ANY_DEFRAG), "no defrag request");
    }

    mu_assert(fill_indexes(indexes, MAXPTRS, maxlen), "filled 2");


    // Dealocate every other data again, then request a large amount of memory.
    // This will require a full defrag to happen first
    // and fill it
    for(i=MAXPTRS-2; i<MAXPTRS; i-=2){
        free_index(indexes, i);
        mu_assert(!tm_status(0, TM_ANY_DEFRAG), "no defrag request");
    }
    i+=2;
    mu_assert(alloc_index(indexes, i, TM_POOL_SIZE / 3), "allocate large");
    // TODO: simple. In threaded make sure full defrag is requested and fulfill it
    mu_assert(check_indexes(indexes, MAXPTRS), "check indexes");
    mu_assert(!check_sizes(indexes, MAXPTRS, maxlen), "check sizes");

    return NULL;
}

char *all_tests(){
    mu_suite_start();

    mu_run_test(test_basic);
    return NULL;
}

RUN_TESTS(all_tests);
