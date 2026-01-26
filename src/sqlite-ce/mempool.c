/*
** SQLite/CE Memory Pool Implementation
**
** Fixed-size block allocator for small allocations.
** Uses segregated free lists for different size classes.
*/

#include <windows.h>
#include "mempool.h"

#if MEMPOOL_ENABLED

/*
** Size classes for pooled allocations.
** Each class handles allocations up to that size.
*/
#define NUM_SIZE_CLASSES 6
static const int g_sizeClasses[NUM_SIZE_CLASSES] = { 8, 16, 24, 32, 48, 64 };

/*
** Chunk size for each size class.
** Larger chunks mean fewer system allocations but more potential waste.
*/
#define CHUNK_SIZE 4096

/*
** Block header stored at the start of each free block.
** When allocated, this space is available for user data.
*/
typedef struct FreeBlock {
    struct FreeBlock *next;
} FreeBlock;

/*
** Chunk header for memory chunks.
*/
typedef struct Chunk {
    struct Chunk *next;
    int sizeClass;
} Chunk;

/*
** Pool state for each size class.
*/
typedef struct SizePool {
    FreeBlock *freeList;    /* Head of free block list */
    Chunk *chunks;          /* List of allocated chunks */
    int blockSize;          /* Size of blocks in this pool */
    int allocCount;         /* Number of active allocations */
} SizePool;

/*
** Global pool state.
*/
static SizePool g_pools[NUM_SIZE_CLASSES];
static int g_poolHits = 0;
static int g_poolMisses = 0;
static int g_poolBytes = 0;
static int g_initialized = 0;

/*
** Magic number to identify pooled allocations.
** Stored before the user pointer.
*/
#define POOL_MAGIC 0x504F4F4C  /* "POOL" */

/*
** Header prepended to each allocation to track source.
*/
typedef struct AllocHeader {
    int magic;      /* POOL_MAGIC if from pool */
    int sizeClass;  /* Which size class, or -1 for system malloc */
    int allocSize;  /* Actual usable size (for realloc support) */
} AllocHeader;

#define HEADER_SIZE sizeof(AllocHeader)

/*
** Get the size class index for a given size.
** Returns -1 if size exceeds pool maximum.
*/
static int getSizeClass(int n) {
    int i;
    for (i = 0; i < NUM_SIZE_CLASSES; i++) {
        if (n <= g_sizeClasses[i]) return i;
    }
    return -1;
}

/*
** Allocate a new chunk for the given size class.
*/
static int allocateChunk(int sizeClass) {
    SizePool *pool = &g_pools[sizeClass];
    int blockSize = pool->blockSize + HEADER_SIZE;
    int blocksPerChunk = (CHUNK_SIZE - sizeof(Chunk)) / blockSize;
    int chunkSize = sizeof(Chunk) + blocksPerChunk * blockSize;
    Chunk *chunk;
    char *p;
    int i;

    chunk = (Chunk *)LocalAlloc(LMEM_FIXED, chunkSize);
    if (!chunk) return 0;

    chunk->next = pool->chunks;
    chunk->sizeClass = sizeClass;
    pool->chunks = chunk;
    g_poolBytes += chunkSize;

    /* Initialize free list with all blocks in this chunk */
    p = (char *)(chunk + 1);
    for (i = 0; i < blocksPerChunk; i++) {
        FreeBlock *block = (FreeBlock *)(p + HEADER_SIZE);
        block->next = pool->freeList;
        pool->freeList = block;
        p += blockSize;
    }

    return 1;
}

/*
** Initialize the memory pool system.
*/
void sqliteMemPoolInit(void) {
    int i;

    if (g_initialized) return;

    for (i = 0; i < NUM_SIZE_CLASSES; i++) {
        g_pools[i].freeList = NULL;
        g_pools[i].chunks = NULL;
        g_pools[i].blockSize = g_sizeClasses[i];
        g_pools[i].allocCount = 0;
    }

    g_poolHits = 0;
    g_poolMisses = 0;
    g_poolBytes = 0;
    g_initialized = 1;
}

/*
** Shutdown the memory pool system.
*/
void sqliteMemPoolShutdown(void) {
    int i;

    if (!g_initialized) return;

    for (i = 0; i < NUM_SIZE_CLASSES; i++) {
        Chunk *chunk = g_pools[i].chunks;
        while (chunk) {
            Chunk *next = chunk->next;
            LocalFree(chunk);
            chunk = next;
        }
        g_pools[i].freeList = NULL;
        g_pools[i].chunks = NULL;
        g_pools[i].allocCount = 0;
    }

    g_poolBytes = 0;
    g_initialized = 0;
}

