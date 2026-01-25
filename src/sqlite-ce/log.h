/*
** SQLite/CE Logging Infrastructure
**
** Simple logging system with configurable log levels.
** All logging goes through OutputDebugString for debugger capture.
**
** Usage:
**   LOG_ERROR("Failed to open file: %s", filename);
**   LOG_WARN("Cache miss, loading from disk");
**   LOG_INFO("Query executed in %d ms", elapsed);
**   LOG_DEBUG("Processing row %d", rownum);
**   LOG_TRACE("Entering function");
*/

#ifndef SQLITE_CE_LOG_H
#define SQLITE_CE_LOG_H

/*============================================================================
** Log Levels
**============================================================================*/

#define LOG_LEVEL_NONE    0   /* No logging */
#define LOG_LEVEL_ERROR   1   /* Errors only */
#define LOG_LEVEL_WARN    2   /* Errors and warnings */
#define LOG_LEVEL_INFO    3   /* Errors, warnings, and info */
#define LOG_LEVEL_DEBUG   4   /* All except trace */
#define LOG_LEVEL_TRACE   5   /* Everything */

/*
** Compile-time log level. Override in config.h or build settings.
** Default: INFO for release, DEBUG for debug builds.
*/
#ifndef SQLITE_CE_LOG_LEVEL
#  ifdef SQLITE_DEBUG
#    define SQLITE_CE_LOG_LEVEL LOG_LEVEL_DEBUG
#  else
#    define SQLITE_CE_LOG_LEVEL LOG_LEVEL_INFO
#  endif
#endif

/*============================================================================
** Logging Functions
**============================================================================*/

/* Core logging function - use macros below instead */
void sqliteLog(int level, const char *file, int line, const char *fmt, ...);

/* Log with file/line for debugging */
#if SQLITE_CE_LOG_LEVEL >= LOG_LEVEL_ERROR
#  define LOG_ERROR(...) sqliteLog(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#else
#  define LOG_ERROR(...) ((void)0)
#endif

#if SQLITE_CE_LOG_LEVEL >= LOG_LEVEL_WARN
#  define LOG_WARN(...) sqliteLog(LOG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#else
#  define LOG_WARN(...) ((void)0)
#endif

#if SQLITE_CE_LOG_LEVEL >= LOG_LEVEL_INFO
#  define LOG_INFO(...) sqliteLog(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#else
#  define LOG_INFO(...) ((void)0)
#endif

#if SQLITE_CE_LOG_LEVEL >= LOG_LEVEL_DEBUG
#  define LOG_DEBUG(...) sqliteLog(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#else
#  define LOG_DEBUG(...) ((void)0)
#endif

#if SQLITE_CE_LOG_LEVEL >= LOG_LEVEL_TRACE
#  define LOG_TRACE(...) sqliteLog(LOG_LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#else
#  define LOG_TRACE(...) ((void)0)
#endif

/*============================================================================
** Runtime Configuration
**============================================================================*/

/*
** Set runtime log level (can't exceed compile-time level).
** Returns previous level.
*/
int sqliteLogSetLevel(int level);

/*
** Get current runtime log level.
*/
int sqliteLogGetLevel(void);

#endif /* SQLITE_CE_LOG_H */
