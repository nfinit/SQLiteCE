/*
** SQLiteCEdit - String Pool Allocator Implementation
**
** Optimized for query result storage where many small strings are
** allocated and then freed all at once.
*/

#include "strpool.h"

/* Minimum chunk size - anything smaller is wasteful */
#define MIN_CHUNK_SIZE 4096

/* Alignment for allocations (4 bytes for 32-bit CE) */
#define POOL_ALIGN 4
#define ALIGN_UP(n) (((n) + POOL_ALIGN - 1) & ~(POOL_ALIGN - 1))

/*
** Create a new chunk of the specified size.
*/
static StrPoolChunk *CreateChunk(int size) {
    StrPoolChunk *chunk;
    int totalSize = sizeof(StrPoolChunk) - 1 + size;

    chunk = (StrPoolChunk *)LocalAlloc(LMEM_FIXED, totalSize);
    if (!chunk) return NULL;

    chunk->next = NULL;
    chunk->size = size;
    chunk->used = 0;

    return chunk;
}

/*
** Create a new string pool.
*/
StrPool *StrPoolCreate(int chunkSize) {
    StrPool *pool;

    if (chunkSize < MIN_CHUNK_SIZE) chunkSize = MIN_CHUNK_SIZE;

    pool = (StrPool *)LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, sizeof(StrPool));
    if (!pool) return NULL;

    pool->chunkSize = chunkSize;
    pool->first = CreateChunk(chunkSize);
    if (!pool->first) {
        LocalFree(pool);
        return NULL;
    }

    pool->current = pool->first;
    pool->totalAllocated = chunkSize;
    pool->totalUsed = 0;
    pool->allocCount = 0;

    return pool;
}

/*
** Destroy a string pool.
*/
void StrPoolDestroy(StrPool *pool) {
    StrPoolChunk *chunk, *next;

    if (!pool) return;

    chunk = pool->first;
    while (chunk) {
        next = chunk->next;
        LocalFree(chunk);
        chunk = next;
    }

    LocalFree(pool);
}

/*
** Reset a string pool - mark all memory as available.
*/
void StrPoolReset(StrPool *pool) {
    StrPoolChunk *chunk;

    if (!pool) return;

    /* Reset all chunks to empty */
    for (chunk = pool->first; chunk; chunk = chunk->next) {
        chunk->used = 0;
    }

    pool->current = pool->first;
    pool->totalUsed = 0;
    pool->allocCount = 0;
}

/*
** Allocate n bytes from the pool.
*/
char *StrPoolAlloc(StrPool *pool, int n) {
    StrPoolChunk *chunk;
    int aligned;
    char *result;

    if (!pool || n <= 0) return NULL;

    aligned = ALIGN_UP(n);

    /* Try current chunk first */
    chunk = pool->current;
    if (chunk && chunk->used + aligned <= chunk->size) {
        result = chunk->data + chunk->used;
        chunk->used += aligned;
        pool->totalUsed += aligned;
        pool->allocCount++;
        return result;
    }

    /* Current chunk is full, try to find a chunk with space */
    for (chunk = pool->first; chunk; chunk = chunk->next) {
        if (chunk->used + aligned <= chunk->size) {
            pool->current = chunk;
            result = chunk->data + chunk->used;
            chunk->used += aligned;
            pool->totalUsed += aligned;
            pool->allocCount++;
            return result;
        }
    }

    /* No chunk has space, allocate a new one */
    {
        int newSize = pool->chunkSize;
        StrPoolChunk *newChunk;

        /* If requested size is larger than chunk size, allocate exact fit */
        if (aligned > newSize) newSize = aligned;

        newChunk = CreateChunk(newSize);
        if (!newChunk) return NULL;

        /* Add to front of list for locality */
        newChunk->next = pool->first;
        pool->first = newChunk;
        pool->current = newChunk;
        pool->totalAllocated += newSize;

        result = newChunk->data;
        newChunk->used = aligned;
        pool->totalUsed += aligned;
        pool->allocCount++;

        return result;
    }
}

/*
** Allocate n bytes and zero-initialize.
*/
char *StrPoolAllocZero(StrPool *pool, int n) {
    char *p = StrPoolAlloc(pool, n);
    if (p) {
        int i;
        for (i = 0; i < n; i++) p[i] = 0;
    }
    return p;
}

/*
** Duplicate a null-terminated string.
*/
char *StrPoolDup(StrPool *pool, const char *s) {
    int len;
    char *result;
    const char *p;

    if (!s) return NULL;

    /* Calculate length */
    for (p = s, len = 0; *p; p++, len++);

    result = StrPoolAlloc(pool, len + 1);
    if (result) {
        int i;
        for (i = 0; i <= len; i++) result[i] = s[i];
    }

    return result;
}

/*
** Duplicate n bytes of a string, adding null terminator.
*/
char *StrPoolNDup(StrPool *pool, const char *s, int n) {
    char *result;

    if (!s || n < 0) return NULL;

    result = StrPoolAlloc(pool, n + 1);
    if (result) {
        int i;
        for (i = 0; i < n; i++) result[i] = s[i];
        result[n] = '\0';
    }

    return result;
}

/*
** Get pool statistics.
*/
void StrPoolStats(StrPool *pool, int *totalAlloc, int *totalUsed, int *allocCount) {
    if (!pool) {
        if (totalAlloc) *totalAlloc = 0;
        if (totalUsed) *totalUsed = 0;
        if (allocCount) *allocCount = 0;
        return;
    }

    if (totalAlloc) *totalAlloc = pool->totalAllocated;
    if (totalUsed) *totalUsed = pool->totalUsed;
    if (allocCount) *allocCount = pool->allocCount;
}
