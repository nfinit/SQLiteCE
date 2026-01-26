/*
** SQLiteCEdit - Prepared Statement Cache Implementation
**
** Uses FNV-1a hash for SQL lookup and LRU eviction for cache management.
*/

#include "stmtcache.h"
#include "allocators.h"
#include <string.h>

/* FNV-1a hash constants */
#define FNV_OFFSET_BASIS 2166136261u
#define FNV_PRIME 16777619u

/* Cache entry */
typedef struct StmtCacheEntry {
    unsigned int hash;              /* Hash of SQL text */
    char *sql;                      /* SQL text (owned) */
    sqlite_vm *vm;                  /* Compiled statement */
    int inUse;                      /* Currently checked out */
    struct StmtCacheEntry *hashNext;/* Hash chain */
    struct StmtCacheEntry *lruPrev; /* LRU chain - newer */
    struct StmtCacheEntry *lruNext; /* LRU chain - older */
} StmtCacheEntry;

/* Statement cache */
struct StmtCache {
    StmtCacheEntry **buckets;       /* Hash buckets */
    int numBuckets;                 /* Number of buckets */
    int mask;                       /* numBuckets - 1 */
    StmtCacheEntry *lruHead;        /* Most recently used */
    StmtCacheEntry *lruTail;        /* Least recently used */
    int count;                      /* Current entry count */
    int maxCount;                   /* Maximum entries */
    int hits;                       /* Cache hits */
    int misses;                     /* Cache misses */
    sqlite *db;                     /* Database (for schema tracking) */
    int lastSchemaCookie;           /* Last known schema version */
};

/*
** Compute FNV-1a hash of SQL string.
*/
static unsigned int HashSQL(const char *sql) {
    unsigned int h = FNV_OFFSET_BASIS;
    while (*sql) {
        h ^= (unsigned char)*sql++;
        h *= FNV_PRIME;
    }
    return h;
}

/*
** Get current schema cookie from database.
*/
static int GetSchemaCookie(sqlite *db) {
    /* Access the schema_cookie from the database structure */
    /* This is stored in db->aDb[0].schema_cookie */
    /* For safety, we'll use a query to get it */
    sqlite_vm *vm;
    const char *tail;
    int cookie = 0;

    if (sqlite_compile(db, "PRAGMA schema_version", &tail, &vm, NULL) == SQLITE_OK) {
        const char **values;
        const char **names;
        int ncols;
        if (sqlite_step(vm, &ncols, &values, &names) == SQLITE_ROW && ncols > 0 && values[0]) {
            const char *p = values[0];
            while (*p >= '0' && *p <= '9') {
                cookie = cookie * 10 + (*p - '0');
                p++;
            }
        }
        sqlite_finalize(vm, NULL);
    }
    return cookie;
}

/*
** Create a new statement cache.
*/
StmtCache *StmtCacheCreate(int maxSize) {
    StmtCache *cache;
    int numBuckets, i;

    if (maxSize < 4) maxSize = 4;
    if (maxSize > 256) maxSize = 256;

    /* Round bucket count to power of 2 */
    numBuckets = 16;
    while (numBuckets < maxSize * 2) numBuckets *= 2;

    cache = ALLOC(StmtCache, 1);
    if (!cache) return NULL;

    cache->buckets = ALLOC(StmtCacheEntry *, numBuckets);
    if (!cache->buckets) {
        FREE(cache);
        return NULL;
    }

    for (i = 0; i < numBuckets; i++) {
        cache->buckets[i] = NULL;
    }

    cache->numBuckets = numBuckets;
    cache->mask = numBuckets - 1;
    cache->lruHead = NULL;
    cache->lruTail = NULL;
    cache->count = 0;
    cache->maxCount = maxSize;
    cache->hits = 0;
    cache->misses = 0;
    cache->db = NULL;
    cache->lastSchemaCookie = 0;

    return cache;
}

/*
** Remove entry from LRU chain.
*/
static void LruRemove(StmtCache *cache, StmtCacheEntry *entry) {
    if (entry->lruPrev) {
        entry->lruPrev->lruNext = entry->lruNext;
    } else {
        cache->lruHead = entry->lruNext;
    }
    if (entry->lruNext) {
        entry->lruNext->lruPrev = entry->lruPrev;
    } else {
        cache->lruTail = entry->lruPrev;
    }
    entry->lruPrev = NULL;
    entry->lruNext = NULL;
}

/*
** Add entry to front of LRU chain (most recently used).
*/
static void LruAddFront(StmtCache *cache, StmtCacheEntry *entry) {
    entry->lruPrev = NULL;
    entry->lruNext = cache->lruHead;
    if (cache->lruHead) {
        cache->lruHead->lruPrev = entry;
    } else {
        cache->lruTail = entry;
    }
    cache->lruHead = entry;
}

