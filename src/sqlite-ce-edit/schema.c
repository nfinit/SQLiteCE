/*
** SQLiteCEdit - Schema viewer (TreeView)
*/

#include "globals.h"
#include "constants.h"
#include "allocators.h"
#include <commctrl.h>

/* Schema image list indices defined in constants.h */
static HIMAGELIST g_hSchemaImages = NULL;

/* Schema counts for status bar */
static int g_nTables = 0;
static int g_nViews = 0;
static int g_nTriggers = 0;

/* Note: g_showSizes, g_gridAutoSize, g_useStorageCard, g_useStorageCardData,
** g_szDataRelPath, g_szLocalBasePath, g_szCardBasePath are now defined in globals.c */

/*============================================================================
** Get database file size
**============================================================================*/

DWORD GetDatabaseSize(void) {
    HANDLE hFile;
    DWORD dwSize = 0;
    
    if (!g_db || !g_szDbPath[0]) return 0;  /* :memory: returns 0 */
    
    hFile = CreateFileW(g_szDbPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        dwSize = GetFileSize(hFile, NULL);
        CloseHandle(hFile);
    }
    return dwSize;
}

/*============================================================================
** Get schema status string
**============================================================================*/

void GetSchemaStatus(wchar_t *buf, int bufLen) {
    DWORD dwSize;
    
    if (!g_db) {
        lstrcpyW(buf, L"No database");
        return;
    }
    
    dwSize = GetDatabaseSize();
    if (dwSize > 0) {
        if (dwSize >= 1048576)
            wsprintfW(buf, L"%d table%s, %d view%s, %d trigger%s (%d.%d MB)",
                g_nTables, g_nTables == 1 ? L"" : L"s",
                g_nViews, g_nViews == 1 ? L"" : L"s",
                g_nTriggers, g_nTriggers == 1 ? L"" : L"s",
                dwSize / 1048576, (dwSize % 1048576) / 104858);
        else
            wsprintfW(buf, L"%d table%s, %d view%s, %d trigger%s (%d KB)",
                g_nTables, g_nTables == 1 ? L"" : L"s",
                g_nViews, g_nViews == 1 ? L"" : L"s",
                g_nTriggers, g_nTriggers == 1 ? L"" : L"s",
                (dwSize + 1023) / 1024);
    } else {
        wsprintfW(buf, L"%d table%s, %d view%s, %d trigger%s",
            g_nTables, g_nTables == 1 ? L"" : L"s",
            g_nViews, g_nViews == 1 ? L"" : L"s",
            g_nTriggers, g_nTriggers == 1 ? L"" : L"s");
    }
}

/*============================================================================
** Create the schema TreeView control
**============================================================================*/

static WNDPROC g_pfnSchemaProc;

static LRESULT CALLBACK SchemaSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    /* Global shortcuts first */
    if (HandleGlobalKeys(msg, wParam))
        return 0;
    /* F5 - Refresh schema tree */
    if (msg == WM_KEYDOWN && wParam == VK_F5) {
        RefreshSchema();
        return 0;
    }
    /* Suppress beep on Enter - only eat WM_CHAR, let WM_KEYDOWN through for TVN_KEYDOWN */
    if (msg == WM_CHAR && wParam == '\r')
        return 0;
    return CallWindowProc(g_pfnSchemaProc, hwnd, msg, wParam, lParam);
}

void CreateSchemaView(HWND hwndParent, int x, int y, int cx, int cy) {
    HBITMAP hBmp;
    
    g_hwndSchema = CreateWindowExW(0, WC_TREEVIEWW, NULL,
        WS_CHILD | WS_BORDER | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT,
        x, y, cx, cy, hwndParent, (HMENU)1005, g_hInst, NULL);
    
    /* Subclass to suppress Enter beep */
    g_pfnSchemaProc = (WNDPROC)SetWindowLong(g_hwndSchema, GWL_WNDPROC, (LONG)SchemaSubclassProc);
    
    /* Create image list from schema.bmp */
    hBmp = LoadBitmapW(g_hInst, MAKEINTRESOURCEW(IDB_SCHEMA));
    if (hBmp) {
        g_hSchemaImages = ImageList_Create(16, 16, ILC_COLOR | ILC_MASK, 7, 0);
        if (g_hSchemaImages) {
            ImageList_AddMasked(g_hSchemaImages, hBmp, RGB(255, 0, 255));
            TreeView_SetImageList(g_hwndSchema, g_hSchemaImages, TVSIL_NORMAL);
        }
        DeleteObject(hBmp);
    }
}

