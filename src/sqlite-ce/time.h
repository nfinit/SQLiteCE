/*
** time.h shim for Windows CE
**
** CE doesn't have standard time.h. Provide minimal stubs for SQLite's date.c
*/

#ifndef _TIME_H_CE
#define _TIME_H_CE

/* time_t - use 32-bit for CE */
typedef long time_t;

/* struct tm */
struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

/*
** These functions are referenced by date.c but we stub them out.
** SQLite's date functions will use our OS layer's sqliteOsCurrentTime() instead.
*/
#define time(t)       ((time_t)-1)
#define localtime(t)  ((struct tm *)0)
#define gmtime(t)     ((struct tm *)0)

#endif /* _TIME_H_CE */
