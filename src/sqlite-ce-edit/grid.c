/*
** SQLiteCEdit - Grid view for query results (Virtual ListView)
*/

#include "globals.h"

/* Sort state */
static int *g_sortIndex = NULL;    /* Maps display row -> data row */
static int g_sortCol = -1;         /* Current sort column, -1 = unsorted */
static int g_sortAsc = 1;          /* 1 = ascending, 0 = descending */

/* Find state */
static int g_gridFindRow = 0;
static int g_gridFindCol = 0;

/* Edit overlay state */
static HWND g_hwndEditOverlay = NULL;
static WNDPROC g_pfnEditOverlayProc = NULL;
static int g_editRow = -1;         /* Display row being edited */
static int g_editCol = -1;         /* Display column being edited */

/* Forward declarations */
static void StartCellEdit(int row, int col);
static void CommitCellEdit(void);
static void CancelCellEdit(void);

void CreateGridView(HWND hwndParent, int x, int y, int cx, int cy) {
    HIMAGELIST hIml;
    
    g_hwndGrid = CreateWindowExW(0, WC_LISTVIEWW, NULL,
        WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_OWNERDATA,
        x, y, cx, cy, hwndParent, (HMENU)IDC_GRID, g_hInst, NULL);
    
    /* Enable full row select and grid lines */
    ListView_SetExtendedListViewStyle(g_hwndGrid, 
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    
    /* Create small imagelist to force row height (adds vertical padding) */
    hIml = ImageList_Create(1, 18, ILC_COLOR, 1, 0);
    ListView_SetImageList(g_hwndGrid, hIml, LVSIL_SMALL);
    
    /* Set font to match grid style */
    if (g_hFontGrid)
        SendMessage(g_hwndGrid, WM_SETFONT, (WPARAM)g_hFontGrid, TRUE);
    
    /* Subclass for keyboard shortcuts */
    g_pfnGridProc = (WNDPROC)SetWindowLong(g_hwndGrid, GWL_WNDPROC, (LONG)GridProc);
}

static void CopySelectedRow(void) {
    int sel = ListView_GetNextItem(g_hwndGrid, -1, LVNI_SELECTED);
    int dataRow, j, len;
    char *buf, *p;
    wchar_t *wbuf;
    HLOCAL hMem;
    int startCol;
    
    if (sel < 0 || !g_lastResult || sel >= g_lastResultRows) return;
    
    /* Map through sort index */
    dataRow = g_sortIndex ? g_sortIndex[sel] : sel;
    
    /* In edit mode, skip column 0 (rowid) */
    startCol = g_editMode ? 1 : 0;
    
    /* Build tab-separated string */
    buf = (char *)LocalAlloc(LMEM_FIXED, 4096);
    if (!buf) return;
    p = buf;
    
    for (j = startCol; j < g_lastResultCols; j++) {
        char *val = g_lastResult[(dataRow + 1) * g_lastResultCols + j];
        if (j > startCol) *p++ = '\t';
        if (val) while (*val) *p++ = *val++;
    }
    *p = 0;
    len = (int)(p - buf);
    
    /* Copy to clipboard as Unicode */
    hMem = LocalAlloc(LMEM_MOVEABLE, (len + 1) * sizeof(wchar_t));
    if (hMem) {
        wbuf = (wchar_t *)LocalLock(hMem);
        for (j = 0; j <= len; j++) wbuf[j] = (wchar_t)(unsigned char)buf[j];
        LocalUnlock(hMem);
        if (OpenClipboard(g_hwndMain)) {
            EmptyClipboard();
            SetClipboardData(CF_UNICODETEXT, hMem);
            CloseClipboard();
        } else {
            LocalFree(hMem);
        }
    }
    LocalFree(buf);
}

LRESULT CALLBACK GridProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_SYSKEYDOWN) {
        /* Alt+X - Exit */
        if (wParam == 'X') {
            SendMessage(g_hwndMain, WM_CLOSE, 0, 0);
            return 0;
        }
    }
    if (msg == WM_KEYDOWN) {
        int ctrl = GetKeyState(VK_CONTROL) < 0;
        
        /* Ctrl+C - Copy selected row */
        if (ctrl && wParam == 'C') {
            CopySelectedRow();
            return 0;
        }
        /* Ctrl+F - Find */
        if (ctrl && wParam == 'F') {
            DoFind();
            return 0;
        }
        /* F3 - Find next */
        if (wParam == VK_F3) {
            if (g_findText[0]) GridFindNext();
            else DoFind();
            return 0;
        }
        /* Ctrl+G - Toggle back to text view (disabled in edit mode) */
        if (ctrl && wParam == 'G') {
            if (!g_editMode)
                SendMessage(g_hwndMain, WM_COMMAND, IDM_EXECATCURSOR, 0);
            return 0;
        }
        /* F6/Escape/Backspace - back to query */
        if (wParam == VK_F6 || wParam == VK_ESCAPE || wParam == VK_BACK) {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWQUERY, 0);
            return 0;
        }
        if (wParam == VK_F7) {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWSCHEMA, 0);
            return 0;
        }
        /* Ctrl+1/2/3 - View switching */
        if (ctrl && wParam == '1') {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWQUERY, 0);
            return 0;
        }
        if (ctrl && wParam == '2') {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWRESULT, 0);
            return 0;
        }
        if (ctrl && wParam == '3') {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWSCHEMA, 0);
            return 0;
        }
        /* F5 or Ctrl+E - Execute */
        if (wParam == VK_F5 || (ctrl && wParam == 'E')) {
            ExecuteQuery();
            return 0;
        }
        /* F2 - Start cell edit (only in edit mode) */
        if (wParam == VK_F2 && g_editMode) {
            int sel = ListView_GetNextItem(g_hwndGrid, -1, LVNI_SELECTED);
            if (sel >= 0) StartCellEdit(sel, 0);
            return 0;
        }
    }
    return CallWindowProc(g_pfnGridProc, hwnd, msg, wParam, lParam);
}

