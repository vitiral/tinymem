# tinymem Developer Documentation

## Table of Contents
- [Basic Theory](#basic-theory)
- [Defragmentation](#defragmentation)
- [Memory Reuse](#memory-reuse)


## Basic Theory

The first thing to understand is that tinymem is created to be simple. The goal 
is to get a defragmenting memory manager working and worry about performance 
later. Despite this fact, most of the features of tinymem are already designed 
to work relatively fast. This is necessary, as you could easily spend billions 
of CPU cycles defragmenting memory with a poorly designed system. For this 
reason, the initial implementation of tinymem carefully balances simplicity with 
speed, opting for the simpler solution when it is “fast enough.” (at least for 
now)

Before going into the inner workings of tinymem, let’s first go over the goals 
of a mico-memory manager (in order of importance):

- easy to use
- small and lightweight
    - able to handle at least 64k of managed memory (for modern ARM processors)
    - able to scale up through repeated implementation
- reliable and robust over long term usage
    - no lost memory
    - able to defragment memory
    - able to handle calls to obtain large chunks of memory

One of the biggest challenges to creating a memory manager in C (without an 
operating system) is that pointers point to a place in memory, so therefore
we can’t move any data to defragment it. For instance, let’s say I have the 
following:

```
# TOTAL MEMORY
 A       B   C
|X X X X|X X|X X X X X|
```

If I free block **B** it will look like:
```
# TOTAL MEMORY
 A       B   C
|X X X X|- -|X X X X X|
```

**Note:**
> These kind of charts will be used frequently. |X X X| represents full memory
> and |- - -| represents freed memory. **A, B and C** are pointers to memory
> locations

There are three options:
- Make sure to use B when another call requires that much memory (or less)
- Move some later chunk of memory (let’s call it D) that is less than or equal 
    to B into B’s current location.
- Move C backwards so the memory is no longer fragmented

The first option has the problem that the application might never want that much 
memory again, or if there are enough “holes” like B, it won’t have enough memory
to grant a request for a large chunk.

What we would like to do is option 2 or 3 which is to move an already existing 
chunk of memory backwards, eliminating B entirely. However, doing so would screw 
up any pointers that are pointing at the original chunk of memory, as they would 
be invalid. For instance, if I move C, backwards, a pointer that thinks it is 
pointing to the beginning of C is now pointing somewhere in the middle!

If only we could move C without breaking the user’s program, then it should be 
relatively simple to defragment memory... 

This is the first thing you have to understand when trying to understand 
tinymem. tinymem reduces all of the issues with memory defragmentation into a 
single problem: I can’t move memory away from the heap when I want to. 

To solve this problem, tinymem simply circumvents it. It allows you to move 
block C backwards by putting an application layer between the user’s code and 
access to pointer C. See “Basic Use” in the README.md for how this works


## Defragmentation

Being able to fully defragment memory is the primary feature of tinymem that
differentiates it from other memory managers. The current implementation does
it very simply. 

> Note: The code is in `src/tm_pool.c/Pool_defrag_full`

From our example above, say we have memory that looks like this:

```
# TOTAL MEMORY
 A       B   C         D
|X X X X|- -|X X X X X|X X X X X ...
```

The full defragmentation does what everybody wants to do, it simply moves **C**
backwards, eliminating **B**

```
# TOTAL MEMORY
 A       C         D
|X X X X|X X X X X|X X X X X ...
```

The process for doing this is as follows:
- reset the micro-pool (`pool->upool`). [Memory Reuse](#memory-reuse) cannot be done
    while a full defragmentation is happening
- copies all “filled” indexes from pool->pointers to the upool
    - sorts the indexes by location
- moves all data left
    - because all data is now sorted by location, it just needs to be
        moved left (i.e. `C->B` as in the diagram above)
- cleans up
    - moves the heap pointer backwards

### Speed concerns
full_defrag is not intended to be fast. It is intended to be simple,
robust, and (most importantly) it completely defragments the memory
(no holes)


### Threading
It should be possible to make the defrag routine re-entrant. During
a defrag, memory could still be allocated -- but only off the heap.

Freed values can be put on the upool stack. If too many values
are freed during a defrag (unlikely), and the ustack would overflow,
a flag will be set to require a remake of the freed bins


## Memory Reuse

> Most of this code is in `src/tm_freed.c`

The other primary requirement for a memory manager is re-using memory
that has previously been freed. If I allocate a 4byte array, and then
free it, I don’t want to use heap memory if I allocate another 4byte
array

There are several `Pool` data structures that are used to store freed
values

- `pool->upool`: this is where all freed values are stored in
    LinkedIndexArrays
- `pool->freed`: a tm_index array of size TM_FREED_BINS. 

The data structure where freed values are stored is the LinkedIndexArray.
This is similar to a LinkedList, except it is a LinkedArray (of indexes).
This data structure was chosen because it is more memory efficient.

When a value is freed:
- Finds the bin of the value:
    - The size of the freed value is put through a hash that
        corresponds to an index in `pool->freed`
    - this index is a pointer to a LinkedIndexArray
- appends the value into the bin

When a value is requested:
- Finds the bin of the value
- searches through the LinkedIndexArray for the value
- if:
    - found: returns the index of the found value
    - not found: allocates new space on the heap


### Speed concerns
When there are many freed values:
- may take a long time to search through the freed bin for
    allocation
- no impact on `tm_free`

When there are many freed sizes:
- there will be size overlap between values, increasing
    allocation time
- no impact on `tm_free`

To reduce these effects, a [Fast Defragmentation](#fast-defragmentation) method
needs to be implemented that consolidates freed indexes

### LinkedIndexArray for hash table implementation

The original LIA (Linked Index Array) simply has a `tm_index_t prev` and then
14 indexes of freed data, with a NULL index indicating the end of the freed data
(and all prev arrays are always full).

This unfortunately does not handle hash overlaps at all, an issue which I would
like to address

the upool has a constraint that all allocations must be the same size, so any
implementation has to address this issue.

Therefore the LinkedIndexArray will have to be split from the LinkedIndexHeadArray
They will both be the same size, except the HeadArray will use one of the indexes
(the last one) to store the size_t of the indexes.

This means that the head index can theoretically be empty, as it won’t be able to
be deleted until it can move all it’s size into the next lowest index. This will
significnatly complicate an already complicated data structure pop/append routine.

It will, however, significantly simply the finding algorithm, as all arrays will
have a guaranteed size!

This will handle overlaps in the traditional manner and, best of all, has the
following advantages:
- search all freed values for an *arbitrarily sized* index
- print statistics -- can now be easily printed by *size* instead of by *bin*
- significantly speed up the worst case time for alloc.
    unless the user is literally allocating/freeing all *differently sized*
    locations, the most time it will take to find an index will be (S), where
    S is the number of sizes. More commonly it will be CEILING(S/B), where B is
    the number of bins/hashes.

This makes the approach more like a binary tree rather than a linked list. In
addition, it allows the possibility of applying a first-fit approach for certain
cases, which could be very beneficial.

Disadvantages are:
- It could end up taking a HUGE amount of the upool, as each size will take it’s
    own LinkedIndexArray Therefore it may be smart to reduce the number of indexes
    in each array to only 4-6 (instead of 14). This however reduces the number of
    freed indexes allowed by about 10%
- in situations where a large number of sizes are being freed, it will not
    significantly improve perforamnce. In situations like this, it may be
    smart to apply a first-fit policy on the whole system
- There are other possibilities, such as:
    - sorting the indexes by size and doing a faster lookup
    - using a binning method instead (very fast first-fit with some lookup policy)

The other possibilities are certainly being considered, and they have a lot of merit,
especially the binning policy. If the binning policy could be tied to a fast
defragmentation method it would be by far the best method, as it has a worst case
of only a few clock cycles for both allocation and freeing (except for very large
amounts of data (>16k))


# Minor Issues
The two major issues with microcontroller memory management are now resolved:
memory re-use and defragmentation. Now it would be nice to solve
the more minor speed and resource usage issues.

- resource usage
    - cpu usage
    - memory usage
- response time
    - defragmentation time
    - allocation time
    - deallocation time: not an issue currently

<a name=”fast_defrag”/>
## Fast Defragmentation
Goals of fast defragmentation:
- use few resources (be fast and efficient)
- consolodate freed memory
    - this reduces the number of freed pointers, fixing the speed concerns in
        [Memory Reuse](#memory-reuse)
    - allows for larger allocations
- *on average* move data away from the heap
    - also make the heap larger when freed values are next to it
        - this will tend to increase the size of the heap
- *on average* fill holes in memory
    - could be done by moving several small values into a larger hole
    - small values would have to also be next to holes, otherwise
        this would increase fragmentation

The correct implementation of a fast defragmenter has not yet been decided.
It is a work in progress.
