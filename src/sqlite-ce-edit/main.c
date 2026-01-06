/*
** SQLiteCEdit - SQL Query Editor for Windows CE
*/

#include <windows.h>
#include <commctrl.h>
#include "sqlite.h"

/*============================================================================
** Common File Dialog (may not be in CE 2.0 SDK headers)
**============================================================================*/

#define OFN_FILEMUSTEXIST   0x00001000
#define OFN_PATHMUSTEXIST   0x00000800
#define OFN_HIDEREADONLY    0x00000004
#define OFN_OVERWRITEPROMPT 0x00000002

typedef struct tagOFN {
    DWORD         lStructSize;
    HWND          hwndOwner;
    HINSTANCE     hInstance;
    LPCWSTR       lpstrFilter;
    LPWSTR        lpstrCustomFilter;
    DWORD         nMaxCustFilter;
    DWORD         nFilterIndex;
    LPWSTR        lpstrFile;
    DWORD         nMaxFile;
    LPWSTR        lpstrFileTitle;
    DWORD         nMaxFileTitle;
    LPCWSTR       lpstrInitialDir;
    LPCWSTR       lpstrTitle;
    DWORD         Flags;
    WORD          nFileOffset;
    WORD          nFileExtension;
    LPCWSTR       lpstrDefExt;
    LPARAM        lCustData;
    void*         lpfnHook;
    LPCWSTR       lpTemplateName;
} CE_OPENFILENAME;

BOOL WINAPI GetOpenFileNameW(CE_OPENFILENAME*);
BOOL WINAPI GetSaveFileNameW(CE_OPENFILENAME*);

/* View toolbar icons (may not be in CE 2.0 headers) */
#ifndef IDB_VIEW_SMALL_COLOR
#define IDB_VIEW_SMALL_COLOR 4
#define VIEW_LIST    2
#define VIEW_DETAILS 3
#endif

/*============================================================================
** Globals
**============================================================================*/

static HINSTANCE g_hInst;
static HWND g_hwndMain;
static HWND g_hwndCB;
static HWND g_hwndStatus;
static HMENU g_hMenu;
static HWND g_hwndQuery;   /* SQL input */
static HWND g_hwndResult;  /* Results output */
static sqlite *g_db = NULL;
static wchar_t g_szDbPath[MAX_PATH] = {0};
static HFONT g_hFontQuery = NULL;
static HFONT g_hFontResult = NULL;
static int g_fontSizes[] = {10, 12, 14, 16};
static int g_fontSizeQuery = 2;   /* Index into g_fontSizes, default 14 */
static int g_fontSizeResult = 2;
static WNDPROC g_pfnQueryProc;   /* Original query edit proc */
static WNDPROC g_pfnResultProc;  /* Original result edit proc */

/* Output buffer */
static char g_szOutput[32000];
static int g_nOutput = 0;

/*============================================================================
** Version
**============================================================================*/

#define SQLITECEDIT_VERSION L"0.2.0"

/*============================================================================
** Menu IDs
**============================================================================*/

#define IDM_NEW      101
#define IDM_OPEN     102
#define IDM_CLOSE    103
#define IDM_EXIT     104
#define IDM_OPENQUERY  105
#define IDM_SAVEQUERY  106
#define IDM_EXPORTCSV  107
#define IDM_EXPORTDB   108
#define IDM_IMPORTCSV  109
#define IDM_EXECUTE  201
#define IDM_FIND     202
#define IDM_FINDNEXT 203
#define IDM_CLEAR    301
#define IDM_VIEWQUERY  401
#define IDM_VIEWRESULT 402
#define IDM_FONTSIZE   403
#define IDM_ABOUT    501

/*============================================================================
** Output Helpers
**============================================================================*/

static int g_clearOnExec = 1;  /* Clear results before each execution */
static int g_viewMode = 0;     /* 0 = query, 1 = results */
static int g_showingHint = 0;  /* 1 = showing startup hint in query pane */
static wchar_t g_lastResultStatus[64] = L"";  /* Saved result status */
static wchar_t g_findText[128] = L"";  /* Last search text */
static int g_searchMode = 0;  /* 1 = Enter triggers Find Next */
static wchar_t g_szQueryPath[MAX_PATH] = L"";  /* Current query file path */

static void UpdateLineCount(void) {
    wchar_t buf[32];
    DWORD sel;
    int cur, total;
    SendMessage(g_hwndQuery, EM_GETSEL, (WPARAM)&sel, 0);
    cur = (int)SendMessage(g_hwndQuery, EM_LINEFROMCHAR, sel, 0) + 1;
    total = (int)SendMessage(g_hwndQuery, EM_GETLINECOUNT, 0, 0);
    wsprintfW(buf, L"Ln %d of %d", cur, total);
    SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)buf);
}

static void SwitchView(int mode) {
    g_viewMode = mode;
    ShowWindow(g_hwndQuery, mode == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndResult, mode == 1 ? SW_SHOW : SW_HIDE);
    SetFocus(mode == 0 ? g_hwndQuery : g_hwndResult);
    if (mode == 0)
        UpdateLineCount();
    else
        SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)g_lastResultStatus);
}

static void ClearOutput(void) {
    g_szOutput[0] = '\0';
    g_nOutput = 0;
}

static void Output(const char *sz) {
    while (*sz && g_nOutput < sizeof(g_szOutput) - 1) {
        g_szOutput[g_nOutput++] = *sz++;
    }
    g_szOutput[g_nOutput] = '\0';
}

static void OutputLine(const char *sz) {
    Output(sz);
    Output("\r\n");
}

static void UpdateQueryFont(void) {
    HFONT hOld = g_hFontQuery;
    LOGFONTW lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = g_fontSizes[g_fontSizeQuery];
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    lstrcpyW(lf.lfFaceName, L"Courier New");
    g_hFontQuery = CreateFontIndirectW(&lf);
    SendMessage(g_hwndQuery, WM_SETFONT, (WPARAM)g_hFontQuery, TRUE);
    if (hOld) DeleteObject(hOld);
}

static void UpdateResultFont(void) {
    HFONT hOld = g_hFontResult;
    LOGFONTW lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = g_fontSizes[g_fontSizeResult];
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    lstrcpyW(lf.lfFaceName, L"Courier New");
    g_hFontResult = CreateFontIndirectW(&lf);
    SendMessage(g_hwndResult, WM_SETFONT, (WPARAM)g_hFontResult, TRUE);
    if (hOld) DeleteObject(hOld);
}

static void CycleFontSize(void) {
    if (g_viewMode == 0) {
        g_fontSizeQuery = (g_fontSizeQuery + 1) % 4;
        UpdateQueryFont();
    } else {
        g_fontSizeResult = (g_fontSizeResult + 1) % 4;
        UpdateResultFont();
    }
}

static void FlushOutput(void) {
    wchar_t *wz = (wchar_t *)LocalAlloc(LMEM_FIXED, (g_nOutput + 1) * sizeof(wchar_t));
    if (wz) {
        MultiByteToWideChar(CP_ACP, 0, g_szOutput, -1, wz, g_nOutput + 1);
        SetWindowTextW(g_hwndResult, wz);
        LocalFree(wz);
    }
}

static void SetStatusDb(const wchar_t *sz) {
    SendMessageW(g_hwndStatus, SB_SETTEXTW, 0, (LPARAM)sz);
}

static const wchar_t *GetFilename(const wchar_t *path);  /* Forward declaration */

static void UpdateDbSize(void) {
    wchar_t buf[64];
    const wchar_t *fn;
    wchar_t name[64];
    wchar_t *p;
    long size = 0;
    int canClose;
    
    if (!g_db) return;
    
    if (g_szDbPath[0] == ':') {
        lstrcpyW(name, L":memory:");
    } else {
        fn = GetFilename(g_szDbPath);
        p = name;
        while (*fn && *fn != '.' && p < name + 60) *p++ = *fn++;
        *p = 0;
    }
    
    /* Get file size for file databases */
    if (g_szDbPath[0] != ':') {
        HANDLE hFile = CreateFileW(g_szDbPath, GENERIC_READ, FILE_SHARE_READ, 
            NULL, OPEN_EXISTING, 0, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            size = GetFileSize(hFile, NULL);
            CloseHandle(hFile);
        }
    }
    
    if (size >= 1024 * 1024)
        wsprintfW(buf, L"%s (%ldM)", name, size / (1024 * 1024));
    else if (size >= 1024)
        wsprintfW(buf, L"%s (%ldk)", name, size / 1024);
    else if (size > 0)
        wsprintfW(buf, L"%s (%ldb)", name, size);
    else
        wsprintfW(buf, L"%s", name);
    SetStatusDb(buf);
    
    /* Enable/disable Close based on whether we have a real file */
    canClose = (g_szDbPath[0] && g_szDbPath[0] != ':');
    EnableMenuItem(g_hMenu, IDM_CLOSE, canClose ? MF_ENABLED : MF_GRAYED);
    SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_CLOSE, canClose);
}