/* Case-insensitive string comparison for sorting */
static int StrCmpNoCase(const char *a, const char *b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return *a - *b;
}

static int g_cmpCol;  /* Data column index for sorting */
static int CmpRows(const void *pa, const void *pb) {
    int ra = *(const int*)pa;
    int rb = *(const int*)pb;
    char *va = g_lastResult[(ra + 1) * g_lastResultCols + g_cmpCol];
    char *vb = g_lastResult[(rb + 1) * g_lastResultCols + g_cmpCol];
    int cmp = StrCmpNoCase(va, vb);
    return g_sortAsc ? cmp : -cmp;
}

static void UpdateColumnHeader(int col, int showIndicator) {
    /* No visual indicator - CE Tahoma lacks arrow glyphs */
    (void)col; (void)showIndicator;
}

void OnGridColumnClick(int col) {
    int i, oldCol = g_sortCol;
    int dataCol;
    if (!g_lastResult || g_lastResultRows < 1) return;
    
    /* Map display column to data column (skip rowid in edit mode) */
    dataCol = g_editMode ? col + 1 : col;
    
    /* Toggle direction if same column, else new sort ascending */
    if (col == g_sortCol) {
        g_sortAsc = !g_sortAsc;
    } else {
        g_sortCol = col;
        g_sortAsc = 1;
    }
    
    /* Allocate/reset index */
    if (!g_sortIndex) {
        g_sortIndex = (int*)LocalAlloc(LMEM_FIXED, g_lastResultRows * sizeof(int));
        if (!g_sortIndex) return;
    }
    for (i = 0; i < g_lastResultRows; i++) g_sortIndex[i] = i;
    
    /* Sort using data column index */
    g_cmpCol = dataCol;
    qsort(g_sortIndex, g_lastResultRows, sizeof(int), CmpRows);
    
    /* Update column headers */
    if (oldCol >= 0 && oldCol != col)
        UpdateColumnHeader(oldCol, 0);
    UpdateColumnHeader(col, 1);
    
    /* Refresh grid */
    InvalidateRect(g_hwndGrid, NULL, TRUE);
    
    /* Update status bar */
    {
        wchar_t colName[128], status[256];
        LVCOLUMNW lvc;
        lvc.mask = LVCF_TEXT;
        lvc.pszText = colName;
        lvc.cchTextMax = 120;
        ListView_GetColumn(g_hwndGrid, col, &lvc);
        wsprintfW(status, L"%d row%s (sorted by %s %s)", 
            g_lastResultRows, g_lastResultRows == 1 ? L"" : L"s",
            colName, g_sortAsc ? L"ASC" : L"DESC");
        SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)status);
    }
}