/*============================================================================
** Populate the schema tree
**============================================================================*/

static HTREEITEM AddTreeItem(HTREEITEM hParent, const wchar_t *text, int image) {
    TV_INSERTSTRUCTW tvis;
    memset(&tvis, 0, sizeof(tvis));
    tvis.hParent = hParent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tvis.item.pszText = (LPWSTR)text;
    tvis.item.iImage = image;
    tvis.item.iSelectedImage = image;
    return TreeView_InsertItem(g_hwndSchema, &tvis);
}

static int GetRowCount(const char *tblname) {
    char sql[192];
    char **results = NULL;
    int nRows = 0, nCols = 0, count = 0;
    char *p = sql;
    const char *s = "SELECT COUNT(*) FROM ";
    
    while (*s) *p++ = *s++;
    s = tblname;
    while (*s) *p++ = *s++;
    *p = 0;
    
    sqlite_get_table(g_db, sql, &results, &nRows, &nCols, NULL);
    if (results && nRows > 0 && results[1])
        count = atoi(results[1]);
    if (results) sqlite_free_table(results);
    return count;
}

/* Estimate table size by summing lengths of all values */
static int GetTableSize(const char *tblname) {
    char sql[192];
    char **results = NULL;
    int nRows = 0, nCols = 0, size = 0, i, j;
    char *p = sql;
    const char *s = "SELECT * FROM ";
    
    while (*s) *p++ = *s++;
    s = tblname;
    while (*s) *p++ = *s++;
    *p = 0;
    
    sqlite_get_table(g_db, sql, &results, &nRows, &nCols, NULL);
    if (results) {
        for (i = 1; i <= nRows; i++) {
            for (j = 0; j < nCols; j++) {
                if (results[i * nCols + j])
                    size += strlen(results[i * nCols + j]);
            }
        }
        sqlite_free_table(results);
    }
    return size;
}

void RefreshSchema(void) {
    HTREEITEM hRoot, hItem;
    wchar_t dbname[64];
    wchar_t wname[128];
    char **results = NULL;
    int nRows = 0, nCols = 0, i;
    char *errmsg = NULL;
    
    if (!g_hwndSchema) return;
    
    /* Clear existing items and counts */
    TreeView_DeleteAllItems(g_hwndSchema);
    g_nTables = 0;
    g_nViews = 0;
    g_nTriggers = 0;
    
    if (!g_db) return;
    
    /* Get database name for root */
    if (g_szDbPath[0]) {
        wchar_t *p = g_szDbPath + lstrlenW(g_szDbPath);
        wchar_t *d = dbname;
        int j = 0;
        while (p > g_szDbPath && p[-1] != '\\') p--;
        while (*p && *p != '.' && j < 63) { *d++ = *p++; j++; }
        *d = 0;
    } else {
        lstrcpyW(dbname, L":memory:");
    }
    
    /* Add root node */
    hRoot = AddTreeItem(TVI_ROOT, dbname, IMG_DATABASE);
    
    /* Get all tables */
    sqlite_get_table(g_db, 
        "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name",
        &results, &nRows, &nCols, &errmsg);
    if (errmsg) { sqlite_freemem(errmsg); errmsg = NULL; }
    g_nTables = nRows;
    
    for (i = 1; i <= nRows && results; i++) {
        if (results[i]) {
            if (g_showSizes) {
                int cnt = GetRowCount(results[i]);
                int sz = GetTableSize(results[i]);
                wchar_t *wp = wname;
                const char *cp = results[i];
                const wchar_t *rows = (cnt == 1) ? L" row, " : L" rows, ";
                while (*cp) *wp++ = (wchar_t)(unsigned char)*cp++;
                if (sz >= 1048576)
                    wsprintfW(wp, L" (%d%s%d.%d MB)", cnt, rows, sz / 1048576, (sz % 1048576) / 104858);
                else if (sz >= 1024)
                    wsprintfW(wp, L" (%d%s%d KB)", cnt, rows, (sz + 512) / 1024);
                else
                    wsprintfW(wp, L" (%d%s%d B)", cnt, rows, sz);
            } else {
                MultiByteToWideChar(CP_ACP, 0, results[i], -1, wname, 128);
            }
            hItem = AddTreeItem(hRoot, wname, IMG_TABLE);
            AddTreeItem(hItem, L"", IMG_COLUMN);  /* Placeholder */
        }
    }
    if (results) { sqlite_free_table(results); results = NULL; }
    
    /* Get all views */
    sqlite_get_table(g_db, 
        "SELECT name FROM sqlite_master WHERE type='view' ORDER BY name",
        &results, &nRows, &nCols, &errmsg);
    if (errmsg) { sqlite_freemem(errmsg); errmsg = NULL; }
    g_nViews = nRows;
    
    for (i = 1; i <= nRows && results; i++) {
        if (results[i]) {
            MultiByteToWideChar(CP_ACP, 0, results[i], -1, wname, 128);
            hItem = AddTreeItem(hRoot, wname, IMG_VIEW);
            AddTreeItem(hItem, L"", IMG_COLUMN);  /* Placeholder */
        }
    }
    if (results) { sqlite_free_table(results); results = NULL; }
    
    /* Get all triggers */
    sqlite_get_table(g_db, 
        "SELECT name FROM sqlite_master WHERE type='trigger' ORDER BY name",
        &results, &nRows, &nCols, &errmsg);
    if (errmsg) { sqlite_freemem(errmsg); errmsg = NULL; }
    g_nTriggers = nRows;
    
    for (i = 1; i <= nRows && results; i++) {
        if (results[i]) {
            MultiByteToWideChar(CP_ACP, 0, results[i], -1, wname, 128);
            AddTreeItem(hRoot, wname, IMG_TRIGGER);  /* No children */
        }
    }
    if (results) sqlite_free_table(results);
    
    /* Expand and select root */
    TreeView_Expand(g_hwndSchema, hRoot, TVE_EXPAND);
    TreeView_SelectItem(g_hwndSchema, hRoot);
    
    /* Update status bar if in schema view */
    if (g_viewMode == 2) {
        wchar_t statusBuf[64];
        GetSchemaStatus(statusBuf, 64);
        SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)statusBuf);
    }
}