/*
** Allocate memory from the pool.
*/
void *sqlitePoolMalloc(int n) {
    int sizeClass;
    SizePool *pool;
    FreeBlock *block;
    AllocHeader *header;

    if (n <= 0) return NULL;

    /* Initialize pool on first use */
    if (!g_initialized) {
        sqliteMemPoolInit();
    }

    sizeClass = getSizeClass(n);

    /* Large allocations go to system malloc */
    if (sizeClass < 0) {
        char *p = (char *)LocalAlloc(LMEM_FIXED, n + HEADER_SIZE);
        if (!p) return NULL;
        header = (AllocHeader *)p;
        header->magic = POOL_MAGIC;
        header->sizeClass = -1;
        header->allocSize = n;
        g_poolMisses++;
        return p + HEADER_SIZE;
    }

    pool = &g_pools[sizeClass];

    /* Get a block from the free list */
    if (!pool->freeList) {
        if (!allocateChunk(sizeClass)) {
            /* Fallback to system malloc */
            char *p = (char *)LocalAlloc(LMEM_FIXED, n + HEADER_SIZE);
            if (!p) return NULL;
            header = (AllocHeader *)p;
            header->magic = POOL_MAGIC;
            header->sizeClass = -1;
            header->allocSize = n;
            g_poolMisses++;
            return p + HEADER_SIZE;
        }
    }

    block = pool->freeList;
    pool->freeList = block->next;
    pool->allocCount++;
    g_poolHits++;

    /* Set up header */
    header = (AllocHeader *)((char *)block - HEADER_SIZE);
    header->magic = POOL_MAGIC;
    header->sizeClass = sizeClass;
    header->allocSize = g_sizeClasses[sizeClass];

    return (void *)block;
}

/*
** Free memory allocated by sqlitePoolMalloc.
*/
void sqlitePoolFree(void *p) {
    AllocHeader *header;
    int sizeClass;

    if (!p) return;

    header = (AllocHeader *)((char *)p - HEADER_SIZE);

    /* Verify this is a pooled allocation */
    if (header->magic != POOL_MAGIC) {
        /* Not our allocation - this shouldn't happen */
        return;
    }

    sizeClass = header->sizeClass;

    if (sizeClass < 0) {
        /* System allocation */
        LocalFree(header);
    } else {
        /* Pool allocation - add to free list */
        SizePool *pool = &g_pools[sizeClass];
        FreeBlock *block = (FreeBlock *)p;
        block->next = pool->freeList;
        pool->freeList = block;
        pool->allocCount--;
    }
}

/*
** Reallocate memory. Since pool blocks are fixed-size, we need to
** allocate new memory, copy, and free the old block.
*/
void *sqlitePoolRealloc(void *p, int n) {
    AllocHeader *header;
    void *pNew;
    int oldSize;
    int copySize;
    char *src, *dst;
    int i;

    /* Handle edge cases */
    if (!p) {
        return sqlitePoolMalloc(n);
    }
    if (n <= 0) {
        sqlitePoolFree(p);
        return NULL;
    }

    header = (AllocHeader *)((char *)p - HEADER_SIZE);

    /* Verify this is our allocation */
    if (header->magic != POOL_MAGIC) {
        /* Not our allocation - shouldn't happen */
        return NULL;
    }

    oldSize = header->allocSize;

    /* If new size fits in current allocation, just return */
    if (n <= oldSize) {
        return p;
    }

    /* Allocate new block */
    pNew = sqlitePoolMalloc(n);
    if (!pNew) return NULL;

    /* Copy old data */
    copySize = (oldSize < n) ? oldSize : n;
    src = (char *)p;
    dst = (char *)pNew;
    for (i = 0; i < copySize; i++) {
        dst[i] = src[i];
    }

    /* Free old block */
    sqlitePoolFree(p);

    return pNew;
}

/*
** Get pool statistics.
*/
void sqliteMemPoolStats(int *poolHits, int *poolMisses, int *poolBytes) {
    if (poolHits) *poolHits = g_poolHits;
    if (poolMisses) *poolMisses = g_poolMisses;
    if (poolBytes) *poolBytes = g_poolBytes;
}

#else /* !MEMPOOL_ENABLED */

/* Stubs when pool is disabled */
void sqliteMemPoolInit(void) {}
void sqliteMemPoolShutdown(void) {}
void *sqlitePoolMalloc(int n) { return LocalAlloc(LMEM_FIXED, n); }
void sqlitePoolFree(void *p) { if (p) LocalFree(p); }
void *sqlitePoolRealloc(void *p, int n) {
    if (!p) return LocalAlloc(LMEM_FIXED, n);
    if (n <= 0) { LocalFree(p); return NULL; }
    return LocalReAlloc(p, n, LMEM_MOVEABLE);
}
void sqliteMemPoolStats(int *h, int *m, int *b) {
    if (h) *h = 0; if (m) *m = 0; if (b) *b = 0;
}

#endif /* MEMPOOL_ENABLED */
