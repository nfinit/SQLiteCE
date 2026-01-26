/*
** SQLiteCEdit - Prepared Statement Cache
**
** Caches compiled SQL statements for reuse, reducing parse/compile
** overhead for repeated queries. Automatically invalidates on schema
** changes via schema_cookie tracking.
**
** Usage:
**   StmtCache *cache = StmtCacheCreate(16);  // 16 entry cache
**   sqlite_vm *vm = StmtCacheGet(cache, db, sql, &tail, &errmsg);
**   if (vm) {
**       // execute using sqlite_step()
**       StmtCacheRelease(cache, vm);  // return to cache for reuse
**   }
**   StmtCacheDestroy(cache);
*/

#ifndef STMTCACHE_H
#define STMTCACHE_H

#include "sqlite.h"

/* Forward declaration */
typedef struct StmtCache StmtCache;

/*
** Create a new statement cache with the given maximum size.
** Returns NULL on allocation failure.
*/
StmtCache *StmtCacheCreate(int maxSize);

/*
** Destroy a statement cache, finalizing all cached statements.
*/
void StmtCacheDestroy(StmtCache *cache);

/*
** Get a compiled statement for the given SQL.
** If cached and valid, returns the cached VM after reset.
** If not cached, compiles and caches the statement.
** Returns NULL on error (check *pzErrMsg).
**
** The caller must call StmtCacheRelease() when done with the VM.
** Do NOT call sqlite_finalize() on the returned VM.
*/
sqlite_vm *StmtCacheGet(
    StmtCache *cache,
    sqlite *db,
    const char *zSql,
    const char **pzTail,
    char **pzErrMsg
);

/*
** Release a statement back to the cache after use.
** The VM will be reset and made available for reuse.
*/
void StmtCacheRelease(StmtCache *cache, sqlite_vm *vm);

/*
** Invalidate all cached statements.
** Call this after schema changes (CREATE, DROP, ALTER).
*/
void StmtCacheInvalidate(StmtCache *cache);

/*
** Get cache statistics for debugging/tuning.
*/
void StmtCacheStats(StmtCache *cache, int *hits, int *misses, int *count);

/*
** Execute SQL using the statement cache.
** This is a cached version of sqlite_exec() for single statements.
** Returns SQLITE_OK on success, error code otherwise.
**
** Note: Only caches single statements (no multi-statement support).
** For multi-statement SQL, falls back to sqlite_exec().
*/
int StmtCacheExec(
    StmtCache *cache,
    sqlite *db,
    const char *zSql,
    int (*callback)(void*,int,char**,char**),
    void *arg,
    char **pzErrMsg
);

#endif /* STMTCACHE_H */