/*============================================================================
** Handle TreeView item expansion - load columns on demand
**============================================================================*/

/* Callback for column enumeration */
typedef struct {
    HTREEITEM hParent;
    int count;
} ColumnCtx;

static int ColumnCallback(void *pArg, int argc, char **argv, char **cols) {
    ColumnCtx *ctx = (ColumnCtx*)pArg;
    wchar_t wtext[256];
    const char *name = argv[1] ? argv[1] : "?";
    const char *type = argv[2] ? argv[2] : "";
    int notNull = (argv[3] && argv[3][0] != '0' && argv[3][0] != '\0');
    int isPK = (argv[5] && argv[5][0] != '0' && argv[5][0] != '\0');
    int i = 0;
    (void)argc; (void)cols;
    
    /* Build display string: "name (TYPE) PK" or "name (TYPE) NN" */
    while (*name && i < 200) wtext[i++] = (wchar_t)(unsigned char)*name++;
    if (type[0]) {
        wtext[i++] = ' '; wtext[i++] = '(';
        while (*type && i < 220) wtext[i++] = (wchar_t)(unsigned char)*type++;
        wtext[i++] = ')';
    }
    if (isPK) { 
        wtext[i++] = ' '; wtext[i++] = 'P'; wtext[i++] = 'K'; 
    } else if (notNull) { 
        wtext[i++] = ' '; wtext[i++] = 'N'; wtext[i++] = 'N'; 
    }
    wtext[i] = 0;
    
    AddTreeItem(ctx->hParent, wtext, isPK ? IMG_KEY : IMG_COLUMN);
    ctx->count++;
    return 0;
}

