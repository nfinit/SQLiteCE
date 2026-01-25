/*
** SQLiteCEdit - String Pool Allocator
**
** A simple chunked allocator for string data that reduces malloc overhead
** by allocating large chunks and sub-allocating from them. This dramatically
** reduces fragmentation and allocation count for query results.
**
** Usage:
**   StrPool *pool = StrPoolCreate(64*1024);  // 64KB chunks
**   char *s1 = StrPoolAlloc(pool, 100);      // allocate 100 bytes
**   char *s2 = StrPoolDup(pool, "hello");    // duplicate string
**   StrPoolReset(pool);                       // free all strings but keep chunks
**   StrPoolDestroy(pool);                     // free everything
*/

#ifndef STRPOOL_H
#define STRPOOL_H

#include <windows.h>

/* String pool chunk - linked list of memory blocks */
typedef struct StrPoolChunk {
    struct StrPoolChunk *next;  /* Next chunk in list */
    int size;                    /* Total size of this chunk */
    int used;                    /* Bytes used in this chunk */
    char data[1];                /* Start of data (flexible array) */
} StrPoolChunk;

/* String pool - manages a list of chunks */
typedef struct StrPool {
    StrPoolChunk *first;        /* First chunk (currently active) */
    StrPoolChunk *current;      /* Current chunk for allocation */
    int chunkSize;              /* Default chunk size */
    int totalAllocated;         /* Total bytes allocated */
    int totalUsed;              /* Total bytes used */
    int allocCount;             /* Number of allocations served */
} StrPool;

/*
** Create a new string pool with the given default chunk size.
** Returns NULL on allocation failure.
*/
StrPool *StrPoolCreate(int chunkSize);

/*
** Destroy a string pool, freeing all memory.
*/
void StrPoolDestroy(StrPool *pool);

/*
** Reset a string pool - marks all memory as free but keeps allocated chunks.
** This is much faster than Destroy+Create for reusing pools.
*/
void StrPoolReset(StrPool *pool);

/*
** Allocate n bytes from the pool. Returns NULL on failure.
** Memory is NOT zero-initialized for speed.
*/
char *StrPoolAlloc(StrPool *pool, int n);

/*
** Allocate n bytes from the pool and zero-initialize.
*/
char *StrPoolAllocZero(StrPool *pool, int n);

/*
** Duplicate a string into the pool. Returns NULL on failure.
*/
char *StrPoolDup(StrPool *pool, const char *s);

/*
** Duplicate n bytes of a string into the pool (adds null terminator).
*/
char *StrPoolNDup(StrPool *pool, const char *s, int n);

/*
** Get pool statistics for debugging/tuning.
*/
void StrPoolStats(StrPool *pool, int *totalAlloc, int *totalUsed, int *allocCount);

#endif /* STRPOOL_H */
