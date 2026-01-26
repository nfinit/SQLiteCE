/*
** SQLiteCEdit - String Interning Table
**
** A hash-based string deduplication system that ensures identical strings
** share the same memory. This significantly reduces memory usage for result
** sets with repeated values (e.g., status columns, categories, boolean flags).
**
** Built on StrPool for efficient allocation; adds a hash table for lookup.
**
** Usage:
**   StringIntern *intern = StringInternCreate(1024);  // 1024 hash buckets
**   char *s1 = StringInternGet(intern, "active");     // stores "active"
**   char *s2 = StringInternGet(intern, "active");     // returns same pointer
**   assert(s1 == s2);                                  // true!
**   StringInternReset(intern);                         // clear for reuse
**   StringInternDestroy(intern);                       // free everything
*/

#ifndef STRINTERN_H
#define STRINTERN_H

#include "strpool.h"

/* Hash bucket entry - stores interned string */
typedef struct InternEntry {
    struct InternEntry *next;   /* Next entry in bucket chain */
    unsigned int hash;          /* Cached hash value */
    char *str;                  /* Pointer to interned string in pool */
} InternEntry;

/* String intern table */
typedef struct StringIntern {
    StrPool *pool;              /* String storage pool */
    StrPool *entryPool;         /* Pool for InternEntry allocations */
    InternEntry **buckets;      /* Hash table buckets */
    int numBuckets;             /* Number of buckets (power of 2) */
    int mask;                   /* numBuckets - 1, for fast modulo */
    int count;                  /* Number of unique strings interned */
    int lookups;                /* Total lookup count (for stats) */
    int hits;                   /* Cache hits (found existing) */
} StringIntern;

/*
** Create a new string intern table with the given number of buckets.
** numBuckets should be a power of 2 for efficiency; if not, it will
** be rounded up to the next power of 2.
** Returns NULL on allocation failure.
*/
StringIntern *StringInternCreate(int numBuckets);

/*
** Destroy a string intern table, freeing all memory.
*/
void StringInternDestroy(StringIntern *intern);

/*
** Reset the intern table - clears all entries but keeps allocated memory.
** Much faster than Destroy+Create for reusing the table.
*/
void StringInternReset(StringIntern *intern);

/*
** Get or create an interned string. If the string already exists in the
** table, returns the existing pointer. Otherwise, copies the string and
** returns the new pointer.
**
** Returns NULL on allocation failure or if s is NULL.
*/
char *StringInternGet(StringIntern *intern, const char *s);

/*
** Get or create an interned string of exactly n bytes (plus null terminator).
** Useful when the source string is not null-terminated.
*/
char *StringInternGetN(StringIntern *intern, const char *s, int n);

/*
** Get intern table statistics for debugging/tuning.
** hitRate = hits / lookups (0.0 to 1.0, higher is better)
*/
void StringInternStats(StringIntern *intern, int *count, int *lookups,
                       int *hits, double *hitRate);

#endif /* STRINTERN_H */