void OnSchemaExpanding(NMTREEVIEWW *pnm) {
    TV_ITEMW item;
    wchar_t text[128];
    wchar_t wname[128];
    char sql[256];
    char tblname[128];
    char **results = NULL;
    int nRows = 0, nCols = 0, i;
    ColumnCtx ctx;
    char *errmsg = NULL;
    HTREEITEM hPlaceholder;
    
    if (pnm->action != TVE_EXPAND) return;
    
    /* Get item text */
    item.mask = TVIF_TEXT | TVIF_IMAGE;
    item.hItem = pnm->itemNew.hItem;
    item.pszText = text;
    item.cchTextMax = 128;
    TreeView_GetItem(g_hwndSchema, &item);
    
    /* Only expand tables or views */
    if (item.iImage != IMG_TABLE && item.iImage != IMG_VIEW) return;
    
    /* Strip size info if present */
    {
        wchar_t *p = text;
        while (*p && *p != ' ' && *p != '(') p++;
        *p = 0;
    }
    
    /* Check if placeholder child exists (means not yet populated) */
    hPlaceholder = TreeView_GetChild(g_hwndSchema, pnm->itemNew.hItem);
    if (hPlaceholder) {
        TV_ITEMW chk;
        wchar_t chkText[8];
        chk.mask = TVIF_TEXT;
        chk.hItem = hPlaceholder;
        chk.pszText = chkText;
        chk.cchTextMax = 8;
        TreeView_GetItem(g_hwndSchema, &chk);
        if (chkText[0] != 0) return;  /* Real child, already populated */
        /* Delete placeholder */
        TreeView_DeleteItem(g_hwndSchema, hPlaceholder);
    }
    
    /* Get table name */
    WideCharToMultiByte(CP_ACP, 0, text, -1, tblname, 128, NULL, NULL);
    
    /* Query columns */
    {
        char *p = sql;
        const char *s = "PRAGMA table_info('";
        while (*s) *p++ = *s++;
        s = tblname;
        while (*s) *p++ = *s++;
        *p++ = '\''; *p++ = ')'; *p = 0;
    }
    ctx.hParent = pnm->itemNew.hItem;
    ctx.count = 0;
    sqlite_exec(g_db, sql, ColumnCallback, &ctx, &errmsg);
    if (errmsg) { sqlite_freemem(errmsg); errmsg = NULL; }
    
    /* Query indexes for this table */
    {
        char *p = sql;
        const char *s = "SELECT name FROM sqlite_master WHERE type='index' AND tbl_name='";
        while (*s) *p++ = *s++;
        s = tblname;
        while (*s) *p++ = *s++;
        *p++ = '\''; *p = 0;
    }
    sqlite_get_table(g_db, sql, &results, &nRows, &nCols, &errmsg);
    if (errmsg) { sqlite_freemem(errmsg); errmsg = NULL; }
    
    for (i = 1; i <= nRows && results; i++) {
        if (results[i]) {
            /* sqlite_autoindex_* = primary key, others = user index */
            int isPK = (strncmp(results[i], "sqlite_autoindex_", 17) == 0);
            MultiByteToWideChar(CP_ACP, 0, results[i], -1, wname, 128);
            AddTreeItem(pnm->itemNew.hItem, wname, isPK ? IMG_KEY : IMG_INDEX);
        }
    }
    if (results) sqlite_free_table(results);
}

/*============================================================================
** Clear edit mode state
**============================================================================*/

void ClearEditMode(void) {
    int i;
    
    if (!g_editMode) return;
    
    g_editMode = 0;
    g_editTableName[0] = '\0';
    
    /* Clear undo stack */
    ClearUndoStack();
    
    /* Free insert mode state */
    if (g_pendingValues) {
        for (i = 0; i < g_colMetaCount; i++) {
            if (g_pendingValues[i]) LocalFree(g_pendingValues[i]);
        }
        LocalFree(g_pendingValues);
        g_pendingValues = NULL;
    }
    g_insertMode = 0;
    
    /* Free column metadata */
    FreeColumnMetadata();
    
    /* Restore previous grid/text view state */
    g_gridView = g_gridViewBeforeEdit;
    ShowWindow(g_hwndResult, g_gridView ? SW_HIDE : SW_SHOW);
    if (g_hwndGrid) ShowWindow(g_hwndGrid, g_gridView ? SW_SHOW : SW_HIDE);
    
    /* Re-enable the grid toggle button */
    SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_EXECATCURSOR, TRUE);
    SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_EXECATCURSOR, g_gridView);
}

/*============================================================================
** Column metadata management
**============================================================================*/

void FreeColumnMetadata(void) {
    if (g_colMeta) {
        LocalFree(g_colMeta);
        g_colMeta = NULL;
    }
    g_colMetaCount = 0;
}

/* Callback context for LoadColumnMetadata */
typedef struct {
    int capacity;
    int count;
} ColMetaCtx;

