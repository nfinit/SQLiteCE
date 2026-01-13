/*
** SQLiteCEdit - Database operations
*/

#include "globals.h"

void SetStatusDb(const wchar_t *sz) {
    SendMessageW(g_hwndStatus, SB_SETTEXTW, 0, (LPARAM)sz);
}

void SetStatusResult(const wchar_t *sz) {
    /* Save for when switching back to results view */
    int i;
    for (i = 0; i < 63 && sz[i]; i++) g_lastResultStatus[i] = sz[i];
    g_lastResultStatus[i] = 0;
    if (g_viewMode == 1)
        SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)sz);
}

/* Helper to extract filename from path */
const wchar_t *GetFilename(const wchar_t *path) {
    const wchar_t *p = path;
    const wchar_t *last = path;
    while (*p) {
        if (*p == '\\' || *p == '/') last = p + 1;
        p++;
    }
    return last;
}

void UpdateDbSize(void) {
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

/* Update title bar: "[query -] database - SQLite/CE" */
void UpdateTitle(void) {
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

void CloseDatabase(void) {
    if (g_db) {
        sqlite_close(g_db);
        g_db = NULL;
    }
    g_szDbPath[0] = '\0';
    ClearEditMode();
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

int OpenDatabase(const wchar_t *path) {
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
    AddRecentFile(path);
    UpdateTitle();
    UpdateDbSize();
    SetStatusResult(L"");
    RefreshSchema();
    
    /* Update hint if still showing */
    if (g_showingHint && path[0] != ':') {
        wchar_t hint[256];
        wchar_t ver[32];
        const char *v = sqlite_libversion();
        int i;
        for (i = 0; v[i] && i < 31; i++) ver[i] = (wchar_t)v[i];
        ver[i] = 0;
        wsprintfW(hint,
            L"-- SQLite/CEdit " SQLITECEDIT_VERSION L" on SQLite %s.\r\n", ver);
        SetWindowTextW(g_hwndQuery, hint);
    }
    
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
