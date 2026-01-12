/*
** SQLiteCEdit - File operations (open, save, export, import)
*/

#include "globals.h"

/*============================================================================
** File New/Open
**============================================================================*/

void DoFileNew(void) {
    CE_OPENFILENAME ofn;
    wchar_t szFile[MAX_PATH];
    int createdDir = 0;
    
    /* Try to create default directory if it doesn't exist */
    if (g_szDefaultDbPath[0]) {
        DWORD attr = GetFileAttributesW(g_szDefaultDbPath);
        if (attr == 0xFFFFFFFF) {
            if (CreateDirectoryW(g_szDefaultDbPath, NULL))
                createdDir = 1;
        }
        lstrcpyW(szFile, L"new.db");
    } else {
        lstrcpyW(szFile, L"new.db");
    }
    
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Database Files (*.db)\0*.db\0All Files (*.*)\0*.*\0";
    ofn.lpstrDefExt = L"db";
    ofn.lpstrTitle = L"New Database";
    ofn.lpstrInitialDir = g_szDefaultDbPath[0] ? g_szDefaultDbPath : NULL;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    
    if (GetSaveFileNameW(&ofn)) {
        DeleteFileW(szFile);
        OpenDatabase(szFile);
    } else if (createdDir) {
        /* Remove directory if we created it and user cancelled */
        RemoveDirectoryW(g_szDefaultDbPath);
    }
}