static void SetStatusResult(const wchar_t *sz) {
    /* Save for when switching back to results view */
    int i;
    for (i = 0; i < 63 && sz[i]; i++) g_lastResultStatus[i] = sz[i];
    g_lastResultStatus[i] = 0;
    if (g_viewMode == 1)
        SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)sz);
}

static void ExecuteQuery(void);  /* Forward declaration */
static void DoFind(void);        /* Forward declaration */
static void DoFindNext(void);    /* Forward declaration */
static void DoOpenQuery(void);   /* Forward declaration */
static void DoSaveQuery(void);   /* Forward declaration */

/* Subclass proc for query edit - catches Ctrl+Enter */
static LRESULT CALLBACK QueryEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    /* Clear hint on focus */
    if (msg == WM_SETFOCUS && g_showingHint) {
        SetWindowTextW(hwnd, L"");
        g_showingHint = 0;
        UpdateLineCount();
    }
    /* Clear search mode on click */
    if (msg == WM_LBUTTONDOWN)
        g_searchMode = 0;
    if (msg == WM_KEYDOWN) {
        int ctrl = GetKeyState(VK_CONTROL) < 0;
        /* Arrow keys exit search mode */
        if (wParam == VK_UP || wParam == VK_DOWN || wParam == VK_LEFT || wParam == VK_RIGHT)
            g_searchMode = 0;
        /* Enter in search mode = Find Next */
        if (wParam == VK_RETURN && !ctrl && g_searchMode) {
            DoFindNext();
            return 0;
        }
        /* Ctrl+Enter or F5 - Execute */
        if ((wParam == VK_RETURN && ctrl) || wParam == VK_F5) {
            ExecuteQuery();
            return 0;
        }
        /* Ctrl+O - Open Query */
        if (ctrl && wParam == 'O') {
            DoOpenQuery();
            return 0;
        }
        /* Ctrl+N - New */
        if (ctrl && wParam == 'N') {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_NEW, 0);
            return 0;
        }
        /* Ctrl+S - Save Query */
        if (ctrl && wParam == 'S') {
            DoSaveQuery();
            return 0;
        }
        /* Ctrl+F - Find, F3 - Find Next */
        if (ctrl && wParam == 'F') {
            DoFind();
            return 0;
        }
        if (wParam == VK_F3) {
            DoFindNext();
            return 0;
        }
        /* F6 - Toggle view, Ctrl+1 - Query, Ctrl+2 - Results */
        if (wParam == VK_F6) {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWRESULT, 0);
            return 0;
        }
        if (ctrl && wParam == '1') {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWQUERY, 0);
            return 0;
        }
        if (ctrl && wParam == '2') {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWRESULT, 0);
            return 0;
        }
        /* Ctrl+A - Select all (CE edit control may not support natively) */
        if (ctrl && wParam == 'A') {
            SendMessage(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
        /* Up arrow on line 1 - go to start of buffer */
        if (wParam == VK_UP) {
            DWORD sel;
            SendMessage(hwnd, EM_GETSEL, (WPARAM)&sel, 0);
            if (SendMessage(hwnd, EM_LINEFROMCHAR, sel, 0) == 0) {
                SendMessage(hwnd, EM_SETSEL, 0, 0);
                return 0;
            }
        }
        /* Home - go to start of buffer */
        if (wParam == VK_HOME) {
            SendMessage(hwnd, EM_SETSEL, 0, 0);
            SendMessage(hwnd, EM_SCROLLCARET, 0, 0);
            return 0;
        }
        /* End - go to end of buffer */
        if (wParam == VK_END) {
            int len = GetWindowTextLengthW(hwnd);
            SendMessage(hwnd, EM_SETSEL, len, len);
            SendMessage(hwnd, EM_SCROLLCARET, 0, 0);
            return 0;
        }
        /* Ctrl+C/V/X - pass through for edit operations */
        if (ctrl && (wParam == 'C' || wParam == 'V' || wParam == 'X'))
            return CallWindowProc(g_pfnQueryProc, hwnd, msg, wParam, lParam);
    }
    /* Update line count on keyup and scroll caret into view for navigation keys */
    if (msg == WM_KEYUP) {
        UpdateLineCount();
        if (wParam == VK_PRIOR || wParam == VK_NEXT || wParam == VK_HOME || wParam == VK_END)
            SendMessage(hwnd, EM_SCROLLCARET, 0, 0);
    }
    if (msg == WM_LBUTTONUP)
        UpdateLineCount();
    /* Clear search mode and suppress beeps on typing */
    if (msg == WM_CHAR) {
        if (GetKeyState(VK_CONTROL) < 0) {
            /* Ctrl+V=22 - paste, then scroll caret into view */
            if (wParam == 22) {
                LRESULT r = CallWindowProc(g_pfnQueryProc, hwnd, msg, wParam, lParam);
                SendMessage(hwnd, EM_SCROLLCARET, 0, 0);
                UpdateLineCount();
                return r;
            }
            /* Ctrl+C=3, Ctrl+X=24 - pass through */
            if (wParam == 3 || wParam == 24)
                return CallWindowProc(g_pfnQueryProc, hwnd, msg, wParam, lParam);
            return 0;
        }
        /* Suppress Enter in search mode (handled in WM_KEYDOWN) */
        if (g_searchMode && (wParam == '\r' || wParam == '\n'))
            return 0;
        g_searchMode = 0;  /* Any typing exits search mode */
    }
    return CallWindowProc(g_pfnQueryProc, hwnd, msg, wParam, lParam);
}

/* Subclass proc for result edit - blocks input but allows copy */
static LRESULT CALLBACK ResultEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        int ctrl = GetKeyState(VK_CONTROL) < 0;
        /* Ctrl+A - Select all */
        if (ctrl && wParam == 'A') {
            SendMessage(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
        /* Ctrl+C - Copy (pass through) */
        if (ctrl && wParam == 'C')
            return CallWindowProc(g_pfnResultProc, hwnd, msg, wParam, lParam);
        /* F5 - Execute */
        if (wParam == VK_F5) {
            ExecuteQuery();
            return 0;
        }
        /* Ctrl+O - Open Query */
        if (ctrl && wParam == 'O') {
            DoOpenQuery();
            return 0;
        }
        /* Ctrl+N - New */
        if (ctrl && wParam == 'N') {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_NEW, 0);
            return 0;
        }
        /* Ctrl+S - Save Query */
        if (ctrl && wParam == 'S') {
            DoSaveQuery();
            return 0;
        }
        /* Ctrl+F - Find, F3 - Find Next */
        if (ctrl && wParam == 'F') {
            DoFind();
            return 0;
        }
        if (wParam == VK_F3) {
            DoFindNext();
            return 0;
        }
        /* F6/Escape - back to query, Ctrl+1 - Query, Ctrl+2 - Results */
        if (wParam == VK_F6 || wParam == VK_ESCAPE || wParam == VK_BACK) {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWQUERY, 0);
            return 0;
        }
        if (ctrl && wParam == '1') {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWQUERY, 0);
            return 0;
        }
        if (ctrl && wParam == '2') {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWRESULT, 0);
            return 0;
        }
        return 0;  /* Block all other keys */
    }
    /* Allow Ctrl+C WM_CHAR through */
    if (msg == WM_CHAR) {
        if (GetKeyState(VK_CONTROL) < 0 && wParam == 3)
            return CallWindowProc(g_pfnResultProc, hwnd, msg, wParam, lParam);
        return 0;
    }
    if (msg == WM_KEYUP)
        return 0;
    return CallWindowProc(g_pfnResultProc, hwnd, msg, wParam, lParam);
}

/*============================================================================
** Query Execution
**============================================================================*/

static int g_nRows;
static int g_nCols;
static int g_totalRows;  /* Cumulative count for status bar */

/* Result buffering for aligned output */
#define MAX_RESULT_COLS 32
#define MAX_RESULT_ROWS 500
#define MAX_CELL_LEN 64

static char *g_results[MAX_RESULT_ROWS + 1][MAX_RESULT_COLS];  /* +1 for headers */
static int g_colWidths[MAX_RESULT_COLS];
static int g_resultRows;

static int strlen_safe(const char *s) {
    int n = 0;
    if (s) while (*s++) n++;
    return n;
}

