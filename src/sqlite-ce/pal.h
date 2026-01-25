/*
** SQLite/CE Platform Abstraction Layer (PAL)
**
** This header defines the platform-specific interfaces used by SQLite/CE.
** The implementation is in os.c for Windows CE. To port to a new platform:
** 1. Create a new os_<platform>.c implementing these functions
** 2. Update config.h to define appropriate platform flags
** 3. Include the new implementation in the build
**
** Current platform support:
** - OS_WINCE: Windows CE 2.0+ (os.c)
*/

#ifndef SQLITE_CE_PAL_H
#define SQLITE_CE_PAL_H

#include <windows.h>

/*============================================================================
** String Formatting
** CE 2.0 lacks standard sprintf/snprintf. These provide compatible versions.
**============================================================================*/

/*
** Format a string with printf-style arguments.
** Returns number of characters written (excluding null terminator).
** Supports: %d, %i, %u, %x, %X, %s, %c, %p, %ld, %lu, %lx, %%
*/
int ce_vsprintf(char *buf, const char *fmt, va_list ap);
int ce_sprintf(char *buf, const char *fmt, ...);

/*============================================================================
** String Conversion
** Wide (Unicode) to multibyte conversion utilities.
**============================================================================*/

/*
** Convert wide string to UTF-8/ANSI.
** Returns allocated string (caller must free with LocalFree).
** Returns NULL on failure.
*/
char *ce_wide_to_utf8(const wchar_t *wz);

/*
** Convert UTF-8/ANSI string to wide.
** Returns allocated string (caller must free with LocalFree).
** Returns NULL on failure.
*/
wchar_t *ce_utf8_to_wide(const char *z);

/*============================================================================
** Debug Output
** Output to debugger or trace system.
**============================================================================*/

/*
** Output a message to the debugger.
** Uses OutputDebugStringW on Windows CE.
*/
void ce_debug_output(const char *zMsg);

/*
** Trace output (enabled by SQLITE_CE_TRACE).
** Outputs message with newline to debugger.
*/
#ifdef SQLITE_CE_TRACE
void ce_trace_output(const char *zMsg);
#endif

/*
** Assertion failure handler.
** Outputs message and breaks into debugger.
*/
void ce_assert_fail(const char *exp, const char *file, int line);

/*============================================================================
** Time Functions
** CE doesn't have standard time functions.
**============================================================================*/

/*
** Get current time as Julian day number.
** Used by SQLite date/time functions.
*/
int ce_current_time(double *pTime);

/*
** Get tick count for elapsed time measurement.
** Returns milliseconds since system start.
*/
#define ce_get_tick_count() GetTickCount()

/*============================================================================
** File Path Utilities
**============================================================================*/

/*
** Get full pathname from relative path.
** Returns allocated string (caller must free with sqliteFree).
*/
char *ce_full_pathname(const char *zRelative);

/*
** Get temporary file directory.
** On CE 3.0+ uses GetTempPath, on CE 2.x uses default.
*/
void ce_get_temp_directory(wchar_t *buf, int bufLen);

/*============================================================================
** Random Number Generation
**============================================================================*/

/*
** Fill buffer with random bytes for use as seed.
** Uses system time, tick count, and process info for entropy.
*/
int ce_random_seed(char *zBuf, int nBuf);

/*============================================================================
** Platform Detection Macros
**============================================================================*/

/* Check if running on specific CE version */
#define CE_VERSION_2_0   (_WIN32_WCE >= 200 && _WIN32_WCE < 211)
#define CE_VERSION_2_11  (_WIN32_WCE >= 211 && _WIN32_WCE < 300)
#define CE_VERSION_3_0   (_WIN32_WCE >= 300)

/* Check for API availability */
#define CE_HAS_LOCKFILEEX   (_WIN32_WCE >= 211)
#define CE_HAS_GETTEMPPATH  (_WIN32_WCE >= 300)

#endif /* SQLITE_CE_PAL_H */