void DoFileOpen(void) {
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
    if (g_szDefaultDbPath[0]) ofn.lpstrInitialDir = g_szDefaultDbPath;
    
    if (GetOpenFileNameW(&ofn)) {
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
            buf = (char*)LocalAlloc(LMEM_FIXED, dwSize + 1);
            if (buf && ReadFile(hFile, buf, dwSize, &dwRead, NULL)) {
                buf[dwRead] = '\0';
                extraCR = 0;
                for (i = 0; i < (int)dwRead; i++)
                    if (buf[i] == '\n' && (i == 0 || buf[i-1] != '\r')) extraCR++;
                wbuf = (wchar_t*)LocalAlloc(LMEM_FIXED, (dwRead + extraCR + 1) * sizeof(wchar_t));
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
    CE_OPENFILENAME ofn;
    wchar_t szFile[MAX_PATH] = L"";
    int i;
    
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"SQL Files (*.sql)\0*.sql\0All Files (*.*)\0*.*\0";
    ofn.lpstrTitle = L"Open Query";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrInitialDir = g_szLastQueryDir;
    
    if (GetOpenFileNameW(&ofn)) {
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
    CE_OPENFILENAME ofn;
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
        lstrcpyW(g_szQueryPath, szFile);
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
            g_queryDirty = 0;
        }
    }
    if (wbuf) LocalFree(wbuf);
    if (buf) LocalFree(buf);
}

/*============================================================================
** Export Results (CSV or Text based on filter selection)
**============================================================================*/

void DoExportResults(void) {
    CE_OPENFILENAME ofn;
    wchar_t szFile[MAX_PATH] = L"results";
    HANDLE hFile;
    DWORD dwLen, dwWritten;
    wchar_t *wbuf, *wp;
    char *buf, *bp;
    int needQuote, isCSV;
    
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"CSV Files (*.csv)\0*.csv\0Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrDefExt = NULL;  /* We'll handle extension ourselves */
    ofn.lpstrTitle = L"Export Results";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.nFilterIndex = 1;
    
    if (!GetSaveFileNameW(&ofn)) return;
    
    /* Check if CSV (filter 1) or Text (filter 2+) */
    isCSV = (ofn.nFilterIndex == 1);
    
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
    
    wbuf = (wchar_t*)LocalAlloc(LMEM_FIXED, (dwLen + 1) * sizeof(wchar_t));
    buf = (char*)LocalAlloc(LMEM_FIXED, (dwLen * 2) + 1);
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
    buf = (char*)LocalAlloc(LMEM_FIXED, (dwLen * 2) + 1);
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
    CE_OPENFILENAME ofn;
    wchar_t szFile[MAX_PATH] = L"results.txt";
    HANDLE hFile;
    DWORD dwLen, dwWritten;
    wchar_t *wbuf;
    char *buf;
    
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrDefExt = L"txt";
    ofn.lpstrTitle = L"Export Results";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    
    if (!GetSaveFileNameW(&ofn)) return;
    
    dwLen = GetWindowTextLengthW(g_hwndResult);
    if (dwLen == 0) return;
    
    wbuf = (wchar_t*)LocalAlloc(LMEM_FIXED, (dwLen + 1) * sizeof(wchar_t));
    buf = (char*)LocalAlloc(LMEM_FIXED, dwLen + 1);
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
** Export Single Table to CSV
**============================================================================*/

/* Table picker dialog state */
static HWND g_hwndTblList;
static char g_selectedTable[128];
static char **g_tableList;
static int g_tableCount;

static LRESULT CALLBACK TablePickerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            RECT rc;
            int i;
            wchar_t wname[128];
            SetWindowTextW(hwnd, L"Select Table");
            GetClientRect(hwnd, &rc);
            g_hwndTblList = CreateWindowW(L"LISTBOX", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                10, 10, rc.right - 20, rc.bottom - 56,
                hwnd, (HMENU)101, g_hInst, NULL);
            for (i = 0; i < g_tableCount; i++) {
                MultiByteToWideChar(CP_ACP, 0, g_tableList[i], -1, wname, 128);
                SendMessageW(g_hwndTblList, LB_ADDSTRING, 0, (LPARAM)wname);
            }
            SendMessage(g_hwndTblList, LB_SETCURSEL, 0, 0);
            CreateWindowW(L"BUTTON", L"Export",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                10, rc.bottom - 36, 60, 26, hwnd, (HMENU)IDOK, g_hInst, NULL);
            CreateWindowW(L"BUTTON", L"Cancel",
                WS_CHILD | WS_VISIBLE,
                80, rc.bottom - 36, 60, 26, hwnd, (HMENU)IDCANCEL, g_hInst, NULL);
            SetFocus(g_hwndTblList);
            return FALSE;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || (LOWORD(wParam) == 101 && HIWORD(wParam) == LBN_DBLCLK)) {
                int sel = (int)SendMessage(g_hwndTblList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < g_tableCount) {
                    strcpy(g_selectedTable, g_tableList[sel]);
                    EndDialog(hwnd, IDOK);
                }
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

static int PickTable(char *tblName) {
    DLGTEMPLATE dlg;
    char **results = NULL;
    int nRows = 0, nCols = 0, ret;
    
    /* Get table list */
    sqlite_get_table(g_db,
        "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name",
        &results, &nRows, &nCols, NULL);
    
    if (!results || nRows < 1) {
        if (results) sqlite_free_table(results);
        MessageBoxW(g_hwndMain, L"No tables in database", L"Export Table", MB_OK);
        return 0;
    }
    
    /* Store for dialog */
    g_tableList = &results[1];  /* Skip header */
    g_tableCount = nRows;
    g_selectedTable[0] = 0;
    
    /* Create dialog */
    memset(&dlg, 0, sizeof(dlg));
    dlg.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_CENTER;
    dlg.cx = 120; dlg.cy = 100;
    
    ret = DialogBoxIndirectW(g_hInst, &dlg, g_hwndMain, (DLGPROC)TablePickerProc);
    
    sqlite_free_table(results);
    
    if (ret == IDOK && g_selectedTable[0]) {
        strcpy(tblName, g_selectedTable);
        return 1;
    }
    return 0;
}

void DoExportTable(void) {
    CE_OPENFILENAME ofn;
    wchar_t szFile[MAX_PATH];
    char tblName[128];
    char sql[512];
    char **result;
    int nRow, nCol, i, j;
    HANDLE hFile;
    DWORD written;
    char line[4096];
    char *lp, *p;
    const char *t;
    
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
    
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"CSV Files (*.csv)\0*.csv\0All Files (*.*)\0*.*\0";
    ofn.lpstrDefExt = L"csv";
    ofn.lpstrTitle = L"Export Table";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    
    if (!GetSaveFileNameW(&ofn)) return;
    
    /* Query table data */
    p = sql;
    for (t = "SELECT * FROM \""; *t; ) *p++ = *t++;
    for (t = tblName; *t; ) *p++ = *t++;
    *p++ = '"'; *p = 0;
    
    if (sqlite_get_table(g_db, sql, &result, &nRow, &nCol, NULL) != SQLITE_OK) return;
    
    hFile = CreateFileW(szFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
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

/*============================================================================
** Export Database
**============================================================================*/

void DoExportDb(void) {
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
            for (t = "SELECT * FROM \""; *t; ) *p++ = *t++;
            for (t = tblName; *t; ) *p++ = *t++;
            *p++ = '"'; *p = '\0';
            
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