static void FreeResults(void);  /* Forward declaration */
static void OutputResults(void);

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
    (void)arg;
    
    if (argc > MAX_RESULT_COLS) argc = MAX_RESULT_COLS;
    
    /* New query detected - flush previous results first */
    if (g_nRows > 0 && argc != g_nCols) {
        FlushResultSet();
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
    
    if (g_resultRows >= MAX_RESULT_ROWS) return 0;  /* Limit rows */
    
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

static void ExecuteQuery(void) {
    int len, rc;
    char *sql;
    char *errmsg = NULL;
    wchar_t *wsql;
    
    if (!g_db) {
        SetWindowTextW(g_hwndResult, L"No database open.");
        return;
    }
    
    /* Get query text */
    len = GetWindowTextLengthW(g_hwndQuery);
    if (len == 0) return;
    
    wsql = (wchar_t *)LocalAlloc(LMEM_FIXED, (len + 1) * sizeof(wchar_t));
    sql = (char *)LocalAlloc(LMEM_FIXED, (len + 1) * 3);  /* UTF-8 worst case */
    if (!wsql || !sql) {
        if (wsql) LocalFree(wsql);
        if (sql) LocalFree(sql);
        return;
    }
    
    GetWindowTextW(g_hwndQuery, wsql, len + 1);
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
    UpdateWindow(g_hwndStatus);  /* Force repaint before query */
    
    {
    DWORD startTick = GetTickCount();
    DWORD elapsed;
    rc = sqlite_exec(g_db, sql, QueryCallback, NULL, &errmsg);
    elapsed = GetTickCount() - startTick;
    
    if (rc != SQLITE_OK) {
        Output("Error: ");
        OutputLine(errmsg ? errmsg : sqlite_error_string(rc));
        if (errmsg) sqlite_freemem(errmsg);
        SetStatusResult(L"Error");
    } else if (g_nRows == 0) {
        /* Check if it was a non-SELECT statement */
        int changes = sqlite_changes(g_db);
        wchar_t wbuf[64];
        if (changes > 0) {
            char buf[32];
            char *p = buf + 30;
            buf[31] = '\0';
            *p = '\0';
            while (changes > 0) { *--p = '0' + (changes % 10); changes /= 10; }
            Output(p);
            OutputLine(" row(s) affected.");
            Output("Query executed in ");
            { char tb[16]; char *tp = tb + 14; long e = (long)elapsed; tb[15] = '\0'; *tp = '\0';
              if (e == 0) *--tp = '0'; else while (e > 0) { *--tp = (char)('0' + (e % 10)); e /= 10; }
              Output(tp); }
            OutputLine("ms.");
            wsprintfW(wbuf, L"%hs row(s) affected (%lums)", p, elapsed);
        } else {
            OutputLine("OK");
            Output("Query executed in ");
            { char tb[16]; char *tp = tb + 14; long e = (long)elapsed; tb[15] = '\0'; *tp = '\0';
              if (e == 0) *--tp = '0'; else while (e > 0) { *--tp = (char)('0' + (e % 10)); e /= 10; }
              Output(tp); }
            OutputLine("ms.");
            wsprintfW(wbuf, L"OK (%lums)", elapsed);
        }
        SetStatusResult(wbuf);
    } else {
        wchar_t wbuf[64];
        /* Flush final result set (adds to g_totalRows) */
        FlushResultSet();
        Output("Query executed in ");
        { char tb[16]; char *tp = tb + 14; long e = (long)elapsed; tb[15] = '\0'; *tp = '\0';
          if (e == 0) *--tp = '0'; else while (e > 0) { *--tp = (char)('0' + (e % 10)); e /= 10; }
          Output(tp); }
        OutputLine("ms.");
        wsprintfW(wbuf, L"%d row(s) returned (%lums)", g_totalRows, elapsed);
        SetStatusResult(wbuf);
    }
    }
    
    LocalFree(sql);
    FlushOutput();
    UpdateDbSize();
    
    /* Switch to results view */
    SwitchView(1);
    SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_VIEWQUERY, FALSE);
    SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_VIEWRESULT, TRUE);
}

/* Helper to extract filename from path */
static const wchar_t *GetFilename(const wchar_t *path) {
    const wchar_t *p = path;
    const wchar_t *last = path;
    while (*p) {
        if (*p == '\\' || *p == '/') last = p + 1;
        p++;
    }
    return last;
}

/* Update title bar: "[query -] database - SQLite/CE" */
static void UpdateTitle(void) {
    wchar_t title[MAX_PATH];
    wchar_t *p = title;
    const wchar_t *fn;
    
    /* Query filename (without extension) */
    if (g_szQueryPath[0]) {
        fn = GetFilename(g_szQueryPath);
        while (*fn && *fn != '.' && p < title + MAX_PATH - 40) *p++ = *fn++;
        *p++ = ' '; *p++ = '-'; *p++ = ' ';
    }
    
    /* Database name (without extension) */
    if (g_szDbPath[0] == ':') {
        lstrcpyW(p, L"(memory) - SQLite/CE");
    } else if (g_szDbPath[0]) {
        fn = GetFilename(g_szDbPath);
        while (*fn && *fn != '.' && p < title + MAX_PATH - 15) *p++ = *fn++;
        lstrcpyW(p, L" - SQLite/CE");
    } else {
        lstrcpyW(p, L"SQLite/CE");
    }
    SetWindowTextW(g_hwndMain, title);
}

static void CloseDatabase(void) {
    if (g_db) {
        sqlite_close(g_db);
        g_db = NULL;
    }
    g_szDbPath[0] = '\0';
    /* Reopen in-memory database as scratchpad */
    g_db = sqlite_open(":memory:", 0, NULL);
    lstrcpyW(g_szDbPath, L":memory:");
    UpdateTitle();
    SetWindowTextW(g_hwndResult, L"");
    SetStatusDb(L":memory:");
    SetStatusResult(L"");
    /* Disable Close for in-memory database */
    EnableMenuItem(g_hMenu, IDM_CLOSE, MF_GRAYED);
    SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_CLOSE, FALSE);
}

static int OpenDatabase(const wchar_t *path) {
    char szPath[MAX_PATH * 2];
    char *errmsg = NULL;
    
    CloseDatabase();
    
    WideCharToMultiByte(CP_ACP, 0, path, -1, szPath, sizeof(szPath), NULL, NULL);
    g_db = sqlite_open(szPath, 0, &errmsg);
    
    if (!g_db) {
        wchar_t msg[256];
        wsprintfW(msg, L"Cannot open: %s", path);
        MessageBoxW(g_hwndMain, msg, L"Error", MB_OK | MB_ICONERROR);
        if (errmsg) sqlite_freemem(errmsg);
        SetStatusDb(L"No database");
        return 0;
    }
    
    lstrcpyW(g_szDbPath, path);
    UpdateTitle();
    UpdateDbSize();
    SetStatusResult(L"");
    
    /* Show hint for in-memory database in query pane */
    if (path[0] == ':') {
        wchar_t hint[256];
        wchar_t ver[32];
        const char *v = sqlite_libversion();
        int i;
        for (i = 0; v[i] && i < 31; i++) ver[i] = (wchar_t)v[i];
        ver[i] = 0;
        wsprintfW(hint,
            L"-- SQLite/CEdit " SQLITECEDIT_VERSION L" on SQLite %s.\r\n"
            L"-- Using in-memory database.\r\n", ver);
        SetWindowTextW(g_hwndQuery, hint);
        g_showingHint = 1;
    }
    return 1;
}

/*============================================================================
** Simple Path Input Dialog
** CE 2.0 lacks common file dialogs, so we use a simple edit prompt.
**============================================================================*/

static wchar_t g_szPathBuf[MAX_PATH];
static HWND g_hwndPathEdit;

static LRESULT CALLBACK PathDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            g_hwndPathEdit = CreateWindowW(L"EDIT", g_szPathBuf,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                10, 10, rc.right - 20, 26,
                hwnd, (HMENU)101, g_hInst, NULL);
            CreateWindowW(L"BUTTON", L"OK",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                10, 46, 60, 26, hwnd, (HMENU)IDOK, g_hInst, NULL);
            CreateWindowW(L"BUTTON", L"Cancel",
                WS_CHILD | WS_VISIBLE,
                80, 46, 60, 26, hwnd, (HMENU)IDCANCEL, g_hInst, NULL);
            SetFocus(g_hwndPathEdit);
            return FALSE;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                GetWindowTextW(g_hwndPathEdit, g_szPathBuf, MAX_PATH);
                EndDialog(hwnd, IDOK);
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwnd, IDCANCEL);
            }
            return TRUE;
        case WM_CLOSE:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

static int PromptForPath(const wchar_t *title, const wchar_t *defPath) {
    /* Build dialog template in memory */
    struct {
        DLGTEMPLATE tmpl;
        WORD menu, wndclass, title;
    } dlg;
    
    lstrcpyW(g_szPathBuf, defPath);
    
    memset(&dlg, 0, sizeof(dlg));
    dlg.tmpl.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
    dlg.tmpl.cx = 160;
    dlg.tmpl.cy = 50;
    dlg.tmpl.x = 20;
    dlg.tmpl.y = 20;
    
    return DialogBoxIndirectW(g_hInst, &dlg.tmpl, g_hwndMain, PathDlgProc) == IDOK;
}