void GridFindNext(void) {
    int r, c, startRow, startCol, findLen, i, j;
    int startDataCol, numDataCols;
    char findBuf[256];
    
    if (!g_findText[0] || !g_lastResult || g_lastResultRows < 1) return;
    
    /* Convert search text to ANSI for comparison */
    for (i = 0; g_findText[i] && i < 255; i++)
        findBuf[i] = (char)g_findText[i];
    findBuf[i] = 0;
    findLen = i;
    
    /* In edit mode, skip column 0 (rowid) when searching */
    startDataCol = g_editMode ? 1 : 0;
    numDataCols = g_lastResultCols;
    
    /* Start from current position + 1 cell */
    startRow = g_gridFindRow;
    startCol = g_gridFindCol + 1;
    if (startCol >= numDataCols) { startCol = startDataCol; startRow++; }
    if (startCol < startDataCol) startCol = startDataCol;
    if (startRow >= g_lastResultRows) startRow = 0;
    
    /* Search from current position to end */
    for (r = startRow; r < g_lastResultRows; r++) {
        int dataRow = g_sortIndex ? g_sortIndex[r] : r;
        for (c = (r == startRow ? startCol : startDataCol); c < numDataCols; c++) {
            char *val = g_lastResult[(dataRow + 1) * g_lastResultCols + c];
            if (!val) continue;
            /* Case-insensitive substring search */
            for (i = 0; val[i]; i++) {
                for (j = 0; j < findLen; j++) {
                    char cv = val[i + j], cf = findBuf[j];
                    if (cv >= 'A' && cv <= 'Z') cv += 32;
                    if (cf >= 'A' && cf <= 'Z') cf += 32;
                    if (cv != cf) break;
                }
                if (j == findLen) {
                    g_gridFindRow = r; g_gridFindCol = c;
                    ListView_SetItemState(g_hwndGrid, -1, 0, LVIS_SELECTED);
                    ListView_SetItemState(g_hwndGrid, r, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                    ListView_EnsureVisible(g_hwndGrid, r, FALSE);
                    return;
                }
            }
        }
    }
    /* Wrap to beginning */
    for (r = 0; r <= startRow; r++) {
        int dataRow = g_sortIndex ? g_sortIndex[r] : r;
        int endCol = (r == startRow) ? startCol : numDataCols;
        for (c = startDataCol; c < endCol; c++) {
            char *val = g_lastResult[(dataRow + 1) * g_lastResultCols + c];
            if (!val) continue;
            for (i = 0; val[i]; i++) {
                for (j = 0; j < findLen; j++) {
                    char cv = val[i + j], cf = findBuf[j];
                    if (cv >= 'A' && cv <= 'Z') cv += 32;
                    if (cf >= 'A' && cf <= 'Z') cf += 32;
                    if (cv != cf) break;
                }
                if (j == findLen) {
                    g_gridFindRow = r; g_gridFindCol = c;
                    ListView_SetItemState(g_hwndGrid, -1, 0, LVIS_SELECTED);
                    ListView_SetItemState(g_hwndGrid, r, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                    ListView_EnsureVisible(g_hwndGrid, r, FALSE);
                    return;
                }
            }
        }
    }
    MessageBoxW(g_hwndMain, L"Text not found.", L"Find", MB_OK);
}

void OnGridGetDispInfo(NMLVDISPINFOW *pdi) {
    int row, dataRow, col, dataCol;
    char *val;
    static wchar_t wbuf[256];
    int numDisplayCols;
    
    if (!(pdi->item.mask & LVIF_TEXT)) return;
    if (!g_lastResult) return;
    
    row = pdi->item.iItem;
    col = pdi->item.iSubItem;
    
    /* In edit mode, column 0 is hidden rowid, so shift display columns */
    dataCol = g_editMode ? col + 1 : col;
    numDisplayCols = g_editMode ? g_lastResultCols - 1 : g_lastResultCols;
    
    if (row >= g_lastResultRows || col >= numDisplayCols) {
        pdi->item.pszText = L"";
        return;
    }
    
    /* Map through sort index if sorted */
    dataRow = g_sortIndex ? g_sortIndex[row] : row;
    
    val = g_lastResult[(dataRow + 1) * g_lastResultCols + dataCol];
    if (val) {
        MultiByteToWideChar(CP_ACP, 0, val, -1, wbuf, 256);
        pdi->item.pszText = wbuf;
    } else {
        pdi->item.pszText = L"(null)";
    }
}

void PopulateGrid(void) {
    int j, startCol, numDisplayCols;
    LVCOLUMNW col;
    wchar_t wbuf[256];
    RECT rc;
    int totalWidth, colWidth;
    DWORD startTick, elapsed;
    
    if (!g_hwndGrid) return;
    
    startTick = GetTickCount();
    
    /* Reset sort state */
    if (g_sortIndex) { LocalFree(g_sortIndex); g_sortIndex = NULL; }
    g_sortCol = -1;
    g_sortAsc = 1;
    g_gridFindRow = 0;
    g_gridFindCol = -1;
    
    /* Disable repainting during setup */
    SendMessage(g_hwndGrid, WM_SETREDRAW, FALSE, 0);
    
    /* Clear existing */
    ListView_SetItemCount(g_hwndGrid, 0);
    while (ListView_DeleteColumn(g_hwndGrid, 0)) ;
    
    if (!g_lastResult || g_lastResultCols < 1 || g_lastResultRows < 1) {
        SendMessage(g_hwndGrid, WM_SETREDRAW, TRUE, 0);
        return;
    }
    
    /* In edit mode, skip column 0 (rowid) */
    startCol = g_editMode ? 1 : 0;
    numDisplayCols = g_editMode ? g_lastResultCols - 1 : g_lastResultCols;
    
    if (numDisplayCols < 1) {
        SendMessage(g_hwndGrid, WM_SETREDRAW, TRUE, 0);
        return;
    }
    
    /* Calculate column width to fill grid */
    GetClientRect(g_hwndGrid, &rc);
    totalWidth = rc.right - GetSystemMetrics(SM_CXVSCROLL) - 4;
    colWidth = totalWidth / numDisplayCols;
    if (colWidth < 60) colWidth = 60;
    
    /* Add columns from header row */
    memset(&col, 0, sizeof(col));
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    col.fmt = LVCFMT_LEFT;
    col.cx = colWidth;
    
    for (j = 0; j < numDisplayCols; j++) {
        int dataCol = startCol + j;
        if (g_lastResult[dataCol]) {
            MultiByteToWideChar(CP_ACP, 0, g_lastResult[dataCol], -1, wbuf, 256);
        } else {
            wbuf[0] = 0;
        }
        col.pszText = wbuf;
        /* Last column gets remaining width */
        if (j == numDisplayCols - 1) {
            col.cx = totalWidth - (colWidth * (numDisplayCols - 1));
        }
        ListView_InsertColumn(g_hwndGrid, j, &col);
    }
    
    /* Set item count - virtual ListView fetches data on demand */
    ListView_SetItemCount(g_hwndGrid, g_lastResultRows);
    
    /* Auto-fit columns (optional - sample first 20 rows for speed) */
    if (g_gridAutoSize) {
        int sampleRows = g_lastResultRows < 20 ? g_lastResultRows : 20;
        ListView_SetItemCount(g_hwndGrid, sampleRows);
        for (j = 0; j < numDisplayCols; j++) {
            int contentWidth, headerWidth;
            ListView_SetColumnWidth(g_hwndGrid, j, LVSCW_AUTOSIZE);
            contentWidth = ListView_GetColumnWidth(g_hwndGrid, j);
            ListView_SetColumnWidth(g_hwndGrid, j, LVSCW_AUTOSIZE_USEHEADER);
            headerWidth = ListView_GetColumnWidth(g_hwndGrid, j);
            if (contentWidth > headerWidth)
                ListView_SetColumnWidth(g_hwndGrid, j, contentWidth);
        }
        ListView_SetItemCount(g_hwndGrid, g_lastResultRows);
    }
    
    /* Re-enable repainting and refresh */
    SendMessage(g_hwndGrid, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_hwndGrid, NULL, TRUE);
    
    /* Update status bar with query and draw time (unless edit mode set its own) */
    if (!g_editMode) {
        elapsed = GetTickCount() - startTick;
        wsprintfW(wbuf, L"%d row(s), query %lums, draw %lums", g_lastResultRows, g_lastQueryTime, elapsed);
        SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)wbuf);
    }
}

