/*
** SQLiteCEdit - Import operations (CSV and CEDB)
*/

#include "globals.h"
#include "constants.h"
#include "strutils.h"
#include "allocators.h"

/*============================================================================
** CSV Import
**============================================================================*/

static int InferType(const char *val) {
    const char *s = val;
    int hasDot = 0;
    if (!*s) return 0;
    if (*s == '-') s++;
    if (!*s) return 3;
    while (*s) {
        if (*s == '.') {
            if (hasDot) return 3;
            hasDot = 1;
        } else if (*s < '0' || *s > '9') {
            return 3;
        }
        s++;
    }
    return hasDot ? 2 : 1;
}

static int ParseCSVLine(char *line, char **fields, int maxFields) {
    int n = 0;
    char *p = line;
    while (*p && n < maxFields) {
        if (*p == '"') {
            p++;
            fields[n++] = p;
            while (*p && !(*p == '"' && (p[1] == ',' || p[1] == '\0' || p[1] == '\r' || p[1] == '\n'))) {
                if (*p == '"' && p[1] == '"') p++;
                p++;
            }
            if (*p == '"') *p++ = '\0';
            if (*p == ',') p++;
        } else {
            fields[n++] = p;
            while (*p && *p != ',' && *p != '\r' && *p != '\n') p++;
            if (*p == ',') *p++ = '\0';
            else if (*p) { *p = '\0'; break; }
        }
    }
    return n;
}

