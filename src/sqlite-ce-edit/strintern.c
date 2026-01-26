/*
** SQLiteCEdit - String Interning Table Implementation
**
** Uses FNV-1a hash for fast, well-distributed hashing of strings.
** Hash table uses separate chaining for collision resolution.
*/

#include "strintern.h"
#include "allocators.h"
#include <string.h>

/*
** FNV-1a hash constants (same as used in SQLite hash.c)
*/
#define FNV_OFFSET_BASIS 2166136261u
#define FNV_PRIME 16777619u

/*
** Compute FNV-1a hash of a null-terminated string.
*/
static unsigned int HashString(const char *s) {
    unsigned int h = FNV_OFFSET_BASIS;
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= FNV_PRIME;
    }
    return h;
}

/*
** Compute FNV-1a hash of a string with known length.
*/
static unsigned int HashStringN(const char *s, int n) {
    unsigned int h = FNV_OFFSET_BASIS;
    while (n-- > 0) {
        h ^= (unsigned char)*s++;
        h *= FNV_PRIME;
    }
    return h;
}

/*
** Round up to next power of 2.
*/
static int NextPowerOf2(int n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n < 16 ? 16 : n;  /* Minimum 16 buckets */
}

/*
** Create a new string intern table.
*/
StringIntern *StringInternCreate(int numBuckets) {
    StringIntern *intern;
    int i;

    intern = ALLOC(StringIntern, 1);
    if (!intern) return NULL;

    /* Round to power of 2 */
    numBuckets = NextPowerOf2(numBuckets);

    /* Allocate bucket array */
    intern->buckets = ALLOC(InternEntry *, numBuckets);
    if (!intern->buckets) {
        FREE(intern);
        return NULL;
    }

    /* Initialize buckets to NULL */
    for (i = 0; i < numBuckets; i++) {
        intern->buckets[i] = NULL;
    }

    /* Create string pool (32KB chunks for strings) */
    intern->pool = StrPoolCreate(32 * 1024);
    if (!intern->pool) {
        FREE(intern->buckets);
        FREE(intern);
        return NULL;
    }

    /* Create entry pool (4KB chunks for InternEntry structs) */
    intern->entryPool = StrPoolCreate(4 * 1024);
    if (!intern->entryPool) {
        StrPoolDestroy(intern->pool);
        FREE(intern->buckets);
        FREE(intern);
        return NULL;
    }

    intern->numBuckets = numBuckets;
    intern->mask = numBuckets - 1;
    intern->count = 0;
    intern->lookups = 0;
    intern->hits = 0;

    return intern;
}

/*
** Destroy a string intern table.
*/
void StringInternDestroy(StringIntern *intern) {
    if (!intern) return;

    if (intern->pool) StrPoolDestroy(intern->pool);
    if (intern->entryPool) StrPoolDestroy(intern->entryPool);
    if (intern->buckets) FREE(intern->buckets);
    FREE(intern);
}

/*
** Reset the intern table for reuse.
*/
void StringInternReset(StringIntern *intern) {
    int i;

    if (!intern) return;

    /* Clear bucket pointers */
    for (i = 0; i < intern->numBuckets; i++) {
        intern->buckets[i] = NULL;
    }

    /* Reset pools (keeps allocated memory) */
    if (intern->pool) StrPoolReset(intern->pool);
    if (intern->entryPool) StrPoolReset(intern->entryPool);

    intern->count = 0;
    /* Keep lookups/hits for cumulative stats, or reset: */
    /* intern->lookups = 0; intern->hits = 0; */
}

/*
** Compare string s with entry, handling length n.
*/
static int StringMatch(const char *s, int n, const char *entry) {
    while (n-- > 0) {
        if (*s++ != *entry++) return 0;
    }
    return *entry == '\0';  /* Entry must also end here */
}

/*
** Get or create an interned string of length n.
*/
char *StringInternGetN(StringIntern *intern, const char *s, int n) {
    unsigned int hash, bucket;
    InternEntry *entry;
    char *newStr;
    InternEntry *newEntry;

    if (!intern || !s) return NULL;
    if (n < 0) return NULL;

    intern->lookups++;

    /* Compute hash and bucket */
    hash = HashStringN(s, n);
    bucket = hash & intern->mask;

    /* Search bucket chain */
    for (entry = intern->buckets[bucket]; entry; entry = entry->next) {
        if (entry->hash == hash && StringMatch(s, n, entry->str)) {
            /* Found existing string */
            intern->hits++;
            return entry->str;
        }
    }

    /* Not found - allocate new string */
    newStr = StrPoolAlloc(intern->pool, n + 1);
    if (!newStr) return NULL;

    /* Copy string data */
    memcpy(newStr, s, n);
    newStr[n] = '\0';

    /* Allocate entry from entry pool */
    newEntry = (InternEntry *)StrPoolAlloc(intern->entryPool, sizeof(InternEntry));
    if (!newEntry) return NULL;  /* String leaked in pool, but pool will clean up */

    /* Initialize and link entry */
    newEntry->hash = hash;
    newEntry->str = newStr;
    newEntry->next = intern->buckets[bucket];
    intern->buckets[bucket] = newEntry;
    intern->count++;

    return newStr;
}

/*
** Get or create an interned string (null-terminated).
*/
char *StringInternGet(StringIntern *intern, const char *s) {
    int n;
    const char *p;

    if (!s) return NULL;

    /* Calculate length */
    n = 0;
    p = s;
    while (*p++) n++;

    return StringInternGetN(intern, s, n);
}

/*
** Get statistics.
*/
void StringInternStats(StringIntern *intern, int *count, int *lookups,
                       int *hits, double *hitRate) {
    if (!intern) {
        if (count) *count = 0;
        if (lookups) *lookups = 0;
        if (hits) *hits = 0;
        if (hitRate) *hitRate = 0.0;
        return;
    }

    if (count) *count = intern->count;
    if (lookups) *lookups = intern->lookups;
    if (hits) *hits = intern->hits;
    if (hitRate) {
        if (intern->lookups > 0) {
            *hitRate = (double)intern->hits / (double)intern->lookups;
        } else {
            *hitRate = 0.0;
        }
    }
}
