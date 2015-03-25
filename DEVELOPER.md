# tinymem Developer Documentation

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
I have two options:
- Make sure to use B when another call requires that much memory (or less)
- Move some later chunk of memory (let’s call it D) that is less than or equal 
    to B into B’s current location.
- Move C backwards so the memory is no longer fragmented

The first option has the problem that the application might never want that much 
memory again, or if there are enough “holes” like B, it won’t have enough memory to grant a request for a large chunk.

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
