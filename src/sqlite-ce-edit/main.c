/*
** SQLiteCEdit - SQL Query Editor for Windows CE
*/

#include <windows.h>
#include <commctrl.h>
#include "sqlite.h"

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
static HFONT g_hFont = NULL;
static WNDPROC g_pfnQueryProc;   /* Original query edit proc */
static WNDPROC g_pfnResultProc;  /* Original result edit proc */

/* Output buffer */
static char g_szOutput[32000];
static int g_nOutput = 0;

/*============================================================================
** Menu IDs
**============================================================================*/

#define IDM_NEW      101
#define IDM_OPEN     102
#define IDM_CLOSE    103
#define IDM_EXIT     104
#define IDM_EXECUTE  201
#define IDM_CLEAR    301

/*============================================================================
** Output Helpers
**============================================================================*/

static int g_clearOnExec = 1;  /* Clear results before each execution */

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

static void SetStatusResult(const wchar_t *sz) {
    SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)sz);
}

static void ExecuteQuery(void);  /* Forward declaration */

/* Subclass proc for query edit - catches Ctrl+Enter */
static LRESULT CALLBACK QueryEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN && GetKeyState(VK_CONTROL) < 0) {
        ExecuteQuery();
        return 0;
    }
    /* Suppress the newline character from Ctrl+Enter */
    if (msg == WM_CHAR && wParam == '\n')
        return 0;
    return CallWindowProc(g_pfnQueryProc, hwnd, msg, wParam, lParam);
}

/* Subclass proc for result edit - blocks input but allows copy */
static LRESULT CALLBACK ResultEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    /* Allow Ctrl+C and Ctrl+A */
    if (msg == WM_KEYDOWN && GetKeyState(VK_CONTROL) < 0 && (wParam == 'C' || wParam == 'A'))
        return CallWindowProc(g_pfnResultProc, hwnd, msg, wParam, lParam);
    if (msg == WM_CHAR || msg == WM_KEYDOWN || msg == WM_KEYUP)
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
    UpdateWindow(g_hwndStatus);  /* Force repaint before query */
    
    rc = sqlite_exec(g_db, sql, QueryCallback, NULL, &errmsg);
    
    if (rc != SQLITE_OK) {
        Output("Error: ");
        OutputLine(errmsg ? errmsg : sqlite_error_string(rc));
        if (errmsg) sqlite_freemem(errmsg);
        SetStatusResult(L"Error");
    } else if (g_nRows == 0) {
        /* Check if it was a non-SELECT statement */
        int changes = sqlite_changes(g_db);
        if (changes > 0) {
            wchar_t wbuf[64];
            char buf[32];
            char *p = buf + 30;
            buf[31] = '\0';
            *p = '\0';
            while (changes > 0) { *--p = '0' + (changes % 10); changes /= 10; }
            Output(p);
            OutputLine(" row(s) affected.");
            wsprintfW(wbuf, L"%hs row(s) affected", p);
            SetStatusResult(wbuf);
        } else {
            OutputLine("OK");
            SetStatusResult(L"OK");
        }
    } else {
        wchar_t wbuf[64];
        /* Flush final result set (adds to g_totalRows) */
        FlushResultSet();
        wsprintfW(wbuf, L"%d row(s) returned", g_totalRows);
        SetStatusResult(wbuf);
    }
    
    LocalFree(sql);
    FlushOutput();
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

/* Update title bar: "filename - SQLite/CE" or just "SQLite/CE" */
static void UpdateTitle(void) {
    wchar_t title[MAX_PATH];
    if (g_szDbPath[0]) {
        const wchar_t *fn = GetFilename(g_szDbPath);
        wchar_t *p = title;
        /* Special case for :memory: */
        if (g_szDbPath[0] == ':') {
            lstrcpyW(title, L"(memory) - SQLite/CE");
        } else {
            while (*fn && p < title + MAX_PATH - 15) *p++ = *fn++;
            lstrcpyW(p, L" - SQLite/CE");
        }
    } else {
        lstrcpyW(title, L"SQLite/CE");
    }
    SetWindowTextW(g_hwndMain, title);
}