/*============================================================================
** Find / Search
**============================================================================*/

static void DoFindNext(void) {
    HWND hwndEdit = g_viewMode == 0 ? g_hwndQuery : g_hwndResult;
    int len, findLen, start, i, j;
    wchar_t *buf;
    DWORD sel;
    
    if (!g_findText[0]) return;
    
    findLen = lstrlenW(g_findText);
    len = GetWindowTextLengthW(hwndEdit);
    if (len == 0) return;
    
    buf = (wchar_t *)LocalAlloc(LMEM_FIXED, (len + 1) * sizeof(wchar_t));
    if (!buf) return;
    GetWindowTextW(hwndEdit, buf, len + 1);
    
    /* Get current position */
    SendMessage(hwndEdit, EM_GETSEL, (WPARAM)&sel, 0);
    start = sel + 1;
    if (start > len) start = 0;
    
    /* Search forward from cursor */
    for (i = start; i <= len - findLen; i++) {
        for (j = 0; j < findLen; j++) {
            wchar_t c1 = buf[i + j], c2 = g_findText[j];
            if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
            if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
            if (c1 != c2) break;
        }
        if (j == findLen) {
            SendMessage(hwndEdit, EM_SETSEL, i, i + findLen);
            SendMessage(hwndEdit, EM_SCROLLCARET, 0, 0);
            LocalFree(buf);
            return;
        }
    }
    /* Wrap around */
    for (i = 0; i < start && i <= len - findLen; i++) {
        for (j = 0; j < findLen; j++) {
            wchar_t c1 = buf[i + j], c2 = g_findText[j];
            if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
            if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
            if (c1 != c2) break;
        }
        if (j == findLen) {
            SendMessage(hwndEdit, EM_SETSEL, i, i + findLen);
            SendMessage(hwndEdit, EM_SCROLLCARET, 0, 0);
            LocalFree(buf);
            return;
        }
    }
    LocalFree(buf);
    MessageBoxW(g_hwndMain, L"Text not found.", L"Find", MB_OK);
}

static HWND g_hwndFindDlg = NULL;
static HWND g_hwndFindEdit = NULL;

static LRESULT CALLBACK FindEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static WNDPROC g_pfnFindEditProc = NULL;

static LRESULT CALLBACK FindWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hwndFindEdit = CreateWindowW(L"EDIT", g_findText,
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP,
                5, 5, 133, 20, hwnd, (HMENU)101, g_hInst, NULL);
            CreateWindowW(L"BUTTON", L"Find",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                145, 4, 45, 22, hwnd, (HMENU)IDOK, g_hInst, NULL);
            g_pfnFindEditProc = (WNDPROC)SetWindowLong(g_hwndFindEdit, GWL_WNDPROC, (LONG)FindEditProc);
            SetFocus(g_hwndFindEdit);
            SendMessage(g_hwndFindEdit, EM_SETSEL, 0, -1);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                GetWindowTextW(g_hwndFindEdit, g_findText, 128);
                DestroyWindow(hwnd);
                g_hwndFindDlg = NULL;
                SetFocus(g_viewMode == 0 ? g_hwndQuery : g_hwndResult);
                if (g_findText[0]) {
                    g_searchMode = 1;
                    DoFindNext();
                }
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            g_hwndFindDlg = NULL;
            SetFocus(g_viewMode == 0 ? g_hwndQuery : g_hwndResult);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK FindEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            SendMessage(g_hwndFindDlg, WM_COMMAND, IDOK, 0);
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            SendMessage(g_hwndFindDlg, WM_CLOSE, 0, 0);
            return 0;
        }
    }
    return CallWindowProc(g_pfnFindEditProc, hwnd, msg, wParam, lParam);
}

static void DoFind(void) {
    WNDCLASSW wc = {0};
    RECT rc;
    
    if (g_hwndFindDlg) {
        SetFocus(g_hwndFindEdit);
        return;
    }
    
    wc.lpfnWndProc = FindWndProc;
    wc.hInstance = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"SQLiteCEFind";
    RegisterClassW(&wc);
    
    GetWindowRect(g_hwndMain, &rc);
    #ifndef WS_EX_TOOLWINDOW
    #define WS_EX_TOOLWINDOW 0x00000080L
    #endif
    g_hwndFindDlg = CreateWindowExW(WS_EX_TOOLWINDOW, L"SQLiteCEFind", L"Find",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        rc.left + 20, rc.top + 50, 200, 52,
        g_hwndMain, NULL, g_hInst, NULL);
    ShowWindow(g_hwndFindDlg, SW_SHOW);
}

/*============================================================================
** About Dialog
**============================================================================*/

#define IDB_LOGO 101

static HBITMAP g_hLogo = NULL;

static LRESULT CALLBACK AboutWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            wchar_t text[256];
            wchar_t ver[32], built[32];
            const char *sv = sqlite_libversion();
            const char *bd = __DATE__;
            int i, j;
            for (i = 0; sv[i] && i < 31; i++) ver[i] = sv[i];
            ver[i] = 0;
            for (i = 0, j = 0; bd[i] && j < 31; i++) {
                if (bd[i] == ' ' && bd[i+1] == ' ') continue;  /* Skip double space */
                built[j++] = bd[i];
            }
            built[j] = 0;
            wsprintfW(text,
                L"SQLite/CEdit " SQLITECEDIT_VERSION L" (using SQLite %s)\n"
                L"(C) Intermountain Systems\n"
                L"Build date: %s", ver, built);
            CreateWindowW(L"STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_BITMAP,
                61, 10, 128, 64, hwnd, (HMENU)100, g_hInst, NULL);
            SendDlgItemMessage(hwnd, 100, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)g_hLogo);
            CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_CENTER,
                5, 80, 240, 50, hwnd, NULL, g_hInst, NULL);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hwnd);
                SetFocus(g_viewMode == 0 ? g_hwndQuery : g_hwndResult);
                return 0;
            }
            break;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE || wParam == VK_RETURN) {
                DestroyWindow(hwnd);
                SetFocus(g_viewMode == 0 ? g_hwndQuery : g_hwndResult);
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            SetFocus(g_viewMode == 0 ? g_hwndQuery : g_hwndResult);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void DoAbout(void) {
    WNDCLASSW wc = {0};
    RECT rc;
    HWND hwndAbout;
    
    if (!g_hLogo)
        g_hLogo = LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_LOGO));
    
    wc.lpfnWndProc = AboutWndProc;
    wc.hInstance = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"SQLiteCEAbout";
    RegisterClassW(&wc);
    
    GetWindowRect(g_hwndMain, &rc);
    hwndAbout = CreateWindowW(L"SQLiteCEAbout", L"About",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        rc.left + 30, rc.top + 30, 250, 160,
        g_hwndMain, NULL, g_hInst, NULL);
    ShowWindow(hwndAbout, SW_SHOW);
}

static void DoFileNew(void) {
    CE_OPENFILENAME ofn;
    wchar_t szFile[MAX_PATH] = L"new.db";
    
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Database Files (*.db)\0*.db\0All Files (*.*)\0*.*\0";
    ofn.lpstrDefExt = L"db";
    ofn.lpstrTitle = L"New Database";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    
    if (GetSaveFileNameW(&ofn)) {
        DeleteFileW(szFile);
        OpenDatabase(szFile);
    }
}

static void DoFileOpen(void) {
    CE_OPENFILENAME ofn;
    wchar_t szFile[MAX_PATH] = L"";
    
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Database Files (*.db)\0*.db\0All Files (*.*)\0*.*\0";
    ofn.lpstrTitle = L"Open Database";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    
    if (GetOpenFileNameW(&ofn)) {
        OpenDatabase(szFile);
    }
}

static void DoOpenQuery(void) {
    CE_OPENFILENAME ofn;
    wchar_t szFile[MAX_PATH] = L"";
    HANDLE hFile;
    DWORD dwSize, dwRead;
    char *buf;
    wchar_t *wbuf;
    int i;
    
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"SQL Files (*.sql)\0*.sql\0All Files (*.*)\0*.*\0";
    ofn.lpstrTitle = L"Open Query";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    
    if (GetOpenFileNameW(&ofn)) {
        hFile = CreateFileW(szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            dwSize = GetFileSize(hFile, NULL);
            if (dwSize < 65536) {
                buf = (char*)LocalAlloc(LMEM_FIXED, dwSize + 1);
                wbuf = (wchar_t*)LocalAlloc(LMEM_FIXED, (dwSize + 1) * sizeof(wchar_t));
                if (buf && wbuf) {
                    if (ReadFile(hFile, buf, dwSize, &dwRead, NULL)) {
                        buf[dwRead] = '\0';
                        for (i = 0; i <= (int)dwRead; i++) wbuf[i] = (wchar_t)(unsigned char)buf[i];
                        SetWindowTextW(g_hwndQuery, wbuf);
                        wcscpy(g_szQueryPath, szFile);
                        UpdateTitle();
                    }
                }
                if (buf) LocalFree(buf);
                if (wbuf) LocalFree(wbuf);
            }
            CloseHandle(hFile);
        }
    }
}