/*
** Free a cache entry.
*/
static void FreeEntry(StmtCacheEntry *entry) {
    if (entry->vm) {
        sqlite_finalize(entry->vm, NULL);
    }
    if (entry->sql) {
        FREE(entry->sql);
    }
    FREE(entry);
}

/*
** Remove entry from hash bucket.
*/
static void HashRemove(StmtCache *cache, StmtCacheEntry *entry) {
    int bucket = entry->hash & cache->mask;
    StmtCacheEntry **pp = &cache->buckets[bucket];

    while (*pp) {
        if (*pp == entry) {
            *pp = entry->hashNext;
            return;
        }
        pp = &(*pp)->hashNext;
    }
}

/*
** Evict least recently used entry.
*/
static void EvictLRU(StmtCache *cache) {
    StmtCacheEntry *victim = cache->lruTail;

    /* Find an entry that's not in use */
    while (victim && victim->inUse) {
        victim = victim->lruPrev;
    }

    if (victim) {
        LruRemove(cache, victim);
        HashRemove(cache, victim);
        FreeEntry(victim);
        cache->count--;
    }
}

/*
** Destroy a statement cache.
*/
void StmtCacheDestroy(StmtCache *cache) {
    int i;

    if (!cache) return;

    /* Free all entries */
    for (i = 0; i < cache->numBuckets; i++) {
        StmtCacheEntry *entry = cache->buckets[i];
        while (entry) {
            StmtCacheEntry *next = entry->hashNext;
            FreeEntry(entry);
            entry = next;
        }
    }

    FREE(cache->buckets);
    FREE(cache);
}

/*
** Invalidate all cached statements.
*/
void StmtCacheInvalidate(StmtCache *cache) {
    int i;

    if (!cache) return;

    for (i = 0; i < cache->numBuckets; i++) {
        StmtCacheEntry *entry = cache->buckets[i];
        while (entry) {
            StmtCacheEntry *next = entry->hashNext;
            if (!entry->inUse) {
                LruRemove(cache, entry);
                FreeEntry(entry);
                cache->count--;
            }
            entry = next;
        }
        cache->buckets[i] = NULL;
    }

    /* Reset LRU for remaining in-use entries */
    cache->lruHead = NULL;
    cache->lruTail = NULL;
}

/*
** Get a compiled statement from cache or compile new.
*/
sqlite_vm *StmtCacheGet(
    StmtCache *cache,
    sqlite *db,
    const char *zSql,
    const char **pzTail,
    char **pzErrMsg
) {
    unsigned int hash;
    int bucket;
    StmtCacheEntry *entry;
    int schemaCookie;
    int rc;

    if (!cache || !db || !zSql) return NULL;

    /* Track database for schema changes */
    if (cache->db != db) {
        StmtCacheInvalidate(cache);
        cache->db = db;
        cache->lastSchemaCookie = GetSchemaCookie(db);
    }

    /* Check for schema changes */
    schemaCookie = GetSchemaCookie(db);
    if (schemaCookie != cache->lastSchemaCookie) {
        StmtCacheInvalidate(cache);
        cache->lastSchemaCookie = schemaCookie;
    }

    /* Hash and search */
    hash = HashSQL(zSql);
    bucket = hash & cache->mask;

    for (entry = cache->buckets[bucket]; entry; entry = entry->hashNext) {
        if (entry->hash == hash && strcmp(entry->sql, zSql) == 0) {
            /* Found cached entry */
            if (entry->inUse) {
                /* Already checked out - compile a new one */
                break;
            }

            /* Reset and return */
            rc = sqlite_reset(entry->vm, pzErrMsg);
            if (rc == SQLITE_SCHEMA) {
                /* Schema changed - invalidate and recompile */
                LruRemove(cache, entry);
                HashRemove(cache, entry);
                FreeEntry(entry);
                cache->count--;
                cache->lastSchemaCookie = GetSchemaCookie(db);
                break;
            }

            /* Move to front of LRU */
            LruRemove(cache, entry);
            LruAddFront(cache, entry);

            entry->inUse = 1;
            cache->hits++;

            if (pzTail) *pzTail = zSql + strlen(zSql);
            return entry->vm;
        }
    }

    /* Cache miss - compile new statement */
    cache->misses++;

    /* Make room if needed */
    while (cache->count >= cache->maxCount) {
        EvictLRU(cache);
    }

    /* Compile */
    {
        sqlite_vm *vm;
        rc = sqlite_compile(db, zSql, pzTail, &vm, pzErrMsg);
        if (rc != SQLITE_OK || !vm) {
            return NULL;
        }

        /* Create cache entry */
        entry = ALLOC(StmtCacheEntry, 1);
        if (!entry) {
            sqlite_finalize(vm, NULL);
            return NULL;
        }

        {
            int len = 0;
            const char *p = zSql;
            while (*p++) len++;
            entry->sql = ALLOC(char, len + 1);
            if (!entry->sql) {
                FREE(entry);
                sqlite_finalize(vm, NULL);
                return NULL;
            }
            memcpy(entry->sql, zSql, len + 1);
        }

        entry->hash = hash;
        entry->vm = vm;
        entry->inUse = 1;

        /* Add to hash bucket */
        entry->hashNext = cache->buckets[bucket];
        cache->buckets[bucket] = entry;

        /* Add to LRU front */
        LruAddFront(cache, entry);

        cache->count++;

        return vm;
    }
}

