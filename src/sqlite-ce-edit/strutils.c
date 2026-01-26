/*
** SQLiteCEdit - String Utility Functions Implementation
*/

#include "strutils.h"

/*============================================================================
** Length Functions
**============================================================================*/

int StrLenSafe(const char *s) {
    int len = 0;
    if (s) {
        while (*s++) len++;
    }
    return len;
}

int StrLenSafeW(const wchar_t *s) {
    int len = 0;
    if (s) {
        while (*s++) len++;
    }
    return len;
}

/*============================================================================
** Copy Functions
**============================================================================*/

char *StrCopySafe(char *dst, const char *src, int maxLen) {
    char *d = dst;
    if (!dst || maxLen <= 0) return dst;

    if (src) {
        while (--maxLen > 0 && *src) {
            *d++ = *src++;
        }
    }
    *d = '\0';
    return dst;
}

wchar_t *StrCopySafeW(wchar_t *dst, const wchar_t *src, int maxLen) {
    wchar_t *d = dst;
    if (!dst || maxLen <= 0) return dst;

    if (src) {
        while (--maxLen > 0 && *src) {
            *d++ = *src++;
        }
    }
    *d = L'\0';
    return dst;
}

/*============================================================================
** Whitespace Functions
**============================================================================*/

void StrSkipWhitespace(const char **p) {
    if (!p || !*p) return;
    while (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n') {
        (*p)++;
    }
}

void StrSkipWhitespaceW(const wchar_t **p) {
    if (!p || !*p) return;
    while (**p == L' ' || **p == L'\t' || **p == L'\r' || **p == L'\n') {
        (*p)++;
    }
}

/*============================================================================
** Find Functions
**============================================================================*/

const char *StrFindChar(const char *s, char ch) {
    if (!s) return NULL;
    while (*s) {
        if (*s == ch) return s;
        s++;
    }
    return NULL;
}

const wchar_t *StrFindCharW(const wchar_t *s, wchar_t ch) {
    if (!s) return NULL;
    while (*s) {
        if (*s == ch) return s;
        s++;
    }
    return NULL;
}

const char *StrAdvanceToChar(const char *p, char ch) {
    if (!p) return p;
    while (*p && *p != ch) p++;
    return p;
}

const wchar_t *StrAdvanceToCharW(const wchar_t *p, wchar_t ch) {
    if (!p) return p;
    while (*p && *p != ch) p++;
    return p;
}

/*============================================================================
** Trim Functions
**============================================================================*/

char *StrTrimRight(char *s) {
    char *end;
    if (!s || !*s) return s;

    end = s;
    while (*end) end++;
    end--;

    while (end >= s && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end-- = '\0';
    }
    return s;
}

wchar_t *StrTrimRightW(wchar_t *s) {
    wchar_t *end;
    if (!s || !*s) return s;

    end = s;
    while (*end) end++;
    end--;

    while (end >= s && (*end == L' ' || *end == L'\t' || *end == L'\r' || *end == L'\n')) {
        *end-- = L'\0';
    }
    return s;
}
