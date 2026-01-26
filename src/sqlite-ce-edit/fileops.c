/*
** SQLiteCEdit - File operations (open, save, export, import)
*/

#include "globals.h"
#include "allocators.h"
#include "strutils.h"

/*============================================================================
** Storage Card Detection Helper
**============================================================================*/

static int FindStorageCard(wchar_t *cardPath, int maxLen) {
    WIN32_FIND_DATAW fd;
    HANDLE hFind;
    
    hFind = FindFirstFileW(L"\\Storage Card*", &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            wsprintfW(cardPath, L"\\%s", fd.cFileName);
            FindClose(hFind);
            return 1;
        }
        FindClose(hFind);
    }
    cardPath[0] = 0;
    return 0;
}

/* Get effective data path based on storage card option */
static void GetDataPath(wchar_t *path, int maxLen) {
    wchar_t cardPath[MAX_PATH];
    
    if (g_useStorageCardData && FindStorageCard(cardPath, MAX_PATH)) {
        /* Card base path is appended after detected card path */
        wsprintfW(path, L"%s%s%s", cardPath, g_szCardBasePath, g_szDataRelPath);
    } else {
        wsprintfW(path, L"%s%s", g_szLocalBasePath, g_szDataRelPath);
    }
    CreateDirectoryW(path, NULL);
}

/*============================================================================
** File New/Open
**============================================================================*/

void DoFileNew(void) {
    wchar_t szFile[MAX_PATH];
    wchar_t szDataPath[MAX_PATH];
    
    /* Get data path (storage card or My Documents) */
    GetDataPath(szDataPath, MAX_PATH);
    lstrcpyW(szFile, L"new.db");
    
    if (CustomFilePicker(g_hwndMain, szFile, MAX_PATH,
            L"New Database", L"Database Files (*.db)\0*.db\0",
            L"db", szDataPath, 1)) {
        DeleteFileW(szFile);
        OpenDatabase(szFile);
    }
}

void DoFileOpen(void) {
    wchar_t szFile[MAX_PATH] = L"";
    wchar_t szDataPath[MAX_PATH];
    
    /* Get data path (storage card or My Documents) */
    GetDataPath(szDataPath, MAX_PATH);
    
    if (CustomFilePicker(g_hwndMain, szFile, MAX_PATH,
            L"Open Database", L"Database Files (*.db)\0*.db\0",
            NULL, szDataPath, 0)) {
        OpenDatabase(szFile);
    }
}

/*============================================================================
** Recent Files
**============================================================================*/

void AddRecentFile(const wchar_t *path) {
    int i, j;
    /* Don't add :memory: */
    if (lstrcmpW(path, L":memory:") == 0) return;
    /* Check if already in list, move to top if so */
    for (i = 0; i < g_recentCount; i++) {
        if (lstrcmpiW(g_recentFiles[i], path) == 0) {
            /* Move to top */
            for (j = i; j > 0; j--)
                lstrcpyW(g_recentFiles[j], g_recentFiles[j-1]);
            lstrcpyW(g_recentFiles[0], path);
            UpdateRecentMenu();
            return;
        }
    }
    /* Shift down and add at top */
    for (i = MAX_RECENT_FILES - 1; i > 0; i--)
        lstrcpyW(g_recentFiles[i], g_recentFiles[i-1]);
    lstrcpyW(g_recentFiles[0], path);
    if (g_recentCount < MAX_RECENT_FILES) g_recentCount++;
    UpdateRecentMenu();
}

void UpdateRecentMenu(void) {
    int i, j;
    
    if (!g_hRecentDbMenu) return;
    
    /* Clear and rebuild */
    while (RemoveMenu(g_hRecentDbMenu, 0, MF_BYPOSITION));
    
    if (g_recentCount == 0) {
        AppendMenuW(g_hRecentDbMenu, MF_STRING | MF_GRAYED, 0, L"(none)");
    } else {
        for (i = 0; i < g_recentCount; i++) {
            wchar_t item[MAX_PATH + 8];
            const wchar_t *name = g_recentFiles[i];
            int len = lstrlenW(name);
            for (j = len - 1; j >= 0; j--)
                if (name[j] == '\\') { name = &g_recentFiles[i][j+1]; break; }
            wsprintfW(item, L"&%d %s", i + 1, name);
            AppendMenuW(g_hRecentDbMenu, MF_STRING, IDM_RECENT_BASE + i, item);
        }
    }
}

void AddRecentQuery(const wchar_t *path) {
    int i, j;
    /* Check if already in list, move to top if so */
    for (i = 0; i < g_recentQueryCount; i++) {
        if (lstrcmpiW(g_recentQueries[i], path) == 0) {
            for (j = i; j > 0; j--)
                lstrcpyW(g_recentQueries[j], g_recentQueries[j-1]);
            lstrcpyW(g_recentQueries[0], path);
            UpdateRecentQueryMenu();
            return;
        }
    }
    /* Shift down and add at top */
    for (i = MAX_RECENT_FILES - 1; i > 0; i--)
        lstrcpyW(g_recentQueries[i], g_recentQueries[i-1]);
    lstrcpyW(g_recentQueries[0], path);
    if (g_recentQueryCount < MAX_RECENT_FILES) g_recentQueryCount++;
    UpdateRecentQueryMenu();
}

void UpdateRecentQueryMenu(void) {
    int i, j;
    
    if (!g_hRecentQueryMenu) return;
    
    while (RemoveMenu(g_hRecentQueryMenu, 0, MF_BYPOSITION));
    
    if (g_recentQueryCount == 0) {
        AppendMenuW(g_hRecentQueryMenu, MF_STRING | MF_GRAYED, 0, L"(none)");
    } else {
        for (i = 0; i < g_recentQueryCount; i++) {
            wchar_t item[MAX_PATH + 8];
            const wchar_t *name = g_recentQueries[i];
            int len = lstrlenW(name);
            for (j = len - 1; j >= 0; j--)
                if (name[j] == '\\') { name = &g_recentQueries[i][j+1]; break; }
            wsprintfW(item, L"&%d %s", i + 1, name);
            AppendMenuW(g_hRecentQueryMenu, MF_STRING, IDM_RECENT_QUERY_BASE + i, item);
        }
    }
}

/*============================================================================
** Query File Operations
**============================================================================*/

