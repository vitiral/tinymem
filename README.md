# tinymem: the tiny memory manager

# This library is not supported anymore. Development is taking place on the rust implementation at https://github.com/vitiral/defrag-rs

tinymem is a tiny memory manager written in pure C. It is designed to be a cross
platform tool to manage memory for both embedded and posix environments. It provides
smart, simple and powerful memory management without the need of an operating
system.

Features:
- lightweight
- free and malloc operations that do not result in lost memory or fragmentation
- defragments when necessary

tinymem is pre-alpha, which means that the foundational operation is complete,
but the fully functional library is still being developed.


## How to import / use
- Copy the two files in `src/`
- Copy/create a `tinymem_platform.h` file into your project directory.
    - Templates for various platforms can be found in the `platform/` folder

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

// ... in main loop
tm_thread();    // defragments memory. Maximum time spent per call is ~5us
```

# Features
The features are discussed in `platform/linux/tinymem_platform.h`. On a typical
configuration for a linux system, they are:

- pool size up to 256k bytes
- up to 64,000 pointers
    - 34bits / pointer overhead
    - 18bits / pointer for <= 256 pointers and <= 1024 bytes of
        dynamic memory (with block_size == 4)

Features of tinymem include:
- can run on 16bit or 32bit systems and microcontrollers
    - no OS or other memory manager needed
    - pure ANSI C, so cross platform
    - small memory footprint, allowing for extreme embedibility
- very fast memory allocation and freeing
    - less than 20 clock cycles normally, a typical max of 100.
- extremely fast pointer referencing (2 clock cycles)
- continuous defragmentation
    - only runs when needed
    - runs with `tm_thread` for only short stints of time, letting the program
        continue to run during defragmentation.
    - memory can continue to be allocated and freed during defragmentation
- `tm_index_t` is an 8 or 16bit value (depending on how many indexes there are).
    This can significantly reduce the footprint of references, especially for
    objects like linked lists, where the same reference might have to be
    used multiple times.


## Vision

The creator if tinymem envisions a whole range of standard C library functions that
could not previously exist on embedded system because of memory constraints being
supported for embedded devices.

This library was specifically designed for the [micropython](http://micropython.org/)
project, but is intended to be useful for any embedded C project.

## Development Goals and Timeline
tinymem has the following features finished 
(crossed out items are not done but in the development plan prior to release):
- finish basic functionality
    - ~~implement all features for tinymem.h~~
    - free
        - store freed values to be allocated later
    - allocate
        - use previously freed indexes when allocating
    - defragment
        - full defragmentation that leaves no holes
        - ~~fast defragmentation that never takes more than 5us~~
    - configurability
        - all features can be confitured in a `tinymem_platform.h` file. The default
            one can be found in `platform/`
    - powerful tests
        - test_tinymem function does random allocation/deallocation for a huge range
            of data
        - pool_isvalid constantly checks the validity of the pool during test
    - basic threading support
        - ~~asyncio like threading with event loop~~
        - ~~automatic detection of when to defrag using `tm_thread()`~~
        - ~~each defrag cycle takes < 2us (or user configurable time)~~

- extensive unit test suite
    - robust unit tests for huge range of possibilities
    - ~~basic threading unit tests~~
    - ~~reliability testing over large amounts of time~~
    - ~~testing of additional features like volatile (interrupted) memory access~~

# Operation and Development

#### Developers

Developer documentation can be found at DEVELOPER.md in this folder. Please reference
this for details on the internal workings of tinymem.

#### Issues
If you find any bugs or feature requests, submit them to:

https://github.com/cloudformdesign/tinymem/issues

