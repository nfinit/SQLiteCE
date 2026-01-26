/*
** SQLiteCEdit - Version Information
**
** This file provides a single source of truth for version information.
** Update VERSION_* macros here for new releases.
**
** For desktop builds with Git, the Makefile can override VERSION_BUILD
** by defining SQLITECEDIT_BUILD_NUM during compilation.
*/

#ifndef VERSION_H
#define VERSION_H

/*============================================================================
** Version Components
**============================================================================*/

#define VERSION_MAJOR 0
#define VERSION_MINOR 10
#define VERSION_PATCH 0
#define VERSION_BUILD 62

/*============================================================================
** String Macros
**============================================================================*/

/* Helper macros for stringification */
#define VERSION_STR_HELPER(x) #x
#define VERSION_STR(x) VERSION_STR_HELPER(x)

#define VERSION_WSTR_HELPER(x) L ## #x
#define VERSION_WSTR(x) VERSION_WSTR_HELPER(x)

/* Full version string: "0.10.0.62" */
#define SQLITECEDIT_VERSION_STR \
    VERSION_STR(VERSION_MAJOR) "." \
    VERSION_STR(VERSION_MINOR) "." \
    VERSION_STR(VERSION_PATCH) "." \
    VERSION_STR(VERSION_BUILD)

/* Wide version string: L"0.10.0.62" */
#define SQLITECEDIT_VERSION_WSTR \
    VERSION_WSTR(VERSION_MAJOR) L"." \
    VERSION_WSTR(VERSION_MINOR) L"." \
    VERSION_WSTR(VERSION_PATCH) L"." \
    VERSION_WSTR(VERSION_BUILD)

/*============================================================================
** Compatibility - SQLITECEDIT_VERSION used by existing code
**============================================================================*/

#define SQLITECEDIT_VERSION SQLITECEDIT_VERSION_WSTR

/*============================================================================
** Numeric Version for Comparisons
** Format: MMmmppbb where MM=major, mm=minor, pp=patch, bb=build
**============================================================================*/

#define SQLITECEDIT_VERSION_NUM \
    ((VERSION_MAJOR * 1000000) + (VERSION_MINOR * 10000) + \
     (VERSION_PATCH * 100) + VERSION_BUILD)

#endif /* VERSION_H */
