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
    
    if (sel < 0 || !g_lastResult || sel >= g_lastResultRows) return;
    
    /* Map through sort index */
    dataRow = g_sortIndex ? g_sortIndex[sel] : sel;
    
    /* Build tab-separated string */
    buf = (char *)LocalAlloc(LMEM_FIXED, 4096);
    if (!buf) return;
    p = buf;
    
    for (j = 0; j < g_lastResultCols; j++) {
        char *val = g_lastResult[(dataRow + 1) * g_lastResultCols + j];
        if (j > 0) *p++ = '\t';
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
        /* Ctrl+G - Toggle back to text view */
        if (ctrl && wParam == 'G') {
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

static int g_cmpCol;
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
    if (!g_lastResult || g_lastResultRows < 1) return;
    
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
    
    /* Sort */
    g_cmpCol = col;
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
    char findBuf[256];
    
    if (!g_findText[0] || !g_lastResult || g_lastResultRows < 1) return;
    
    /* Convert search text to ANSI for comparison */
    for (i = 0; g_findText[i] && i < 255; i++)
        findBuf[i] = (char)g_findText[i];
    findBuf[i] = 0;
    findLen = i;
    
    /* Start from current position + 1 cell */
    startRow = g_gridFindRow;
    startCol = g_gridFindCol + 1;
    if (startCol >= g_lastResultCols) { startCol = 0; startRow++; }
    if (startRow >= g_lastResultRows) startRow = 0;
    
    /* Search from current position to end */
    for (r = startRow; r < g_lastResultRows; r++) {
        int dataRow = g_sortIndex ? g_sortIndex[r] : r;
        for (c = (r == startRow ? startCol : 0); c < g_lastResultCols; c++) {
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
        int endCol = (r == startRow) ? startCol : g_lastResultCols;
        for (c = 0; c < endCol; c++) {
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
    int row, dataRow, col;
    char *val;
    static wchar_t wbuf[256];
    
    if (!(pdi->item.mask & LVIF_TEXT)) return;
    if (!g_lastResult) return;
    
    row = pdi->item.iItem;
    col = pdi->item.iSubItem;
    
    if (row >= g_lastResultRows || col >= g_lastResultCols) {
        pdi->item.pszText = L"";
        return;
    }
    
    /* Map through sort index if sorted */
    dataRow = g_sortIndex ? g_sortIndex[row] : row;
    
    val = g_lastResult[(dataRow + 1) * g_lastResultCols + col];
    if (val) {
        MultiByteToWideChar(CP_ACP, 0, val, -1, wbuf, 256);
        pdi->item.pszText = wbuf;
    } else {
        pdi->item.pszText = L"(null)";
    }
}

void PopulateGrid(void) {
    int j;
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
    
    /* Calculate column width to fill grid */
    GetClientRect(g_hwndGrid, &rc);
    totalWidth = rc.right - GetSystemMetrics(SM_CXVSCROLL) - 4;
    colWidth = totalWidth / g_lastResultCols;
    if (colWidth < 60) colWidth = 60;
    
    /* Add columns from header row */
    memset(&col, 0, sizeof(col));
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    col.fmt = LVCFMT_LEFT;
    col.cx = colWidth;
    
    for (j = 0; j < g_lastResultCols; j++) {
        if (g_lastResult[j]) {
            MultiByteToWideChar(CP_ACP, 0, g_lastResult[j], -1, wbuf, 256);
        } else {
            wbuf[0] = 0;
        }
        col.pszText = wbuf;
        /* Last column gets remaining width */
        if (j == g_lastResultCols - 1) {
            col.cx = totalWidth - (colWidth * (g_lastResultCols - 1));
        }
        ListView_InsertColumn(g_hwndGrid, j, &col);
    }
    
    /* Set item count - virtual ListView fetches data on demand */
    ListView_SetItemCount(g_hwndGrid, g_lastResultRows);
    
    /* Auto-fit columns (optional - sample first 20 rows for speed) */
    if (g_gridAutoSize) {
        int sampleRows = g_lastResultRows < 20 ? g_lastResultRows : 20;
        ListView_SetItemCount(g_hwndGrid, sampleRows);
        for (j = 0; j < g_lastResultCols; j++) {
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
    
    /* Update status bar with query and draw time */
    elapsed = GetTickCount() - startTick;
    wsprintfW(wbuf, L"%d row(s), query %lums, draw %lums", g_lastResultRows, g_lastQueryTime, elapsed);
    SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)wbuf);
}