void DoImportCSV(void) {
    wchar_t szFile[MAX_PATH] = L"";
    wchar_t tblName[64];
    wchar_t *wp;
    const wchar_t *fn;
    HANDLE hFile;
    DWORD dwSize, dwRead;
    char *buf, *line, *nextLine;
    char *fields[CSV_MAX_COLS];
    char *headers[CSV_MAX_COLS];
    int types[CSV_MAX_COLS];
    int nCols, i, rowNum;
    char sql[4096];
    char *p, *t;
    
    if (!g_db) {
        MessageBoxW(g_hwndMain, L"No database open", L"Import", MB_OK | MB_ICONERROR);
        return;
    }
    
    if (!CustomFilePicker(g_hwndMain, szFile, MAX_PATH,
            L"Import CSV", L"CSV Files (*.csv)\0*.csv\0",
            NULL, NULL, 0)) return;
    
    fn = GetFilename(szFile);
    wp = tblName;
    while (*fn && *fn != '.' && wp < tblName + 60) *wp++ = *fn++;
    *wp = 0;
    
    hFile = CreateFileW(szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(g_hwndMain, L"Could not open file", L"Import Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    dwSize = GetFileSize(hFile, NULL);
    if (dwSize > CSV_MAX_FILE_SIZE) {
        CloseHandle(hFile);
        MessageBoxW(g_hwndMain, L"File too large (max 1MB)", L"Import Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    buf = ALLOC(char, dwSize + 1);
    if (!buf) {
        CloseHandle(hFile);
        return;
    }
    
    ReadFile(hFile, buf, dwSize, &dwRead, NULL);
    CloseHandle(hFile);
    buf[dwRead] = '\0';
    
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
    
    for (i = 0; i < nCols; i++) {
        headers[i] = fields[i];
        types[i] = 0;
    }
    
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
                int tp = InferType(fields[i]);
                if (tp == 3) types[i] = 3;
                else if (tp == 2 && types[i] < 2) types[i] = 2;
                else if (tp == 1 && types[i] == 0) types[i] = 1;
            }
        }
        line = nextLine;
    }
    
    for (i = 0; i < nCols; i++) {
        if (types[i] == 0) types[i] = 3;
    }
    
    p = sql;
    for (t = "CREATE TABLE \""; *t; ) *p++ = *t++;
    for (i = 0; tblName[i]; i++) *p++ = (char)tblName[i];
    for (t = "\" ("; *t; ) *p++ = *t++;
    for (i = 0; i < nCols; i++) {
        if (i > 0) { *p++ = ','; *p++ = ' '; }
        *p++ = '"';
        STR_COPY(p, headers[i]);
        *p++ = '"';
        *p++ = ' ';
        if (types[i] == 1) { STR_COPY(p, "INTEGER"); }
        else if (types[i] == 2) { STR_COPY(p, "REAL"); }
        else { STR_COPY(p, "TEXT"); }
    }
    *p++ = ')'; *p = '\0';
    
    if (sqlite_exec(g_db, sql, NULL, NULL, NULL) != SQLITE_OK) {
        LocalFree(buf);
        MessageBoxW(g_hwndMain, L"Could not create table (may already exist)", L"Import Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    hFile = CreateFileW(szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    ReadFile(hFile, buf, dwSize, &dwRead, NULL);
    CloseHandle(hFile);
    buf[dwRead] = '\0';
    
    line = buf;
    while (*line && *line != '\r' && *line != '\n') line++;
    if (*line == '\r' && line[1] == '\n') line += 2;
    else if (*line) line++;
    
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
                    STR_COPY(p, val);
                } else {
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
    
    /* Show completion in status bar instead of blocking message box */
    {
        wchar_t msg[128];
        wsprintfW(msg, L"Imported %d rows into '%s'", rowNum, tblName);
        SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)msg);
    }
    RefreshSchema();
}

/*============================================================================
** CEDB Import
**============================================================================*/

static wchar_t **g_cedbList;
static int g_cedbCount;
static int g_cedbSel;
static HWND g_hwndCedbList;

static LRESULT CALLBACK CedbDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            RECT rc;
            int i;
            GetClientRect(hwnd, &rc);
            g_hwndCedbList = CreateWindowW(L"LISTBOX", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                10, 10, rc.right - 20, rc.bottom - 56,
                hwnd, (HMENU)101, g_hInst, NULL);
            for (i = 0; i < g_cedbCount; i++) {
                SendMessageW(g_hwndCedbList, LB_ADDSTRING, 0, (LPARAM)g_cedbList[i]);
            }
            SendMessage(g_hwndCedbList, LB_SETCURSEL, 0, 0);
            CreateWindowW(L"BUTTON", L"Import",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                10, rc.bottom - 36, 60, 26, hwnd, (HMENU)IDOK, g_hInst, NULL);
            CreateWindowW(L"BUTTON", L"Cancel",
                WS_CHILD | WS_VISIBLE,
                80, rc.bottom - 36, 60, 26, hwnd, (HMENU)IDCANCEL, g_hInst, NULL);
            SetFocus(g_hwndCedbList);
            return FALSE;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || (LOWORD(wParam) == 101 && HIWORD(wParam) == LBN_DBLCLK)) {
                g_cedbSel = (int)SendMessage(g_hwndCedbList, LB_GETCURSEL, 0, 0);
                if (g_cedbSel < 0) g_cedbSel = 0;
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

static wchar_t g_cedbTblName[64];
static HWND g_hwndTblEdit;
static HWND g_hwndTblDlg;
static WNDPROC g_pfnTblEditProc;

static LRESULT CALLBACK TblEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        GetWindowTextW(hwnd, g_cedbTblName, 64);
        EndDialog(g_hwndTblDlg, IDOK);
        return 0;
    }
    if (msg == WM_CHAR && wParam == '\r')
        return 0;
    return CallWindowProc(g_pfnTblEditProc, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK CedbNameDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            RECT rc;
            g_hwndTblDlg = hwnd;
            GetClientRect(hwnd, &rc);
            CreateWindowW(L"STATIC", L"Table name:",
                WS_CHILD | WS_VISIBLE,
                10, 12, 70, 20, hwnd, NULL, g_hInst, NULL);
            g_hwndTblEdit = CreateWindowW(L"EDIT", g_cedbTblName,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                80, 10, rc.right - 90, 24,
                hwnd, (HMENU)101, g_hInst, NULL);
            g_pfnTblEditProc = (WNDPROC)SetWindowLong(g_hwndTblEdit, GWL_WNDPROC, (LONG)TblEditProc);
            CreateWindowW(L"BUTTON", L"Import",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                10, 44, 60, 26, hwnd, (HMENU)IDOK, g_hInst, NULL);
            CreateWindowW(L"BUTTON", L"Cancel",
                WS_CHILD | WS_VISIBLE,
                80, 44, 60, 26, hwnd, (HMENU)IDCANCEL, g_hInst, NULL);
            SendMessage(g_hwndTblEdit, EM_SETSEL, 0, -1);
            SetFocus(g_hwndTblEdit);
            return FALSE;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                GetWindowTextW(g_hwndTblEdit, g_cedbTblName, 64);
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

void DoImportCEDB(void) {
    HANDLE hEnum, hDb;
    CEOID oid, recOid;
    CEOIDINFO oidInfo;
    wchar_t *dbNames[64];
    CEOID dbOids[64];
    int dbCount = 0;
    int i, sel = 0;
    WORD nProps;
    CEPROPVAL *pProps = NULL;
    DWORD cbBuf = 0;
    int rowCount = 0;
    char sql[4096];
    char *p;
    wchar_t tblName[64];
    
    if (!g_db) {
        MessageBoxW(g_hwndMain, L"No database open.", L"Import", MB_OK | MB_ICONWARNING);
        return;
    }
    
    hEnum = (HANDLE)CeFindFirstDatabase(0);
    if (hEnum == INVALID_HANDLE_VALUE) {
        MessageBoxW(g_hwndMain, L"Could not enumerate databases.", L"Import", MB_OK | MB_ICONERROR);
        return;
    }
    
    while ((oid = CeFindNextDatabase(hEnum)) != 0 && dbCount < 64) {
        if (CeOidGetInfo(oid, &oidInfo) && oidInfo.wObjType == OBJTYPE_DATABASE) {
            dbNames[dbCount] = ALLOC(wchar_t, 64);
            if (dbNames[dbCount]) {
                lstrcpyW(dbNames[dbCount], oidInfo.infDatabase.szDbaseName);
                dbOids[dbCount] = oid;
                dbCount++;
            }
        }
    }
    CloseHandle(hEnum);
    
    if (dbCount == 0) {
        MessageBoxW(g_hwndMain, L"No CE databases found.", L"Import", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    {
        struct {
            DLGTEMPLATE tmpl;
            WORD menu, wndclass, title;
        } dlg;
        
        g_cedbList = dbNames;
        g_cedbCount = dbCount;
        g_cedbSel = 0;
        
        memset(&dlg, 0, sizeof(dlg));
        dlg.tmpl.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
        dlg.tmpl.cx = 140;
        dlg.tmpl.cy = 100;
        dlg.tmpl.x = 20;
        dlg.tmpl.y = 20;
        
        if (DialogBoxIndirectW(g_hInst, &dlg.tmpl, g_hwndMain, CedbDlgProc) != IDOK) {
            goto cleanup;
        }
        sel = g_cedbSel;
    }
    
    {
        wchar_t *src = dbNames[sel];
        wchar_t *lastSlash = src;
        wchar_t *dst = tblName;
        while (*src) {
            if (*src == '\\' || *src == '/') lastSlash = src + 1;
            src++;
        }
        src = lastSlash;
        while (*src) {
            if (*src != ' ') *dst++ = *src;
            src++;
        }
        *dst = 0;
    }
    {
        int len = lstrlenW(tblName);
        if (len > 8 && lstrcmpiW(tblName + len - 8, L"Database") == 0) {
            tblName[len - 8] = 0;
        } else if (len > 2 && lstrcmpiW(tblName + len - 2, L"DB") == 0) {
            tblName[len - 2] = 0;
        }
    }
    
    {
        struct {
            DLGTEMPLATE tmpl;
            WORD menu, wndclass, title;
        } dlg;
        
        lstrcpyW(g_cedbTblName, tblName);
        
        memset(&dlg, 0, sizeof(dlg));
        dlg.tmpl.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
        dlg.tmpl.cx = 140;
        dlg.tmpl.cy = 50;
        dlg.tmpl.x = 20;
        dlg.tmpl.y = 20;
        
        if (DialogBoxIndirectW(g_hInst, &dlg.tmpl, g_hwndMain, CedbNameDlgProc) != IDOK) {
            goto cleanup;
        }
        lstrcpyW(tblName, g_cedbTblName);
    }
    
    oid = dbOids[sel];
    hDb = CeOpenDatabase(&oid, NULL, 0, CEDB_AUTOINCREMENT, NULL);
    if (hDb == INVALID_HANDLE_VALUE) {
        MessageBoxW(g_hwndMain, L"Could not open CE database.", L"Import", MB_OK | MB_ICONERROR);
        goto cleanup;
    }
    
    p = sql;
    STR_COPY(p, "CREATE TABLE \"");
    for (i = 0; tblName[i]; i++) *p++ = (char)tblName[i];
    STR_COPY(p, "\" (ceoid INTEGER PRIMARY KEY, data TEXT)");
    *p = 0;
    
    {
        char *errmsg = NULL;
        int rc = sqlite_exec(g_db, sql, NULL, NULL, &errmsg);
        if (rc != SQLITE_OK) {
            wchar_t wmsg[256];
            wsprintfW(wmsg, L"Could not create table: %S", errmsg ? errmsg : "unknown error");
            MessageBoxW(g_hwndMain, wmsg, L"Import", MB_OK | MB_ICONERROR);
            if (errmsg) sqlite_freemem(errmsg);
            CloseHandle(hDb);
            goto cleanup;
        }
    }
    
    nProps = 0;
    while ((recOid = CeReadRecordProps(hDb, CEDB_ALLOWREALLOC, &nProps, NULL, (LPBYTE*)&pProps, &cbBuf)) != 0) {
        p = sql;
        STR_COPY(p, "INSERT INTO \"");
        for (i = 0; tblName[i]; i++) *p++ = (char)tblName[i];
        STR_COPY(p, "\" (ceoid, data) VALUES (");
        
        { 
            char num[16]; char *np = num + 14; DWORD n = recOid;
            num[15] = 0; *np = 0;
            if (n == 0) *--np = '0';
            else while (n > 0) { *--np = (char)('0' + (n % 10)); n /= 10; }
            STR_COPY(p, np);
        }
        
        STR_COPY(p, ", '");
        
        for (i = 0; i < nProps && (p - sql) < 3800; i++) {
            WORD type = LOWORD(pProps[i].propid);
            if (i > 0) *p++ = '|';
            
            if (type == CEVT_LPWSTR && pProps[i].val.lpwstr) {
                wchar_t *ws = pProps[i].val.lpwstr;
                while (*ws && (p - sql) < 3800) {
                    if (*ws == '\'') *p++ = '\'';
                    *p++ = (char)*ws++;
                }
            } else if (type == CEVT_I4) {
                char num[16]; char *np = num + 14; long n = pProps[i].val.lVal;
                int neg = 0;
                num[15] = 0; *np = 0;
                if (n < 0) { neg = 1; n = -n; }
                if (n == 0) *--np = '0';
                else while (n > 0) { *--np = '0' + (n % 10); n /= 10; }
                if (neg) *--np = '-';
                STR_COPY(p, np);
            } else if (type == CEVT_I2) {
                char num[16]; char *np = num + 14; int n = pProps[i].val.iVal;
                int neg = 0;
                num[15] = 0; *np = 0;
                if (n < 0) { neg = 1; n = -n; }
                if (n == 0) *--np = '0';
                else while (n > 0) { *--np = '0' + (n % 10); n /= 10; }
                if (neg) *--np = '-';
                STR_COPY(p, np);
            }
        }
        
        STR_COPY(p, "')");
        *p = 0;
        
        sqlite_exec(g_db, sql, NULL, NULL, NULL);
        rowCount++;
        
        if (pProps) { LocalFree(pProps); pProps = NULL; }
        cbBuf = 0;
        nProps = 0;
    }
    
    CloseHandle(hDb);
    UpdateDbSize();
    
    ClearOutput();
    {
        char msg[256];
        char *mp = msg;
        STR_COPY(mp, "Imported ");
        { int n = rowCount; char nb[16]; char *np = nb + 14; nb[15] = 0; *np = 0;
          if (n == 0) *--np = '0'; else while (n > 0) { *--np = '0' + (n % 10); n /= 10; }
          STR_COPY(mp, np); }
        STR_COPY(mp, " records from '");
        for (i = 0; dbNames[sel][i]; i++) *mp++ = (char)dbNames[sel][i];
        STR_COPY(mp, "' to table '");
        for (i = 0; tblName[i]; i++) *mp++ = (char)tblName[i];
        STR_COPY(mp, "'.");
        *mp = 0;
        OutputLine(msg);
    }
    FlushOutput();
    ClearEditMode();
    g_lastResultRows = 0;
    SwitchView(VIEW_RESULT);
    SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_EXECATCURSOR, FALSE);
    
cleanup:
    for (i = 0; i < dbCount; i++) {
        if (dbNames[i]) LocalFree(dbNames[i]);
    }
}