static void CloseDatabase(void) {
    if (g_db) {
        sqlite_close(g_db);
        g_db = NULL;
    }
    g_szDbPath[0] = '\0';
    UpdateTitle();
    SetWindowTextW(g_hwndResult, L"");
    SetStatusDb(L"No database");
    SetStatusResult(L"");
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
    SetStatusDb(GetFilename(path));
    SetStatusResult(L"");
    
    /* Show hint for in-memory database */
    if (path[0] == ':') {
        SetWindowTextW(g_hwndResult, 
            L"Using in-memory database.\r\n"
            L"Use File > Open to open a database file.\r\n"
            L"Use File > New to create a new file.");
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

static void DoFileNew(void) {
    if (PromptForPath(L"New Database", L"\\Temp\\new.db")) {
        DeleteFileW(g_szPathBuf);
        OpenDatabase(g_szPathBuf);
    }
}

static void DoFileOpen(void) {
    if (PromptForPath(L"Open Database", L"\\Temp\\")) {
        OpenDatabase(g_szPathBuf);
    }
}

/*============================================================================
** Window Procedure
**============================================================================*/

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            RECT rc;
            int cbHeight, h;
            HMENU hMenu, hFile, hQuery, hView;
            
            /* Command bar with menus */
            g_hwndCB = CommandBar_Create(g_hInst, hwnd, 1);
            
            hMenu = CreateMenu();
            hFile = CreatePopupMenu();
            AppendMenuW(hFile, MF_STRING, IDM_NEW, L"&New Database...");
            AppendMenuW(hFile, MF_STRING, IDM_OPEN, L"&Open Database...");
            AppendMenuW(hFile, MF_STRING, IDM_CLOSE, L"&Close Database");
            AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hFile, MF_STRING, IDM_EXIT, L"E&xit");
            AppendMenuW(hMenu, MF_POPUP, (UINT)hFile, L"&File");
            
            hView = CreatePopupMenu();
            AppendMenuW(hView, MF_STRING | MF_CHECKED, IDM_CLEAR, L"&Clear on Execute");
            AppendMenuW(hMenu, MF_POPUP, (UINT)hView, L"&View");
            
            hQuery = CreatePopupMenu();
            AppendMenuW(hQuery, MF_STRING, IDM_EXECUTE, L"&Execute\tCtrl+Enter");
            AppendMenuW(hMenu, MF_POPUP, (UINT)hQuery, L"&Query");
            
            g_hMenu = hMenu;
            CommandBar_InsertMenubarEx(g_hwndCB, NULL, (LPTSTR)hMenu, 0);
            CommandBar_AddAdornments(g_hwndCB, 0, 0);
            cbHeight = CommandBar_Height(g_hwndCB);
            
            GetClientRect(hwnd, &rc);
            h = (rc.bottom - cbHeight) / 3;  /* Query gets 1/3, results 2/3 */
            
            /* Query input */
            g_hwndQuery = CreateWindowW(
                L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
                ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                0, cbHeight, rc.right, h,
                hwnd, (HMENU)1001, g_hInst, NULL);
            
            /* Subclass to catch Ctrl+Enter */
            g_pfnQueryProc = (WNDPROC)SetWindowLong(g_hwndQuery, GWL_WNDPROC, (LONG)QueryEditProc);
            
            /* Results output - no ES_READONLY so background paints correctly */
            g_hwndResult = CreateWindowW(
                L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL |
                ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                0, cbHeight + h, rc.right, rc.bottom - cbHeight - h,
                hwnd, (HMENU)1002, g_hInst, NULL);
            
            /* Subclass to block input */
            g_pfnResultProc = (WNDPROC)SetWindowLong(g_hwndResult, GWL_WNDPROC, (LONG)ResultEditProc);
            
            /* Status bar with two panes */
            g_hwndStatus = CreateWindowW(STATUSCLASSNAMEW, NULL,
                WS_CHILD | WS_VISIBLE,
                0, 0, 0, 0, hwnd, (HMENU)1003, g_hInst, NULL);
            {
                int parts[2] = {120, -1};  /* 120px for db name, rest for results */
                SendMessage(g_hwndStatus, SB_SETPARTS, 2, (LPARAM)parts);
            }
            
            /* Set monospace font on both panes */
            {
                LOGFONTW lf;
                memset(&lf, 0, sizeof(lf));
                lf.lfHeight = 14;
                lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
                lstrcpyW(lf.lfFaceName, L"Courier New");
                g_hFont = CreateFontIndirectW(&lf);
                SendMessage(g_hwndQuery, WM_SETFONT, (WPARAM)g_hFont, TRUE);
                SendMessage(g_hwndResult, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            }
            
            return 0;
        }
        
        case WM_SIZE: {
            RECT rc, rcStatus;
            int cbHeight, sbHeight, h;
            
            SendMessage(g_hwndStatus, WM_SIZE, 0, 0);  /* Auto-position status bar */
            GetWindowRect(g_hwndStatus, &rcStatus);
            sbHeight = rcStatus.bottom - rcStatus.top;
            
            cbHeight = CommandBar_Height(g_hwndCB);
            GetClientRect(hwnd, &rc);
            h = (rc.bottom - cbHeight - sbHeight) / 3;
            
            MoveWindow(g_hwndQuery, 0, cbHeight, rc.right, h, TRUE);
            MoveWindow(g_hwndResult, 0, cbHeight + h, rc.right, rc.bottom - cbHeight - sbHeight - h, TRUE);
            return 0;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDM_NEW:     DoFileNew(); break;
                case IDM_OPEN:    DoFileOpen(); break;
                case IDM_CLOSE:   CloseDatabase(); break;
                case IDM_EXIT:    DestroyWindow(hwnd); break;
                case IDM_EXECUTE: ExecuteQuery(); break;
                case IDM_CLEAR:
                    g_clearOnExec = !g_clearOnExec;
                    CheckMenuItem(g_hMenu, IDM_CLEAR, g_clearOnExec ? MF_CHECKED : MF_UNCHECKED);
                    break;
                case IDOK:        DestroyWindow(hwnd); break;
            }
            return 0;
        
        case WM_CTLCOLOREDIT:
            SetBkColor((HDC)wParam, RGB(255, 255, 255));
            return (LRESULT)GetStockObject(WHITE_BRUSH);
        
        case WM_DESTROY:
            CloseDatabase();
            if (g_hFont) DeleteObject(g_hFont);
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