static int ColMetaCallback(void *pArg, int argc, char **argv, char **cols) {
    ColMetaCtx *ctx = (ColMetaCtx *)pArg;
    ColumnMeta *col;
    const char *s;
    char *d;
    int i;
    (void)argc; (void)cols;
    
    if (ctx->count >= ctx->capacity) return 0;
    
    col = &g_colMeta[ctx->count];
    
    /* name (argv[1]) */
    col->name[0] = '\0';
    if (argv[1]) {
        s = argv[1]; d = col->name; i = 0;
        while (*s && i < 63) { *d++ = *s++; i++; }
        *d = '\0';
    }
    
    /* type (argv[2]) */
    col->type[0] = '\0';
    if (argv[2]) {
        s = argv[2]; d = col->type; i = 0;
        while (*s && i < 31) { *d++ = *s++; i++; }
        *d = '\0';
    }
    
    /* notnull (argv[3]) - non-zero means NOT NULL */
    col->notNull = (argv[3] && argv[3][0] != '0' && argv[3][0] != '\0') ? 1 : 0;
    
    /* dflt_value (argv[4]) - hasDefault if non-NULL */
    col->hasDefault = (argv[4] != NULL) ? 1 : 0;
    
    /* pk (argv[5]) - non-zero means PRIMARY KEY */
    col->isPK = (argv[5] && argv[5][0] != '0' && argv[5][0] != '\0') ? 1 : 0;
    
    /* isAutoInc: INTEGER PRIMARY KEY is alias for rowid in SQLite 2.x */
    col->isAutoInc = 0;
    if (col->isPK && col->type[0]) {
        /* Case-insensitive check for "INTEGER" */
        const char *t = col->type;
        if ((t[0]=='I'||t[0]=='i') && (t[1]=='N'||t[1]=='n') &&
            (t[2]=='T'||t[2]=='t') && (t[3]=='E'||t[3]=='e') &&
            (t[4]=='G'||t[4]=='g') && (t[5]=='E'||t[5]=='e') &&
            (t[6]=='R'||t[6]=='r') && t[7]=='\0') {
            col->isAutoInc = 1;
        }
    }
    
    ctx->count++;
    return 0;
}

int LoadColumnMetadata(const char *tablename) {
    char sql[256];
    char *p;
    const char *s;
    char *errmsg = NULL;
    ColMetaCtx ctx;
    
    /* Free any existing metadata */
    FreeColumnMetadata();
    
    if (!g_db || !tablename) return 0;
    
    /* First pass: count columns */
    {
        char **results = NULL;
        int nRows = 0, nCols = 0;
        
        p = sql;
        s = "PRAGMA table_info('";
        while (*s) *p++ = *s++;
        s = tablename;
        while (*s) *p++ = *s++;
        *p++ = '\''; *p++ = ')'; *p = '\0';
        
        sqlite_get_table(g_db, sql, &results, &nRows, &nCols, &errmsg);
        if (errmsg) { sqlite_freemem(errmsg); errmsg = NULL; }
        if (results) sqlite_free_table(results);
        
        if (nRows < 1) return 0;
        
        /* Allocate array */
        g_colMeta = ALLOC_ZERO(ColumnMeta, nRows);
        if (!g_colMeta) return 0;
        
        ctx.capacity = nRows;
        ctx.count = 0;
    }
    
    /* Second pass: populate metadata */
    sqlite_exec(g_db, sql, ColMetaCallback, &ctx, &errmsg);
    if (errmsg) sqlite_freemem(errmsg);
    
    g_colMetaCount = ctx.count;
    return g_colMetaCount;
}

/*============================================================================
** Open a table for editing in grid view
**============================================================================*/

