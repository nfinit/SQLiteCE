/*
** SQLite/CE Memory Pool for Small Allocations
**
** This module provides a fixed-size block allocator optimized for small
** allocations (<=64 bytes). It reduces fragmentation and malloc overhead
** by pre-allocating chunks and managing fixed-size blocks.
**
** The pool uses size classes: 8, 16, 24, 32, 48, 64 bytes
** Each size class has its own free list for O(1) allocation/deallocation.
*/

#ifndef MEMPOOL_H
#define MEMPOOL_H

/*
** Maximum allocation size handled by the pool.
** Allocations larger than this go to the system allocator.
*/
#define MEMPOOL_MAX_SIZE 64

/*
** Enable or disable the memory pool.
** Set to 0 to fall back to standard malloc for all allocations.
*/
#ifndef MEMPOOL_ENABLED
#define MEMPOOL_ENABLED 1
#endif

/*
** Initialize the memory pool system.
** Call once at startup before any allocations.
*/
void sqliteMemPoolInit(void);

/*
** Shutdown the memory pool system.
** Frees all pooled memory. Call at shutdown.
*/
void sqliteMemPoolShutdown(void);

/*
** Allocate memory from the pool if size <= MEMPOOL_MAX_SIZE,
** otherwise use system malloc.
*/
void *sqlitePoolMalloc(int n);

/*
** Free memory allocated by sqlitePoolMalloc.
** Automatically detects if memory came from pool or system.
*/
void sqlitePoolFree(void *p);

/*
** Reallocate memory. If p is NULL, equivalent to sqlitePoolMalloc.
** If n is 0, frees p and returns NULL.
*/
void *sqlitePoolRealloc(void *p, int n);

/*
** Get pool statistics for debugging/tuning.
*/
void sqliteMemPoolStats(int *poolHits, int *poolMisses, int *poolBytes);

#endif /* MEMPOOL_H */