/*============================================================================
** Cell Edit Overlay
**============================================================================*/

static int g_commitNull = 0;  /* Flag to commit NULL instead of text value */

static LRESULT CALLBACK EditOverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    int ctrl = GetKeyState(VK_CONTROL) < 0;
    
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            CommitCellEdit();
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            CancelCellEdit();
            return 0;
        }
        /* Ctrl+Delete - set NULL */
        if (ctrl && wParam == VK_DELETE) {
            g_commitNull = 1;
            CommitCellEdit();
            return 0;
        }
    }
    /* Ctrl+0 comes through as WM_CHAR with value 0x30 or as null char */
    if (msg == WM_CHAR && ctrl && (wParam == '0' || wParam == 0)) {
        g_commitNull = 1;
        CommitCellEdit();
        return 0;
    }
    if (msg == WM_KILLFOCUS) {
        /* Commit on focus loss */
        CommitCellEdit();
        return 0;
    }
    return CallWindowProc(g_pfnEditOverlayProc, hwnd, msg, wParam, lParam);
}

static void StartCellEdit(int row, int col) {
    RECT rcItem, rcGrid;
    int dataRow, dataCol;
    char *val;
    wchar_t wval[256];
    int i, x, width;
    
    if (!g_editMode || !g_hwndGrid || row < 0) return;
    if (g_hwndEditOverlay) CancelCellEdit();  /* Close any existing edit */
    
    /* Get cell rectangle - need to calculate from column positions */
    GetClientRect(g_hwndGrid, &rcGrid);
    rcItem.top = row;
    rcItem.left = LVIR_BOUNDS;
    ListView_GetItemRect(g_hwndGrid, row, &rcItem, LVIR_BOUNDS);
    
    /* Calculate column X position and width */
    x = rcItem.left;
    for (i = 0; i < col; i++) {
        x += ListView_GetColumnWidth(g_hwndGrid, i);
    }
    width = ListView_GetColumnWidth(g_hwndGrid, col);
    
    rcItem.left = x;
    rcItem.right = x + width;
    
    /* Map display column to data column (skip rowid in column 0) */
    dataCol = col + 1;  /* +1 because column 0 is hidden rowid */
    dataRow = g_sortIndex ? g_sortIndex[row] : row;
    
    /* Get current value */
    val = g_lastResult[(dataRow + 1) * g_lastResultCols + dataCol];
    if (val) {
        MultiByteToWideChar(CP_ACP, 0, val, -1, wval, 256);
    } else {
        wval[0] = 0;  /* NULL becomes empty for editing */
    }
    
    /* Create edit control */
    g_hwndEditOverlay = CreateWindowExW(0, L"EDIT", wval,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        rcItem.left, rcItem.top, rcItem.right - rcItem.left, rcItem.bottom - rcItem.top,
        g_hwndGrid, NULL, g_hInst, NULL);
    
    if (!g_hwndEditOverlay) return;
    
    /* Set font and select all text */
    if (g_hFontGrid)
        SendMessage(g_hwndEditOverlay, WM_SETFONT, (WPARAM)g_hFontGrid, TRUE);
    SendMessage(g_hwndEditOverlay, EM_SETSEL, 0, -1);
    
    /* Subclass for Enter/Escape handling */
    g_pfnEditOverlayProc = (WNDPROC)SetWindowLong(g_hwndEditOverlay, GWL_WNDPROC, (LONG)EditOverlayProc);
    
    /* Store edit position */
    g_editRow = row;
    g_editCol = col;
    
    SetFocus(g_hwndEditOverlay);
}

