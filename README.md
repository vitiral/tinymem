# tinymem: the tiny memory manager

tinymem is a tiny memory manager written in pure C. It is designed to be a cross platform tool to manage memory for both embedded and posix environments. It provides smart, simple and powerful memory management without the need of an operating system.
Features:
- lightweight
- free and malloc operations that do not result in lost memory or fragmentation
- defragments when necessary

tinymem is pre-alpha, which means that the foundational operation is complete, but the fully functional library is still being developed.


## Basic Use
```
// Alocate the memory at some point in your code
int len = 100;
tm_index_t index = tm_alloc(sizeof(int) * len);  // index never changes

// ... later use the memory
int *array = (int *) tm_void(index);  // cast index as integer
for(i=0; i<len; i++){
    array[i] = read_value(); // do some work
}
// ... do more stuff

// ... When or if you want to free the data
tm_free(index)  // free the memory again
```


## Development Goals and Timeline
tinymem has the following features finished 
(crossed out items are not done but in the development plan prior to release):
- finish basic functionality
    - implement all features for tinymem.h
    - allocate
        - use previously freed values if same size
        - ~~switch to binned structure?~~
            - pro: is faster (known execution time), allows for reuse of large memory
                chunks
            - con: will lead to more fragmentation and need another method
                to “clean up.” This may be acceptable though
        - automatically indicate that fragmentation needs to happen under numerous
            “failure” conditions
    - free
        - store freed values to be allocated later
    - defragment
        - full defragmentation: slow(ish) but reliable method that leaves no “holes”
    - configurability
        - all features can be confitured in a `tinymem_platform.h` file. The default
            one can be found in `platform/`
    - full suite of unit tests
        - tests for basic functionality
        - tests for basic functionality of tinymem.h
        - test_t-test1.c test from the interwebs
    - basic threading support
        - asyncio like threading with event loop
        - ~~automatic detection of when to defrag using `tm_thread()`~~
        - ~~each defrag cycle takes < 2us (or user configurable time)~~

- extensive unit test suite
    - robust unit tests for huge range of possibilities
    - basic threading unit tests
    - ~~reliability testing over large amounts of time~~
    - ~~testing of additional features like volatile (interrupted) memory access~~

- ~~Implement multiple tables~~
    - ~~tm_index returned by `tinymem.h` will have first 4 bits be the table.
        this will be auto selected by `tm_void_p`~~
    - ~~full volatile (interrupt) support~~
        - `0xFF` as first 4 bits in tm_index will corespond to special
            “volatile” table that has two (potential) indexes that are
            copied before defrag is done on one table

## Operation and Development

#### Developers

Developer documentation can be found at DEVELOPER.md in this folder. Please reference
this for details on the internal workings of tinymem.

#### Issues
If you find any bugs or feature requests, submit them to: 

https://github.com/cloudformdesign/tinymem/issues

