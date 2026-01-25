/*
** SQLite/CE OS Layer Implementation
**
** This file implements the SQLite OS abstraction layer for Windows CE.
** It replaces os.c entirely for CE builds - do not include os.c in the
** project when building for CE.
**
** This implements the Platform Abstraction Layer (PAL) interface defined
** in pal.h. To port to a new platform, implement pal.h functions in a
** new os_<platform>.c file.
**
** See docs/PORTING.md for design rationale and implementation notes.
*/

/*
** IMPORTANT: config_wince.h must be included first.
** It defines OS_WIN=0 and OS_WINCE=1, and provides the OsFile structure
** and off_t type that os.h would normally provide under OS_WIN.
*/
#include "config.h"

#include <windows.h>
#include <stdarg.h>

/*
** CE 2.0 SDK may be missing some constants. Define them here.
*/
#ifndef CP_UTF8
#define CP_UTF8 65001
#endif
#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER ((DWORD)-1)
#endif

#include "os.h"
#include "sqliteInt.h"

/*============================================================================
** sprintf implementation for CE
** CE 2.0 lacks standard sprintf. We implement a minimal version.
**============================================================================*/

int ce_vsprintf(char *buf, const char *fmt, va_list ap) {
    char *p = buf;
    const char *f = fmt;
    char tmp[64];
    int flag_minus, flag_plus, flag_space, flag_hash, flag_zero;
    int width, precision, is_long;
    
    while (*f) {
        if (*f != '%') {
            *p++ = *f++;
            continue;
        }
        f++; /* skip '%' */
        
        /* Parse flags */
        flag_minus = flag_plus = flag_space = flag_hash = flag_zero = 0;
        while (1) {
            if (*f == '-') { flag_minus = 1; f++; }
            else if (*f == '+') { flag_plus = 1; f++; }
            else if (*f == ' ') { flag_space = 1; f++; }
            else if (*f == '#') { flag_hash = 1; f++; }
            else if (*f == '0') { flag_zero = 1; f++; }
            else break;
        }
        if (flag_minus) flag_zero = 0;  /* - overrides 0 */
        if (flag_plus) flag_space = 0;  /* + overrides space */
        
        /* Parse width */
        width = 0;
        if (*f == '*') {
            width = va_arg(ap, int);
            if (width < 0) { flag_minus = 1; width = -width; }
            f++;
        } else {
            while (*f >= '0' && *f <= '9') {
                width = width * 10 + (*f - '0');
                f++;
            }
        }
        
        /* Parse precision */
        precision = -1;
        if (*f == '.') {
            f++;
            if (*f == '*') {
                precision = va_arg(ap, int);
                if (precision < 0) precision = -1;
                f++;
            } else {
                precision = 0;
                while (*f >= '0' && *f <= '9') {
                    precision = precision * 10 + (*f - '0');
                    f++;
                }
            }
        }
        
        /* Parse length modifier */
        is_long = 0;
        if (*f == 'l') { is_long = 1; f++; }
        if (*f == 'l') { f++; }  /* ll treated as l on 32-bit */
        
        /* Format specifier */
        switch (*f) {
            case 'd': case 'i': {
                long val = is_long ? va_arg(ap, long) : (long)va_arg(ap, int);
                int neg = 0;
                char *t = tmp + sizeof(tmp) - 1;
                unsigned long uval;
                int len, pad;
                char sign = 0;
                
                *t = '\0';
                if (val < 0) { neg = 1; uval = (unsigned long)(-(val + 1)) + 1; }
                else { uval = (unsigned long)val; }
                if (uval == 0) *--t = '0';
                while (uval > 0) { *--t = '0' + (uval % 10); uval /= 10; }
                
                /* Apply precision (minimum digits) */
                len = (int)((tmp + sizeof(tmp) - 1) - t);
                if (precision > 0) {
                    while (len < precision) { *--t = '0'; len++; }
                }
                
                /* Determine sign character */
                if (neg) sign = '-';
                else if (flag_plus) sign = '+';
                else if (flag_space) sign = ' ';
                
                len = (int)((tmp + sizeof(tmp) - 1) - t);
                if (sign) len++;
                pad = (width > len) ? width - len : 0;
                
                if (!flag_minus && !flag_zero) while (pad-- > 0) *p++ = ' ';
                if (sign) *p++ = sign;
                if (!flag_minus && flag_zero) while (pad-- > 0) *p++ = '0';
                while (*t) *p++ = *t++;
                if (flag_minus) while (pad-- > 0) *p++ = ' ';
                break;
            }
            case 'u': {
                unsigned long val = is_long ? va_arg(ap, unsigned long) : (unsigned long)va_arg(ap, unsigned);
                char *t = tmp + sizeof(tmp) - 1;
                int len, pad;
                
                *t = '\0';
                if (val == 0) *--t = '0';
                while (val > 0) { *--t = '0' + (val % 10); val /= 10; }
                
                if (precision > 0) {
                    len = (int)((tmp + sizeof(tmp) - 1) - t);
                    while (len < precision) { *--t = '0'; len++; }
                }
                
                len = (int)((tmp + sizeof(tmp) - 1) - t);
                pad = (width > len) ? width - len : 0;
                
                if (!flag_minus && !flag_zero) while (pad-- > 0) *p++ = ' ';
                if (!flag_minus && flag_zero) while (pad-- > 0) *p++ = '0';
                while (*t) *p++ = *t++;
                if (flag_minus) while (pad-- > 0) *p++ = ' ';
                break;
            }
            case 'o': {
                unsigned long val = is_long ? va_arg(ap, unsigned long) : (unsigned long)va_arg(ap, unsigned);
                char *t = tmp + sizeof(tmp) - 1;
                int len, pad, need_prefix;
                
                *t = '\0';
                need_prefix = (flag_hash && val != 0);
                if (val == 0) *--t = '0';
                while (val > 0) { *--t = '0' + (val & 7); val >>= 3; }
                
                if (precision > 0) {
                    len = (int)((tmp + sizeof(tmp) - 1) - t);
                    while (len < precision) { *--t = '0'; len++; }
                }
                if (need_prefix && *t != '0') *--t = '0';
                
                len = (int)((tmp + sizeof(tmp) - 1) - t);
                pad = (width > len) ? width - len : 0;
                
                if (!flag_minus && !flag_zero) while (pad-- > 0) *p++ = ' ';
                if (!flag_minus && flag_zero) while (pad-- > 0) *p++ = '0';
                while (*t) *p++ = *t++;
                if (flag_minus) while (pad-- > 0) *p++ = ' ';
                break;
            }
            case 'x': case 'X': {
                unsigned long val = is_long ? va_arg(ap, unsigned long) : (unsigned long)va_arg(ap, unsigned);
                char *t = tmp + sizeof(tmp) - 1;
                const char *hex = (*f == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
                int len, pad, need_prefix;
                
                *t = '\0';
                need_prefix = (flag_hash && val != 0);
                if (val == 0) *--t = '0';
                while (val > 0) { *--t = hex[val & 0xF]; val >>= 4; }
                
                if (precision > 0) {
                    len = (int)((tmp + sizeof(tmp) - 1) - t);
                    while (len < precision) { *--t = '0'; len++; }
                }
                
                len = (int)((tmp + sizeof(tmp) - 1) - t);
                if (need_prefix) len += 2;
                pad = (width > len) ? width - len : 0;
                
                if (!flag_minus && !flag_zero) while (pad-- > 0) *p++ = ' ';
                if (need_prefix) { *p++ = '0'; *p++ = (*f == 'X') ? 'X' : 'x'; }
                if (!flag_minus && flag_zero) while (pad-- > 0) *p++ = '0';
                while (*t) *p++ = *t++;
                if (flag_minus) while (pad-- > 0) *p++ = ' ';
                break;
            }
            case 'p': {
                unsigned long val = (unsigned long)va_arg(ap, void*);
                char *t = tmp + sizeof(tmp) - 1;
                int len, pad;
                
                *t = '\0';
                if (val == 0) *--t = '0';
                while (val > 0) { *--t = "0123456789abcdef"[val & 0xF]; val >>= 4; }
                
                len = (int)((tmp + sizeof(tmp) - 1) - t) + 2;
                pad = (width > len) ? width - len : 0;
                
                if (!flag_minus) while (pad-- > 0) *p++ = ' ';
                *p++ = '0'; *p++ = 'x';
                while (*t) *p++ = *t++;
                if (flag_minus) while (pad-- > 0) *p++ = ' ';
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char*);
                int len = 0, pad;
                const char *t;
                
                if (!s) s = "(null)";
                t = s;
                while (*t) { len++; t++; }
                if (precision >= 0 && len > precision) len = precision;
                pad = (width > len) ? width - len : 0;
                
                if (!flag_minus) while (pad-- > 0) *p++ = ' ';
                while (len-- > 0) *p++ = *s++;
                if (flag_minus) while (pad-- > 0) *p++ = ' ';
                break;
            }
            case 'c': {
                char c = (char)va_arg(ap, int);
                int pad = (width > 1) ? width - 1 : 0;
                
                if (!flag_minus) while (pad-- > 0) *p++ = ' ';
                *p++ = c;
                if (flag_minus) while (pad-- > 0) *p++ = ' ';
                break;
            }
            case '%':
                *p++ = '%';
                break;
            case 'e': case 'E': {
                double val = va_arg(ap, double);
                int neg = 0, exp = 0, i;
                int prec = (precision < 0) ? 6 : precision;
                char *t;
                double rounder;
                int len, pad;
                char sign = 0;
                char expchar = *f;
                
                if (val < 0) { neg = 1; val = -val; }
                
                /* Normalize to 1.xxx * 10^exp */
                if (val != 0) {
                    while (val >= 10) { val /= 10; exp++; }
                    while (val < 1) { val *= 10; exp--; }
                }
                
                /* Apply rounding */
                rounder = 0.5;
                for (i = 0; i < prec; i++) rounder *= 0.1;
                val += rounder;
                if (val >= 10) { val /= 10; exp++; }
                
                /* Build number in tmp */
                t = tmp;
                if (neg) sign = '-';
                else if (flag_plus) sign = '+';
                else if (flag_space) sign = ' ';
                
                /* Integer part (single digit) */
                *t++ = '0' + (int)val;
                val -= (int)val;
                
                /* Fractional part */
                if (prec > 0 || flag_hash) {
                    *t++ = '.';
                    for (i = 0; i < prec; i++) {
                        val *= 10;
                        *t++ = '0' + (int)val;
                        val -= (int)val;
                    }
                }
                
                /* Exponent */
                *t++ = expchar;
                *t++ = (exp < 0) ? '-' : '+';
                if (exp < 0) exp = -exp;
                if (exp < 10) *t++ = '0';
                if (exp >= 100) { *t++ = '0' + (exp / 100); exp %= 100; }
                if (exp >= 10) { *t++ = '0' + (exp / 10); exp %= 10; }
                *t++ = '0' + exp;
                *t = '\0';
                
                len = (int)(t - tmp);
                if (sign) len++;
                pad = (width > len) ? width - len : 0;
                
                if (!flag_minus && !flag_zero) while (pad-- > 0) *p++ = ' ';
                if (sign) *p++ = sign;
                if (!flag_minus && flag_zero) while (pad-- > 0) *p++ = '0';
                t = tmp;
                while (*t) *p++ = *t++;
                if (flag_minus) while (pad-- > 0) *p++ = ' ';
                break;
            }
            case 'g': case 'f': {
                double val = va_arg(ap, double);
                int neg = 0;
                int prec = (precision < 0) ? 6 : precision;
                int intpart, i;
                char *t;
                double rounder;
                int len, pad;
                char sign = 0;
                
                if (val < 0) { neg = 1; val = -val; }
                
                /* Apply rounding */
                rounder = 0.5;
                for (i = 0; i < prec; i++) rounder *= 0.1;
                val += rounder;
                
                intpart = (int)val;
                
                /* Build number in tmp */
                t = tmp + 30;  /* Start in middle, build int part backwards */
                *t = '\0';
                if (intpart == 0) *--t = '0';
                while (intpart > 0) { *--t = '0' + (intpart % 10); intpart /= 10; }
                
                if (neg) sign = '-';
                else if (flag_plus) sign = '+';
                else if (flag_space) sign = ' ';
                
                /* Calculate length */
                len = (int)((tmp + 30) - t);
                if (prec > 0 || flag_hash) len += 1 + prec;
                if (sign) len++;
                pad = (width > len) ? width - len : 0;
                
                /* Output */
                if (!flag_minus && !flag_zero) while (pad-- > 0) *p++ = ' ';
                if (sign) *p++ = sign;
                if (!flag_minus && flag_zero) while (pad-- > 0) *p++ = '0';
                while (*t) *p++ = *t++;
                
                if (prec > 0 || flag_hash) {
                    *p++ = '.';
                    val = val - (int)(val + rounder);
                    for (i = 0; i < prec; i++) {
                        val *= 10;
                        *p++ = '0' + (int)val;
                        val -= (int)val;
                    }
                }
                if (flag_minus) while (pad-- > 0) *p++ = ' ';
                break;
            }
            default:
                *p++ = '%';
                *p++ = *f;
                break;
        }
        f++;
    }
    *p = '\0';
    return (int)(p - buf);
}

