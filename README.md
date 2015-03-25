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
tm_ptr ptr = tmalloc(sizeof(int) * len);// ptr that never changes

// ... later use the memory
int *array = (int *) tm_void(ptr);
for(i=0; i<len; i++){
    array[i] = read_value(); // do some work
}
// ... do more stuff

// ... When or if you want to free the data
tmfree(ptr)  // free the memory again
```


## Development Goals and Timeline
tinymem has the following development goals
- finish basic functionality
    - implement all features for tinymem.h (currently just implemented for Pool)
    - allocate
        - ~~use previously freed values if same size~~
        - indicate that defrag needs to happen if allocation fails
    - ~~free~~
        - ~~store freed values to be allocated later~~
    - ~~defragment~~
        - ~~full defragmentation~~: slow(ish) but reliable method that leaves no “holes”
    - configurability
        - move `#define`s to a `tm_platform.h` file that the user can include to
            set various settings (like table size, etc)
    - full suite of unit tests
        - ~~tests for basic functionality~~
        - tests for basic functionality of tinymem.h
    - basic threading support 

- extensive unit test suite
    - robust unit tests for huge range of possibilities
    - reliability testing over large amounts of time
    - testing of additional features like threading and volatile (interrupted)
        memory access

- full threading support
    - select max amount of time a defrag pass will run (defrag is a thread)
    - speed or break things up as necessary to have every run < 100us

- Implement multiple tables
    - tm_index returned by `tinymem.h` will have first 4 bits be the table.
        this will be auto selected by `tm_void_p`
    - full volatile (interrupt) support
        - `0xFF` as first 4 bits in tm_index will corespond to special
            “volatile” table that has two (potential) indexes that are
            copied before defrag is done on one table
        

## Operation and Development

#### Issues
If you find any bugs or feature requests, submit them to: 

https://github.com/cloudformdesign/tinymem/issues

#### Developers

Developer documentation can be found at DEVELOPER.md in this folder