static void DoSaveQuery(void) {
    CE_OPENFILENAME ofn;
    wchar_t szFile[MAX_PATH];
    HANDLE hFile;
    DWORD dwLen, dwWritten;
    wchar_t *wbuf;
    char *buf;
    DWORD i;
    
    if (g_szQueryPath[0]) {
        wcscpy(szFile, g_szQueryPath);
    } else {
        wcscpy(szFile, L"query.sql");
        memset(&ofn, 0, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_hwndMain;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = L"SQL Files (*.sql)\0*.sql\0All Files (*.*)\0*.*\0";
        ofn.lpstrDefExt = L"sql";
        ofn.lpstrTitle = L"Save Query";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        
        if (!GetSaveFileNameW(&ofn)) return;
        wcscpy(g_szQueryPath, szFile);
        UpdateTitle();
    }
    
    dwLen = GetWindowTextLengthW(g_hwndQuery);
    wbuf = (wchar_t*)LocalAlloc(LMEM_FIXED, (dwLen + 1) * sizeof(wchar_t));
    buf = (char*)LocalAlloc(LMEM_FIXED, dwLen + 1);
    if (wbuf && buf) {
        GetWindowTextW(g_hwndQuery, wbuf, dwLen + 1);
        for (i = 0; i <= dwLen; i++) buf[i] = (char)wbuf[i];
        hFile = CreateFileW(szFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            WriteFile(hFile, buf, dwLen, &dwWritten, NULL);
            CloseHandle(hFile);
        }
    }
    if (wbuf) LocalFree(wbuf);
    if (buf) LocalFree(buf);
}

static void DoExportCSV(void) {
    CE_OPENFILENAME ofn;
    wchar_t szFile[MAX_PATH] = L"results.csv";
    HANDLE hFile;
    DWORD dwLen, dwWritten;
    wchar_t *wbuf, *wp;
    char *buf, *bp;
    int needQuote;
    
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"CSV Files (*.csv)\0*.csv\0All Files (*.*)\0*.*\0";
    ofn.lpstrDefExt = L"csv";
    ofn.lpstrTitle = L"Export Results";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    
    if (!GetSaveFileNameW(&ofn)) return;
    
    dwLen = GetWindowTextLengthW(g_hwndResult);
    if (dwLen == 0) return;
    
    wbuf = (wchar_t*)LocalAlloc(LMEM_FIXED, (dwLen + 1) * sizeof(wchar_t));
    buf = (char*)LocalAlloc(LMEM_FIXED, (dwLen * 2) + 1);  /* Extra space for quotes */
    if (wbuf && buf) {
        GetWindowTextW(g_hwndResult, wbuf, dwLen + 1);
        bp = buf;
        wp = wbuf;
        while (*wp) {
            if (*wp == '\t') {
                *bp++ = ',';
                wp++;
            } else if (*wp == '\r') {
                wp++;  /* Skip CR, keep LF */
            } else if (*wp == '\n') {
                *bp++ = '\r';
                *bp++ = '\n';
                wp++;
            } else {
                /* Check if field needs quoting (contains comma or quote) */
                wchar_t *fieldStart = wp;
                needQuote = 0;
                while (*wp && *wp != '\t' && *wp != '\r' && *wp != '\n') {
                    if (*wp == ',' || *wp == '"') needQuote = 1;
                    wp++;
                }
                if (needQuote) {
                    *bp++ = '"';
                    while (fieldStart < wp) {
                        if (*fieldStart == '"') *bp++ = '"';  /* Escape quotes */
                        *bp++ = (char)*fieldStart++;
                    }
                    *bp++ = '"';
                } else {
                    while (fieldStart < wp) *bp++ = (char)*fieldStart++;
                }
            }
        }
        *bp = '\0';
        
        hFile = CreateFileW(szFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            WriteFile(hFile, buf, (DWORD)(bp - buf), &dwWritten, NULL);
            CloseHandle(hFile);
        }
    }
    if (wbuf) LocalFree(wbuf);
    if (buf) LocalFree(buf);
}

static void ExportTableToCSV(const wchar_t *dir, const char *tblName) {
    wchar_t path[MAX_PATH];
    wchar_t *wp;
    const wchar_t *wd;
    const char *t;
    char sql[512];
    char *p;
    char **result;
    int nRow, nCol, i, j;
    HANDLE hFile;
    DWORD written;
    char line[4096];
    char *lp;
    
    /* Build path: dir\tablename.csv */
    wp = path;
    for (wd = dir; *wd; ) *wp++ = *wd++;
    *wp++ = '\\';
    for (t = tblName; *t; ) *wp++ = (wchar_t)*t++;
    *wp++ = '.'; *wp++ = 'c'; *wp++ = 's'; *wp++ = 'v'; *wp = 0;
    
    /* Build SELECT */
    p = sql;
    for (t = "SELECT * FROM \""; *t; ) *p++ = *t++;
    for (t = tblName; *t; ) *p++ = *t++;
    *p++ = '"'; *p = 0;
    
    if (sqlite_get_table(g_db, sql, &result, &nRow, &nCol, NULL) != SQLITE_OK) return;
    
    hFile = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        sqlite_free_table(result);
        return;
    }
    
    /* Write header row */
    lp = line;
    for (j = 0; j < nCol; j++) {
        char *val = result[j];
        if (j > 0) *lp++ = ',';
        if (val) while (*val) *lp++ = *val++;
    }
    *lp++ = '\r'; *lp++ = '\n';
    WriteFile(hFile, line, (DWORD)(lp - line), &written, NULL);
    
    /* Write data rows */
    for (i = 1; i <= nRow; i++) {
        lp = line;
        for (j = 0; j < nCol; j++) {
            char *val = result[i * nCol + j];
            int needQuote = 0;
            char *v;
            if (j > 0) *lp++ = ',';
            if (val) {
                for (v = val; *v; v++) if (*v == ',' || *v == '"' || *v == '\n') needQuote = 1;
                if (needQuote) {
                    *lp++ = '"';
                    for (v = val; *v; v++) {
                        if (*v == '"') *lp++ = '"';
                        *lp++ = *v;
                    }
                    *lp++ = '"';
                } else {
                    while (*val) *lp++ = *val++;
                }
            }
        }
        *lp++ = '\r'; *lp++ = '\n';
        WriteFile(hFile, line, (DWORD)(lp - line), &written, NULL);
    }
    
    CloseHandle(hFile);
    sqlite_free_table(result);
}