void OpenTableForEditing(const char *tablename) {
    char sql[256];
    char *p = sql;
    const char *s;
    int rc;
    char *errmsg = NULL;
    wchar_t wbuf[64];
    DWORD startTick, elapsed;
    char **results = NULL;
    int nRows = 0, nCols = 0;
    int i, total;
    int sameTable = 0;
    
    if (!g_db || !tablename) return;
    
    /* Check if re-opening same table (preserve undo stack) */
    if (g_editMode && g_editTableName[0]) {
        const char *a = tablename;
        const char *b = g_editTableName;
        sameTable = 1;
        while (*a && *b) {
            if (*a++ != *b++) { sameTable = 0; break; }
        }
        if (*a || *b) sameTable = 0;
    }
    
    /* Clear previous edit state (but preserve undo if same table) */
    if (!sameTable) {
        ClearEditMode();
    } else {
        /* Just clear insert mode, keep undo stack */
        if (g_pendingValues) {
            for (i = 0; i < g_colMetaCount; i++) {
                if (g_pendingValues[i]) LocalFree(g_pendingValues[i]);
            }
            LocalFree(g_pendingValues);
            g_pendingValues = NULL;
        }
        g_insertMode = 0;
        FreeColumnMetadata();
    }
    
    /* Load column metadata first - needed for empty tables */
    LoadColumnMetadata(tablename);
    if (g_colMetaCount < 1) {
        SetStatusResult(L"Error: no columns found");
        return;
    }
    
    /* Build SELECT rowid, * FROM tablename */
    s = "SELECT rowid, * FROM ";
    while (*s) *p++ = *s++;
    s = tablename;
    while (*s) *p++ = *s++;
    *p++ = ';'; *p = 0;
    
    /* Setup for query */
    g_abortQuery = 0;
    if (g_clearOnExec) ClearOutput();
    
    startTick = GetTickCount();
    
    rc = sqlite_get_table(g_db, sql, &results, &nRows, &nCols, &errmsg);
    
    elapsed = GetTickCount() - startTick;
    g_lastQueryTime = elapsed;
    
    if (rc != SQLITE_OK) {
        if (errmsg) {
            OutputLine(errmsg);
            sqlite_freemem(errmsg);
        }
        FreeColumnMetadata();
        SetStatusResult(L"Error opening table");
        return;
    }
    
    /* Free previous results */
    FreeLastResults();
    
    /* Handle empty table: nCols=0 from sqlite_get_table, use metadata */
    if (nCols == 0) {
        nCols = g_colMetaCount + 1;  /* +1 for rowid */
    }
    
    /* Store results - includes rowid as column 0 */
    g_lastResultRows = nRows;
    g_lastResultCols = nCols;
    total = (nRows + 1) * nCols;
    
    g_lastResult = ALLOC_ZERO(char *, total);
    if (g_lastResult) {
        /* Build header row from column metadata if no results */
        if (!results || nRows == 0) {
            /* First column is rowid */
            g_lastResult[0] = ALLOC(char, 6);
            if (g_lastResult[0]) {
                g_lastResult[0][0] = 'r'; g_lastResult[0][1] = 'o';
                g_lastResult[0][2] = 'w'; g_lastResult[0][3] = 'i';
                g_lastResult[0][4] = 'd'; g_lastResult[0][5] = '\0';
            }
            /* Remaining columns from metadata */
            for (i = 0; i < g_colMetaCount; i++) {
                int len = 0;
                const char *src = g_colMeta[i].name;
                while (src[len]) len++;
                g_lastResult[i + 1] = ALLOC(char, len + 1);
                if (g_lastResult[i + 1]) {
                    int j;
                    for (j = 0; j <= len; j++) g_lastResult[i + 1][j] = src[j];
                }
            }
        } else {
            /* Copy from results */
            for (i = 0; i < total; i++) {
                if (results[i]) {
                    int len = 0;
                    const char *src = results[i];
                    while (src[len]) len++;
                    g_lastResult[i] = ALLOC(char, len + 1);
                    if (g_lastResult[i]) {
                        int j;
                        for (j = 0; j <= len; j++) g_lastResult[i][j] = results[i][j];
                    }
                }
            }
        }
    }
    if (results) sqlite_free_table(results);
    
    /* Save current grid view state before entering edit mode */
    g_gridViewBeforeEdit = g_gridView;
    
    /* Set edit mode */
    g_editMode = 1;
    s = tablename;
    p = g_editTableName;
    while (*s && (p - g_editTableName) < 127) *p++ = *s++;
    *p = '\0';
    
    /* Update status */
    {
        wchar_t wtbl[128];
        MultiByteToWideChar(CP_ACP, 0, g_editTableName, -1, wtbl, 128);
        wsprintfW(wbuf, L"Editing: %s (%d rows)", wtbl, nRows);
    }
    SetStatusResult(wbuf);
    
    /* Populate grid and switch to grid view */
    g_gridView = 1;
    PopulateGrid();
    SwitchView(VIEW_RESULT);
    
    /* Disable grid/text toggle button while in edit mode */
    SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_EXECATCURSOR, FALSE);
    SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_EXECATCURSOR, TRUE);
}

/*============================================================================
** Handle double-click - open table for editing or view read-only
**============================================================================*/