/*
** Release a statement back to the cache.
*/
void StmtCacheRelease(StmtCache *cache, sqlite_vm *vm) {
    int i;
    StmtCacheEntry *entry;

    if (!cache || !vm) return;

    /* Find the entry for this VM */
    for (i = 0; i < cache->numBuckets; i++) {
        for (entry = cache->buckets[i]; entry; entry = entry->hashNext) {
            if (entry->vm == vm) {
                entry->inUse = 0;
                return;
            }
        }
    }

    /* VM not in cache - must have been compiled outside, finalize it */
    sqlite_finalize(vm, NULL);
}

/*
** Get cache statistics.
*/
void StmtCacheStats(StmtCache *cache, int *hits, int *misses, int *count) {
    if (!cache) {
        if (hits) *hits = 0;
        if (misses) *misses = 0;
        if (count) *count = 0;
        return;
    }

    if (hits) *hits = cache->hits;
    if (misses) *misses = cache->misses;
    if (count) *count = cache->count;
}

/*
** Check if SQL contains multiple statements.
*/
static int IsMultiStatement(const char *sql) {
    int inString = 0;
    char stringChar = 0;

    while (*sql) {
        if (inString) {
            if (*sql == stringChar) {
                if (sql[1] == stringChar) {
                    sql++; /* Escaped quote */
                } else {
                    inString = 0;
                }
            }
        } else {
            if (*sql == '\'' || *sql == '"') {
                inString = 1;
                stringChar = *sql;
            } else if (*sql == ';') {
                /* Check if there's more non-whitespace after semicolon */
                const char *p = sql + 1;
                while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
                if (*p && *p != '\0') {
                    return 1; /* Multiple statements */
                }
            }
        }
        sql++;
    }
    return 0;
}

/*
** Execute SQL using the statement cache.
*/
int StmtCacheExec(
    StmtCache *cache,
    sqlite *db,
    const char *zSql,
    int (*callback)(void*,int,char**,char**),
    void *arg,
    char **pzErrMsg
) {
    sqlite_vm *vm;
    const char *tail;
    int rc;
    int ncols;
    const char **values;
    const char **names;

    if (!db || !zSql) return SQLITE_ERROR;

    /* Skip leading whitespace */
    while (*zSql == ' ' || *zSql == '\t' || *zSql == '\r' || *zSql == '\n') {
        zSql++;
    }

    /* Empty SQL */
    if (!*zSql) return SQLITE_OK;

    /* Multi-statement SQL - fall back to sqlite_exec */
    if (!cache || IsMultiStatement(zSql)) {
        return sqlite_exec(db, zSql, callback, arg, pzErrMsg);
    }

    /* Get cached or compile new statement */
    vm = StmtCacheGet(cache, db, zSql, &tail, pzErrMsg);
    if (!vm) {
        return SQLITE_ERROR;
    }

    /* Execute the statement */
    while ((rc = sqlite_step(vm, &ncols, &values, &names)) == SQLITE_ROW) {
        if (callback) {
            /* sqlite_step returns const char**, callback expects char** */
            if (callback(arg, ncols, (char**)values, (char**)names)) {
                rc = SQLITE_ABORT;
                break;
            }
        }
    }

    /* Handle final result */
    if (rc == SQLITE_DONE) {
        rc = SQLITE_OK;
    } else if (rc == SQLITE_SCHEMA) {
        /* Schema changed - invalidate cache and retry */
        StmtCacheRelease(cache, vm);
        StmtCacheInvalidate(cache);
        return StmtCacheExec(cache, db, zSql, callback, arg, pzErrMsg);
    } else if (rc != SQLITE_OK && pzErrMsg) {
        /* Get error message */
        char *errCopy;
        const char *errMsg = sqlite_error_string(rc);
        int len = 0;
        const char *p = errMsg;
        while (*p++) len++;
        errCopy = ALLOC(char, len + 1);
        if (errCopy) {
            memcpy(errCopy, errMsg, len + 1);
            *pzErrMsg = errCopy;
        }
    }

    /* Release back to cache */
    StmtCacheRelease(cache, vm);

    return rc;
}