void OpenQueryFile(const wchar_t *path) {
    HANDLE hFile;
    DWORD dwSize, dwRead;
    char *buf;
    wchar_t *wbuf;
    int i, j, extraCR;
    
    hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        dwSize = GetFileSize(hFile, NULL);
        if (dwSize < 65536) {
            buf = ALLOC(char, dwSize + 1);
            if (buf && ReadFile(hFile, buf, dwSize, &dwRead, NULL)) {
                buf[dwRead] = '\0';
                extraCR = 0;
                for (i = 0; i < (int)dwRead; i++)
                    if (buf[i] == '\n' && (i == 0 || buf[i-1] != '\r')) extraCR++;
                wbuf = ALLOC(wchar_t, dwRead + extraCR + 1);
                if (wbuf) {
                    for (i = 0, j = 0; i < (int)dwRead; i++) {
                        if (buf[i] == '\n' && (i == 0 || buf[i-1] != '\r'))
                            wbuf[j++] = '\r';
                        wbuf[j++] = (wchar_t)(unsigned char)buf[i];
                    }
                    wbuf[j] = 0;
                    g_showingHint = 0;
                    SetWindowTextW(g_hwndQuery, wbuf);
                    UpdateWindow(g_hwndQuery);
                    lstrcpyW(g_szQueryPath, path);
                    AddRecentQuery(path);
                    g_queryDirty = 0;
                    UpdateTitle();
                    UpdateLineNumbers();
                    LocalFree(wbuf);
                    /* Switch to query view */
                    if (g_viewMode != 0) {
                        SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWQUERY, 0);
                    }
                }
            }
            if (buf) LocalFree(buf);
        }
        CloseHandle(hFile);
    }
}

void DoOpenQuery(void) {
    wchar_t szFile[MAX_PATH] = L"";
    int i;
    
    if (CustomFilePicker(g_hwndMain, szFile, MAX_PATH,
            L"Open Query", L"SQL Files (*.sql)\0*.sql\0",
            NULL, g_szLastQueryDir, 0)) {
        /* Remember directory for next time */
        lstrcpyW(g_szLastQueryDir, szFile);
        for (i = lstrlenW(g_szLastQueryDir) - 1; i >= 0; i--) {
            if (g_szLastQueryDir[i] == '\\') { g_szLastQueryDir[i] = 0; break; }
        }
        if (g_szLastQueryDir[0] == 0) lstrcpyW(g_szLastQueryDir, L"\\");
        OpenQueryFile(szFile);
    }
}

void DoSaveQuery(void) {
    wchar_t szFile[MAX_PATH];
    HANDLE hFile;
    DWORD dwLen, dwWritten;
    wchar_t *wbuf;
    char *buf;
    DWORD i;
    
    if (g_szQueryPath[0]) {
        lstrcpyW(szFile, g_szQueryPath);
    } else {
        lstrcpyW(szFile, L"query.sql");
        
        if (!CustomFilePicker(g_hwndMain, szFile, MAX_PATH,
                L"Save Query", L"SQL Files (*.sql)\0*.sql\0",
                L"sql", g_szLastQueryDir, 1)) return;
        lstrcpyW(g_szQueryPath, szFile);
        UpdateTitle();
    }
    
    dwLen = GetWindowTextLengthW(g_hwndQuery);
    wbuf = ALLOC(wchar_t, dwLen + 1);
    buf = ALLOC(char, dwLen + 1);
    if (wbuf && buf) {
        GetWindowTextW(g_hwndQuery, wbuf, dwLen + 1);
        for (i = 0; i <= dwLen; i++) buf[i] = (char)wbuf[i];
        hFile = CreateFileW(szFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            WriteFile(hFile, buf, dwLen, &dwWritten, NULL);
            CloseHandle(hFile);
            g_queryDirty = 0;
        }
    }
    if (wbuf) LocalFree(wbuf);
    if (buf) LocalFree(buf);
}

/*============================================================================
** New Query - prompt save if dirty, then clear editor
**============================================================================*/

void DoNewQuery(void) {
    if (g_queryDirty) {
        int r = MessageBoxW(g_hwndMain, L"Save changes to query?", L"SQLite/CE",
                            MB_YESNOCANCEL | MB_ICONQUESTION);
        if (r == IDCANCEL) return;
        if (r == IDYES) DoSaveQuery();
    }
    SetWindowTextW(g_hwndQuery, L"");
    g_szQueryPath[0] = 0;
    g_queryDirty = 0;
    UpdateTitle();
}

/*============================================================================
** Export Results (CSV or Text based on filter selection)
**============================================================================*/

void DoExportResults(void) {
    wchar_t szFile[MAX_PATH] = L"results.csv";
    HANDLE hFile;
    DWORD dwLen, dwWritten;
    wchar_t *wbuf, *wp;
    char *buf, *bp;
    int needQuote, isCSV;
    wchar_t *dot;
    
    if (!CustomFilePicker(g_hwndMain, szFile, MAX_PATH,
            L"Export Results", L"CSV Files (*.csv)\0*.csv\0",
            L"csv", NULL, 1)) return;
    
    /* Check extension to determine format */
    dot = szFile + lstrlenW(szFile) - 4;
    isCSV = (dot > szFile && (lstrcmpiW(dot, L".csv") == 0));
    
    /* Append extension if none present */
    {
        wchar_t *p = szFile + lstrlenW(szFile);
        wchar_t *dot = NULL;
        while (p > szFile && *p != '\\') {
            if (*p == '.') dot = p;
            p--;
        }
        if (!dot) {
            lstrcatW(szFile, isCSV ? L".csv" : L".txt");
        }
    }
    
    dwLen = GetWindowTextLengthW(g_hwndResult);
    if (dwLen == 0) return;
    
    wbuf = ALLOC(wchar_t, dwLen + 1);
    buf = ALLOC(char, (dwLen * 2) + 1);
    if (wbuf && buf) {
        GetWindowTextW(g_hwndResult, wbuf, dwLen + 1);

        if (isCSV) {
            /* Convert tabs to commas, handle quoting */
            bp = buf;
            wp = wbuf;
            while (*wp) {
                if (*wp == '\t') {
                    *bp++ = ',';
                    wp++;
                } else if (*wp == '\r') {
                    wp++;
                } else if (*wp == '\n') {
                    *bp++ = '\r';
                    *bp++ = '\n';
                    wp++;
                } else {
                    wchar_t *fieldStart = wp;
                    needQuote = 0;
                    while (*wp && *wp != '\t' && *wp != '\r' && *wp != '\n') {
                        if (*wp == ',' || *wp == '"') needQuote = 1;
                        wp++;
                    }
                    if (needQuote) {
                        *bp++ = '"';
                        while (fieldStart < wp) {
                            if (*fieldStart == '"') *bp++ = '"';
                            *bp++ = (char)*fieldStart++;
                        }
                        *bp++ = '"';
                    } else {
                        while (fieldStart < wp) *bp++ = (char)*fieldStart++;
                    }
                }
            }
            *bp = '\0';
            dwLen = (DWORD)(bp - buf);
        } else {
            /* Plain text - just convert to ANSI */
            WideCharToMultiByte(CP_ACP, 0, wbuf, -1, buf, (dwLen * 2) + 1, NULL, NULL);
            dwLen = (DWORD)strlen(buf);
        }
        
        hFile = CreateFileW(szFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            WriteFile(hFile, buf, dwLen, &dwWritten, NULL);
            CloseHandle(hFile);
        }
    }
    if (wbuf) LocalFree(wbuf);
    if (buf) LocalFree(buf);
}

