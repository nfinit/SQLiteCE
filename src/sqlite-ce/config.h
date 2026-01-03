/*
** SQLite/CE Configuration Header
**
** This file should be included at the top of os_wince.c and sets up all
** the platform-specific defines needed for the Windows CE port.
**
** The Visual Studio 6 CE Toolkit automatically defines:
**   _WIN32_WCE    - Set to CE version (200, 211, 300, etc.) by SDK selection
**   UNDER_CE      - Always defined for CE targets
**   UNICODE       - Defined by default for CE
**   _UNICODE      - Defined by default for CE
**   SH3/MIPS/ARM  - Set by CPU target selection
**
** This header auto-detects CE and derives all capability flags from
** _WIN32_WCE, so no manual configuration is required beyond selecting
** the target SDK and CPU in Visual Studio's build settings.
*/

#ifndef CONFIG_WINCE_H
#define CONFIG_WINCE_H

/*
** Verify we're actually building for Windows CE
*/
#if !defined(_WIN32_WCE) && !defined(UNDER_CE)
#error "config_wince.h included but not building for Windows CE"
#endif

/* Ensure _WIN32_WCE is defined even if only UNDER_CE was set */
#ifndef _WIN32_WCE
#  define _WIN32_WCE 200  /* Assume oldest if not specified */
#endif

/*
** OS type flags for SQLite
** We set OS_WIN to 0 so that os.h's OS_WIN blocks don't activate.
** Our os_wince.c provides the complete implementation.
*/
#define OS_WINCE 1
#define OS_WIN   0
#define OS_UNIX  0
#define OS_MAC   0

/*
** Version string for identification
*/
#if _WIN32_WCE >= 300
#  define SQLITE_CE_VERSION_STRING "CE 3.0+"
#elif _WIN32_WCE >= 211
#  define SQLITE_CE_VERSION_STRING "CE 2.11"
#elif _WIN32_WCE >= 201
#  define SQLITE_CE_VERSION_STRING "CE 2.01"
#else
#  define SQLITE_CE_VERSION_STRING "CE 2.0"
#endif

/*============================================================================
** Platform capability flags
** These indicate which APIs are available based on _WIN32_WCE version
**============================================================================*/

/*
** LockFileEx() - available on CE 2.11 and later
** CE 2.0 only has LockFile() which doesn't support shared locks
*/
#if _WIN32_WCE >= 211
#  define CE_HAS_LOCKFILEEX 1
#else
#  define CE_HAS_LOCKFILEEX 0
#endif

/*
** GetTempPath() - not available on early CE
*/
#if _WIN32_WCE >= 300
#  define CE_HAS_GETTEMPPATH 1
#else
#  define CE_HAS_GETTEMPPATH 0
#endif

/*
** GetSystemTimeAsFileTime() - may not exist on CE 2.0
*/
#if _WIN32_WCE >= 211
#  define CE_HAS_GETSYSTEMTIMEASFILETIME 1
#else
#  define CE_HAS_GETSYSTEMTIMEASFILETIME 0
#endif

/*
** GetVersionEx() - not available on CE (used in desktop isNT() check)
** We never have this, so we handle locking differently
*/
#define CE_HAS_GETVERSIONEX 0

/*
** Console functions - not available on CE
*/
#define CE_HAS_CONSOLE 0

/*============================================================================
** SQLite compile-time options for CE
**============================================================================*/

/* Disable large file support - CE doesn't have it */
#define SQLITE_DISABLE_LFS 1

/*
** Memory tuning for constrained devices
** Defaults are aggressive for 16MB devices like the HP 620LX
** Override these in project settings if targeting beefier hardware
*/
#ifndef SQLITE_DEFAULT_CACHE_SIZE
#  define SQLITE_DEFAULT_CACHE_SIZE 64   /* Pages (default is 2000) */
#endif

#ifndef SQLITE_DEFAULT_PAGE_SIZE
#  define SQLITE_DEFAULT_PAGE_SIZE 1024  /* Bytes (default is 1024, ok) */
#endif

/*
** Temp file configuration
*/
#ifndef TEMP_FILE_PREFIX
#  define TEMP_FILE_PREFIX "sqlite_"
#endif