int ce_sprintf(char *buf, const char *fmt, ...) {
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = ce_vsprintf(buf, fmt, ap);
    va_end(ap);
    return ret;
}

/*============================================================================
** Debug/Trace support
**============================================================================*/

/*
** Output a UTF-8 string to debugger via OutputDebugStringW.
** Part of PAL - see pal.h for interface documentation.
*/
void ce_debug_output(const char *zMsg) {
    wchar_t wzMsg[512];
    if (zMsg) {
        MultiByteToWideChar(CP_ACP, 0, zMsg, -1, wzMsg, 512);
        OutputDebugStringW(wzMsg);
    }
}

#ifdef SQLITE_CE_TRACE
void ce_trace_output(const char *zMsg) {
    ce_debug_output(zMsg);
    OutputDebugStringW(L"\r\n");
}
#endif

/*
** Assertion failure handler - outputs to debugger and breaks.
*/
void ce_assert_fail(const char *exp, const char *file, int line) {
    char zBuf[512];
    sprintf(zBuf, "ASSERT FAILED: %s at %s:%d", exp, file, line);
    ce_debug_output(zBuf);
    OutputDebugStringW(L"\r\n");
    DebugBreak();
}

/*
** Debug printf/fprintf implementations for stdio.h shim.
*/
#ifdef SQLITE_DEBUG
#include <stdarg.h>
int ce_printf(const char *fmt, ...) {
    char zBuf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(zBuf, fmt, ap);
    va_end(ap);
    ce_debug_output(zBuf);
    return 0;
}
int ce_fprintf(FILE *f, const char *fmt, ...) {
    char zBuf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(zBuf, fmt, ap);
    va_end(ap);
    ce_debug_output(zBuf);
    return 0;
}
#endif