void OnSchemaDoubleClick(void) {
    HTREEITEM hItem;
    TV_ITEMW item;
    wchar_t text[128];
    char name[128];
    
    hItem = TreeView_GetSelection(g_hwndSchema);
    if (!hItem) return;
    
    item.mask = TVIF_TEXT | TVIF_IMAGE;
    item.hItem = hItem;
    item.pszText = text;
    item.cchTextMax = 128;
    TreeView_GetItem(g_hwndSchema, &item);
    
    /* Only for tables and views */
    if (item.iImage != IMG_TABLE && item.iImage != IMG_VIEW) return;
    
    /* Strip size info if present (e.g., "tablename (10, 2 KB)" -> "tablename") */
    {
        wchar_t *p = text;
        while (*p && *p != ' ' && *p != '(') p++;
        *p = 0;
    }
    
    WideCharToMultiByte(CP_ACP, 0, text, -1, name, 128, NULL, NULL);
    
    if (item.iImage == IMG_TABLE) {
        /* Tables open in editable grid mode */
        OpenTableForEditing(name);
    } else {
        /* Views open read-only via ExecuteSQL */
        char sql[256];
        char *p = sql;
        const char *s = "SELECT * FROM ";
        while (*s) *p++ = *s++;
        s = name;
        while (*s) *p++ = *s++;
        *p++ = ';'; *p = 0;
        ClearEditMode();
        ExecuteSQL(sql);
    }
}

/*============================================================================
** Handle Delete key - drop selected object
**============================================================================*/

