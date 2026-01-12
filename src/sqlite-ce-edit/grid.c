/*
** SQLiteCEdit - Grid view for query results
*/

#include "globals.h"

void CreateGridView(HWND hwndParent, int x, int y, int cx, int cy) {
    HIMAGELIST hIml;
    
    g_hwndGrid = CreateWindowExW(0, WC_LISTVIEWW, NULL,
        WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
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
    if (msg == WM_KEYDOWN) {
        int ctrl = GetKeyState(VK_CONTROL) < 0;
        int alt = GetKeyState(VK_MENU) < 0;
        
        /* Alt+X - Exit */
        if (alt && wParam == 'X') {
            SendMessage(g_hwndMain, WM_CLOSE, 0, 0);
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

void PopulateGrid(void) {
    int i, j;
    LVCOLUMNW col;
    LVITEMW item;
    wchar_t wbuf[256];
    RECT rc;
    int totalWidth, colWidth;
    
    if (!g_hwndGrid) return;
    
    /* Clear existing */
    ListView_DeleteAllItems(g_hwndGrid);
    while (ListView_DeleteColumn(g_hwndGrid, 0)) ;
    
    if (!g_lastResult || g_lastResultCols < 1 || g_lastResultRows < 1) return;
    
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
    
    /* Add data rows */
    memset(&item, 0, sizeof(item));
    item.mask = LVIF_TEXT;
    
    for (i = 1; i <= g_lastResultRows; i++) {
        /* First column */
        item.iItem = i - 1;
        item.iSubItem = 0;
        if (g_lastResult[i * g_lastResultCols]) {
            MultiByteToWideChar(CP_ACP, 0, g_lastResult[i * g_lastResultCols], -1, wbuf, 256);
        } else {
            lstrcpyW(wbuf, L"(null)");
        }
        item.pszText = wbuf;
        ListView_InsertItem(g_hwndGrid, &item);
        
        /* Remaining columns */
        for (j = 1; j < g_lastResultCols; j++) {
            char *val = g_lastResult[i * g_lastResultCols + j];
            if (val) {
                MultiByteToWideChar(CP_ACP, 0, val, -1, wbuf, 256);
            } else {
                lstrcpyW(wbuf, L"(null)");
            }
            ListView_SetItemText(g_hwndGrid, i - 1, j, wbuf);
        }
    }
    
    /* Auto-fit columns to content and header (optional - slow on older devices) */
    if (g_gridAutoSize) {
        for (j = 0; j < g_lastResultCols; j++) {
            int contentWidth, headerWidth;
            ListView_SetColumnWidth(g_hwndGrid, j, LVSCW_AUTOSIZE);
            contentWidth = ListView_GetColumnWidth(g_hwndGrid, j);
            ListView_SetColumnWidth(g_hwndGrid, j, LVSCW_AUTOSIZE_USEHEADER);
            headerWidth = ListView_GetColumnWidth(g_hwndGrid, j);
            /* Use the larger of content or header width */
            if (contentWidth > headerWidth)
                ListView_SetColumnWidth(g_hwndGrid, j, contentWidth);
        }
    }
}
