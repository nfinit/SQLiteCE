/*
** SQLiteCEdit - File operations (open, save, export, import)
*/

#include "globals.h"

/*============================================================================
** File New/Open
**============================================================================*/

void DoFileNew(void) {
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
    
    if (GetOpenFileNameW(&ofn)) {
        OpenDatabase(szFile);
    }
}

/*============================================================================
** Query File Operations
**============================================================================*/

void DoOpenQuery(void) {
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
                        g_showingHint = 0;
                        SetWindowTextW(g_hwndQuery, wbuf);
                        UpdateWindow(g_hwndQuery);
                        lstrcpyW(g_szQueryPath, szFile);
                        UpdateTitle();
                        UpdateLineNumbers();
                    }
                }
                if (buf) LocalFree(buf);
                if (wbuf) LocalFree(wbuf);
            }
            CloseHandle(hFile);
        }
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
        }
    }
    if (wbuf) LocalFree(wbuf);
    if (buf) LocalFree(buf);
}

/*============================================================================
** Export Results to CSV
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
