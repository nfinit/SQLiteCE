/*
** SQLiteCEdit - Schema viewer (TreeView)
*/

#include "globals.h"
#include <commctrl.h>

/* Schema image list indices */
#define IMG_DATABASE 0
#define IMG_TABLE    1
#define IMG_COLUMN   2
#define IMG_KEY      3
#define IMG_VIEW     4
#define IMG_TRIGGER  5

static HIMAGELIST g_hSchemaImages = NULL;

/* Schema counts for status bar */
static int g_nTables = 0;
static int g_nViews = 0;
static int g_nTriggers = 0;

/*============================================================================
** Get schema status string
**============================================================================*/

void GetSchemaStatus(wchar_t *buf, int bufLen) {
    if (!g_db) {
        lstrcpyW(buf, L"No database");
        return;
    }
    wsprintfW(buf, L"%d table%s, %d view%s, %d trigger%s",
        g_nTables, g_nTables == 1 ? L"" : L"s",
        g_nViews, g_nViews == 1 ? L"" : L"s",
        g_nTriggers, g_nTriggers == 1 ? L"" : L"s");
}

/*============================================================================
** Create the schema TreeView control
**============================================================================*/

void CreateSchemaView(HWND hwndParent, int x, int y, int cx, int cy) {
    HBITMAP hBmp;
    
    g_hwndSchema = CreateWindowExW(0, WC_TREEVIEWW, NULL,
        WS_CHILD | WS_BORDER | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT,
        x, y, cx, cy, hwndParent, (HMENU)1005, g_hInst, NULL);
    
    /* Create image list from schema.bmp */
    hBmp = LoadBitmapW(g_hInst, MAKEINTRESOURCEW(IDB_SCHEMA));
    if (hBmp) {
        g_hSchemaImages = ImageList_Create(16, 16, ILC_COLOR | ILC_MASK, 5, 0);
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
            MultiByteToWideChar(CP_ACP, 0, results[i], -1, wname, 128);
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
    int isPK = (argv[5] && argv[5][0] == '1');
    int i = 0;
    (void)argc; (void)cols;
    
    /* Build display string: "name (TYPE) PK" */
    while (*name && i < 200) wtext[i++] = (wchar_t)(unsigned char)*name++;
    if (type[0]) {
        wtext[i++] = ' '; wtext[i++] = '(';
        while (*type && i < 230) wtext[i++] = (wchar_t)(unsigned char)*type++;
        wtext[i++] = ')';
    }
    if (isPK) { wtext[i++] = ' '; wtext[i++] = 'P'; wtext[i++] = 'K'; }
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
            MultiByteToWideChar(CP_ACP, 0, results[i], -1, wname, 128);
            AddTreeItem(pnm->itemNew.hItem, wname, IMG_KEY);
        }
    }
    if (results) sqlite_free_table(results);
}

/*============================================================================
** Handle double-click - generate SELECT query
**============================================================================*/

void OnSchemaDoubleClick(void) {
    HTREEITEM hItem;
    TV_ITEMW item;
    wchar_t text[128];
    char sql[256];
    
    hItem = TreeView_GetSelection(g_hwndSchema);
    if (!hItem) return;
    
    item.mask = TVIF_TEXT | TVIF_IMAGE;
    item.hItem = hItem;
    item.pszText = text;
    item.cchTextMax = 128;
    TreeView_GetItem(g_hwndSchema, &item);
    
    /* Only for tables and views */
    if (item.iImage != IMG_TABLE && item.iImage != IMG_VIEW) return;
    
    /* Build and execute SELECT without touching query buffer */
    {
        char *p = sql;
        const char *s = "SELECT * FROM ";
        char name[128];
        while (*s) *p++ = *s++;
        WideCharToMultiByte(CP_ACP, 0, text, -1, name, 128, NULL, NULL);
        s = name;
        while (*s) *p++ = *s++;
        *p++ = ';'; *p = 0;
    }
    ExecuteSQL(sql);
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