/* Default temp directory for CE when GetTempPath unavailable */
#ifndef SQLITE_CE_TEMP_DIRECTORY
#  define SQLITE_CE_TEMP_DIRECTORY "\\Temp"
#endif

/* Max path length */
#ifndef SQLITE_CE_MAX_PATH
#  define SQLITE_CE_MAX_PATH 260
#endif

/* Buffer size for temp filenames */
#define SQLITE_TEMPNAME_SIZE (SQLITE_CE_MAX_PATH + 50)

/* Minimum sleep granularity in milliseconds */
#define SQLITE_MIN_SLEEP_MS 1

/*============================================================================
** Threading configuration
**============================================================================*/

/*
** CE supports critical sections, which we use for the mutex.
** Enable threadsafe by default; can be disabled with THREADSAFE=0
*/
#ifndef THREADSAFE
#  define THREADSAFE 1
#endif

#if THREADSAFE
#  define SQLITE_W32_THREADS 1
#endif

/*============================================================================
** Processor/byte order detection
** The toolkit defines processor macros based on CPU selection
**============================================================================*/

#if defined(SHx) || defined(SH3) || defined(SH4) || defined(_SH3_) || defined(_SH4_)
#  define SQLITE_CE_PROCESSOR "SH3"
#  define SQLITE_CE_BIG_ENDIAN 1
#elif defined(MIPS) || defined(_MIPS_)
#  define SQLITE_CE_PROCESSOR "MIPS"
#  define SQLITE_CE_BIG_ENDIAN 0  /* Usually little-endian on CE */
#elif defined(ARM) || defined(_ARM_)
#  define SQLITE_CE_PROCESSOR "ARM"
#  define SQLITE_CE_BIG_ENDIAN 0
#elif defined(_X86_) || defined(x86) || defined(_M_IX86)
#  define SQLITE_CE_PROCESSOR "x86"
#  define SQLITE_CE_BIG_ENDIAN 0
#else
#  define SQLITE_CE_PROCESSOR "Unknown"
#  define SQLITE_CE_BIG_ENDIAN 0
#endif

/*============================================================================
** OsFile structure
** This must match what os.h expects. Under OS_WIN, it's:
**   struct OsFile { HANDLE h; int locked; };
** We define it here for when os.h is included with OS_WIN=0
**============================================================================*/

/* Forward declare HANDLE if windows.h not yet included */
#ifndef _WINDEF_
typedef void *HANDLE;
#endif

typedef struct OsFile OsFile;
struct OsFile {
    HANDLE h;               /* Handle for accessing the file */
    int locked;             /* 0: unlocked, <0: write lock, >0: read lock */
};

/*
** off_t type for file offsets
** CE doesn't have this in its headers
*/
#if defined(_MSC_VER)
typedef __int64 off_t;
#else
typedef long long off_t;
#endif

/*============================================================================
** Optional feature omissions for code size reduction
** Uncomment these if you need a smaller DLL
**============================================================================*/

/* #define SQLITE_OMIT_VACUUM 1 */
/* #define SQLITE_OMIT_PAGER_PRAGMAS 1 */
/* #define SQLITE_OMIT_DATETIME_FUNCS 1 */
/* #define SQLITE_OMIT_PROGRESS_CALLBACK 1 */
/* #define SQLITE_OMIT_AUTHORIZATION 1 */
/* #define SQLITE_OMIT_SUBQUERY 1 */

/*============================================================================
** Debug and tracing
**============================================================================*/

/* #define SQLITE_DEBUG 1 */
/* #define SQLITE_CE_TRACE 1 */

#ifdef SQLITE_CE_TRACE
#  define CE_TRACE(msg) ce_trace_output(msg)
   void ce_trace_output(const char *msg);  /* Implement in os_wince.c */
#else
#  define CE_TRACE(msg)
#endif

/*============================================================================
** Port version information
**============================================================================*/

#define SQLITE_CE_PORT_VERSION "0.1.0"
#define SQLITE_CE_PORT_DATE    "2026-01-02"

/*
** Build a combined version string for identification
*/
#define SQLITE_CE_FULL_VERSION \
    "SQLite 2.8.17 / " SQLITE_CE_VERSION_STRING " / " SQLITE_CE_PROCESSOR

#endif /* CONFIG_WINCE_H */
