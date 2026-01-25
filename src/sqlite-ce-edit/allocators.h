/*
** SQLiteCEdit - Type-Safe Memory Allocation Macros
**
** These macros provide type-safe wrappers around LocalAlloc to reduce
** casting errors and improve code readability.
**
** Usage:
**   int *arr = ALLOC(int, 100);          // Allocate 100 ints
**   char *buf = ALLOC_ZERO(char, 256);   // Allocate zeroed buffer
**   FREE(arr);                           // Free memory
*/

#ifndef SQLITE_CE_ALLOCATORS_H
#define SQLITE_CE_ALLOCATORS_H

#include <windows.h>

/*
** ALLOC - Allocate memory for count elements of type
** Returns NULL on failure
*/
#define ALLOC(type, count) \
    ((type*)LocalAlloc(LMEM_FIXED, sizeof(type) * (count)))

/*
** ALLOC_ZERO - Allocate zeroed memory for count elements of type
** Returns NULL on failure
*/
#define ALLOC_ZERO(type, count) \
    ((type*)LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, sizeof(type) * (count)))

/*
** REALLOC - Reallocate memory block
** Returns NULL on failure (original block unchanged)
*/
#define REALLOC(ptr, type, count) \
    ((type*)LocalReAlloc((ptr), sizeof(type) * (count), LMEM_MOVEABLE))

/*
** FREE - Free memory allocated with ALLOC/ALLOC_ZERO
** Safe to call with NULL pointer
*/
#define FREE(ptr) \
    do { if (ptr) { LocalFree(ptr); (ptr) = NULL; } } while(0)

/*
** ALLOC_SIZE - Allocate memory of specific byte size
** Use when size is already calculated
*/
#define ALLOC_SIZE(size) \
    LocalAlloc(LMEM_FIXED, (size))

/*
** ALLOC_SIZE_ZERO - Allocate zeroed memory of specific byte size
*/
#define ALLOC_SIZE_ZERO(size) \
    LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, (size))

#endif /* SQLITE_CE_ALLOCATORS_H */