void OnSchemaDelete(void) {
    HTREEITEM hItem;
    TV_ITEMW item;
    wchar_t text[128];
    wchar_t msg[192];
    char sql[256];
    char name[128];
    const char *type;
    char *p;
    const char *s;
    
    hItem = TreeView_GetSelection(g_hwndSchema);
    if (!hItem) return;
    
    item.mask = TVIF_TEXT | TVIF_IMAGE;
    item.hItem = hItem;
    item.pszText = text;
    item.cchTextMax = 128;
    TreeView_GetItem(g_hwndSchema, &item);
    
    /* Only tables, views, triggers can be dropped */
    if (item.iImage == IMG_TABLE) type = "DROP TABLE ";
    else if (item.iImage == IMG_VIEW) type = "DROP VIEW ";
    else if (item.iImage == IMG_TRIGGER) type = "DROP TRIGGER ";
    else return;
    
    /* Strip size info if present */
    {
        wchar_t *wp = text;
        while (*wp && *wp != ' ' && *wp != '(') wp++;
        *wp = 0;
    }
    
    /* Confirm */
    wsprintfW(msg, L"Drop '%s'?", text);
    if (MessageBoxW(g_hwndMain, msg, L"Confirm", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;
    
    /* Build and execute DROP */
    WideCharToMultiByte(CP_ACP, 0, text, -1, name, 128, NULL, NULL);
    p = sql;
    s = type;
    while (*s) *p++ = *s++;
    s = name;
    while (*s) *p++ = *s++;
    *p++ = ';'; *p = 0;
    
    ExecuteSQL(sql);
    RefreshSchema();
}

/*============================================================================
** Get selected object type (-1 if none or not a droppable object)
**============================================================================*/

int GetSelectedObjectType(void) {
    HTREEITEM hItem;
    TV_ITEMW item;
    wchar_t text[128];
    
    if (!g_hwndSchema) return -1;
    
    hItem = TreeView_GetSelection(g_hwndSchema);
    if (!hItem) return -1;
    
    item.mask = TVIF_IMAGE;
    item.hItem = hItem;
    item.pszText = text;
    item.cchTextMax = 128;
    TreeView_GetItem(g_hwndSchema, &item);
    
    /* Return type for tables, views, triggers only */
    if (item.iImage == IMG_TABLE || item.iImage == IMG_VIEW || item.iImage == IMG_TRIGGER)
        return item.iImage;
    
    return -1;
}

/*============================================================================
** Export DDL for selected object
**============================================================================*/

void ExportSelectedDDL(void) {
    HTREEITEM hItem;
    TV_ITEMW item;
    wchar_t text[128];
    wchar_t szFile[MAX_PATH];
    char name[128];
    char sql[256];
    char **results = NULL;
    int nRows = 0, nCols = 0;
    HANDLE hFile;
    DWORD dwWritten;
    
    hItem = TreeView_GetSelection(g_hwndSchema);
    if (!hItem) return;
    
    item.mask = TVIF_TEXT | TVIF_IMAGE;
    item.hItem = hItem;
    item.pszText = text;
    item.cchTextMax = 128;
    TreeView_GetItem(g_hwndSchema, &item);
    
    /* Only tables, views, triggers have DDL */
    if (item.iImage != IMG_TABLE && item.iImage != IMG_VIEW && item.iImage != IMG_TRIGGER)
        return;
    
    /* Get object name (strip size info if present) */
    {
        wchar_t *p = text;
        while (*p && *p != ' ' && *p != '(') p++;
        *p = 0;
    }
    WideCharToMultiByte(CP_ACP, 0, text, -1, name, 128, NULL, NULL);
    
    /* Query DDL from sqlite_master */
    {
        char *p = sql;
        const char *s = "SELECT sql FROM sqlite_master WHERE name='";
        while (*s) *p++ = *s++;
        s = name;
        while (*s) *p++ = *s++;
        *p++ = '\''; *p = 0;
    }
    
    sqlite_get_table(g_db, sql, &results, &nRows, &nCols, NULL);
    if (!results || nRows < 1 || !results[1]) {
        if (results) sqlite_free_table(results);
        return;
    }
    
    /* Default filename */
    wsprintfW(szFile, L"%s.sql", text);
    
    if (CustomFilePicker(g_hwndMain, szFile, MAX_PATH,
            L"Export DDL", L"SQL Files (*.sql)\0*.sql\0",
            L"sql", NULL, 1)) {
        hFile = CreateFileW(szFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            WriteFile(hFile, results[1], strlen(results[1]), &dwWritten, NULL);
            WriteFile(hFile, ";\r\n", 3, &dwWritten, NULL);
            CloseHandle(hFile);
        }
    }
    
    sqlite_free_table(results);
}

/*============================================================================
** Export DDL for entire database
**============================================================================*/

void ExportAllDDL(void) {
    wchar_t szFile[MAX_PATH];
    wchar_t dbname[64];
    char header[256];
    char **results = NULL;
    int nRows = 0, nCols = 0, i;
    HANDLE hFile;
    DWORD dwWritten;
    SYSTEMTIME st;
    
    if (!g_db) return;
    
    /* Get database name for default filename */
    if (g_szDbPath[0] && g_szDbPath[0] != ':') {
        wchar_t *p = g_szDbPath + lstrlenW(g_szDbPath);
        wchar_t *d = dbname;
        while (p > g_szDbPath && p[-1] != '\\') p--;
        while (*p && *p != '.') *d++ = *p++;
        *d = 0;
    } else {
        lstrcpyW(dbname, L"memory");
    }
    wsprintfW(szFile, L"%s.sql", dbname);
    
    if (!CustomFilePicker(g_hwndMain, szFile, MAX_PATH,
            L"Export All DDL", L"SQL Files (*.sql)\0*.sql\0",
            L"sql", NULL, 1)) return;
    
    /* Get all DDL ordered: tables first, then views, then triggers, then indexes */
    sqlite_get_table(g_db,
        "SELECT sql FROM sqlite_master WHERE sql IS NOT NULL "
        "ORDER BY CASE type WHEN 'table' THEN 1 WHEN 'view' THEN 2 WHEN 'trigger' THEN 3 WHEN 'index' THEN 4 END, name",
        &results, &nRows, &nCols, NULL);
    
    if (!results || nRows < 1) {
        if (results) sqlite_free_table(results);
        return;
    }
    
    hFile = CreateFileW(szFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        /* Write header with database name and timestamp */
        GetLocalTime(&st);
        {
            char dbname8[64];
            wchar_t datebuf[32];
            char *p = header;
            const char *s;
            int di;
            WideCharToMultiByte(CP_ACP, 0, dbname, -1, dbname8, 64, NULL, NULL);
            for (s = "-- Schema export: "; *s; ) *p++ = *s++;
            for (s = dbname8; *s; ) *p++ = *s++;
            *p++ = '\r'; *p++ = '\n';
            for (s = "-- Exported: "; *s; ) *p++ = *s++;
            wsprintfW(datebuf, L"%04d-%02d-%02d %02d:%02d:%02d",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
            for (di = 0; datebuf[di]; di++) *p++ = (char)datebuf[di];
            *p++ = '\r'; *p++ = '\n'; *p++ = '\r'; *p++ = '\n'; *p = 0;
        }
        WriteFile(hFile, header, strlen(header), &dwWritten, NULL);
        
        for (i = 1; i <= nRows; i++) {
            if (results[i]) {
                WriteFile(hFile, results[i], strlen(results[i]), &dwWritten, NULL);
                WriteFile(hFile, ";\r\n\r\n", 5, &dwWritten, NULL);
            }
        }
        CloseHandle(hFile);
    }
    
    sqlite_free_table(results);
}