static void DoExportDb(void) {
    CE_OPENFILENAME ofn;
    wchar_t szFile[MAX_PATH] = L"export.db";
    char szDestPath[MAX_PATH * 2];
    sqlite *destDb;
    char **result;
    int nRow, nCol, i;
    char *errmsg;
    char sql[4096];
    char *p;
    int csvMode;
    
    if (!g_db) return;
    
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"SQLite Database (*.db)\0*.db\0CSV Folder (*.csv)\0*.csv\0";
    ofn.lpstrTitle = L"Export Database";
    ofn.Flags = OFN_PATHMUSTEXIST;
    
    if (!GetSaveFileNameW(&ofn)) return;
    
    /* Detect mode by extension - CE 2.0 doesn't return nFilterIndex reliably */
    {
        wchar_t *p = szFile;
        int len;
        while (*p) p++;
        len = (int)(p - szFile);
        
        /* Check for .db extension = database mode, otherwise CSV mode */
        if (len >= 3 && 
            (szFile[len-3] == '.') &&
            (szFile[len-2] == 'd' || szFile[len-2] == 'D') &&
            (szFile[len-1] == 'b' || szFile[len-1] == 'B')) {
            csvMode = 0;
        } else {
            csvMode = 1;
            /* Strip .csv extension if present */
            if (len >= 4 &&
                (szFile[len-4] == '.') &&
                (szFile[len-3] == 'c' || szFile[len-3] == 'C') &&
                (szFile[len-2] == 's' || szFile[len-2] == 'S') &&
                (szFile[len-1] == 'v' || szFile[len-1] == 'V')) {
                szFile[len-4] = 0;
            }
        }
    }
    
    if (csvMode) {
        DWORD attr;
        /* Remove existing file/dir if present */
        attr = GetFileAttributesW(szFile);
        if (attr != 0xFFFFFFFF) {
            if (attr & FILE_ATTRIBUTE_DIRECTORY) {
                /* Directory exists - need to remove contents first, skip for now */
            } else {
                DeleteFileW(szFile);
            }
        }
        
        CreateDirectoryW(szFile, NULL);
        
        /* Verify directory was created */
        attr = GetFileAttributesW(szFile);
        if (attr == 0xFFFFFFFF || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            MessageBoxW(g_hwndMain, szFile, L"Failed to create folder", MB_OK | MB_ICONERROR);
            return;
        }
        
        if (sqlite_get_table(g_db, "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'",
                &result, &nRow, &nCol, NULL) == SQLITE_OK) {
            if (nRow == 0) {
                RemoveDirectoryW(szFile);
                MessageBoxW(g_hwndMain, L"No tables to export", L"Export", MB_OK | MB_ICONINFORMATION);
            } else {
                for (i = 1; i <= nRow; i++) {
                    ExportTableToCSV(szFile, result[i]);
                }
            }
            sqlite_free_table(result);
        }
        return;
    }
    
    /* Database export mode */
    DeleteFileW(szFile);
    WideCharToMultiByte(CP_ACP, 0, szFile, -1, szDestPath, sizeof(szDestPath), NULL, NULL);
    destDb = sqlite_open(szDestPath, 0, &errmsg);
    if (!destDb) {
        if (errmsg) sqlite_freemem(errmsg);
        MessageBoxW(g_hwndMain, L"Could not create database", L"Export Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    /* Get all tables from sqlite_master */
    if (sqlite_get_table(g_db, "SELECT name, sql FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'", 
            &result, &nRow, &nCol, NULL) == SQLITE_OK) {
        for (i = 1; i <= nRow; i++) {
            char *tblName = result[i * nCol];
            char *tblSql = result[i * nCol + 1];
            char *t;
            
            /* Create table in destination */
            if (tblSql) {
                sqlite_exec(destDb, tblSql, NULL, NULL, NULL);
            }
            
            /* Copy data - build SELECT */
            p = sql;
            for (t = "SELECT * FROM \""; *t; ) *p++ = *t++;
            for (t = tblName; *t; ) *p++ = *t++;
            *p++ = '"'; *p = '\0';
            
            /* For each row, insert into dest */
            {
                char **dataResult;
                int dataRow, dataCol, j, k;
                if (sqlite_get_table(g_db, sql, &dataResult, &dataRow, &dataCol, NULL) == SQLITE_OK) {
                    for (j = 1; j <= dataRow; j++) {
                        char *ins = sql;
                        for (t = "INSERT INTO \""; *t; ) *ins++ = *t++;
                        for (t = tblName; *t; ) *ins++ = *t++;
                        for (t = "\" VALUES("; *t; ) *ins++ = *t++;
                        for (k = 0; k < dataCol; k++) {
                            char *val = dataResult[j * dataCol + k];
                            if (k > 0) *ins++ = ',';
                            if (val == NULL) {
                                *ins++ = 'N'; *ins++ = 'U'; *ins++ = 'L'; *ins++ = 'L';
                            } else {
                                char *s = val;
                                *ins++ = '\'';
                                while (*s) {
                                    if (*s == '\'') *ins++ = '\'';
                                    *ins++ = *s++;
                                }
                                *ins++ = '\'';
                            }
                        }
                        *ins++ = ')';
                        *ins = '\0';
                        sqlite_exec(destDb, sql, NULL, NULL, NULL);
                    }
                    sqlite_free_table(dataResult);
                }
            }
        }
        sqlite_free_table(result);
    }
    
    /* Copy indexes */
    if (sqlite_get_table(g_db, "SELECT sql FROM sqlite_master WHERE type='index' AND sql IS NOT NULL", 
            &result, &nRow, &nCol, NULL) == SQLITE_OK) {
        for (i = 1; i <= nRow; i++) {
            if (result[i]) sqlite_exec(destDb, result[i], NULL, NULL, NULL);
        }
        sqlite_free_table(result);
    }
    
    sqlite_close(destDb);
}

/*============================================================================
** CSV Import
**============================================================================*/

#define CSV_MAX_COLS 64
#define CSV_MAX_LINE 8192

/* Type inference: 0=unknown, 1=integer, 2=real, 3=text */
static int InferType(const char *val) {
    const char *s = val;
    int hasDot = 0;
    if (!*s) return 0;  /* Empty = unknown */
    if (*s == '-') s++;
    if (!*s) return 3;  /* Just "-" = text */
    while (*s) {
        if (*s == '.') {
            if (hasDot) return 3;  /* Multiple dots = text */
            hasDot = 1;
        } else if (*s < '0' || *s > '9') {
            return 3;  /* Non-numeric = text */
        }
        s++;
    }
    return hasDot ? 2 : 1;  /* real or integer */
}

static int ParseCSVLine(char *line, char **fields, int maxFields) {
    int n = 0;
    char *p = line;
    while (*p && n < maxFields) {
        if (*p == '"') {
            /* Quoted field */
            p++;
            fields[n++] = p;
            while (*p && !(*p == '"' && (p[1] == ',' || p[1] == '\0' || p[1] == '\r' || p[1] == '\n'))) {
                if (*p == '"' && p[1] == '"') p++;  /* Escaped quote */
                p++;
            }
            if (*p == '"') *p++ = '\0';
            if (*p == ',') p++;
        } else {
            /* Unquoted field */
            fields[n++] = p;
            while (*p && *p != ',' && *p != '\r' && *p != '\n') p++;
            if (*p == ',') *p++ = '\0';
            else if (*p) { *p = '\0'; break; }
        }
    }
    return n;
}

static void DoImportCSV(void) {
    CE_OPENFILENAME ofn;
    wchar_t szFile[MAX_PATH] = L"";
    wchar_t tblName[64];
    wchar_t *wp;
    const wchar_t *fn;
    HANDLE hFile;
    DWORD dwSize, dwRead;
    char *buf, *line, *nextLine;
    char *fields[CSV_MAX_COLS];
    char *headers[CSV_MAX_COLS];
    int types[CSV_MAX_COLS];  /* 0=unk, 1=int, 2=real, 3=text */
    int nCols, i, rowNum;
    char sql[4096];
    char *p, *t;
    
    if (!g_db) {
        MessageBoxW(g_hwndMain, L"No database open", L"Import", MB_OK | MB_ICONERROR);
        return;
    }
    
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"CSV Files (*.csv)\0*.csv\0All Files (*.*)\0*.*\0";
    ofn.lpstrTitle = L"Import CSV";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    
    if (!GetOpenFileNameW(&ofn)) return;
    
    /* Derive table name from filename */
    fn = GetFilename(szFile);
    wp = tblName;
    while (*fn && *fn != '.' && wp < tblName + 60) *wp++ = *fn++;
    *wp = 0;
    
    /* Read entire file */
    hFile = CreateFileW(szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(g_hwndMain, L"Could not open file", L"Import Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    dwSize = GetFileSize(hFile, NULL);
    if (dwSize > 1024 * 1024) {  /* 1MB limit */
        CloseHandle(hFile);
        MessageBoxW(g_hwndMain, L"File too large (max 1MB)", L"Import Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    buf = (char*)LocalAlloc(LMEM_FIXED, dwSize + 1);
    if (!buf) {
        CloseHandle(hFile);
        return;
    }
    
    ReadFile(hFile, buf, dwSize, &dwRead, NULL);
    CloseHandle(hFile);
    buf[dwRead] = '\0';
    
    /* Parse header line */
    line = buf;
    nextLine = line;
    while (*nextLine && *nextLine != '\r' && *nextLine != '\n') nextLine++;
    if (*nextLine) {
        if (*nextLine == '\r' && nextLine[1] == '\n') { *nextLine = '\0'; nextLine += 2; }
        else { *nextLine = '\0'; nextLine++; }
    }
    
    nCols = ParseCSVLine(line, fields, CSV_MAX_COLS);
    if (nCols == 0) {
        LocalFree(buf);
        MessageBoxW(g_hwndMain, L"No columns found", L"Import Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    /* Store headers and init types */
    for (i = 0; i < nCols; i++) {
        headers[i] = fields[i];
        types[i] = 0;
    }
    
    /* First pass: infer types from all data rows */
    line = nextLine;
    while (*line) {
        nextLine = line;
        while (*nextLine && *nextLine != '\r' && *nextLine != '\n') nextLine++;
        if (*nextLine) {
            if (*nextLine == '\r' && nextLine[1] == '\n') { *nextLine = '\0'; nextLine += 2; }
            else { *nextLine = '\0'; nextLine++; }
        }
        
        if (*line) {
            int n = ParseCSVLine(line, fields, nCols);
            for (i = 0; i < n; i++) {
                int t = InferType(fields[i]);
                if (t == 3) types[i] = 3;  /* Text trumps all */
                else if (t == 2 && types[i] < 2) types[i] = 2;  /* Real trumps int */
                else if (t == 1 && types[i] == 0) types[i] = 1;  /* Int if unknown */
            }
        }
        line = nextLine;
    }
    
    /* Default unknown types to TEXT */
    for (i = 0; i < nCols; i++) {
        if (types[i] == 0) types[i] = 3;
    }
    
    /* Build CREATE TABLE */
    p = sql;
    for (t = "CREATE TABLE \""; *t; ) *p++ = *t++;
    for (i = 0; tblName[i]; i++) *p++ = (char)tblName[i];
    for (t = "\" ("; *t; ) *p++ = *t++;
    for (i = 0; i < nCols; i++) {
        if (i > 0) { *p++ = ','; *p++ = ' '; }
        *p++ = '"';
        for (t = headers[i]; *t; ) *p++ = *t++;
        *p++ = '"';
        *p++ = ' ';
        if (types[i] == 1) { for (t = "INTEGER"; *t; ) *p++ = *t++; }
        else if (types[i] == 2) { for (t = "REAL"; *t; ) *p++ = *t++; }
        else { for (t = "TEXT"; *t; ) *p++ = *t++; }
    }
    *p++ = ')'; *p = '\0';
    
    if (sqlite_exec(g_db, sql, NULL, NULL, NULL) != SQLITE_OK) {
        LocalFree(buf);
        MessageBoxW(g_hwndMain, L"Could not create table (may already exist)", L"Import Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    /* Re-read file for data insertion */
    hFile = CreateFileW(szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    ReadFile(hFile, buf, dwSize, &dwRead, NULL);
    CloseHandle(hFile);
    buf[dwRead] = '\0';
    
    /* Skip header */
    line = buf;
    while (*line && *line != '\r' && *line != '\n') line++;
    if (*line == '\r' && line[1] == '\n') line += 2;
    else if (*line) line++;
    
    /* Insert rows */
    rowNum = 0;
    while (*line) {
        nextLine = line;
        while (*nextLine && *nextLine != '\r' && *nextLine != '\n') nextLine++;
        if (*nextLine) {
            if (*nextLine == '\r' && nextLine[1] == '\n') { *nextLine = '\0'; nextLine += 2; }
            else { *nextLine = '\0'; nextLine++; }
        }
        
        if (*line) {
            int n = ParseCSVLine(line, fields, nCols);
            
            /* Build INSERT */
            p = sql;
            for (t = "INSERT INTO \""; *t; ) *p++ = *t++;
            for (i = 0; tblName[i]; i++) *p++ = (char)tblName[i];
            for (t = "\" VALUES("; *t; ) *p++ = *t++;
            
            for (i = 0; i < nCols; i++) {
                char *val = (i < n) ? fields[i] : "";
                if (i > 0) *p++ = ',';
                if (!*val) {
                    *p++ = 'N'; *p++ = 'U'; *p++ = 'L'; *p++ = 'L';
                } else if (types[i] == 1 || types[i] == 2) {
                    /* Numeric - insert as-is */
                    while (*val) *p++ = *val++;
                } else {
                    /* Text - quote and escape */
                    *p++ = '\'';
                    while (*val) {
                        if (*val == '\'') *p++ = '\'';
                        *p++ = *val++;
                    }
                    *p++ = '\'';
                }
            }
            *p++ = ')'; *p = '\0';
            
            sqlite_exec(g_db, sql, NULL, NULL, NULL);
            rowNum++;
        }
        line = nextLine;
    }
    
    LocalFree(buf);
    UpdateDbSize();
    
    {
        wchar_t msg[128];
        wsprintfW(msg, L"Imported %d rows into table '%s'", rowNum, tblName);
        MessageBoxW(g_hwndMain, msg, L"Import Complete", MB_OK | MB_ICONINFORMATION);
    }
}

/*============================================================================
** Window Procedure
**============================================================================*/

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            RECT rc;
            int cbHeight;
            HMENU hMenu, hFile, hQuery, hView;
            TBBUTTON tbButtons[11];
            
            /* Command bar with menus */
            g_hwndCB = CommandBar_Create(g_hInst, hwnd, 1);
            
            hMenu = CreateMenu();
            hFile = CreatePopupMenu();
            AppendMenuW(hFile, MF_STRING, IDM_NEW, L"&New Database...");
            AppendMenuW(hFile, MF_STRING, IDM_OPEN, L"&Open Database...");
            AppendMenuW(hFile, MF_STRING, IDM_CLOSE, L"&Close Database");
            AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hFile, MF_STRING, IDM_OPENQUERY, L"Open &Query...\tCtrl+O");
            AppendMenuW(hFile, MF_STRING, IDM_SAVEQUERY, L"&Save Query...\tCtrl+S");
            AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hFile, MF_STRING, IDM_EXPORTCSV, L"&Export Results...");
            AppendMenuW(hFile, MF_STRING, IDM_EXPORTDB, L"Export &Database...");
            AppendMenuW(hFile, MF_STRING, IDM_IMPORTCSV, L"&Import CSV...");
            AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hFile, MF_STRING, IDM_EXIT, L"E&xit");
            AppendMenuW(hMenu, MF_POPUP, (UINT)hFile, L"&File");
            
            hView = CreatePopupMenu();
            AppendMenuW(hView, MF_STRING | MF_CHECKED, IDM_CLEAR, L"&Clear on Execute");
            AppendMenuW(hMenu, MF_POPUP, (UINT)hView, L"&View");
            
            hQuery = CreatePopupMenu();
            AppendMenuW(hQuery, MF_STRING, IDM_EXECUTE, L"&Execute\tCtrl+Enter");
            AppendMenuW(hQuery, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hQuery, MF_STRING, IDM_FIND, L"&Find...\tCtrl+F");
            AppendMenuW(hQuery, MF_STRING, IDM_FINDNEXT, L"Find &Next\tF3");
            AppendMenuW(hMenu, MF_POPUP, (UINT)hQuery, L"&Query");
            
            AppendMenuW(hMenu, MF_STRING, IDM_ABOUT, L"&About");
            
            g_hMenu = hMenu;
            CommandBar_InsertMenubarEx(g_hwndCB, NULL, (LPTSTR)hMenu, 0);
            
            /* Add toolbar bitmaps */
            #define IDB_TOOLBAR 100
            #define TB_PLAY    0   /* Custom: Play triangle for Execute */
            #define TB_QUERY   1   /* Custom: Query editor icon */
            #define TB_RESULTS 2   /* Custom: Text results icon */
            #define TB_GRID    3   /* Custom: Grid/results icon (future) */
            #define TB_OPEN    4   /* Custom: Open database icon */
            #define TB_CLOSE   5   /* Custom: Close database icon */
            #define TB_NEW     6   /* Custom: New database icon */
            #define TB_STD_BASE 7  /* Standard icons start here */
            #define TB_VIEW_BASE (TB_STD_BASE + 15)  /* View icons after standard */
            
            /* Add custom toolbar bitmap (use gray background to match button face) */
            CommandBar_AddBitmap(g_hwndCB, g_hInst, IDB_TOOLBAR, 7, 0, 0);
            CommandBar_AddBitmap(g_hwndCB, HINST_COMMCTRL, IDB_STD_SMALL_COLOR, 15, 0, 0);
            CommandBar_AddBitmap(g_hwndCB, HINST_COMMCTRL, IDB_VIEW_SMALL_COLOR, 12, 0, 0);
            
            memset(tbButtons, 0, sizeof(tbButtons));
            
            /* Separator/gripper between menu and buttons */
            tbButtons[0].fsStyle = TBSTYLE_SEP;
            tbButtons[0].iBitmap = 8;  /* Wider separator acts as gripper */
            
            /* Group 1: View switching */
            tbButtons[1].iBitmap = TB_QUERY;  /* Custom query editor icon */
            tbButtons[1].idCommand = IDM_VIEWQUERY;
            tbButtons[1].fsState = TBSTATE_ENABLED | TBSTATE_CHECKED;
            tbButtons[1].fsStyle = TBSTYLE_CHECK | TBSTYLE_GROUP;
            
            tbButtons[2].iBitmap = TB_RESULTS;  /* Custom text results icon */
            tbButtons[2].idCommand = IDM_VIEWRESULT;
            tbButtons[2].fsState = TBSTATE_ENABLED;
            tbButtons[2].fsStyle = TBSTYLE_CHECK | TBSTYLE_GROUP;
            
            /* Separator */
            tbButtons[3].fsStyle = TBSTYLE_SEP;
            
            /* Font size button (uses standard FIND icon as magnifier) */
            tbButtons[4].iBitmap = TB_STD_BASE + STD_FIND;
            tbButtons[4].idCommand = IDM_FONTSIZE;
            tbButtons[4].fsState = TBSTATE_ENABLED;
            tbButtons[4].fsStyle = TBSTYLE_BUTTON;
            
            /* Separator */
            tbButtons[5].fsStyle = TBSTYLE_SEP;
            
            /* Group 2: File operations */
            tbButtons[6].iBitmap = TB_OPEN;
            tbButtons[6].idCommand = IDM_OPEN;
            tbButtons[6].fsState = TBSTATE_ENABLED;
            tbButtons[6].fsStyle = TBSTYLE_BUTTON;
            
            tbButtons[7].iBitmap = TB_CLOSE;
            tbButtons[7].idCommand = IDM_CLOSE;
            tbButtons[7].fsState = TBSTATE_ENABLED;
            tbButtons[7].fsStyle = TBSTYLE_BUTTON;
            
            tbButtons[8].iBitmap = TB_NEW;
            tbButtons[8].idCommand = IDM_NEW;
            tbButtons[8].fsState = TBSTATE_ENABLED;
            tbButtons[8].fsStyle = TBSTYLE_BUTTON;
            
            /* Separator */
            tbButtons[9].fsStyle = TBSTYLE_SEP;
            
            /* Group 3: Execute (rightmost) */
            tbButtons[10].iBitmap = TB_PLAY;
            tbButtons[10].idCommand = IDM_EXECUTE;
            tbButtons[10].fsState = TBSTATE_ENABLED;
            tbButtons[10].fsStyle = TBSTYLE_BUTTON;
            
            CommandBar_AddButtons(g_hwndCB, 11, tbButtons);
            
            CommandBar_AddAdornments(g_hwndCB, 0, 0);
            cbHeight = CommandBar_Height(g_hwndCB);
            
            GetClientRect(hwnd, &rc);
            
            /* Status bar with two panes */
            g_hwndStatus = CreateWindowW(STATUSCLASSNAMEW, NULL,
                WS_CHILD | WS_VISIBLE,
                0, 0, 0, 0, hwnd, (HMENU)1003, g_hInst, NULL);
            {
                int parts[2] = {120, -1};
                RECT rcStatus;
                int sbHeight, editHeight;
                SendMessage(g_hwndStatus, SB_SETPARTS, 2, (LPARAM)parts);
                GetWindowRect(g_hwndStatus, &rcStatus);
                sbHeight = rcStatus.bottom - rcStatus.top;
                editHeight = rc.bottom - cbHeight - sbHeight;
            
            /* Query input - full height */
            g_hwndQuery = CreateWindowW(
                L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
                ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                0, cbHeight, rc.right, editHeight,
                hwnd, (HMENU)1001, g_hInst, NULL);
            
            /* Subclass to catch Ctrl+Enter */
            g_pfnQueryProc = (WNDPROC)SetWindowLong(g_hwndQuery, GWL_WNDPROC, (LONG)QueryEditProc);
            
            /* Results output - full height, initially hidden */
            g_hwndResult = CreateWindowW(
                L"EDIT", L"",
                WS_CHILD | WS_BORDER | WS_VSCROLL | WS_HSCROLL |
                ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                0, cbHeight, rc.right, editHeight,
                hwnd, (HMENU)1002, g_hInst, NULL);
            }
            
            /* Subclass to block input */
            g_pfnResultProc = (WNDPROC)SetWindowLong(g_hwndResult, GWL_WNDPROC, (LONG)ResultEditProc);
            
            /* Set monospace font on both panes */
            UpdateQueryFont();
            UpdateResultFont();
            
            /* Start in query view */
            g_viewMode = 0;
            
            return 0;
        }
        
        case WM_SIZE: {
            RECT rc, rcStatus;
            int cbHeight, sbHeight, editHeight;
            
            SendMessage(g_hwndStatus, WM_SIZE, 0, 0);  /* Auto-position status bar */
            GetWindowRect(g_hwndStatus, &rcStatus);
            sbHeight = rcStatus.bottom - rcStatus.top;
            
            cbHeight = CommandBar_Height(g_hwndCB);
            GetClientRect(hwnd, &rc);
            editHeight = rc.bottom - cbHeight - sbHeight;
            
            /* Both panes same size, only one visible at a time */
            MoveWindow(g_hwndQuery, 0, cbHeight, rc.right, editHeight, TRUE);
            MoveWindow(g_hwndResult, 0, cbHeight, rc.right, editHeight, TRUE);
            return 0;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDM_NEW:     DoFileNew(); break;
                case IDM_OPEN:    DoFileOpen(); break;
                case IDM_CLOSE:   CloseDatabase(); break;
                case IDM_OPENQUERY: DoOpenQuery(); break;
                case IDM_SAVEQUERY: DoSaveQuery(); break;
                case IDM_EXPORTCSV: DoExportCSV(); break;
                case IDM_EXPORTDB: DoExportDb(); break;
                case IDM_IMPORTCSV: DoImportCSV(); break;
                case IDM_EXIT:    DestroyWindow(hwnd); break;
                case IDM_EXECUTE: ExecuteQuery(); break;
                case IDM_FIND:    DoFind(); break;
                case IDM_FINDNEXT: DoFindNext(); break;
                case IDM_ABOUT:
                    DoAbout();
                    break;
                case IDM_CLEAR:
                    g_clearOnExec = !g_clearOnExec;
                    CheckMenuItem(g_hMenu, IDM_CLEAR, g_clearOnExec ? MF_CHECKED : MF_UNCHECKED);
                    break;
                case IDM_VIEWQUERY:
                    SwitchView(0);
                    SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_VIEWQUERY, TRUE);
                    SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_VIEWRESULT, FALSE);
                    break;
                case IDM_VIEWRESULT:
                    SwitchView(1);
                    SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_VIEWQUERY, FALSE);
                    SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_VIEWRESULT, TRUE);
                    break;
                case IDM_FONTSIZE:
                    CycleFontSize();
                    break;
                case IDOK:        DestroyWindow(hwnd); break;
                case 1001:  /* Query edit control */
                    if (HIWORD(wParam) == EN_CHANGE && g_viewMode == 0)
                        UpdateLineCount();
                    break;
            }
            return 0;
        
        case WM_CTLCOLOREDIT:
            SetBkColor((HDC)wParam, RGB(255, 255, 255));
            return (LRESULT)GetStockObject(WHITE_BRUSH);
        
        case WM_KEYDOWN:
            /* Global shortcuts (when command bar has focus) */
            if (GetKeyState(VK_CONTROL) < 0) {
                if (wParam == 'O') { DoOpenQuery(); return 0; }
                if (wParam == 'N') { DoFileNew(); return 0; }
            }
            if (wParam == VK_F5) { ExecuteQuery(); return 0; }
            /* Enter or arrow keys focus the editor */
            if (wParam == VK_RETURN || wParam == VK_UP || 
                wParam == VK_DOWN || wParam == VK_LEFT || wParam == VK_RIGHT) {
                SetFocus(g_viewMode == 0 ? g_hwndQuery : g_hwndResult);
                return 0;
            }
            break;
        
        case WM_DESTROY:
            CloseDatabase();
            if (g_hFontQuery) DeleteObject(g_hFontQuery);
            if (g_hFontResult) DeleteObject(g_hFontResult);
            CommandBar_Destroy(g_hwndCB);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/*============================================================================
** Entry Point
**============================================================================*/

#define IDI_MAIN 1

#ifndef ICON_SMALL
#define ICON_SMALL 0
#endif

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR lpCmd, int nShow) {
    WNDCLASSW wc = {0};
    MSG msg;
    RECT rcWork;
    
    (void)hPrev; (void)lpCmd; (void)nShow;
    
    g_hInst = hInst;
    InitCommonControls();
    
    SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWork, 0);
    
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_MAIN));
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"SQLiteCEdit";
    RegisterClassW(&wc);
    
    g_hwndMain = CreateWindowW(L"SQLiteCEdit", L"SQLite/CE",
        WS_VISIBLE,
        rcWork.left, rcWork.top,
        rcWork.right - rcWork.left, rcWork.bottom - rcWork.top,
        NULL, NULL, hInst, NULL);
    
    /* Set small icon for taskbar */
    SendMessage(g_hwndMain, WM_SETICON, ICON_SMALL, 
        (LPARAM)LoadImage(hInst, MAKEINTRESOURCE(IDI_MAIN), IMAGE_ICON, 16, 16, 0));
    
    /* Open in-memory database by default */
    OpenDatabase(L":memory:");
    
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}