/*============================================================================
** String conversion helpers
**
** Windows CE only provides Unicode (wide character) APIs, but SQLite uses
** char* strings internally (UTF-8 assumed). These functions convert between.
**============================================================================*/

/*
** Convert a UTF-8 string to wide characters.
** Assumes wzWide has room for nWide characters including null terminator.
** Note: CE 2.0 may not support CP_UTF8, so we use CP_ACP for compatibility.
*/
static void ce_utf8_to_wide(const char *zUtf8, wchar_t *wzWide, int nWide) {
    if (zUtf8 == NULL || wzWide == NULL || nWide <= 0) {
        if (wzWide && nWide > 0) wzWide[0] = 0;
        return;
    }
    MultiByteToWideChar(CP_ACP, 0, zUtf8, -1, wzWide, nWide);
}

/*
** Convert a wide character string to UTF-8.
** Assumes zUtf8 has room for nUtf8 bytes including null terminator.
*/
static void ce_wide_to_utf8(const wchar_t *wzWide, char *zUtf8, int nUtf8) {
    if (wzWide == NULL || zUtf8 == NULL || nUtf8 <= 0) {
        if (zUtf8 && nUtf8 > 0) zUtf8[0] = 0;
        return;
    }
    WideCharToMultiByte(CP_UTF8, 0, wzWide, -1, zUtf8, nUtf8, NULL, NULL);
}

