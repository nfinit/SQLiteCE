/*
** stdio.h shim for Windows CE
**
** CE has no console, so stdout/stderr don't exist. SQLite uses stdio mainly
** for sprintf() and debug fprintf() calls.
**
** CE 2.0 has very limited CRT - we implement sprintf ourselves.
*/

#ifndef _STDIO_H_CE
#define _STDIO_H_CE

#include <windows.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
** sprintf/vsprintf - implemented in os.c since CE 2.0 lacks them
*/
int ce_sprintf(char *buf, const char *fmt, ...);
int ce_vsprintf(char *buf, const char *fmt, va_list ap);
#define sprintf  ce_sprintf
#define vsprintf ce_vsprintf

/* sscanf - stub function, returns 0 (no matches) */
static int ce_dummy_sscanf(const char *str, const char *fmt, ...) { return 0; }
#define sscanf ce_dummy_sscanf

#ifdef __cplusplus
}
#endif

/* FILE type - stub for declarations that reference it */
typedef struct _FILE_CE { int unused; } FILE;

/* Standard streams - dummy pointers, never dereferenced in CE build */
#define stdin  ((FILE *)0)
#define stdout ((FILE *)1)
#define stderr ((FILE *)2)

/* EOF constant */
#define EOF (-1)

/*
** fprintf/printf - no-op on CE (no console).
** We can't use variadic macros in VS6, so we use inline functions
** that accept the format string and discard everything.
*/
static int ce_dummy_printf(const char *fmt, ...) { return 0; }
static int ce_dummy_fprintf(FILE *f, const char *fmt, ...) { return 0; }
#define printf  ce_dummy_printf
#define fprintf ce_dummy_fprintf

/* fflush - no-op */
#define fflush(f) (0)

/*
** File operations - stub out. SQLite's COPY command uses these but
** COPY FROM file is not practical on CE anyway.
*/
#define fopen(name, mode)  ((FILE *)0)
#define fclose(f)          (0)
#define getc(f)            (EOF)
#define ungetc(c, f)       (EOF)

#endif /* _STDIO_H_CE */
