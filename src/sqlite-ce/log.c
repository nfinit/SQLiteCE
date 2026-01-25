/*
** SQLite/CE Logging Implementation
**
** Outputs log messages to debugger via OutputDebugStringW.
** Format: [LEVEL] file:line message
*/

#include <windows.h>
#include <stdarg.h>
#include "log.h"

/* Runtime log level (defaults to compile-time level) */
static int g_logLevel = SQLITE_CE_LOG_LEVEL;

/* Level names for output */
static const char *g_levelNames[] = {
    "NONE",
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG",
    "TRACE"
};

/*
** Core logging function.
** Formats message and outputs to debugger.
*/
void sqliteLog(int level, const char *file, int line, const char *fmt, ...) {
    char buf[512];
    wchar_t wbuf[512];
    char *p;
    const char *s;
    va_list ap;
    int len;

    /* Check runtime level */
    if (level > g_logLevel || level <= LOG_LEVEL_NONE) {
        return;
    }

    /* Format: [LEVEL] filename:line: message */
    p = buf;

    /* Add level tag */
    *p++ = '[';
    s = g_levelNames[level];
    while (*s) *p++ = *s++;
    *p++ = ']';
    *p++ = ' ';

    /* Add filename (just the base name, not full path) */
    s = file;
    {
        const char *lastSlash = file;
        while (*s) {
            if (*s == '\\' || *s == '/') lastSlash = s + 1;
            s++;
        }
        s = lastSlash;
    }
    while (*s) *p++ = *s++;
    *p++ = ':';

    /* Add line number */
    {
        char numBuf[16];
        int n = line;
        int i = 0;
        if (n == 0) {
            numBuf[i++] = '0';
        } else {
            while (n > 0) {
                numBuf[i++] = (char)('0' + (n % 10));
                n /= 10;
            }
        }
        while (i > 0) {
            *p++ = numBuf[--i];
        }
    }
    *p++ = ':';
    *p++ = ' ';

    /* Format message */
    va_start(ap, fmt);
    len = wvsprintfA(p, fmt, ap);
    va_end(ap);
    p += len;

    /* Add newline */
    *p++ = '\r';
    *p++ = '\n';
    *p = '\0';

    /* Convert to wide and output */
    MultiByteToWideChar(CP_ACP, 0, buf, -1, wbuf, 512);
    OutputDebugStringW(wbuf);
}

/*
** Set runtime log level.
** Cannot exceed compile-time level.
*/
int sqliteLogSetLevel(int level) {
    int old = g_logLevel;

    if (level < LOG_LEVEL_NONE) level = LOG_LEVEL_NONE;
    if (level > SQLITE_CE_LOG_LEVEL) level = SQLITE_CE_LOG_LEVEL;

    g_logLevel = level;
    return old;
}

/*
** Get current runtime log level.
*/
int sqliteLogGetLevel(void) {
    return g_logLevel;
}
