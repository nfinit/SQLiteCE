/*
** SQLiteCEdit - Grid view for query results (Virtual ListView)
*/

#include "globals.h"

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

void OnGridGetDispInfo(NMLVDISPINFOW *pdi) {
    int row, col;
    char *val;
    static wchar_t wbuf[256];
    
    if (!(pdi->item.mask & LVIF_TEXT)) return;
    if (!g_lastResult) return;
    
    row = pdi->item.iItem + 1;  /* +1 to skip header row in g_lastResult */
    col = pdi->item.iSubItem;
    
    if (row > g_lastResultRows || col >= g_lastResultCols) {
        pdi->item.pszText = L"";
        return;
    }
    
    val = g_lastResult[row * g_lastResultCols + col];
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
