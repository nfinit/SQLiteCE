/*
** SQLiteCEdit - String Utility Functions
**
** Safe string operations for Windows CE. These functions provide
** bounds-checked string manipulation to replace scattered pointer
** arithmetic patterns throughout the codebase.
*/

#ifndef SQLITE_CE_STRUTILS_H
#define SQLITE_CE_STRUTILS_H

#include <windows.h>

/*
** StrLenSafe - Safe string length (handles NULL)
** Returns 0 for NULL pointer, otherwise strlen
*/
int StrLenSafe(const char *s);

/*
** StrLenSafeW - Wide string version of StrLenSafe
*/
int StrLenSafeW(const wchar_t *s);

/*
** StrCopySafe - Copy string with bounds checking
** dst: destination buffer
** src: source string
** maxLen: maximum characters to copy (including null terminator)
** Returns: pointer to dst
** Always null-terminates if maxLen > 0
*/
char *StrCopySafe(char *dst, const char *src, int maxLen);

/*
** StrCopySafeW - Wide string version
*/
wchar_t *StrCopySafeW(wchar_t *dst, const wchar_t *src, int maxLen);

/*
** StrSkipWhitespace - Skip whitespace characters
** p: pointer to string pointer (will be advanced)
** Advances *p past any space, tab, CR, LF characters
*/
void StrSkipWhitespace(const char **p);

/*
** StrSkipWhitespaceW - Wide string version
*/
void StrSkipWhitespaceW(const wchar_t **p);

/*
** StrFindChar - Find character in string
** s: string to search
** ch: character to find
** Returns: pointer to first occurrence, or NULL if not found
*/
const char *StrFindChar(const char *s, char ch);

/*
** StrFindCharW - Wide string version
*/
const wchar_t *StrFindCharW(const wchar_t *s, wchar_t ch);

/*
** StrAdvanceToChar - Advance pointer to character or end
** p: current position
** ch: character to find
** Returns: pointer to ch or to null terminator
*/
const char *StrAdvanceToChar(const char *p, char ch);

/*
** StrAdvanceToCharW - Wide string version
*/
const wchar_t *StrAdvanceToCharW(const wchar_t *p, wchar_t ch);

/*
** StrTrimRight - Trim trailing whitespace in-place
** s: string to trim
** Returns: s (for chaining)
*/
char *StrTrimRight(char *s);

/*
** StrTrimRightW - Wide string version
*/
wchar_t *StrTrimRightW(wchar_t *s);

/*
** StrIsEmpty - Check if string is NULL or empty
*/
#define StrIsEmpty(s) ((s) == NULL || *(s) == '\0')
#define StrIsEmptyW(s) ((s) == NULL || *(s) == L'\0')

/*
** StrEquals - Case-sensitive string comparison
** Returns non-zero if strings are equal
*/
#define StrEquals(a, b) ((a) && (b) && lstrcmpA((a), (b)) == 0)
#define StrEqualsW(a, b) ((a) && (b) && lstrcmpW((a), (b)) == 0)

/*
** StrEqualsI - Case-insensitive string comparison
** Returns non-zero if strings are equal (ignoring case)
*/
#define StrEqualsI(a, b) ((a) && (b) && lstrcmpiA((a), (b)) == 0)
#define StrEqualsIW(a, b) ((a) && (b) && lstrcmpiW((a), (b)) == 0)

/*
** STR_COPY - Inline string copy for building SQL and buffer strings
** Replaces the common pattern: while (*s) *p++ = *s++;
**
** Usage:
**   char *p = buf;
**   STR_COPY(p, "SELECT * FROM ");
**   STR_COPY(p, tableName);
**   *p = '\0';
**
** Note: Does NOT null-terminate. Caller must add *p = '\0' at end.
*/
#define STR_COPY(dst, src) \
    do { const char *_s = (src); while (*_s) *(dst)++ = *_s++; } while(0)

#define STR_COPY_W(dst, src) \
    do { const wchar_t *_s = (src); while (*_s) *(dst)++ = *_s++; } while(0)

/*
** STR_COPY_N - Copy up to N characters
** Note: Does NOT null-terminate.
*/
#define STR_COPY_N(dst, src, n) \
    do { const char *_s = (src); int _n = (n); while (_n-- > 0 && *_s) *(dst)++ = *_s++; } while(0)

#endif /* SQLITE_CE_STRUTILS_H */
