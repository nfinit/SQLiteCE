/*
** SQLiteCEdit - Query execution
*/

#include "globals.h"

/*============================================================================
** Query Execution State
**============================================================================*/

static int g_nRows;
static int g_nCols;
static int g_totalRows;

/* Result buffering for aligned output */
#define MAX_RESULT_COLS 32
#define MAX_RESULT_ROWS 500
#define MAX_CELL_LEN 64

static char *g_results[MAX_RESULT_ROWS + 1][MAX_RESULT_COLS];
static int g_colWidths[MAX_RESULT_COLS];
static int g_resultRows;

static int strlen_safe(const char *s) {
    int n = 0;
    if (s) while (*s++) n++;
    return n;
}

static void FreeResults(void) {
    int r, c;
    for (r = 0; r <= g_resultRows; r++) {
        for (c = 0; c < g_nCols; c++) {
            if (g_results[r][c]) { LocalFree(g_results[r][c]); g_results[r][c] = NULL; }
        }
    }
    for (c = 0; c < MAX_RESULT_COLS; c++) g_colWidths[c] = 0;
    g_resultRows = 0;
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
    FreeResults();
    g_nRows = 0;
    g_nCols = 0;
}

static int QueryCallback(void *arg, int argc, char **argv, char **cols) {
    int i, len;
    int colsChanged = 0;
    (void)arg;
    
    if (argc > MAX_RESULT_COLS) argc = MAX_RESULT_COLS;
    
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
            g_results[0][i] = (char *)LocalAlloc(LMEM_FIXED, len + 1);
            if (g_results[0][i]) {
                int j; for (j = 0; j < len; j++) g_results[0][i][j] = s[j];
                g_results[0][i][len] = '\0';
            }
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
        g_results[g_resultRows][i] = (char *)LocalAlloc(LMEM_FIXED, len + 1);
        if (g_results[g_resultRows][i]) {
            int j; for (j = 0; j < len; j++) g_results[g_resultRows][i][j] = s[j];
            g_results[g_resultRows][i][len] = '\0';
        }
        if (len > g_colWidths[i]) g_colWidths[i] = len;
    }
    g_nRows++;
    
    return 0;
}

void ExecuteQuery(void) {
    int len, rc, hadError = 0;
    char *sql;
    char *errmsg = NULL;
    wchar_t *wsql;
    DWORD selStart, selEnd;
    
    if (!g_db) {
        SetWindowTextW(g_hwndResult, L"No database open.");
        return;
    }
    
    /* Disable Execute while running */
    EnableMenuItem(g_hMenu, IDM_EXECUTE, MF_GRAYED);
    SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_EXECUTE, FALSE);
    UpdateWindow(g_hwndCB);
    
    /* Check for selection */
    SendMessage(g_hwndQuery, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
    
    if (selStart != selEnd) {
        /* Execute selected text only */
        len = selEnd - selStart;
        wsql = (wchar_t *)LocalAlloc(LMEM_FIXED, (len + 1) * sizeof(wchar_t));
        sql = (char *)LocalAlloc(LMEM_FIXED, (len + 1) * 3);
        if (!wsql || !sql) {
            if (wsql) LocalFree(wsql);
            if (sql) LocalFree(sql);
            return;
        }
        SendMessage(g_hwndQuery, EM_SETSEL, selStart, selEnd);
        SendMessage(g_hwndQuery, WM_COPY, 0, 0);
        /* Get selected text via buffer */
        {
            int fullLen = GetWindowTextLengthW(g_hwndQuery);
            wchar_t *full = (wchar_t *)LocalAlloc(LMEM_FIXED, (fullLen + 1) * sizeof(wchar_t));
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
            EnableMenuItem(g_hMenu, IDM_EXECUTE, MF_ENABLED);
            SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_EXECUTE, TRUE);
            return;
        }
        
        full = (wchar_t *)LocalAlloc(LMEM_FIXED, (fullLen + 1) * sizeof(wchar_t));
        if (!full) return;
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
                    if (i < cursorPos) stmtStart = i + 1;
                    else if (stmtEnd == fullLen) stmtEnd = i;
                }
            }
        }
        /* Skip leading whitespace */
        while (stmtStart < stmtEnd && (full[stmtStart] == ' ' || full[stmtStart] == '\t' || 
               full[stmtStart] == '\r' || full[stmtStart] == '\n')) stmtStart++;
        
        len = stmtEnd - stmtStart;
        wsql = (wchar_t *)LocalAlloc(LMEM_FIXED, (len + 1) * sizeof(wchar_t));
        sql = (char *)LocalAlloc(LMEM_FIXED, (len + 1) * 3);
        if (!wsql || !sql) {
            LocalFree(full);
            if (wsql) LocalFree(wsql);
            if (sql) LocalFree(sql);
            return;
        }
        for (i = 0; i < len; i++) wsql[i] = full[stmtStart + i];
        wsql[len] = 0;
        selStart = stmtStart;
        LocalFree(full);
    } else {
        /* Execute entire buffer */
        len = GetWindowTextLengthW(g_hwndQuery);
        if (len == 0) return;
        
        wsql = (wchar_t *)LocalAlloc(LMEM_FIXED, (len + 1) * sizeof(wchar_t));
        sql = (char *)LocalAlloc(LMEM_FIXED, (len + 1) * 3);
        if (!wsql || !sql) {
            if (wsql) LocalFree(wsql);
            if (sql) LocalFree(sql);
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
            else if (*p == ';') {
                char saved = p[1];
                p[1] = '\0';
                rc = sqlite_exec(g_db, stmt, QueryCallback, NULL, &errmsg);
                p[1] = saved;
                if (rc != SQLITE_OK) {
                    int ln = 1, i;
                    char lb[16]; char *lp = lb + 14;
                    for (i = 0; i < stmtOffset; i++) if (sql[i] == '\n') ln++;
                    lb[15] = '\0'; *lp = '\0';
                    while (ln > 0) { *--lp = '0' + (ln % 10); ln /= 10; }
                    Output("Line "); Output(lp); Output(": ");
                    OutputLine(errmsg ? errmsg : sqlite_error_string(rc));
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
        rc = sqlite_exec(g_db, stmt, QueryCallback, NULL, &errmsg);
        if (rc != SQLITE_OK) {
            int ln = 1, i;
            char lb[16]; char *lp = lb + 14;
            for (i = 0; i < stmtOffset; i++) if (sql[i] == '\n') ln++;
            lb[15] = '\0'; *lp = '\0';
            while (ln > 0) { *--lp = '0' + (ln % 10); ln /= 10; }
            errorOffset = stmtOffset;
            Output("Line "); Output(lp); Output(": ");
            OutputLine(errmsg ? errmsg : sqlite_error_string(rc));
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
    
    if (hadError) {
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
    
    /* Re-enable Execute */
    EnableMenuItem(g_hMenu, IDM_EXECUTE, MF_ENABLED);
    SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_EXECUTE, TRUE);
    
    /* Switch to results view (unless error - stay in query to show cursor) */
    if (!hadError) {
        SwitchView(1);
        SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_VIEWQUERY, FALSE);
        SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_VIEWRESULT, TRUE);
    }
}