/*============================================================================
** File Operations
**============================================================================*/

/*
** Delete the named file.
*/
int sqliteOsDelete(const char *zFilename) {
    wchar_t wzFilename[SQLITE_CE_MAX_PATH];

    CE_TRACE("sqliteOsDelete");
    ce_utf8_to_wide(zFilename, wzFilename, SQLITE_CE_MAX_PATH);

    if (!DeleteFileW(wzFilename)) {
        /* File doesn't exist or couldn't be deleted - check error */
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            /* File doesn't exist - treat as success for SQLite compatibility */
            return SQLITE_OK;
        }
        return SQLITE_IOERR;
    }

    return SQLITE_OK;
}

/*
** Return TRUE if the named file exists.
*/
int sqliteOsFileExists(const char *zFilename) {
    wchar_t wzFilename[SQLITE_CE_MAX_PATH];
    DWORD attrs;
    
    ce_utf8_to_wide(zFilename, wzFilename, SQLITE_CE_MAX_PATH);
    attrs = GetFileAttributesW(wzFilename);
    
    return (attrs != 0xFFFFFFFF);
}

/*
** Attempt to open a file for both reading and writing. If that fails,
** try opening it read-only. If the file does not exist, try to create it.
*/
int sqliteOsOpenReadWrite(
    const char *zFilename,
    OsFile *id,
    int *pReadonly
) {
    wchar_t wzFilename[SQLITE_CE_MAX_PATH];
    HANDLE h;
    DWORD err;
    
    CE_TRACE("sqliteOsOpenReadWrite");
    if (zFilename) {
        CE_TRACE(zFilename);
    } else {
        CE_TRACE("(null filename)");
    }
    ce_utf8_to_wide(zFilename, wzFilename, SQLITE_CE_MAX_PATH);
    
    /* Try read-write first */
    h = CreateFileW(wzFilename,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    
    if (h == INVALID_HANDLE_VALUE) {
        err = GetLastError();
        CE_TRACE("CreateFileW RW failed");
        /* Fall back to read-only */
        h = CreateFileW(wzFilename,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        
        if (h == INVALID_HANDLE_VALUE) {
            CE_TRACE("sqliteOsOpenReadWrite: CANTOPEN");
            return SQLITE_CANTOPEN;
        }
        *pReadonly = 1;
    } else {
        *pReadonly = 0;
    }
    
    id->h = h;
    id->locked = 0;
    CE_TRACE("sqliteOsOpenReadWrite: OK");
    
    return SQLITE_OK;
}

/*
** Attempt to open a new file for exclusive access.
*/
int sqliteOsOpenExclusive(const char *zFilename, OsFile *id, int delFlag) {
    wchar_t wzFilename[SQLITE_CE_MAX_PATH];
    HANDLE h;
    DWORD fileflags;
    
    CE_TRACE("sqliteOsOpenExclusive");
    ce_utf8_to_wide(zFilename, wzFilename, SQLITE_CE_MAX_PATH);
    
    if (delFlag) {
        fileflags = FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE;
    } else {
        fileflags = FILE_ATTRIBUTE_NORMAL;
    }
    
    h = CreateFileW(wzFilename,
        GENERIC_READ | GENERIC_WRITE,
        0,  /* No sharing - exclusive */
        NULL,
        CREATE_ALWAYS,
        fileflags,
        NULL);
    
    if (h == INVALID_HANDLE_VALUE) {
        CE_TRACE("sqliteOsOpenExclusive: CANTOPEN");
        return SQLITE_CANTOPEN;
    }
    
    id->h = h;
    id->locked = 0;
    
    return SQLITE_OK;
}

/*
** Attempt to open a file for read-only access.
*/
int sqliteOsOpenReadOnly(const char *zFilename, OsFile *id) {
    wchar_t wzFilename[SQLITE_CE_MAX_PATH];
    HANDLE h;
    
    CE_TRACE("sqliteOsOpenReadOnly");
    ce_utf8_to_wide(zFilename, wzFilename, SQLITE_CE_MAX_PATH);
    
    h = CreateFileW(wzFilename,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    
    if (h == INVALID_HANDLE_VALUE) {
        CE_TRACE("sqliteOsOpenReadOnly: CANTOPEN");
        return SQLITE_CANTOPEN;
    }
    
    id->h = h;
    id->locked = 0;
    
    return SQLITE_OK;
}

/*
** Open a directory. This is a no-op on Windows CE (and Windows generally)
** as directory syncing isn't needed for file system consistency.
*/
int sqliteOsOpenDirectory(const char *zDirname, OsFile *id) {
    /* No-op on CE - directory syncing not required */
    return SQLITE_OK;
}

/*
** Create a temporary file name.
*/
int sqliteOsTempFileName(char *zBuf) {
    static const char zChars[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";
    int i, j;
    char zDir[SQLITE_CE_MAX_PATH];
    
    CE_TRACE("sqliteOsTempFileName");
    
#if CE_HAS_GETTEMPPATH
    {
        wchar_t wzTempPath[SQLITE_CE_MAX_PATH];
        GetTempPathW(SQLITE_CE_MAX_PATH - 30, wzTempPath);
        ce_wide_to_utf8(wzTempPath, zDir, SQLITE_CE_MAX_PATH);
        
        /* Remove trailing backslash */
        for (i = strlen(zDir); i > 0 && zDir[i-1] == '\\'; i--) {}
        zDir[i] = 0;
    }
#else
    /* CE 2.0 doesn't have GetTempPath - use our configured default */
    {
        const char *src = SQLITE_CE_TEMP_DIRECTORY;
        int n = 0;
        while (src[n] && n < SQLITE_CE_MAX_PATH - 1) {
            zDir[n] = src[n];
            n++;
        }
        zDir[n] = 0;
    }
#endif
    
    /* Generate random filename, retry until we find one that doesn't exist */
    for (;;) {
        sprintf(zBuf, "%s\\" TEMP_FILE_PREFIX, zDir);
        j = strlen(zBuf);
        sqliteRandomness(15, &zBuf[j]);
        for (i = 0; i < 15; i++, j++) {
            zBuf[j] = zChars[((unsigned char)zBuf[j]) % (sizeof(zChars) - 1)];
        }
        zBuf[j] = 0;
        
        if (!sqliteOsFileExists(zBuf)) break;
    }
    
    return SQLITE_OK;
}

/*
** Close a file.
*/
int sqliteOsClose(OsFile *id) {
    CE_TRACE("sqliteOsClose");
    CloseHandle(id->h);
    return SQLITE_OK;
}

/*
** Read data from a file into a buffer.
*/
int sqliteOsRead(OsFile *id, void *pBuf, int amt) {
    DWORD got;
    
    if (!ReadFile(id->h, pBuf, amt, &got, NULL)) {
        CE_TRACE("sqliteOsRead: ReadFile failed");
        got = 0;
    }
    
    if (got == (DWORD)amt) {
        return SQLITE_OK;
    } else {
        CE_TRACE("sqliteOsRead: IOERR (short read)");
        return SQLITE_IOERR;
    }
}

/*
** Write data from a buffer into a file.
*/
int sqliteOsWrite(OsFile *id, const void *pBuf, int amt) {
    DWORD wrote;
    int rc;
    
    while (amt > 0) {
        rc = WriteFile(id->h, pBuf, amt, &wrote, NULL);
        if (!rc || wrote == 0) {
            CE_TRACE("sqliteOsWrite: FULL");
            return SQLITE_FULL;
        }
        amt -= wrote;
        pBuf = &((const char *)pBuf)[wrote];
    }
    
    return SQLITE_OK;
}

/*
** Move the read/write pointer in a file.
*/
int sqliteOsSeek(OsFile *id, off_t offset) {
    LONG upperBits = (LONG)(offset >> 32);
    LONG lowerBits = (LONG)(offset & 0xFFFFFFFF);
    DWORD result;
    
    result = SetFilePointer(id->h, lowerBits, &upperBits, FILE_BEGIN);
    
    if (result == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
        CE_TRACE("sqliteOsSeek: SetFilePointer failed");
        return SQLITE_IOERR;
    }
    
    return SQLITE_OK;
}

/*
** Sync the file to disk.
*/
int sqliteOsSync(OsFile *id) {
    CE_TRACE("sqliteOsSync");
    if (FlushFileBuffers(id->h)) {
        return SQLITE_OK;
    } else {
        CE_TRACE("sqliteOsSync: IOERR");
        return SQLITE_IOERR;
    }
}

/*
** Truncate an open file to a specified size.
*/
int sqliteOsTruncate(OsFile *id, off_t nByte) {
    LONG upperBits = (LONG)(nByte >> 32);
    LONG lowerBits = (LONG)(nByte & 0xFFFFFFFF);
    DWORD result;

    CE_TRACE("sqliteOsTruncate");

    result = SetFilePointer(id->h, lowerBits, &upperBits, FILE_BEGIN);
    if (result == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
        CE_TRACE("sqliteOsTruncate: seek IOERR");
        return SQLITE_IOERR;
    }

    if (!SetEndOfFile(id->h)) {
        CE_TRACE("sqliteOsTruncate: IOERR");
        return SQLITE_IOERR;
    }

    return SQLITE_OK;
}

/*
** Determine the current size of a file in bytes.
*/
int sqliteOsFileSize(OsFile *id, off_t *pSize) {
    DWORD upperBits;
    DWORD lowerBits;
    
    lowerBits = GetFileSize(id->h, &upperBits);
    
    if (lowerBits == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) {
        CE_TRACE("sqliteOsFileSize: IOERR");
        return SQLITE_IOERR;
    }
    
    *pSize = (((off_t)upperBits) << 32) + lowerBits;
    return SQLITE_OK;
}

/*============================================================================
** File Locking
**
** CE 2.0 does not have LockFileEx(). We implement a simplified locking
** scheme that assumes single-process access for CE 2.0, and use proper
** locking for CE 2.11+.
**============================================================================*/

/*
** Locking byte range constants - same as desktop Windows SQLite
*/
#define N_LOCKBYTE       10239
#define FIRST_LOCKBYTE   (0xFFFFFFFF - N_LOCKBYTE)

#if CE_HAS_LOCKFILEEX

/*
** CE 2.11+ locking using LockFileEx for shared locks
*/

int sqliteOsReadLock(OsFile *id) {
    int res;
    int lk;
    int cnt = 100;
    OVERLAPPED ovlp;
    
    CE_TRACE("sqliteOsReadLock (LockFileEx path)");
    
    if (id->locked > 0) {
        /* Already have a read lock */
        return SQLITE_OK;
    }
    
    /* Get a random lock byte */
    sqliteRandomness(sizeof(lk), &lk);
    lk = (lk & 0x7FFFFFFF) % N_LOCKBYTE + 1;
    
    /* Acquire coordination lock */
    while (cnt-- > 0 && (res = LockFile(id->h, FIRST_LOCKBYTE, 0, 1, 0)) == 0) {
        Sleep(1);
    }
    
    if (res) {
        /* Release any existing write lock */
        UnlockFile(id->h, FIRST_LOCKBYTE + 1, 0, N_LOCKBYTE, 0);
        
        /* Acquire shared read lock */
        memset(&ovlp, 0, sizeof(ovlp));
        ovlp.Offset = FIRST_LOCKBYTE + 1;
        ovlp.OffsetHigh = 0;
        
        res = LockFileEx(id->h, LOCKFILE_FAIL_IMMEDIATELY, 0, N_LOCKBYTE, 0, &ovlp);
        
        /* Release coordination lock */
        UnlockFile(id->h, FIRST_LOCKBYTE, 0, 1, 0);
    }
    
    if (res) {
        id->locked = lk;
        return SQLITE_OK;
    } else {
        CE_TRACE("sqliteOsReadLock: BUSY");
        return SQLITE_BUSY;
    }
}

int sqliteOsWriteLock(OsFile *id) {
    int res;
    int cnt = 100;
    
    CE_TRACE("sqliteOsWriteLock (LockFileEx path)");
    
    if (id->locked < 0) {
        /* Already have a write lock */
        return SQLITE_OK;
    }
    
    /* Acquire coordination lock */
    while (cnt-- > 0 && (res = LockFile(id->h, FIRST_LOCKBYTE, 0, 1, 0)) == 0) {
        Sleep(1);
    }
    
    if (res) {
        /* Release any existing read lock */
        if (id->locked > 0) {
            UnlockFile(id->h, FIRST_LOCKBYTE + 1, 0, N_LOCKBYTE, 0);
        }
        
        /* Acquire exclusive write lock */
        res = LockFile(id->h, FIRST_LOCKBYTE + 1, 0, N_LOCKBYTE, 0);
        
        /* Release coordination lock */
        UnlockFile(id->h, FIRST_LOCKBYTE, 0, 1, 0);
    }
    
    if (res) {
        id->locked = -1;
        return SQLITE_OK;
    } else {
        CE_TRACE("sqliteOsWriteLock: BUSY");
        return SQLITE_BUSY;
    }
}

int sqliteOsUnlock(OsFile *id) {
    CE_TRACE("sqliteOsUnlock (LockFileEx path)");
    
    if (id->locked == 0) {
        return SQLITE_OK;
    }
    
    UnlockFile(id->h, FIRST_LOCKBYTE + 1, 0, N_LOCKBYTE, 0);
    id->locked = 0;
    
    return SQLITE_OK;
}

#else /* !CE_HAS_LOCKFILEEX - CE 2.0 simple locking */

/*
** CE 2.0 locking - single process assumption
** We just track lock state internally without actual file locks.
** This is safe as long as only one process accesses the database.
*/

int sqliteOsReadLock(OsFile *id) {
    CE_TRACE("sqliteOsReadLock (no-op path)");
    
    if (id->locked < 0) {
        /* Downgrade from write to read */
        id->locked = 1;
    } else if (id->locked == 0) {
        id->locked = 1;
    }
    /* Already have read lock - do nothing */
    return SQLITE_OK;
}

int sqliteOsWriteLock(OsFile *id) {
    CE_TRACE("sqliteOsWriteLock (no-op path)");
    id->locked = -1;
    return SQLITE_OK;
}

int sqliteOsUnlock(OsFile *id) {
    CE_TRACE("sqliteOsUnlock (no-op path)");
    id->locked = 0;
    return SQLITE_OK;
}

#endif /* CE_HAS_LOCKFILEEX */

/*============================================================================
** Miscellaneous OS Functions
**============================================================================*/

/*
** Get information to seed the random number generator.
** The buffer is 256 bytes.
*/
int sqliteOsRandomSeed(char *zBuf) {
    SYSTEMTIME st;
    DWORD ticks;
    
    /* Initialize buffer to zero (required - see SQLite comments) */
    memset(zBuf, 0, 256);
    
#ifndef SQLITE_TEST
    /* Get system time */
    GetSystemTime(&st);
    memcpy(zBuf, &st, sizeof(st));
    
    /* Add tick count for additional entropy */
    ticks = GetTickCount();
    memcpy(&zBuf[sizeof(SYSTEMTIME)], &ticks, sizeof(ticks));
    
    /* On CE we could also use CeGenRandom if available (CE 3.0+) */
    /* but GetTickCount provides reasonable entropy for SQLite's needs */
#endif
    
    return SQLITE_OK;
}

/*
** Sleep for a little while. Return the amount of time slept in ms.
*/
int sqliteOsSleep(int ms) {
    Sleep(ms);
    return ms;
}

/*
** Find the current time (in Universal Coordinated Time).
** Write the current time and date as a Julian Day number into *prNow.
** Return 0 on success, 1 on failure.
*/
int sqliteOsCurrentTime(double *prNow) {
#if CE_HAS_GETSYSTEMTIMEASFILETIME
    FILETIME ft;
    double now;
    
    GetSystemTimeAsFileTime(&ft);
    
    /* FILETIME: 100-nanosecond intervals since January 1, 1601 (JD 2305813.5) */
    now = ((double)ft.dwHighDateTime) * 4294967296.0;
    *prNow = (now + ft.dwLowDateTime) / 864000000000.0 + 2305813.5;
#else
    /* CE 2.0 fallback - use GetSystemTime and calculate Julian Day */
    SYSTEMTIME st;
    int Y, M, D;
    int A, B;
    double JD;
    
    GetSystemTime(&st);
    
    Y = st.wYear;
    M = st.wMonth;
    D = st.wDay;
    
    /* Julian Day calculation algorithm */
    if (M <= 2) {
        Y -= 1;
        M += 12;
    }
    A = Y / 100;
    B = 2 - A + (A / 4);
    
    JD = (int)(365.25 * (Y + 4716)) + (int)(30.6001 * (M + 1)) + D + B - 1524.5;
    
    /* Add time of day as fraction */
    JD += st.wHour / 24.0
        + st.wMinute / 1440.0
        + st.wSecond / 86400.0
        + st.wMilliseconds / 86400000.0;
    
    *prNow = JD;
#endif
    
#ifdef SQLITE_TEST
    {
        extern int sqlite_current_time;
        if (sqlite_current_time) {
            *prNow = sqlite_current_time / 86400.0 + 2440587.5;
        }
    }
#endif
    
    return 0;
}

/*============================================================================
** Mutex / Critical Section
**============================================================================*/

static int inMutex = 0;

#if THREADSAFE
static CRITICAL_SECTION cs;
static int csInitialized = 0;
#endif

/*
** Enter the global mutex.
*/
void sqliteOsEnterMutex(void) {
#if THREADSAFE
    if (!csInitialized) {
        InitializeCriticalSection(&cs);
        csInitialized = 1;
    }
    EnterCriticalSection(&cs);
#endif
    inMutex = 1;
}

/*
** Leave the global mutex.
*/
void sqliteOsLeaveMutex(void) {
    inMutex = 0;
#if THREADSAFE
    LeaveCriticalSection(&cs);
#endif
}

/*============================================================================
** Full Pathname Conversion
**============================================================================*/

/*
** Turn a relative pathname into a full pathname.
** Returns a pointer to memory obtained from sqliteMalloc().
** Caller must free with sqliteFree().
*/
char *sqliteOsFullPathname(const char *zRelative) {
    char *zFull;
    int nByte;
    
    /*
    ** On CE, we don't have a true "current directory" concept in the
    ** same way as desktop Windows. Paths starting with \ are absolute.
    ** For relative paths, we prepend \.
    */
    if (zRelative[0] == '\\' || zRelative[0] == '/') {
        /* Already absolute */
        nByte = strlen(zRelative) + 1;
        zFull = sqliteMalloc(nByte);
        if (zFull) {
            strcpy(zFull, zRelative);
        }
    } else if (zRelative[0] != '\0' && zRelative[1] == ':') {
        /* Has drive letter (unusual on CE but handle it) */
        nByte = strlen(zRelative) + 1;
        zFull = sqliteMalloc(nByte);
        if (zFull) {
            strcpy(zFull, zRelative);
        }
    } else {
        /* Relative path - prepend backslash to make it absolute */
        nByte = strlen(zRelative) + 2;
        zFull = sqliteMalloc(nByte);
        if (zFull) {
            zFull[0] = '\\';
            strcpy(&zFull[1], zRelative);
        }
    }
    
    /* Convert forward slashes to backslashes for consistency */
    if (zFull) {
        char *p;
        for (p = zFull; *p; p++) {
            if (*p == '/') *p = '\\';
        }
    }
    
    return zFull;
}

/*============================================================================
** End of os_wince.c
**============================================================================*/