/*============================================================================
** Export Results to CSV (legacy, kept for File menu)
**============================================================================*/

void DoExportCSV(void) {
    wchar_t szFile[MAX_PATH] = L"results";
    HANDLE hFile;
    DWORD dwLen, dwWritten;
    wchar_t *wbuf, *wp;
    char *buf, *bp;
    int needQuote;
    
    if (!CustomFilePicker(g_hwndMain, szFile, MAX_PATH,
            L"Export Results",
            L"CSV Files\0*.csv\0All Files\0*.*\0",
            L"csv", NULL, 1)) return;
    
    dwLen = GetWindowTextLengthW(g_hwndResult);
    if (dwLen == 0) return;
    
    wbuf = ALLOC(wchar_t, dwLen + 1);
    buf = ALLOC(char, (dwLen * 2) + 1);
    if (wbuf && buf) {
        GetWindowTextW(g_hwndResult, wbuf, dwLen + 1);
        bp = buf;
        wp = wbuf;
        while (*wp) {
            if (*wp == '\t') {
                *bp++ = ',';
                wp++;
            } else if (*wp == '\r') {
                wp++;
            } else if (*wp == '\n') {
                *bp++ = '\r';
                *bp++ = '\n';
                wp++;
            } else {
                wchar_t *fieldStart = wp;
                needQuote = 0;
                while (*wp && *wp != '\t' && *wp != '\r' && *wp != '\n') {
                    if (*wp == ',' || *wp == '"') needQuote = 1;
                    wp++;
                }
                if (needQuote) {
                    *bp++ = '"';
                    while (fieldStart < wp) {
                        if (*fieldStart == '"') *bp++ = '"';
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

/*============================================================================
** Export Results to Text File
**============================================================================*/

void DoExportTxt(void) {
    wchar_t szFile[MAX_PATH] = L"results";
    HANDLE hFile;
    DWORD dwLen, dwWritten;
    wchar_t *wbuf;
    char *buf;
    
    if (!CustomFilePicker(g_hwndMain, szFile, MAX_PATH,
            L"Export Results",
            L"Text Files\0*.txt\0All Files\0*.*\0",
            L"txt", NULL, 1)) return;
    
    dwLen = GetWindowTextLengthW(g_hwndResult);
    if (dwLen == 0) return;
    
    wbuf = ALLOC(wchar_t, dwLen + 1);
    buf = ALLOC(char, dwLen + 1);
    if (wbuf && buf) {
        GetWindowTextW(g_hwndResult, wbuf, dwLen + 1);
        WideCharToMultiByte(CP_ACP, 0, wbuf, -1, buf, dwLen + 1, NULL, NULL);
        
        hFile = CreateFileW(szFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            WriteFile(hFile, buf, (DWORD)strlen(buf), &dwWritten, NULL);
            CloseHandle(hFile);
        }
    }
    if (wbuf) LocalFree(wbuf);
    if (buf) LocalFree(buf);
}

/*============================================================================
** Export Single Table
**============================================================================*/

static HWND g_hwndPickDlg;
static char g_pickResult[128];
static int g_pickDone;

static LRESULT CALLBACK PickWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_COMMAND) {
        WORD cmd = LOWORD(wParam);
        if (cmd == IDCANCEL) {
            g_pickResult[0] = 0;
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        if (cmd == IDOK || (cmd == 101 && HIWORD(wParam) == LBN_DBLCLK)) {
            HWND hwndList = GetDlgItem(hwnd, 101);
            int sel = (int)SendMessage(hwndList, LB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                wchar_t wname[128];
                SendMessageW(hwndList, LB_GETTEXT, sel, (LPARAM)wname);
                WideCharToMultiByte(CP_ACP, 0, wname, -1, g_pickResult, 128, NULL, NULL);
            }
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
    }
    if (msg == WM_CLOSE) {
        DestroyWindow(hwnd);
        return 0;
    }
    if (msg == WM_DESTROY) {
        g_pickDone = 1;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK PickListProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        SendMessage(GetParent(hwnd), WM_COMMAND, IDOK, 0);
        return 0;
    }
    if (msg == WM_CHAR && wParam == '\r')
        return 0;
    return CallWindowProc((WNDPROC)GetWindowLong(hwnd, GWL_USERDATA), hwnd, msg, wParam, lParam);
}

static int PickTable(char *tblName) {
    char **results = NULL;
    int nRows = 0, nCols = 0;
    MSG msg;
    HWND hwndList, hwndOK, hwndCancel;
    int i;
    wchar_t wname[128];
    WNDCLASSW wc;
    WNDPROC pfnOrig;
    
    /* Get table list */
    sqlite_get_table(g_db,
        "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name",
        &results, &nRows, &nCols, NULL);
    
    if (!results || nRows < 1) {
        if (results) sqlite_free_table(results);
        MessageBoxW(g_hwndMain, L"No tables in database", L"Export Table", MB_OK);
        return 0;
    }
    
    /* Register window class once */
    {
        static int classRegistered = 0;
        if (!classRegistered) {
            memset(&wc, 0, sizeof(wc));
            wc.lpfnWndProc = PickWndProc;
            wc.hInstance = g_hInst;
            wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
            wc.lpszClassName = L"PickTableWnd";
            RegisterClassW(&wc);
            classRegistered = 1;
        }
    }
    
    /* Create popup window */
    g_pickResult[0] = 0;
    g_pickDone = 0;
    g_hwndPickDlg = CreateWindowExW(0, L"PickTableWnd", L"Select Table",
        WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU,
        50, 50, 180, 160, g_hwndMain, NULL, g_hInst, NULL);
    
    hwndList = CreateWindowW(L"LISTBOX", NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
        10, 10, 156, 80, g_hwndPickDlg, (HMENU)101, g_hInst, NULL);
    
    /* Subclass listbox for Enter key */
    pfnOrig = (WNDPROC)SetWindowLong(hwndList, GWL_WNDPROC, (LONG)PickListProc);
    SetWindowLong(hwndList, GWL_USERDATA, (LONG)pfnOrig);
    
    for (i = 0; i < nRows; i++) {
        MultiByteToWideChar(CP_ACP, 0, results[i + 1], -1, wname, 128);
        SendMessageW(hwndList, LB_ADDSTRING, 0, (LPARAM)wname);
    }
    SendMessage(hwndList, LB_SETCURSEL, 0, 0);
    
    hwndOK = CreateWindowW(L"BUTTON", L"Export",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        10, 100, 70, 26, g_hwndPickDlg, (HMENU)IDOK, g_hInst, NULL);
    hwndCancel = CreateWindowW(L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE,
        90, 100, 70, 26, g_hwndPickDlg, (HMENU)IDCANCEL, g_hInst, NULL);
    
    SetFocus(hwndList);
    EnableWindow(g_hwndMain, FALSE);
    
    while (!g_pickDone && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    
    EnableWindow(g_hwndMain, TRUE);
    ShowWindow(g_hwndMain, SW_SHOWNORMAL);
    SetForegroundWindow(g_hwndMain);
    sqlite_free_table(results);
    
    if (g_pickResult[0]) {
        strcpy(tblName, g_pickResult);
        return 1;
    }
    return 0;
}

void DoExportTable(void) {
    wchar_t szFile[MAX_PATH];
    char tblName[128];
    char sql[512];
    char **result;
    int nRow, nCol, i, j, fmt;
    HANDLE hFile;
    DWORD written;
    char line[4096];
    char *lp, *p;
    const char *t;
    wchar_t *ext;
    
    if (!g_db) return;
    
    /* Get table name from schema selection or picker */
    tblName[0] = 0;
    if (g_viewMode == 2 && g_hwndSchema) {
        HTREEITEM hItem = TreeView_GetSelection(g_hwndSchema);
        if (hItem) {
            TV_ITEMW item;
            wchar_t text[128];
            item.mask = TVIF_TEXT | TVIF_IMAGE;
            item.hItem = hItem;
            item.pszText = text;
            item.cchTextMax = 128;
            TreeView_GetItem(g_hwndSchema, &item);
            if (item.iImage == 1) {  /* IMG_TABLE */
                wchar_t *wp = text;
                while (*wp && *wp != ' ' && *wp != '(') wp++;
                *wp = 0;
                WideCharToMultiByte(CP_ACP, 0, text, -1, tblName, 128, NULL, NULL);
            }
        }
    }
    
    /* If no table selected, show picker */
    if (!tblName[0]) {
        if (!PickTable(tblName)) return;
    }
    
    /* Default filename from table name */
    MultiByteToWideChar(CP_ACP, 0, tblName, -1, szFile, MAX_PATH);
    lstrcatW(szFile, L".csv");
    
    if (!CustomFilePicker(g_hwndMain, szFile, MAX_PATH,
            L"Export Table", L"CSV (*.csv)\0*.csv\0",
            L"csv", NULL, 1)) return;
    
    /* Determine format from extension: csv=1, sql=2 (INSERT), default csv */
    ext = szFile + lstrlenW(szFile) - 4;
    if (ext > szFile && lstrcmpiW(ext, L".sql") == 0) {
        fmt = 2;  /* SQL INSERT */
    } else {
        fmt = 1;  /* CSV */
    }
    
    /* Query table data */
    p = sql;
    STR_COPY(p, "SELECT * FROM \"");
    STR_COPY(p, tblName);
    *p++ = '"'; *p = 0;
    
    if (sqlite_get_table(g_db, sql, &result, &nRow, &nCol, NULL) != SQLITE_OK) return;
    
    hFile = CreateFileW(szFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        sqlite_free_table(result);
        return;
    }
    
    if (fmt == 1) {
        /* CSV format */
        /* Write header row */
        lp = line;
        for (j = 0; j < nCol; j++) {
            char *val = result[j];
            if (j > 0) *lp++ = ',';
            if (val) STR_COPY(lp, val);
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
                        STR_COPY(lp, val);
                    }
                }
            }
            *lp++ = '\r'; *lp++ = '\n';
            WriteFile(hFile, line, (DWORD)(lp - line), &written, NULL);
        }
    } else {
        /* SQL format */
        if (fmt == 3) {
            /* Write CREATE statement */
            char **ddlRes;
            int ddlRow, ddlCol;
            p = sql;
            STR_COPY(p, "SELECT sql FROM sqlite_master WHERE type='table' AND name='");
            STR_COPY(p, tblName);
            *p++ = '\''; *p = 0;
            if (sqlite_get_table(g_db, sql, &ddlRes, &ddlRow, &ddlCol, NULL) == SQLITE_OK) {
                if (ddlRow > 0 && ddlRes[1]) {
                    WriteFile(hFile, ddlRes[1], strlen(ddlRes[1]), &written, NULL);
                    WriteFile(hFile, ";\r\n\r\n", 5, &written, NULL);
                }
                sqlite_free_table(ddlRes);
            }
        }
        
        /* Write INSERT statements */
        for (i = 1; i <= nRow; i++) {
            lp = line;
            STR_COPY(lp, "INSERT INTO \"");
            STR_COPY(lp, tblName);
            STR_COPY(lp, "\" VALUES (");
            for (j = 0; j < nCol; j++) {
                char *val = result[i * nCol + j];
                if (j > 0) { *lp++ = ','; *lp++ = ' '; }
                if (!val) {
                    STR_COPY(lp, "NULL");
                } else {
                    *lp++ = '\'';
                    while (*val) {
                        if (*val == '\'') *lp++ = '\'';
                        *lp++ = *val++;
                    }
                    *lp++ = '\'';
                }
            }
            *lp++ = ')'; *lp++ = ';'; *lp++ = '\r'; *lp++ = '\n';
            WriteFile(hFile, line, (DWORD)(lp - line), &written, NULL);
        }
    }
    
    CloseHandle(hFile);
    sqlite_free_table(result);
}

/*============================================================================
** Export HTML Table
**============================================================================*/

static HWND g_hwndHtmlDlg;
static int g_htmlIncludeHeader = 1;
static int g_htmlToClipboard = 0;
static int g_htmlResult = 0;
static wchar_t g_htmlTableId[64] = L"";
static wchar_t g_htmlTableClass[64] = L"";

static LRESULT CALLBACK HtmlDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_COMMAND) {
        WORD cmd = LOWORD(wParam);
        if (cmd == IDCANCEL) {
            g_htmlResult = 0;
            DestroyWindow(hwnd);
            return 0;
        }
        if (cmd == 201 || cmd == 202) {  /* Save to File or Clipboard */
            g_htmlIncludeHeader = (SendMessage(GetDlgItem(hwnd, 101), BM_GETCHECK, 0, 0) == BST_CHECKED);
            GetWindowTextW(GetDlgItem(hwnd, 102), g_htmlTableId, 64);
            GetWindowTextW(GetDlgItem(hwnd, 103), g_htmlTableClass, 64);
            g_htmlToClipboard = (cmd == 202);
            g_htmlResult = 1;
            DestroyWindow(hwnd);
            return 0;
        }
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void DoExportHTML(void) {
    char tblName[128];
    char sql[512];
    char **result;
    int nRow, nCol, i, j;
    char *p, *buf, *bp;
    const char *t;
    size_t bufSize, len;
    WNDCLASSW wc;
    MSG msg;
    HWND hwndChk, hwndLbl, hwndId, hwndClass, hwndSave, hwndClip, hwndCancel;
    
    if (!g_db) return;
    
    /* Get table name */
    tblName[0] = 0;
    if (g_viewMode == 2 && g_hwndSchema) {
        HTREEITEM hItem = TreeView_GetSelection(g_hwndSchema);
        if (hItem) {
            TV_ITEMW item;
            wchar_t text[128];
            item.mask = TVIF_TEXT | TVIF_IMAGE;
            item.hItem = hItem;
            item.pszText = text;
            item.cchTextMax = 128;
            TreeView_GetItem(g_hwndSchema, &item);
            if (item.iImage == 1) {
                wchar_t *wp = text;
                while (*wp && *wp != ' ' && *wp != '(') wp++;
                *wp = 0;
                WideCharToMultiByte(CP_ACP, 0, text, -1, tblName, 128, NULL, NULL);
            }
        }
    }
    if (!tblName[0]) {
        if (!PickTable(tblName)) return;
    }
    
    /* Show options dialog */
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = HtmlDlgProc;
    wc.hInstance = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wc.lpszClassName = L"HtmlExportDlg";
    RegisterClassW(&wc);
    
    g_htmlResult = 0;
    g_hwndHtmlDlg = CreateWindowExW(0, L"HtmlExportDlg", L"HTML Export Options",
        WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU,
        30, 25, 250, 175, g_hwndMain, NULL, g_hInst, NULL);
    
    hwndChk = CreateWindowW(L"BUTTON", L"Include header row (<thead>)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        10, 10, 225, 20, g_hwndHtmlDlg, (HMENU)101, g_hInst, NULL);
    SendMessage(hwndChk, BM_SETCHECK, g_htmlIncludeHeader ? BST_CHECKED : BST_UNCHECKED, 0);
    
    hwndLbl = CreateWindowW(L"STATIC", L"Table ID:",
        WS_CHILD | WS_VISIBLE, 10, 38, 60, 18, g_hwndHtmlDlg, NULL, g_hInst, NULL);
    hwndId = CreateWindowW(L"EDIT", g_htmlTableId,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        75, 35, 160, 22, g_hwndHtmlDlg, (HMENU)102, g_hInst, NULL);
    
    hwndLbl = CreateWindowW(L"STATIC", L"Table class:",
        WS_CHILD | WS_VISIBLE, 10, 63, 60, 18, g_hwndHtmlDlg, NULL, g_hInst, NULL);
    hwndClass = CreateWindowW(L"EDIT", g_htmlTableClass,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        75, 60, 160, 22, g_hwndHtmlDlg, (HMENU)103, g_hInst, NULL);
    
    hwndSave = CreateWindowW(L"BUTTON", L"Save to File...",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        10, 95, 110, 26, g_hwndHtmlDlg, (HMENU)201, g_hInst, NULL);
    hwndClip = CreateWindowW(L"BUTTON", L"To Clipboard",
        WS_CHILD | WS_VISIBLE,
        125, 95, 110, 26, g_hwndHtmlDlg, (HMENU)202, g_hInst, NULL);
    hwndCancel = CreateWindowW(L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE,
        80, 130, 80, 26, g_hwndHtmlDlg, (HMENU)IDCANCEL, g_hInst, NULL);
    
    EnableWindow(g_hwndMain, FALSE);
    
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    
    EnableWindow(g_hwndMain, TRUE);
    SetForegroundWindow(g_hwndMain);
    UnregisterClassW(L"HtmlExportDlg", g_hInst);
    
    if (!g_htmlResult) return;
    
    /* Query table data */
    p = sql;
    STR_COPY(p, "SELECT * FROM \"");
    STR_COPY(p, tblName);
    *p++ = '"'; *p = 0;
    
    if (sqlite_get_table(g_db, sql, &result, &nRow, &nCol, NULL) != SQLITE_OK) return;
    
    /* Estimate buffer size and allocate */
    bufSize = 256 + (nRow + 1) * (nCol * 64 + 32);
    buf = ALLOC(char, bufSize);
    if (!buf) { sqlite_free_table(result); return; }
    bp = buf;
    
    /* Build HTML */
    /* <table> opening tag with optional id/class */
    STR_COPY(bp, "<table");
    if (g_htmlTableId[0]) {
        STR_COPY(bp, " id=\"");
        { wchar_t *w = g_htmlTableId; while (*w) *bp++ = (char)*w++; }
        *bp++ = '"';
    }
    if (g_htmlTableClass[0]) {
        STR_COPY(bp, " class=\"");
        { wchar_t *w = g_htmlTableClass; while (*w) *bp++ = (char)*w++; }
        *bp++ = '"';
    }
    STR_COPY(bp, ">\r\n");

    /* Header row */
    if (g_htmlIncludeHeader) {
        STR_COPY(bp, "<thead><tr>");
        for (j = 0; j < nCol; j++) {
            STR_COPY(bp, "<th>");
            if (result[j]) {
                char *v = result[j];
                while (*v) {
                    if (*v == '<') { STR_COPY(bp, "&lt;"); }
                    else if (*v == '>') { STR_COPY(bp, "&gt;"); }
                    else if (*v == '&') { STR_COPY(bp, "&amp;"); }
                    else *bp++ = *v;
                    v++;
                }
            }
            STR_COPY(bp, "</th>");
        }
        STR_COPY(bp, "</tr></thead>\r\n");
    }

    /* Data rows */
    STR_COPY(bp, "<tbody>\r\n");
    for (i = 1; i <= nRow; i++) {
        STR_COPY(bp, "<tr>");
        for (j = 0; j < nCol; j++) {
            char *val = result[i * nCol + j];
            STR_COPY(bp, "<td>");
            if (val) {
                while (*val) {
                    if (*val == '<') { STR_COPY(bp, "&lt;"); }
                    else if (*val == '>') { STR_COPY(bp, "&gt;"); }
                    else if (*val == '&') { STR_COPY(bp, "&amp;"); }
                    else *bp++ = *val;
                    val++;
                }
            }
            STR_COPY(bp, "</td>");
        }
        STR_COPY(bp, "</tr>\r\n");
    }
    STR_COPY(bp, "</tbody>\r\n</table>\r\n");
    *bp = 0;
    len = bp - buf;
    
    sqlite_free_table(result);
    
    if (g_htmlToClipboard) {
        /* Copy to clipboard - CE needs CF_UNICODETEXT */
        HLOCAL hMem = LocalAlloc(LMEM_MOVEABLE, (len + 1) * sizeof(wchar_t));
        if (hMem) {
            wchar_t *pMem = (wchar_t *)LocalLock(hMem);
            for (i = 0; i <= (int)len; i++) pMem[i] = (wchar_t)(unsigned char)buf[i];
            LocalUnlock(hMem);
            if (OpenClipboard(g_hwndMain)) {
                EmptyClipboard();
                SetClipboardData(CF_UNICODETEXT, hMem);
                CloseClipboard();
            } else {
                LocalFree(hMem);
            }
        }
    } else {
        /* Save to file */
        wchar_t szFile[MAX_PATH];
        HANDLE hFile;
        DWORD written;
        
        MultiByteToWideChar(CP_ACP, 0, tblName, -1, szFile, MAX_PATH);
        
        if (CustomFilePicker(g_hwndMain, szFile, MAX_PATH,
                L"Export HTML Table",
                L"HTML Files\0*.html\0All Files\0*.*\0",
                L"html", NULL, 1)) {
            hFile = CreateFileW(szFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                WriteFile(hFile, buf, (DWORD)len, &written, NULL);
                CloseHandle(hFile);
            }
        }
    }
    
    LocalFree(buf);
}

/*============================================================================
** Export Results as HTML Table
**============================================================================*/

void DoExportHTMLResults(void) {
    int i, j, len;
    char *buf, *bp;
    const char *t;
    size_t bufSize;
    WNDCLASSW wc;
    MSG msg;
    HWND hwndChk, hwndLbl, hwndId, hwndClass, hwndSave, hwndClip, hwndCancel;
    
    /* Check for results */
    if (!g_lastResult || g_lastResultRows < 1 || g_lastResultCols < 1) {
        MessageBoxW(g_hwndMain, L"No results to export", L"Export HTML", MB_OK);
        return;
    }
    
    /* Show options dialog */
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = HtmlDlgProc;
    wc.hInstance = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wc.lpszClassName = L"HtmlExportDlg";
    RegisterClassW(&wc);
    
    g_htmlResult = 0;
    g_hwndHtmlDlg = CreateWindowExW(0, L"HtmlExportDlg", L"HTML Export Options",
        WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU,
        30, 25, 250, 175, g_hwndMain, NULL, g_hInst, NULL);
    
    hwndChk = CreateWindowW(L"BUTTON", L"Include header row (<thead>)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        10, 10, 225, 20, g_hwndHtmlDlg, (HMENU)101, g_hInst, NULL);
    SendMessage(hwndChk, BM_SETCHECK, g_htmlIncludeHeader ? BST_CHECKED : BST_UNCHECKED, 0);
    
    hwndLbl = CreateWindowW(L"STATIC", L"Table ID:",
        WS_CHILD | WS_VISIBLE, 10, 38, 60, 18, g_hwndHtmlDlg, NULL, g_hInst, NULL);
    hwndId = CreateWindowW(L"EDIT", g_htmlTableId,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        75, 35, 160, 22, g_hwndHtmlDlg, (HMENU)102, g_hInst, NULL);
    
    hwndLbl = CreateWindowW(L"STATIC", L"Table class:",
        WS_CHILD | WS_VISIBLE, 10, 63, 60, 18, g_hwndHtmlDlg, NULL, g_hInst, NULL);
    hwndClass = CreateWindowW(L"EDIT", g_htmlTableClass,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        75, 60, 160, 22, g_hwndHtmlDlg, (HMENU)103, g_hInst, NULL);
    
    hwndSave = CreateWindowW(L"BUTTON", L"Save to File...",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        10, 95, 110, 26, g_hwndHtmlDlg, (HMENU)201, g_hInst, NULL);
    hwndClip = CreateWindowW(L"BUTTON", L"To Clipboard",
        WS_CHILD | WS_VISIBLE,
        125, 95, 110, 26, g_hwndHtmlDlg, (HMENU)202, g_hInst, NULL);
    hwndCancel = CreateWindowW(L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE,
        80, 130, 80, 26, g_hwndHtmlDlg, (HMENU)IDCANCEL, g_hInst, NULL);
    
    EnableWindow(g_hwndMain, FALSE);
    
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    
    EnableWindow(g_hwndMain, TRUE);
    SetForegroundWindow(g_hwndMain);
    UnregisterClassW(L"HtmlExportDlg", g_hInst);
    
    if (!g_htmlResult) return;
    
    /* Estimate buffer size and allocate */
    bufSize = 256 + (g_lastResultRows + 1) * (g_lastResultCols * 80 + 32);
    buf = ALLOC(char, bufSize);
    if (!buf) return;
    bp = buf;
    
    /* Build HTML table opening */
    STR_COPY(bp, "<table");
    if (g_htmlTableId[0]) {
        STR_COPY(bp, " id=\"");
        { wchar_t *w = g_htmlTableId; while (*w) *bp++ = (char)*w++; }
        *bp++ = '"';
    }
    if (g_htmlTableClass[0]) {
        STR_COPY(bp, " class=\"");
        { wchar_t *w = g_htmlTableClass; while (*w) *bp++ = (char)*w++; }
        *bp++ = '"';
    }
    STR_COPY(bp, ">\r\n");

    /* Header row */
    if (g_htmlIncludeHeader) {
        STR_COPY(bp, "<thead><tr>");
        for (j = 0; j < g_lastResultCols; j++) {
            char *val = g_lastResult[j];
            STR_COPY(bp, "<th>");
            if (val) {
                while (*val) {
                    if (*val == '<') { STR_COPY(bp, "&lt;"); }
                    else if (*val == '>') { STR_COPY(bp, "&gt;"); }
                    else if (*val == '&') { STR_COPY(bp, "&amp;"); }
                    else *bp++ = *val;
                    val++;
                }
            }
            STR_COPY(bp, "</th>");
        }
        STR_COPY(bp, "</tr></thead>\r\n");
    }

    /* Data rows */
    STR_COPY(bp, "<tbody>\r\n");
    for (i = 1; i <= g_lastResultRows; i++) {
        STR_COPY(bp, "<tr>");
        for (j = 0; j < g_lastResultCols; j++) {
            char *val = g_lastResult[i * g_lastResultCols + j];
            STR_COPY(bp, "<td>");
            if (val) {
                while (*val) {
                    if (*val == '<') { STR_COPY(bp, "&lt;"); }
                    else if (*val == '>') { STR_COPY(bp, "&gt;"); }
                    else if (*val == '&') { STR_COPY(bp, "&amp;"); }
                    else *bp++ = *val;
                    val++;
                }
            }
            STR_COPY(bp, "</td>");
        }
        STR_COPY(bp, "</tr>\r\n");
    }
    STR_COPY(bp, "</tbody>\r\n</table>\r\n");
    *bp = 0;
    len = (int)(bp - buf);
    
    if (g_htmlToClipboard) {
        HLOCAL hMem = LocalAlloc(LMEM_MOVEABLE, (len + 1) * sizeof(wchar_t));
        if (hMem) {
            wchar_t *pMem = (wchar_t *)LocalLock(hMem);
            for (i = 0; i <= len; i++) pMem[i] = (wchar_t)(unsigned char)buf[i];
            LocalUnlock(hMem);
            if (OpenClipboard(g_hwndMain)) {
                EmptyClipboard();
                SetClipboardData(CF_UNICODETEXT, hMem);
                CloseClipboard();
            } else {
                LocalFree(hMem);
            }
        }
    } else {
        wchar_t szFile[MAX_PATH] = L"results";
        HANDLE hFile;
        DWORD written;
        
        if (CustomFilePicker(g_hwndMain, szFile, MAX_PATH,
                L"Export Results as HTML",
                L"HTML Files\0*.html\0All Files\0*.*\0",
                L"html", NULL, 1)) {
            hFile = CreateFileW(szFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                WriteFile(hFile, buf, (DWORD)len, &written, NULL);
                CloseHandle(hFile);
            }
        }
    }
    
    LocalFree(buf);
}

/*============================================================================
** Export Table to CSV (helper for DoExportDb)
**============================================================================*/

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
    STR_COPY_W(wp, dir);
    *wp++ = '\\';
    for (t = tblName; *t; ) *wp++ = (wchar_t)*t++;
    *wp++ = '.'; *wp++ = 'c'; *wp++ = 's'; *wp++ = 'v'; *wp = 0;
    
    /* Build SELECT */
    p = sql;
    STR_COPY(p, "SELECT * FROM \"");
    STR_COPY(p, tblName);
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
        if (val) STR_COPY(lp, val);
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
                    STR_COPY(lp, val);
                }
            }
        }
        *lp++ = '\r'; *lp++ = '\n';
        WriteFile(hFile, line, (DWORD)(lp - line), &written, NULL);
    }

    CloseHandle(hFile);
    sqlite_free_table(result);
}

/*============================================================================
** Export Database
**============================================================================*/

void DoExportDb(void) {
    wchar_t szFile[MAX_PATH] = L"export";
    char szDestPath[MAX_PATH * 2];
    sqlite *destDb;
    char **result;
    int nRow, nCol, i;
    char *errmsg;
    char sql[4096];
    char *p;
    int csvMode;
    
    if (!g_db) return;
    
    if (!CustomFilePicker(g_hwndMain, szFile, MAX_PATH,
            L"Export Database",
            L"SQLite Database\0*.db\0CSV Folder\0*.csv\0",
            L"db", NULL, 1)) return;
    
    /* Detect mode by extension */
    {
        wchar_t *ep = szFile;
        int len;
        while (*ep) ep++;
        len = (int)(ep - szFile);
        
        if (len >= 3 && 
            (szFile[len-3] == '.') &&
            (szFile[len-2] == 'd' || szFile[len-2] == 'D') &&
            (szFile[len-1] == 'b' || szFile[len-1] == 'B')) {
            csvMode = 0;
        } else {
            csvMode = 1;
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
        attr = GetFileAttributesW(szFile);
        if (attr != 0xFFFFFFFF) {
            if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                DeleteFileW(szFile);
            }
        }
        
        CreateDirectoryW(szFile, NULL);
        
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
            
            if (tblSql) {
                sqlite_exec(destDb, tblSql, NULL, NULL, NULL);
            }
            
            p = sql;
            STR_COPY(p, "SELECT * FROM \"");
            STR_COPY(p, tblName);
            *p++ = '"'; *p = '\0';
            
            {
                char **dataResult;
                int dataRow, dataCol, j, k;
                if (sqlite_get_table(g_db, sql, &dataResult, &dataRow, &dataCol, NULL) == SQLITE_OK) {
                    for (j = 1; j <= dataRow; j++) {
                        char *ins = sql;
                        STR_COPY(ins, "INSERT INTO \"");
                        STR_COPY(ins, tblName);
                        STR_COPY(ins, "\" VALUES(");
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
** Backup Database
**============================================================================*/

void DoBackupDatabase(void) {
    wchar_t szBackup[MAX_PATH];
    wchar_t szBackupDir[MAX_PATH];
    wchar_t szCardPath[MAX_PATH];
    wchar_t szDbName[64];
    wchar_t szStatus[128];
    SYSTEMTIME st;
    const wchar_t *fn;
    wchar_t *d;
    HANDLE hSrc, hDst;
    BYTE buf[16384];  /* 16KB buffer for faster backup (was 4KB) */
    DWORD dwRead, dwWritten;
    int ok = 0;
    
    /* Must have a file-based database */
    if (!g_db || g_szDbPath[0] == ':' || g_szDbPath[0] == 0) {
        MessageBoxW(g_hwndMain, L"No database file to backup", L"Backup", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    /* Extract database name (without extension) */
    fn = GetFilename(g_szDbPath);
    d = szDbName;
    while (*fn && *fn != '.' && d < szDbName + 60) *d++ = *fn++;
    *d = 0;
    
    /* Build backup directory path */
    if (g_useStorageCard && FindStorageCard(szCardPath, MAX_PATH)) {
        wsprintfW(szBackupDir, L"%s%s%s", szCardPath, g_szCardBasePath, g_szDataRelPath);
        CreateDirectoryW(szBackupDir, NULL);
        lstrcatW(szBackupDir, L"\\Backups");
    } else {
        wsprintfW(szBackupDir, L"%s%s", g_szLocalBasePath, g_szDataRelPath);
        CreateDirectoryW(szBackupDir, NULL);
        lstrcatW(szBackupDir, L"\\Backups");
    }
    
    /* Create directory structure: BackupDir\dbname\ */
    CreateDirectoryW(szBackupDir, NULL);
    lstrcatW(szBackupDir, L"\\");
    lstrcatW(szBackupDir, szDbName);
    CreateDirectoryW(szBackupDir, NULL);
    
    /* Build backup filename: BackupDir\dbname\dbname_YYYYMMDD_HHMMSS.bak.db */
    GetLocalTime(&st);
    wsprintfW(szBackup, L"%s\\%s_%04d%02d%02d_%02d%02d%02d.bak.db",
        szBackupDir, szDbName,
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    
    /* Show status during backup */
    SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)L"Backing up...");
    UpdateWindow(g_hwndStatus);
    
    /* Close database to ensure file is flushed */
    sqlite_close(g_db);
    g_db = NULL;
    
    /* Copy file */
    hSrc = CreateFileW(g_szDbPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hSrc != INVALID_HANDLE_VALUE) {
        hDst = CreateFileW(szBackup, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hDst != INVALID_HANDLE_VALUE) {
            ok = 1;
            while (ReadFile(hSrc, buf, sizeof(buf), &dwRead, NULL) && dwRead > 0) {
                if (!WriteFile(hDst, buf, dwRead, &dwWritten, NULL) || dwWritten != dwRead) {
                    ok = 0;
                    break;
                }
            }
            CloseHandle(hDst);
            if (!ok) DeleteFileW(szBackup);
        }
        CloseHandle(hSrc);
    }
    
    /* Reopen database */
    {
        char szPath[MAX_PATH * 2];
        WideCharToMultiByte(CP_ACP, 0, g_szDbPath, -1, szPath, sizeof(szPath), NULL, NULL);
        g_db = sqlite_open(szPath, 0, NULL);
    }
    
    if (ok) {
        fn = GetFilename(szBackup);
        wsprintfW(szStatus, L"Backed up to %s", fn);
    } else {
        lstrcpyW(szStatus, L"Backup failed");
        MessageBoxW(g_hwndMain, L"Backup failed", L"Error", MB_OK | MB_ICONERROR);
    }
    SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)szStatus);
    
    RefreshSchema();
}

void DoRestoreDatabase(void) {
    wchar_t szFile[MAX_PATH];
    wchar_t szInitDir[MAX_PATH];
    wchar_t szCardPath[MAX_PATH];
    wchar_t szDbName[64];
    wchar_t szMsg[MAX_PATH + 64];
    const wchar_t *fn;
    wchar_t *d;
    HANDLE hSrc, hDst, hFind;
    WIN32_FIND_DATAW fd;
    BYTE buf[16384];  /* 16KB buffer for faster restore (was 4KB) */
    DWORD dwRead, dwWritten;
    int ok = 0;

    /* Extract database name (without extension) */
    fn = GetFilename(g_szDbPath);
    d = szDbName;
    while (*fn && *fn != '.' && d < szDbName + 60) *d++ = *fn++;
    *d = 0;
    
    /* Build initial directory: Backups\<dbname>\ if exists, else Backups\ */
    if (g_useStorageCard && FindStorageCard(szCardPath, MAX_PATH)) {
        wsprintfW(szInitDir, L"%s%s%s\\Backups\\%s", szCardPath, g_szCardBasePath, g_szDataRelPath, szDbName);
    } else {
        wsprintfW(szInitDir, L"%s%s\\Backups\\%s", g_szLocalBasePath, g_szDataRelPath, szDbName);
    }
    /* Check if db-specific backup dir exists, fall back to Backups\ */
    hFind = FindFirstFileW(szInitDir, &fd);
    if (hFind == INVALID_HANDLE_VALUE || !(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        /* Strip dbname, use parent Backups dir */
        d = szInitDir + lstrlenW(szInitDir);
        while (d > szInitDir && *(d-1) != '\\') d--;
        if (d > szInitDir) *(d-1) = 0;
    }
    if (hFind != INVALID_HANDLE_VALUE) FindClose(hFind);
    
    /* File picker */
    szFile[0] = 0;
    if (!CustomFilePicker(g_hwndMain, szFile, MAX_PATH,
                          L"Restore Database", L"*.db", L"db",
                          szInitDir, 0)) return;
    
    /* Confirmation */
    fn = GetFilename(szFile);
    wsprintfW(szMsg, L"Replace current database with:\n%s?", fn);
    if (MessageBoxW(g_hwndMain, szMsg, L"Restore Database", 
                    MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    
    SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)L"Restoring...");
    UpdateWindow(g_hwndStatus);
    
    /* Close database */
    sqlite_close(g_db);
    g_db = NULL;
    
    /* Copy backup over current database */
    hSrc = CreateFileW(szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hSrc != INVALID_HANDLE_VALUE) {
        hDst = CreateFileW(g_szDbPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hDst != INVALID_HANDLE_VALUE) {
            ok = 1;
            while (ReadFile(hSrc, buf, sizeof(buf), &dwRead, NULL) && dwRead > 0) {
                if (!WriteFile(hDst, buf, dwRead, &dwWritten, NULL) || dwWritten != dwRead) {
                    ok = 0;
                    break;
                }
            }
            CloseHandle(hDst);
        }
        CloseHandle(hSrc);
    }
    
    /* Reopen database */
    {
        char szPath[MAX_PATH * 2];
        WideCharToMultiByte(CP_ACP, 0, g_szDbPath, -1, szPath, sizeof(szPath), NULL, NULL);
        g_db = sqlite_open(szPath, 0, NULL);
    }
    
    /* Reset UI to clean state */
    ClearEditMode();
    if (ok) {
        ShowResultMessage(L"Database restored successfully.", 1);
        SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)L"Database restored");
    } else {
        ShowResultMessage(L"Restore failed.", 1);
        MessageBoxW(g_hwndMain, L"Restore failed", L"Error", MB_OK | MB_ICONERROR);
        SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)L"Restore failed");
    }
    
    RefreshSchema();
}