static void CancelCellEdit(void) {
    if (g_hwndEditOverlay) {
        DestroyWindow(g_hwndEditOverlay);
        g_hwndEditOverlay = NULL;
    }
    g_editRow = -1;
    g_editCol = -1;
    SetFocus(g_hwndGrid);
}

static void CommitCellEdit(void) {
    wchar_t wval[256];
    char newVal[512];
    char sql[1024];
    char *p;
    const char *s;
    int dataRow, dataCol, rowidIdx;
    char *rowid;
    char *colName;
    char *errmsg = NULL;
    int rc, len;
    int setNull;
    int clearedToEmpty = 0;  /* Track NULL->empty fallback */
    
    if (!g_hwndEditOverlay || g_editRow < 0 || g_editCol < 0) {
        CancelCellEdit();
        return;
    }
    
    /* Check and reset NULL flag */
    setNull = g_commitNull;
    g_commitNull = 0;
    
    /* Get new value */
    GetWindowTextW(g_hwndEditOverlay, wval, 256);
    WideCharToMultiByte(CP_ACP, 0, wval, -1, newVal, 512, NULL, NULL);
    
    /* Empty string = NULL (user can clear field to set NULL) */
    if (newVal[0] == '\0') {
        setNull = 1;
    }
    
    /* Map to data indices */
    dataRow = g_sortIndex ? g_sortIndex[g_editRow] : g_editRow;
    dataCol = g_editCol + 1;  /* +1 for hidden rowid */
    rowidIdx = (dataRow + 1) * g_lastResultCols;  /* rowid is column 0 */
    
    rowid = g_lastResult[rowidIdx];
    colName = g_lastResult[dataCol];  /* Column name from header row */
    
    if (!rowid || !colName) {
        CancelCellEdit();
        return;
    }
    
    /* Build UPDATE statement */
    p = sql;
    s = "UPDATE \"";
    while (*s) *p++ = *s++;
    s = g_editTableName;
    while (*s) *p++ = *s++;
    s = "\" SET \"";
    while (*s) *p++ = *s++;
    s = colName;
    while (*s) *p++ = *s++;
    
    if (setNull) {
        /* SET column = NULL */
        s = "\" = NULL WHERE rowid = ";
        while (*s) *p++ = *s++;
    } else {
        /* SET column = 'value' */
        s = "\" = '";
        while (*s) *p++ = *s++;
        /* Escape single quotes in value */
        s = newVal;
        while (*s) {
            if (*s == '\'') *p++ = '\'';  /* Double up quotes */
            *p++ = *s++;
        }
        s = "' WHERE rowid = ";
        while (*s) *p++ = *s++;
    }
    s = rowid;
    while (*s) *p++ = *s++;
    *p++ = ';';
    *p = 0;
    
    /* Execute UPDATE */
    rc = sqlite_exec(g_db, sql, NULL, NULL, &errmsg);
    
    /* If NULL failed (likely NOT NULL constraint), retry with empty string */
    if (rc != SQLITE_OK && setNull) {
        if (errmsg) { sqlite_freemem(errmsg); errmsg = NULL; }
        
        /* Rebuild as empty string: SET "col" = '' WHERE rowid = N */
        p = sql;
        s = "UPDATE \"";
        while (*s) *p++ = *s++;
        s = g_editTableName;
        while (*s) *p++ = *s++;
        s = "\" SET \"";
        while (*s) *p++ = *s++;
        s = colName;
        while (*s) *p++ = *s++;
        s = "\" = '' WHERE rowid = ";
        while (*s) *p++ = *s++;
        s = rowid;
        while (*s) *p++ = *s++;
        *p++ = ';';
        *p = 0;
        
        rc = sqlite_exec(g_db, sql, NULL, NULL, &errmsg);
        if (rc == SQLITE_OK) {
            setNull = 0;  /* Actually stored empty string */
            clearedToEmpty = 1;
        }
    }
    
    if (rc == SQLITE_OK) {
        /* Free old value */
        if (g_lastResult[(dataRow + 1) * g_lastResultCols + dataCol])
            LocalFree(g_lastResult[(dataRow + 1) * g_lastResultCols + dataCol]);
        
        if (setNull) {
            /* Store NULL */
            g_lastResult[(dataRow + 1) * g_lastResultCols + dataCol] = NULL;
        } else {
            /* Store new value (or empty string) */
            len = 0;
            s = newVal;
            while (*s++) len++;
            
            g_lastResult[(dataRow + 1) * g_lastResultCols + dataCol] = 
                (char *)LocalAlloc(LMEM_FIXED, len + 1);
            if (g_lastResult[(dataRow + 1) * g_lastResultCols + dataCol]) {
                int i;
                for (i = 0; i <= len; i++)
                    g_lastResult[(dataRow + 1) * g_lastResultCols + dataCol][i] = newVal[i];
            }
        }
        
        /* Refresh the cell display */
        ListView_RedrawItems(g_hwndGrid, g_editRow, g_editRow);
        
        /* Update status */
        {
            const wchar_t *msg;
            if (setNull) msg = L"Cell set to NULL";
            else if (clearedToEmpty) msg = L"Cell cleared";
            else msg = L"Cell updated";
            SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)msg);
        }
    } else {
        /* Show error in dialog and status bar */
        wchar_t werr[256];
        wchar_t wstatus[280];
        if (errmsg) {
            MultiByteToWideChar(CP_ACP, 0, errmsg, -1, werr, 256);
            sqlite_freemem(errmsg);
        } else {
            lstrcpyW(werr, L"Update failed");
        }
        wsprintfW(wstatus, L"Error: %s", werr);
        SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)wstatus);
        MessageBoxW(g_hwndMain, werr, L"Error", MB_OK | MB_ICONERROR);
    }
    
    /* Close edit overlay */
    DestroyWindow(g_hwndEditOverlay);
    g_hwndEditOverlay = NULL;
    g_editRow = -1;
    g_editCol = -1;
    SetFocus(g_hwndGrid);
}

/*============================================================================
** Handle double-click to start cell edit
**============================================================================*/

void OnGridDoubleClick(int row, int col) {
    if (g_editMode && row >= 0 && col >= 0) {
        StartCellEdit(row, col);
    }
}
