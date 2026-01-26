/*
** SQLiteCEdit - Query execution
*/

#include "globals.h"
#include "constants.h"
#include "strpool.h"
#include "strintern.h"
#include "stmtcache.h"
#include "allocators.h"

/*============================================================================
** Progress callback for query abort
**============================================================================*/

static int ProgressCallback(void *arg) {
    MSG msg;
    (void)arg;
    /* Process pending messages to keep UI responsive */
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        /* Check for Ctrl+C to abort */
        if (msg.message == WM_KEYDOWN && msg.wParam == 'C' &&
            GetKeyState(VK_CONTROL) < 0) {
            g_abortQuery = 1;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return g_abortQuery;
}

/*============================================================================
** Query Execution State
**============================================================================*/

static int g_nRows;
static int g_nCols;
static int g_totalRows;

/* Result buffering for aligned output - limits defined in constants.h */
static char *g_results[MAX_RESULT_ROWS + 1][MAX_RESULT_COLS];
static int g_colWidths[MAX_RESULT_COLS];
static int g_resultRows;

/* String pool for result data - reduces LocalAlloc calls by ~95% */
static StrPool *g_resultPool = NULL;

/* String intern table for last result storage - deduplicates repeated values */
static StringIntern *g_lastResultIntern = NULL;

/* Prepared statement cache - reduces compile overhead for repeated queries */
static StmtCache *g_stmtCache = NULL;

/*
** Execute SQL using cached prepared statements.
** Creates cache on first use (16 entry LRU cache).
*/
static int CachedExec(const char *sql, int (*callback)(void*,int,char**,char**),
                      void *arg, char **errmsg) {
    /* Initialize cache on first use */
    if (!g_stmtCache) {
        g_stmtCache = StmtCacheCreate(16);
    }
    return StmtCacheExec(g_stmtCache, g_db, sql, callback, arg, errmsg);
}

static int strlen_safe(const char *s) {
    int n = 0;
    if (s) while (*s++) n++;
    return n;
}

static void FreeResults(void) {
    int r, c;

    /* Clear pointer array */
    for (r = 0; r <= g_resultRows; r++) {
        for (c = 0; c < g_nCols; c++) {
            g_results[r][c] = NULL;
        }
    }
    for (c = 0; c < MAX_RESULT_COLS; c++) g_colWidths[c] = 0;
    g_resultRows = 0;

    /* Reset the string pool - keeps allocated chunks for reuse */
    if (g_resultPool) {
        StrPoolReset(g_resultPool);
    }
}

static void OutputPadded(const char *s, int width) {
    int len = strlen_safe(s);
    Output(s);
    while (len++ < width) Output(" ");
}

static void OutputResults(void) {
    int r, c;

    /* Output headers */
    for (c = 0; c < g_nCols; c++) {
        if (c > 0) Output("  ");
        OutputPadded(g_results[0][c] ? g_results[0][c] : "", g_colWidths[c]);
    }
    OutputLine("");

    /* Output separator */
    for (c = 0; c < g_nCols; c++) {
        int w = g_colWidths[c];
        if (c > 0) Output("  ");
        while (w-- > 0) Output("-");
    }
    OutputLine("");

    /* Output data rows */
    for (r = 1; r <= g_resultRows; r++) {
        for (c = 0; c < g_nCols; c++) {
            if (c > 0) Output("  ");
            OutputPadded(g_results[r][c] ? g_results[r][c] : "", g_colWidths[c]);
        }
        OutputLine("");
    }
}

void FreeLastResults(void) {
    /* Free the pointer array only - strings are in the intern table */
    FREE(g_lastResult);
    g_lastResultRows = 0;
    g_lastResultCols = 0;

    /* Reset the intern table - keeps allocated memory for reuse */
    if (g_lastResultIntern) {
        StringInternReset(g_lastResultIntern);
    }
}

static void StoreLastResults(void) {
    int r, c;

    /* Free previous */
    FreeLastResults();
    if (g_nCols == 0 || g_resultRows == 0) return;

    /* Create or reset the intern table - 1024 buckets for good distribution */
    if (!g_lastResultIntern) {
        g_lastResultIntern = StringInternCreate(1024);
        if (!g_lastResultIntern) return;
    }

    /* Allocate flat array like sqlite_get_table: (nRows+1) * nCols pointers */
    g_lastResult = ALLOC(char *, (g_resultRows + 1) * g_nCols);
    if (!g_lastResult) return;

    g_lastResultRows = g_resultRows;
    g_lastResultCols = g_nCols;

    /* Copy header row - use string interning (headers often repeat across queries) */
    for (c = 0; c < g_nCols; c++) {
        if (g_results[0][c]) {
            g_lastResult[c] = StringInternGet(g_lastResultIntern, g_results[0][c]);
        } else {
            g_lastResult[c] = NULL;
        }
    }

    /* Copy data rows - use string interning (deduplicates repeated values) */
    for (r = 1; r <= g_resultRows; r++) {
        for (c = 0; c < g_nCols; c++) {
            int idx = r * g_nCols + c;
            if (g_results[r][c]) {
                g_lastResult[idx] = StringInternGet(g_lastResultIntern, g_results[r][c]);
            } else {
                g_lastResult[idx] = NULL;
            }
        }
    }
}

static void FlushResultSet(void) {
    char buf[32];
    char *p = buf + 30;
    int n = g_nRows;
    buf[31] = '\0';
    *p = '\0';
    while (n > 0) { *--p = '0' + (n % 10); n /= 10; }
    OutputResults();
    OutputLine("");
    Output(p);
    OutputLine(" row(s) returned.");
    OutputLine("");
    g_totalRows += g_nRows;
    StoreLastResults();
    FreeResults();
    g_nRows = 0;
    g_nCols = 0;
}

static int QueryCallback(void *arg, int argc, char **argv, char **cols) {
    int i, len;
    int colsChanged = 0;
    (void)arg;

    if (argc > MAX_RESULT_COLS) argc = MAX_RESULT_COLS;

    /* Ensure we have a result pool */
    if (!g_resultPool) {
        g_resultPool = StrPoolCreate(32 * 1024);  /* 32KB chunks */
        if (!g_resultPool) return 1;  /* Allocation failure */
    }

    /* Check if columns changed (new statement) */
    if (g_nRows > 0) {
        if (argc != g_nCols) {
            colsChanged = 1;
        } else {
            /* Same count - check if names differ */
            for (i = 0; i < argc; i++) {
                const char *newCol = cols[i] ? cols[i] : "";
                const char *oldCol = g_results[0][i] ? g_results[0][i] : "";
                const char *a = newCol, *b = oldCol;
                while (*a && *b && *a == *b) { a++; b++; }
                if (*a != *b) { colsChanged = 1; break; }
            }
        }
        if (colsChanged) FlushResultSet();
    }

    /* Store column headers on first row */
    if (g_nRows == 0) {
        g_nCols = argc;
        for (i = 0; i < argc; i++) {
            const char *s = cols[i] ? cols[i] : "";
            len = strlen_safe(s);
            if (len > MAX_CELL_LEN - 1) len = MAX_CELL_LEN - 1;
            /* Use string pool instead of LocalAlloc */
            g_results[0][i] = StrPoolNDup(g_resultPool, s, len);
            if (len > g_colWidths[i]) g_colWidths[i] = len;
        }
    }

    if (g_resultRows >= MAX_RESULT_ROWS) return 0;

    /* Store row data */
    g_resultRows++;
    for (i = 0; i < argc; i++) {
        const char *s = argv[i] ? argv[i] : "(null)";
        len = strlen_safe(s);
        if (len > MAX_CELL_LEN - 1) len = MAX_CELL_LEN - 1;
        /* Use string pool instead of LocalAlloc */
        g_results[g_resultRows][i] = StrPoolNDup(g_resultPool, s, len);
        if (len > g_colWidths[i]) g_colWidths[i] = len;
    }
    g_nRows++;

    return 0;
}

/*============================================================================
** Execute a SQL string directly (for schema browser, etc.)
**============================================================================*/

void ExecuteSQL(const char *sql) {
    int rc;
    char *errmsg = NULL;
    DWORD startTick, elapsed;
    wchar_t wbuf[64];

    if (!g_db || !sql) return;

    /* Clear edit mode - this is a read-only query path */
    ClearEditMode();

    /* Clear previous results */
    FreeLastResults();

    /* Setup */
    g_abortQuery = 0;
    if (g_clearOnExec) ClearOutput();
    FreeResults();
    g_nRows = 0;
    g_nCols = 0;
    g_totalRows = 0;

    sqlite_progress_handler(g_db, 1000, ProgressCallback, NULL);
    startTick = GetTickCount();

    rc = CachedExec(sql, QueryCallback, NULL, &errmsg);

    elapsed = GetTickCount() - startTick;
    g_lastQueryTime = elapsed;

    if (rc == SQLITE_OK && g_nRows > 0) {
        FlushResultSet();
    }
    if (errmsg) {
        OutputLine(errmsg);
        sqlite_freemem(errmsg);
    }

    if (g_totalRows > 0)
        wsprintfW(wbuf, L"%d row(s) returned (%lums)", g_totalRows, elapsed);
    else
        wsprintfW(wbuf, L"OK (%lums)", elapsed);
    SetStatusResult(wbuf);

    FlushOutput();
    sqlite_progress_handler(g_db, 0, NULL, NULL);

    /* Switch to results */
    if (g_gridView && g_hwndGrid)
        SendMessage(g_hwndGrid, WM_SETREDRAW, FALSE, 0);
    SwitchView(VIEW_RESULT);
    if (g_lastResultRows > 0) {
        if (g_gridView) PopulateGrid();
    } else {
        /* No rowset - show text view, disable toggle */
        if (g_gridView) {
            ShowWindow(g_hwndGrid, SW_HIDE);
            ShowWindow(g_hwndResult, SW_SHOW);
            SetFocus(g_hwndResult);
        }
        SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_EXECATCURSOR, FALSE);
    }
    if (g_gridView && g_hwndGrid) {
        SendMessage(g_hwndGrid, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(g_hwndGrid, NULL, TRUE);
    }
}

void ExecuteQuery(void) {
    int len, rc, hadError = 0;
    char *sql;
    char *errmsg = NULL;
    wchar_t *wsql;
    wchar_t lastError[256];
    DWORD selStart, selEnd;

    if (!g_db) {
        SetWindowTextW(g_hwndResult, L"No database open.");
        return;
    }

    /* Clear edit mode - query editor results are read-only */
    ClearEditMode();

    /* Clear previous results */
    FreeLastResults();

    /* Disable Execute while running, enable Stop */
    EnableMenuItem(g_hMenu, IDM_EXECUTE, MF_GRAYED);
    SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_EXECUTE, FALSE);
    SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_STOP, TRUE);
    g_abortQuery = 0;
    UpdateWindow(g_hwndCB);

    /* Check for selection */
    SendMessage(g_hwndQuery, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);

    if (selStart != selEnd) {
        /* Execute selected text only */
        len = selEnd - selStart;
        wsql = ALLOC(wchar_t, len + 1);
        sql = ALLOC(char, (len + 1) * 3);
        if (!wsql || !sql) {
            if (wsql) LocalFree(wsql);
            if (sql) LocalFree(sql);
            EnableMenuItem(g_hMenu, IDM_EXECUTE, MF_ENABLED);
            SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_EXECUTE, TRUE);
            return;
        }
        SendMessage(g_hwndQuery, EM_SETSEL, selStart, selEnd);
        SendMessage(g_hwndQuery, WM_COPY, 0, 0);
        /* Get selected text via buffer */
        {
            int fullLen = GetWindowTextLengthW(g_hwndQuery);
            wchar_t *full = ALLOC(wchar_t, fullLen + 1);
            if (full) {
                int i;
                GetWindowTextW(g_hwndQuery, full, fullLen + 1);
                for (i = 0; i < len && (selStart + i) <= (DWORD)fullLen; i++)
                    wsql[i] = full[selStart + i];
                wsql[i] = 0;
                LocalFree(full);
            }
        }
    } else if (g_execAtCursor) {
        /* Execute statement at cursor */
        int fullLen = GetWindowTextLengthW(g_hwndQuery);
        wchar_t *full;
        int cursorPos = (int)selStart;
        int stmtStart = 0, stmtEnd = fullLen;
        int i, inStr = 0, inCmt = 0;

        if (fullLen == 0) {
            SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)L"Nothing to execute");
            EnableMenuItem(g_hMenu, IDM_EXECUTE, MF_ENABLED);
            SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_EXECUTE, TRUE);
            SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_STOP, FALSE);
            return;
        }

        full = ALLOC(wchar_t, fullLen + 1);
        if (!full) {
            EnableMenuItem(g_hMenu, IDM_EXECUTE, MF_ENABLED);
            SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_EXECUTE, TRUE);
            SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_STOP, FALSE);
            return;
        }
        GetWindowTextW(g_hwndQuery, full, fullLen + 1);

        /* Find statement boundaries */
        for (i = 0; i < fullLen; i++) {
            wchar_t c = full[i];
            if (inStr) {
                if (c == '\'') { if (full[i+1] == '\'') i++; else inStr = 0; }
            } else if (inCmt == 1) {
                if (c == '\n') inCmt = 0;
            } else if (inCmt == 2) {
                if (c == '*' && full[i+1] == '/') { i++; inCmt = 0; }
            } else {
                if (c == '\'') inStr = 1;
                else if (c == '-' && full[i+1] == '-') inCmt = 1;
                else if (c == '/' && full[i+1] == '*') { i++; inCmt = 2; }
                else if (c == ';') {
                    /* Cursor after this semicolon = this statement's end was found */
                    if (i + 1 < cursorPos) stmtStart = i + 1;
                    else if (stmtEnd == fullLen) stmtEnd = i + 1;
                }
            }
        }
        /* Skip leading whitespace */
        while (stmtStart < stmtEnd && (full[stmtStart] == ' ' || full[stmtStart] == '\t' ||
               full[stmtStart] == '\r' || full[stmtStart] == '\n')) stmtStart++;

        len = stmtEnd - stmtStart;
        wsql = ALLOC(wchar_t, len + 1);
        sql = ALLOC(char, (len + 1) * 3);
        if (!wsql || !sql) {
            LocalFree(full);
            if (wsql) LocalFree(wsql);
            if (sql) LocalFree(sql);
            EnableMenuItem(g_hMenu, IDM_EXECUTE, MF_ENABLED);
            SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_EXECUTE, TRUE);
            SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_STOP, FALSE);
            return;
        }
        for (i = 0; i < len; i++) wsql[i] = full[stmtStart + i];
        wsql[len] = 0;
        selStart = stmtStart;
        LocalFree(full);
    } else {
        /* Execute entire buffer */
        len = GetWindowTextLengthW(g_hwndQuery);
        if (len == 0 || g_showingHint) {
            SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)L"Nothing to execute");
            EnableMenuItem(g_hMenu, IDM_EXECUTE, MF_ENABLED);
            SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_EXECUTE, TRUE);
            SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_STOP, FALSE);
            return;
        }

        wsql = ALLOC(wchar_t, len + 1);
        sql = ALLOC(char, (len + 1) * 3);
        if (!wsql || !sql) {
            if (wsql) LocalFree(wsql);
            if (sql) LocalFree(sql);
            EnableMenuItem(g_hMenu, IDM_EXECUTE, MF_ENABLED);
            SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_EXECUTE, TRUE);
            return;
        }
        GetWindowTextW(g_hwndQuery, wsql, len + 1);
    }

    WideCharToMultiByte(CP_ACP, 0, wsql, -1, sql, (len + 1) * 3, NULL, NULL);
    LocalFree(wsql);

    /* Execute */
    if (g_clearOnExec) ClearOutput();
    FreeResults();
    g_nRows = 0;
    g_nCols = 0;
    g_totalRows = 0;

    /* Install progress handler for abort support */
    sqlite_progress_handler(g_db, 1000, ProgressCallback, NULL);

    SetStatusResult(L"Executing...");
    SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)L"Executing...");
    UpdateWindow(g_hwndStatus);

    {
    DWORD startTick = GetTickCount();
    DWORD elapsed;
    char *stmt = sql;
    char *p = sql;
    int inString = 0;
    int inComment = 0;
    int inTrigger = 0;
    int stmtOffset = 0;
    int errorOffset = 0;

    /* Split on semicolons and execute each statement */
    while (*p && !hadError) {
        if (inComment) {
            if (inComment == 1 && *p == '\n') inComment = 0;
            else if (inComment == 2 && *p == '*' && p[1] == '/') { inComment = 0; p++; }
        } else if (inString) {
            if (*p == '\'' && p[1] == '\'') p++;
            else if (*p == '\'') inString = 0;
        } else {
            if (*p == '\'') inString = 1;
            else if (*p == '-' && p[1] == '-') inComment = 1;
            else if (*p == '/' && p[1] == '*') { inComment = 2; p++; }
            else if (!inTrigger && (p[0]=='C'||p[0]=='c') && (p[1]=='R'||p[1]=='r') &&
                     (p[2]=='E'||p[2]=='e') && (p[3]=='A'||p[3]=='a') &&
                     (p[4]=='T'||p[4]=='t') && (p[5]=='E'||p[5]=='e') &&
                     (p[6]==' '||p[6]=='\t'||p[6]=='\r'||p[6]=='\n')) {
                /* Look ahead for TRIGGER keyword */
                char *t = p + 7;
                while (*t == ' ' || *t == '\t' || *t == '\r' || *t == '\n') t++;
                if ((t[0]=='T'||t[0]=='t') && (t[1]=='R'||t[1]=='r') &&
                    (t[2]=='I'||t[2]=='i') && (t[3]=='G'||t[3]=='g') &&
                    (t[4]=='G'||t[4]=='g') && (t[5]=='E'||t[5]=='e') &&
                    (t[6]=='R'||t[6]=='r') &&
                    (t[7]==' '||t[7]=='\t'||t[7]=='\r'||t[7]=='\n')) {
                    inTrigger = 1;
                }
            }
            else if (inTrigger && *p == ';') {
                /* Check for END after semicolon */
                char *t = p + 1;
                while (*t == ' ' || *t == '\t' || *t == '\r' || *t == '\n') t++;
                if ((t[0]=='E'||t[0]=='e') && (t[1]=='N'||t[1]=='n') &&
                    (t[2]=='D'||t[2]=='d') &&
                    (t[3]==';'||t[3]==' '||t[3]=='\t'||t[3]=='\r'||t[3]=='\n'||t[3]==0)) {
                    /* Found ;END - skip to the final semicolon */
                    p = t + 2;
                    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
                    /* p now points to final ; or next char - let it fall through */
                    inTrigger = 0;
                } else {
                    /* Semicolon inside trigger body - skip it */
                    p++;
                    continue;
                }
            }
            if (*p == ';' && !inTrigger) {
                char saved = p[1];
                p[1] = '\0';
                rc = CachedExec(stmt, QueryCallback, NULL, &errmsg);
                p[1] = saved;
                if (rc == SQLITE_INTERRUPT) {
                    OutputLine("Query aborted by user.");
                    if (errmsg) sqlite_freemem(errmsg);
                    hadError = 1;
                } else if (rc != SQLITE_OK) {
                    int ln = 1, i;
                    char lb[16]; char *lp = lb + 14;
                    const char *es = errmsg ? errmsg : sqlite_error_string(rc);
                    for (i = 0; i < stmtOffset; i++) if (sql[i] == '\n') ln++;
                    lb[15] = '\0'; *lp = '\0';
                    while (ln > 0) { *--lp = '0' + (ln % 10); ln /= 10; }
                    Output("Line "); Output(lp); Output(": ");
                    OutputLine(es);
                    for (i = 0; es[i] && i < 255; i++) lastError[i] = (wchar_t)(unsigned char)es[i];
                    lastError[i] = 0;
                    if (errmsg) sqlite_freemem(errmsg);
                    errorOffset = stmtOffset;
                    hadError = 1;
                } else if (g_nRows > 0) {
                    FlushResultSet();
                } else {
                    int changes = sqlite_changes(g_db);
                    if (changes > 0) {
                        char buf[32]; char *bp = buf + 30; int c = changes;
                        buf[31] = '\0'; *bp = '\0';
                        while (c > 0) { *--bp = '0' + (c % 10); c /= 10; }
                        Output(bp); OutputLine(" row(s) affected.");
                    }
                }
                stmt = p + 1;
                stmtOffset = (int)(stmt - sql);
                while (*stmt == ' ' || *stmt == '\t' || *stmt == '\r' || *stmt == '\n') { stmt++; stmtOffset++; }
            }
        }
        p++;
    }

    /* Execute any remaining statement (no trailing semicolon) */
    if (!hadError && *stmt) {
        rc = CachedExec(stmt, QueryCallback, NULL, &errmsg);
        if (rc == SQLITE_INTERRUPT) {
            OutputLine("Query aborted by user.");
            if (errmsg) sqlite_freemem(errmsg);
            hadError = 1;
        } else if (rc != SQLITE_OK) {
            int ln = 1, i;
            char lb[16]; char *lp = lb + 14;
            const char *es = errmsg ? errmsg : sqlite_error_string(rc);
            for (i = 0; i < stmtOffset; i++) if (sql[i] == '\n') ln++;
            lb[15] = '\0'; *lp = '\0';
            while (ln > 0) { *--lp = '0' + (ln % 10); ln /= 10; }
            errorOffset = stmtOffset;
            Output("Line "); Output(lp); Output(": ");
            OutputLine(es);
            for (i = 0; es[i] && i < 255; i++) lastError[i] = (wchar_t)(unsigned char)es[i];
            lastError[i] = 0;
            if (errmsg) sqlite_freemem(errmsg);
            hadError = 1;
        } else if (g_nRows > 0) {
            FlushResultSet();
        } else {
            int changes = sqlite_changes(g_db);
            if (changes > 0) {
                char buf[32]; char *bp = buf + 30; int c = changes;
                buf[31] = '\0'; *bp = '\0';
                while (c > 0) { *--bp = '0' + (c % 10); c /= 10; }
                Output(bp); OutputLine(" row(s) affected.");
            }
        }
    }

    elapsed = GetTickCount() - startTick;

    if (g_abortQuery) {
        /* Query was aborted by user */
        SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)L"Aborted");
        SetStatusResult(L"Aborted");
    } else if (hadError) {
        /* Position cursor at the errored statement */
        int adjOffset = errorOffset;
        int lineNum = 1;
        int i;
        wchar_t wbuf[48];
        for (i = 0; i < errorOffset; i++) {
            if (sql[i] == '\n') lineNum++;
        }
        if (selStart != selEnd) adjOffset += selStart;
        SendMessage(g_hwndQuery, EM_SETSEL, adjOffset, adjOffset);
        SendMessage(g_hwndQuery, EM_SCROLLCARET, 0, 0);
        wsprintfW(wbuf, L"Error at line %d", lineNum);
        SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)wbuf);
        g_suppressLineCount = 3;
        MessageBeep(MB_ICONEXCLAMATION);
        if (g_showErrorMsgBox)
            MessageBoxW(g_hwndMain, lastError, L"SQL Error", MB_OK | MB_ICONERROR);
    } else {
        wchar_t wbuf[64];
        Output("Query executed in ");
        { char tb[16]; char *tp = tb + 14; long e = (long)elapsed; tb[15] = '\0'; *tp = '\0';
          if (e == 0) *--tp = '0'; else while (e > 0) { *--tp = (char)('0' + (e % 10)); e /= 10; }
          Output(tp); }
        OutputLine("ms.");
        if (g_totalRows > 0)
            wsprintfW(wbuf, L"%d row(s) returned (%lums)", g_totalRows, elapsed);
        else
            wsprintfW(wbuf, L"OK (%lums)", elapsed);
        SetStatusResult(wbuf);
    }
    }

    LocalFree(sql);
    FlushOutput();
    UpdateDbSize();

    /* Remove progress handler and re-enable Execute, disable Stop */
    sqlite_progress_handler(g_db, 0, NULL, NULL);
    EnableMenuItem(g_hMenu, IDM_EXECUTE, MF_ENABLED);
    SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_EXECUTE, TRUE);
    SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_STOP, FALSE);

    /* Switch to results view (unless error - stay in query to show cursor) */
    if (!hadError) {
        /* Suppress redraw during view switch to avoid flicker */
        if (g_gridView && g_hwndGrid)
            SendMessage(g_hwndGrid, WM_SETREDRAW, FALSE, 0);
        SwitchView(VIEW_RESULT);
        if (g_lastResultRows > 0) {
            if (g_gridView) PopulateGrid();
        } else {
            /* No rowset - show text view, disable toggle */
            if (g_gridView) {
                ShowWindow(g_hwndGrid, SW_HIDE);
                ShowWindow(g_hwndResult, SW_SHOW);
                SetFocus(g_hwndResult);
            }
            SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_EXECATCURSOR, FALSE);
        }
        if (g_gridView && g_hwndGrid) {
            SendMessage(g_hwndGrid, WM_SETREDRAW, TRUE, 0);
            InvalidateRect(g_hwndGrid, NULL, TRUE);
        }
        RefreshSchema();  /* Update schema in case CREATE/DROP was executed */
    }
}

/*============================================================================
** Cleanup function - call on application exit
**============================================================================*/

void CleanupExecute(void) {
    /* Free last results */
    FreeLastResults();

    /* Destroy intern table */
    if (g_lastResultIntern) {
        StringInternDestroy(g_lastResultIntern);
        g_lastResultIntern = NULL;
    }

    /* Destroy result pool */
    if (g_resultPool) {
        StrPoolDestroy(g_resultPool);
        g_resultPool = NULL;
    }

    /* Destroy statement cache */
    if (g_stmtCache) {
        StmtCacheDestroy(g_stmtCache);
        g_stmtCache = NULL;
    }
}

void InvalidateStmtCache(void) {
    if (g_stmtCache) {
        StmtCacheInvalidate(g_stmtCache);
    }
}
