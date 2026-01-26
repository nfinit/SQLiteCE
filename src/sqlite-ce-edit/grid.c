/*
** SQLiteCEdit - Grid view for query results (Virtual ListView)
*/

#include "globals.h"
#include "constants.h"
#include "allocators.h"
#include "strutils.h"

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

/* Undo state for deleted rows - UNDO_MAX_BYTES defined in constants.h */
typedef struct {
    char **values;   /* Column values (NULL-terminated strings) */
    char *data;      /* Contiguous storage for all string data */
    int numCols;
    int dataBytes;   /* Actual bytes used (for accurate memory tracking) */
} UndoRow;
static UndoRow *g_undoStack = NULL;
static int g_undoCount = 0;
static int g_undoCapacity = 0;
static int g_undoBytes = 0;  /* Current memory usage */

/* Forward declarations */
static void StartCellEdit(int row, int col);
static void CommitCellEdit(void);
static void CancelCellEdit(void);
static void DeleteSelectedRow(void);
static void InitInsertMode(void);
static void ClearInsertMode(void);
static void CommitInsert(void);
static int NextEditableColumn(int col, int direction);
static void PushUndo(char **values, int numCols);
static void UndoDelete(void);

void CreateGridView(HWND hwndParent, int x, int y, int cx, int cy) {
    HIMAGELIST hIml;
    
    g_hwndGrid = CreateWindowExW(0, WC_LISTVIEWW, NULL,
        WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_OWNERDATA,
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
    buf = ALLOC(char, 4096);
    if (!buf) return;
    p = buf;
    
    for (j = startCol; j < g_lastResultCols; j++) {
        char *val = g_lastResult[(dataRow + 1) * g_lastResultCols + j];
        if (j > startCol) *p++ = '\t';
        if (val) STR_COPY(p, val);
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
    /* Tell Windows we want Enter key */
    if (msg == WM_GETDLGCODE) {
        return DLGC_WANTALLKEYS;
    }
    /* Global shortcuts first */
    if (HandleGlobalKeys(msg, wParam))
        return 0;
    if (msg == WM_KEYDOWN) {
        int ctrl = GetKeyState(VK_CONTROL) < 0;
        int sel = ListView_GetNextItem(g_hwndGrid, -1, LVNI_SELECTED);
        
        /* Arrow/Enter with no selection - select first row and focus */
        if (sel < 0 && (wParam == VK_UP || wParam == VK_DOWN || wParam == VK_RETURN)) {
            ListView_SetItemState(g_hwndGrid, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            SetFocus(g_hwndGrid);
            return 0;
        }
        
        /* Ctrl+C - Copy selected row */
        if (ctrl && wParam == 'C') {
            CopySelectedRow();
            return 0;
        }
        /* Ctrl+A - Select all rows */
        if (ctrl && wParam == 'A') {
            ListView_SetItemState(g_hwndGrid, -1, LVIS_SELECTED, LVIS_SELECTED);
            return 0;
        }
        /* Ctrl+Home - Go to first row */
        if (ctrl && wParam == VK_HOME) {
            int count = ListView_GetItemCount(g_hwndGrid);
            if (count > 0) {
                ListView_SetItemState(g_hwndGrid, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
                ListView_SetItemState(g_hwndGrid, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                ListView_EnsureVisible(g_hwndGrid, 0, FALSE);
            }
            return 0;
        }
        /* Ctrl+End - Go to last row */
        if (ctrl && wParam == VK_END) {
            int count = ListView_GetItemCount(g_hwndGrid);
            if (count > 0) {
                ListView_SetItemState(g_hwndGrid, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
                ListView_SetItemState(g_hwndGrid, count - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                ListView_EnsureVisible(g_hwndGrid, count - 1, FALSE);
            }
            return 0;
        }
        /* Ctrl+Z - Undo last delete (only in edit mode) */
        if (ctrl && wParam == 'Z' && g_editMode) {
            if (g_undoCount > 0) UndoDelete();
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
        /* Escape/Backspace - back to query */
        if (wParam == VK_ESCAPE || wParam == VK_BACK) {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWQUERY, 0);
            return 0;
        }
        /* Ctrl+E - Execute */
        if (ctrl && wParam == 'E') {
            ExecuteQuery();
            return 0;
        }
        /* F2 - Start cell edit (only in edit mode) */
        if (wParam == VK_F2 && g_editMode) {
            int sel = ListView_GetNextItem(g_hwndGrid, -1, LVNI_SELECTED);
            if (sel >= 0) StartCellEdit(sel, 0);
            return 0;
        }
        /* Enter - Start editing selected row, or commit if in insert mode with data */
        if (wParam == VK_RETURN && g_editMode && !g_hwndEditOverlay) {
            int sel = ListView_GetNextItem(g_hwndGrid, -1, LVNI_SELECTED);
            if (sel >= 0) {
                /* If on placeholder row with no pending data, start editing */
                if (sel == g_lastResultRows) {
                    int hasData = 0, i, startCol = 0;
                    if (g_pendingValues) {
                        for (i = 0; i < g_colMetaCount && !hasData; i++)
                            if (g_pendingValues[i]) hasData = 1;
                    }
                    if (hasData) {
                        CommitInsert();
                    } else {
                        /* Find first non-autoincrement column */
                        for (i = 0; i < g_colMetaCount; i++) {
                            if (!g_colMeta[i].isAutoInc) { startCol = i; break; }
                        }
                        StartCellEdit(sel, startCol);
                    }
                } else {
                    StartCellEdit(sel, 0);
                }
            }
            return 0;
        }
        /* Delete - Delete selected rows (only in edit mode) */
        if (wParam == VK_DELETE && g_editMode) {
            DeleteSelectedRow();
            return 0;
        }
        /* Escape - Cancel insert mode if active (when not in cell edit) */
        if (wParam == VK_ESCAPE && g_insertMode && !g_hwndEditOverlay) {
            ClearInsertMode();
            return 0;
        }
    }
    return CallWindowProc(g_pfnGridProc, hwnd, msg, wParam, lParam);
}

/*
** Type-aware comparison for grid sorting.
** Detects numeric values and compares them numerically.
** Falls back to case-insensitive string comparison otherwise.
*/

/* Check if string is a valid number (integer or decimal) */
static int IsNumeric(const char *s, double *pVal) {
    const char *p = s;
    double val = 0.0;
    double frac = 0.0;
    double div = 1.0;
    int neg = 0;
    int hasDigit = 0;
    int hasDot = 0;

    if (!s || !*s) return 0;

    /* Skip leading whitespace */
    while (*p == ' ' || *p == '\t') p++;

    /* Handle sign */
    if (*p == '-') { neg = 1; p++; }
    else if (*p == '+') { p++; }

    /* Parse integer part */
    while (*p >= '0' && *p <= '9') {
        val = val * 10.0 + (*p - '0');
        hasDigit = 1;
        p++;
    }

    /* Parse decimal part */
    if (*p == '.') {
        hasDot = 1;
        p++;
        while (*p >= '0' && *p <= '9') {
            div *= 10.0;
            frac = frac * 10.0 + (*p - '0');
            hasDigit = 1;
            p++;
        }
    }

    /* Skip trailing whitespace */
    while (*p == ' ' || *p == '\t') p++;

    /* Must have digits and be at end of string */
    if (!hasDigit || *p != '\0') return 0;

    val = val + frac / div;
    if (neg) val = -val;

    if (pVal) *pVal = val;
    return 1;
}

/* Case-insensitive string comparison */
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

/* Type-aware value comparison */
static int CmpValues(const char *a, const char *b) {
    double na, nb;

    /* NULL handling */
    if (!a && !b) return 0;
    if (!a) return -1;  /* NULL sorts first */
    if (!b) return 1;

    /* Try numeric comparison first */
    if (IsNumeric(a, &na) && IsNumeric(b, &nb)) {
        if (na < nb) return -1;
        if (na > nb) return 1;
        return 0;
    }

    /* Fall back to string comparison */
    return StrCmpNoCase(a, b);
}

static int g_cmpCol;  /* Data column index for sorting */
static int CmpRows(const void *pa, const void *pb) {
    int ra = *(const int*)pa;
    int rb = *(const int*)pb;
    char *va = g_lastResult[(ra + 1) * g_lastResultCols + g_cmpCol];
    char *vb = g_lastResult[(rb + 1) * g_lastResultCols + g_cmpCol];
    int cmp = CmpValues(va, vb);
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
        g_sortIndex = ALLOC(int, g_lastResultRows);
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
    /* Use status bar instead of blocking message box */
    SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)L"Text not found");
}

/* Build placeholder hint for column: (auto), (TYPE null), (TYPE req), (TYPE) */
static void GetPlaceholderHint(int col, wchar_t *buf, int buflen) {
    ColumnMeta *cm;
    const char *t;
    wchar_t *p = buf;
    wchar_t *end = buf + buflen - 1;
    
    if (col >= g_colMetaCount) { buf[0] = '\0'; return; }
    cm = &g_colMeta[col];
    
    if (cm->isAutoInc) {
        lstrcpyW(buf, L"(auto)");
        return;
    }
    
    *p++ = '(';
    t = cm->type;
    /* Shorten INTEGER to INT */
    if (t[0]=='I' && t[1]=='N' && t[2]=='T' && t[3]=='E' && t[4]=='G' && t[5]=='E' && t[6]=='R' && t[7]=='\0') {
        *p++ = 'I'; *p++ = 'N'; *p++ = 'T';
    } else {
        while (*t && p < end - 6) *p++ = (wchar_t)*t++;
    }
    /* Add null/req suffix */
    if (!cm->notNull) {
        *p++ = ' '; *p++ = 'n'; *p++ = 'u'; *p++ = 'l'; *p++ = 'l';
    } else if (!cm->hasDefault) {
        *p++ = ' '; *p++ = 'r'; *p++ = 'e'; *p++ = 'q';
    }
    *p++ = ')'; *p = '\0';
}

void OnGridGetDispInfo(NMLVDISPINFOW *pdi) {
    int row, dataRow, col, dataCol;
    char *val;
    static wchar_t wbuf[256];
    static wchar_t hintbuf[32];
    int numDisplayCols;
    
    if (!(pdi->item.mask & LVIF_TEXT)) return;
    if (!g_lastResult) return;
    
    row = pdi->item.iItem;
    col = pdi->item.iSubItem;
    
    /* In edit mode, column 0 is hidden rowid, so shift display columns */
    dataCol = g_editMode ? col + 1 : col;
    numDisplayCols = g_editMode ? g_lastResultCols - 1 : g_lastResultCols;
    
    /* Handle placeholder row (last row in edit mode) */
    if (g_editMode && row == g_lastResultRows) {
        /* Show pending value if in insert mode, otherwise show hints */
        if (g_insertMode && g_pendingValues && g_pendingValues[col]) {
            /* Check for explicit NULL marker */
            if (g_pendingValues[col][0] == '\x01' && g_pendingValues[col][1] == '\0') {
                pdi->item.pszText = L"(null)";
            } else {
                MultiByteToWideChar(CP_ACP, 0, g_pendingValues[col], -1, wbuf, 256);
                pdi->item.pszText = wbuf;
            }
        } else {
            GetPlaceholderHint(col, hintbuf, 32);
            pdi->item.pszText = hintbuf;
        }
        return;
    }
    
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

/*
** PopulateGrid - Fill ListView with query results
**
** Long function (~130 lines) that could be refactored into:
** - ClearGridColumns() - remove existing columns
** - SetupGridColumns() - add columns from result metadata
** - AutoSizeColumns() - calculate optimal column widths
** - PopulateGridRows() - set row count and trigger redraw
**
** See OPTIMIZATION_PLAN.md D-006 for details.
*/
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
    FREE(g_sortIndex);
    g_sortCol = -1;
    g_sortAsc = 1;
    g_gridFindRow = 0;
    g_gridFindCol = -1;
    
    /* Disable repainting during setup */
    SendMessage(g_hwndGrid, WM_SETREDRAW, FALSE, 0);
    
    /* Clear existing */
    ListView_SetItemCount(g_hwndGrid, 0);
    while (ListView_DeleteColumn(g_hwndGrid, 0)) ;
    
    if (!g_lastResult || g_lastResultCols < 1) {
        SendMessage(g_hwndGrid, WM_SETREDRAW, TRUE, 0);
        return;
    }
    
    /* Allow 0 data rows in edit mode (placeholder row still shown) */
    if (g_lastResultRows < 1 && !g_editMode) {
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
    /* In edit mode, add one extra row for the placeholder (new row) */
    ListView_SetItemCount(g_hwndGrid, g_lastResultRows + (g_editMode ? 1 : 0));
    
    /* Auto-fit columns (optional - sample first 20 rows for speed) */
    if (g_gridAutoSize) {
        int totalRows = g_lastResultRows + (g_editMode ? 1 : 0);
        int sampleRows = totalRows < 20 ? totalRows : 20;
        ListView_SetItemCount(g_hwndGrid, sampleRows);
        for (j = 0; j < numDisplayCols; j++) {
            int contentWidth, headerWidth;
            ListView_SetColumnWidth(g_hwndGrid, j, LVSCW_AUTOSIZE);
            contentWidth = ListView_GetColumnWidth(g_hwndGrid, j);
            ListView_SetColumnWidth(g_hwndGrid, j, LVSCW_AUTOSIZE_USEHEADER);
            headerWidth = ListView_GetColumnWidth(g_hwndGrid, j);
            if (contentWidth > headerWidth)
                ListView_SetColumnWidth(g_hwndGrid, j, contentWidth);
            /* In edit mode, ensure column fits placeholder hints */
            if (g_editMode && j < g_colMetaCount) {
                HDC hdc = GetDC(g_hwndGrid);
                if (hdc) {
                    SIZE sz;
                    int hintWidth, curWidth;
                    wchar_t hint[32];
                    GetPlaceholderHint(j, hint, 32);
                    GetTextExtentPoint32W(hdc, hint, lstrlenW(hint), &sz);
                    hintWidth = sz.cx + 12;  /* padding */
                    curWidth = ListView_GetColumnWidth(g_hwndGrid, j);
                    if (hintWidth > curWidth)
                        ListView_SetColumnWidth(g_hwndGrid, j, hintWidth);
                    ReleaseDC(g_hwndGrid, hdc);
                }
            }
        }
        ListView_SetItemCount(g_hwndGrid, totalRows);
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
static int g_inCommit = 0;    /* Guard against re-entry */
static int g_cellDirty = 0;   /* Track if user typed anything in cell */

/* Find next editable column (skips autoincrement) */
static int NextEditableColumn(int col, int direction) {
    int next = col + direction;
    while (next >= 0 && next < g_colMetaCount) {
        if (!g_colMeta[next].isAutoInc) return next;
        next += direction;
    }
    return -1;  /* No more editable columns */
}

/*
** Centralized edit overlay keyboard handler.
** Returns 1 if key was handled, 0 otherwise.
*/
static int HandleEditKey(UINT msg, WPARAM wParam) {
    int ctrl = GetKeyState(VK_CONTROL) < 0;
    int shift = GetKeyState(VK_SHIFT) < 0;
    int nextCol, row, wasInsertMode;
    
    /* Ctrl+Delete - set NULL and advance */
    if (msg == WM_KEYDOWN && ctrl && wParam == VK_DELETE) {
        nextCol = NextEditableColumn(g_editCol, 1);
        row = g_editRow;
        g_commitNull = 1;
        CommitCellEdit();
        if (nextCol >= 0) StartCellEdit(row, nextCol);
        return 1;
    }
    
    /* Ctrl+0 - set NULL and advance (WM_KEYDOWN or WM_CHAR) */
    if (ctrl && wParam == '0' && (msg == WM_KEYDOWN || msg == WM_CHAR)) {
        nextCol = NextEditableColumn(g_editCol, 1);
        row = g_editRow;
        g_commitNull = 1;
        CommitCellEdit();
        if (nextCol >= 0) StartCellEdit(row, nextCol);
        return 1;
    }
    
    /* Tab / Shift+Tab - move between columns */
    if (msg == WM_KEYDOWN && wParam == VK_TAB) {
        nextCol = NextEditableColumn(g_editCol, shift ? -1 : 1);
        row = g_editRow;
        CommitCellEdit();
        if (nextCol >= 0) {
            StartCellEdit(row, nextCol);
        } else if (!shift && g_insertMode) {
            CommitInsert();
        }
        return 1;
    }
    
    /* Enter - commit cell (and row if in insert mode) */
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        wasInsertMode = g_insertMode;
        row = g_editRow;
        CommitCellEdit();
        if (wasInsertMode && row == g_lastResultRows) {
            CommitInsert();
        }
        return 1;
    }
    
    /* Escape - cancel edit */
    if (msg == WM_KEYDOWN && wParam == VK_ESCAPE) {
        CancelCellEdit();
        return 1;
    }
    
    return 0;
}

static LRESULT CALLBACK EditOverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    /* Handle edit shortcuts */
    if (msg == WM_KEYDOWN || msg == WM_CHAR) {
        if (HandleEditKey(msg, wParam)) return 0;
    }
    
    /* Track dirty state - user typed something */
    if (msg == WM_CHAR && wParam >= 32) {
        g_cellDirty = 1;
    }
    /* Also mark dirty on backspace/delete (user is modifying) */
    if (msg == WM_KEYDOWN && (wParam == VK_BACK || wParam == VK_DELETE)) {
        g_cellDirty = 1;
    }
    
    /* Commit on focus loss */
    if (msg == WM_KILLFOCUS && g_hwndEditOverlay) {
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
    
    /* Reset dirty flag for new edit */
    g_cellDirty = 0;
    
    /* Placeholder row (new row) - enter insert mode */
    if (row == g_lastResultRows) {
        /* Don't allow editing autoincrement columns */
        if (col < g_colMetaCount && g_colMeta[col].isAutoInc) {
            return;
        }
        
        /* Initialize insert mode if not already */
        if (!g_insertMode) {
            InitInsertMode();
            if (!g_insertMode) return;  /* Allocation failed */
        }
        
        /* Get cell rectangle */
        GetClientRect(g_hwndGrid, &rcGrid);
        rcItem.top = row;
        rcItem.left = LVIR_BOUNDS;
        ListView_GetItemRect(g_hwndGrid, row, &rcItem, LVIR_BOUNDS);
        
        x = rcItem.left;
        for (i = 0; i < col; i++) {
            x += ListView_GetColumnWidth(g_hwndGrid, i);
        }
        width = ListView_GetColumnWidth(g_hwndGrid, col);
        rcItem.left = x;
        rcItem.right = x + width;
        
        /* Get pending value if any (show empty for NULL marker) */
        wval[0] = 0;
        if (g_pendingValues && g_pendingValues[col]) {
            if (!(g_pendingValues[col][0] == '\x01' && g_pendingValues[col][1] == '\0')) {
                MultiByteToWideChar(CP_ACP, 0, g_pendingValues[col], -1, wval, 256);
            }
        }
        
        /* Create edit control */
        g_hwndEditOverlay = CreateWindowExW(0, L"EDIT", wval,
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            rcItem.left, rcItem.top, rcItem.right - rcItem.left, rcItem.bottom - rcItem.top,
            g_hwndGrid, NULL, g_hInst, NULL);
        
        if (!g_hwndEditOverlay) return;
        
        if (g_hFontGrid)
            SendMessage(g_hwndEditOverlay, WM_SETFONT, (WPARAM)g_hFontGrid, TRUE);
        SendMessage(g_hwndEditOverlay, EM_SETSEL, 0, -1);
        g_pfnEditOverlayProc = (WNDPROC)SetWindowLong(g_hwndEditOverlay, GWL_WNDPROC, (LONG)EditOverlayProc);
        
        g_editRow = row;
        g_editCol = col;
        SetFocus(g_hwndEditOverlay);
        return;
    }
    
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
    HWND hwnd = g_hwndEditOverlay;
    /* Clear handle first to prevent WM_KILLFOCUS from committing */
    g_hwndEditOverlay = NULL;
    g_editRow = -1;
    g_editCol = -1;
    if (hwnd) {
        DestroyWindow(hwnd);
    }
    SetFocus(g_hwndGrid);
}

/*============================================================================
** Undo Support for Row Deletion
**============================================================================*/

static void FreeUndoRow(UndoRow *row) {
    /* Single deallocation for values array */
    if (row->values) LocalFree(row->values);
    /* Single deallocation for all string data */
    if (row->data) LocalFree(row->data);
    row->values = NULL;
    row->data = NULL;
}

void ClearUndoStack(void) {
    int i;
    for (i = 0; i < g_undoCount; i++) {
        FreeUndoRow(&g_undoStack[i]);
    }
    FREE(g_undoStack);
    g_undoCount = 0;
    g_undoCapacity = 0;
    g_undoBytes = 0;
}

int CanUndo(void) {
    return g_undoCount > 0;
}

void DoUndo(void) {
    if (g_undoCount > 0) UndoDelete();
}

void DoDeleteRows(void) {
    if (g_editMode) DeleteSelectedRow();
}

/* Check available memory before caching */
static int CanCacheUndo(int bytesNeeded) {
    /* Drop oldest entries if over limit */
    while (g_undoCount > 0 && g_undoBytes + bytesNeeded > UNDO_MAX_BYTES) {
        int oldBytes = g_undoStack[0].dataBytes;  /* Use actual tracked bytes */
        int j;
        FreeUndoRow(&g_undoStack[0]);
        g_undoBytes -= oldBytes;
        if (g_undoBytes < 0) g_undoBytes = 0;
        g_undoCount--;
        for (j = 0; j < g_undoCount; j++)
            g_undoStack[j] = g_undoStack[j + 1];
    }
    return 1;
}

static void PushUndo(char **values, int numCols) {
    int i, dataBytes = 0, ptrBytes;
    int *lengths;
    char *dataPtr;
    UndoRow *row;

    if (!values || numCols < 1) return;

    /* Calculate total data size needed */
    lengths = ALLOC(int, numCols);
    if (!lengths) return;

    for (i = 0; i < numCols; i++) {
        if (values[i]) {
            int len = 0;
            while (values[i][len]) len++;
            lengths[i] = len + 1;
            dataBytes += len + 1;
        } else {
            lengths[i] = 0;
        }
    }

    ptrBytes = numCols * sizeof(char *);

    if (!CanCacheUndo(dataBytes + ptrBytes)) {
        LocalFree(lengths);
        return;
    }

    /* Grow stack if needed */
    if (g_undoCount >= g_undoCapacity) {
        int newCap = g_undoCapacity ? g_undoCapacity * 2 : 8;
        UndoRow *newStack = ALLOC(UndoRow, newCap);
        if (!newStack) {
            LocalFree(lengths);
            return;
        }
        if (g_undoStack) {
            for (i = 0; i < g_undoCount; i++) newStack[i] = g_undoStack[i];
            LocalFree(g_undoStack);
        }
        g_undoStack = newStack;
        g_undoCapacity = newCap;
    }

    /* Allocate contiguous storage for all column data */
    row = &g_undoStack[g_undoCount];
    row->numCols = numCols;
    row->dataBytes = dataBytes + ptrBytes;
    row->values = (char **)ALLOC_SIZE(ptrBytes);
    row->data = dataBytes > 0 ? (char *)ALLOC_SIZE(dataBytes) : NULL;

    if (!row->values || (dataBytes > 0 && !row->data)) {
        if (row->values) LocalFree(row->values);
        if (row->data) LocalFree(row->data);
        LocalFree(lengths);
        return;
    }

    /* Copy all strings into contiguous block */
    dataPtr = row->data;
    for (i = 0; i < numCols; i++) {
        if (lengths[i] > 0) {
            int j;
            row->values[i] = dataPtr;
            for (j = 0; j < lengths[i]; j++) dataPtr[j] = values[i][j];
            dataPtr += lengths[i];
        } else {
            row->values[i] = NULL;
        }
    }

    LocalFree(lengths);
    g_undoCount++;
    g_undoBytes += row->dataBytes;
}

static void UndoDelete(void) {
    UndoRow *row;
    char sql[2048];
    char *p;
    const char *s;
    int i, rc;
    char *errmsg = NULL;
    char tableName[128];
    
    if (g_undoCount < 1 || !g_editMode) return;
    
    /* Save table name */
    p = tableName;
    STR_COPY(p, g_editTableName);
    *p = '\0';

    row = &g_undoStack[g_undoCount - 1];

    /* Build INSERT - skip column 0 (rowid) */
    p = sql;
    STR_COPY(p, "INSERT INTO \"");
    STR_COPY(p, tableName);
    STR_COPY(p, "\" (");

    /* Column names from metadata (skip rowid) */
    for (i = 1; i < row->numCols && i < g_colMetaCount + 1; i++) {
        if (i > 1) *p++ = ',';
        *p++ = '"';
        STR_COPY(p, g_colMeta[i - 1].name);
        *p++ = '"';
    }

    STR_COPY(p, ") VALUES (");
    
    /* Values (skip rowid at index 0) */
    for (i = 1; i < row->numCols; i++) {
        if (i > 1) *p++ = ',';
        if (row->values[i]) {
            *p++ = '\'';
            s = row->values[i];
            while (*s) {
                if (*s == '\'') *p++ = '\'';  /* Escape quotes */
                *p++ = *s++;
            }
            *p++ = '\'';
        } else {
            STR_COPY(p, "NULL");
        }
    }
    *p++ = ')';
    *p++ = ';';
    *p = '\0';
    
    rc = sqlite_exec(g_db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        wchar_t wmsg[256];
        MultiByteToWideChar(CP_ACP, 0, errmsg ? errmsg : "Unknown error", -1, wmsg, 256);
        ReportError(ERR_ERROR, L"Undo Failed", wmsg, 1);
        if (errmsg) sqlite_freemem(errmsg);
        return;
    }
    
    /* Remove from undo stack */
    g_undoBytes -= row->dataBytes;
    if (g_undoBytes < 0) g_undoBytes = 0;
    FreeUndoRow(row);
    g_undoCount--;

    /* Refresh grid */
    OpenTableForEditing(tableName);
    SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)L"Row restored");
}

static void DeleteSelectedRow(void) {
    int sel, count, dataRow, rowidIdx, deleted, i;
    char *rowid;
    char sql[256];
    char tableName[128];
    char *p;
    const char *s;
    char *errmsg = NULL;
    int rc;
    wchar_t msg[64];
    char **rowids;
    int *selRows;
    
    /* Count selected rows (excluding placeholder) */
    count = 0;
    sel = -1;
    while ((sel = ListView_GetNextItem(g_hwndGrid, sel, LVNI_SELECTED)) >= 0) {
        if (sel < g_lastResultRows) count++;
    }
    if (count < 1) return;
    
    /* Collect selection indices BEFORE dialog (dialog may clear selection) */
    selRows = ALLOC(int, count);
    if (!selRows) return;
    i = 0;
    sel = -1;
    while ((sel = ListView_GetNextItem(g_hwndGrid, sel, LVNI_SELECTED)) >= 0) {
        if (sel < g_lastResultRows && i < count) selRows[i++] = sel;
    }
    count = i;
    
    /* Save table name before it gets cleared */
    p = tableName;
    STR_COPY(p, g_editTableName);
    *p = '\0';
    
    /* Confirm deletion */
    if (count == 1)
        lstrcpyW(msg, L"Delete this row?");
    else
        wsprintfW(msg, L"Delete %d rows?", count);
    if (MessageBoxW(g_hwndMain, msg, L"Confirm Delete",
                    MB_YESNO | MB_ICONQUESTION) != IDYES) {
        LocalFree(selRows);
        return;
    }
    
    /* Collect rowids and save row data for undo */
    rowids = ALLOC(char *, count);
    if (!rowids) { LocalFree(selRows); return; }
    
    for (i = 0; i < count; i++) {
        int j, len;
        sel = selRows[i];
        dataRow = g_sortIndex ? g_sortIndex[sel] : sel;
        rowidIdx = (dataRow + 1) * g_lastResultCols;
        rowid = g_lastResult[rowidIdx];
        if (rowid) {
            /* Save row data for undo (entire row including rowid) */
            PushUndo(&g_lastResult[rowidIdx], g_lastResultCols);
            
            len = 0;
            while (rowid[len]) len++;
            rowids[i] = ALLOC(char, len + 1);
            if (rowids[i]) {
                for (j = 0; j <= len; j++) rowids[i][j] = rowid[j];
            }
        } else {
            rowids[i] = NULL;
        }
    }
    LocalFree(selRows);
    
    /* Delete by rowid */
    deleted = 0;
    for (i = 0; i < count; i++) {
        if (!rowids[i]) continue;
        p = sql;
        STR_COPY(p, "DELETE FROM \"");
        STR_COPY(p, tableName);
        STR_COPY(p, "\" WHERE rowid = ");
        STR_COPY(p, rowids[i]);
        *p++ = ';';
        *p = '\0';
        
        rc = sqlite_exec(g_db, sql, NULL, NULL, &errmsg);
        if (rc != SQLITE_OK) {
            wchar_t wmsg[256];
            MultiByteToWideChar(CP_ACP, 0, errmsg ? errmsg : "Unknown error", -1, wmsg, 256);
            ReportError(ERR_ERROR, L"Delete Failed", wmsg, 1);
            if (errmsg) sqlite_freemem(errmsg);
            break;
        }
        deleted++;
    }
    
    /* Free rowid copies */
    for (i = 0; i < count; i++) {
        if (rowids[i]) LocalFree(rowids[i]);
    }
    LocalFree(rowids);
    
    /* Re-query to refresh grid */
    OpenTableForEditing(tableName);
    
    /* Status feedback */
    if (deleted > 0) {
        wsprintfW(msg, L"%d row%s deleted", deleted, deleted == 1 ? L"" : L"s");
        SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)msg);
    }
}

static void ClearInsertMode(void) {
    int i;
    if (g_pendingValues) {
        for (i = 0; i < g_colMetaCount; i++) {
            FREE(g_pendingValues[i]);
        }
        FREE(g_pendingValues);
    }
    g_insertMode = 0;
    
    /* Refresh placeholder row display */
    if (g_hwndGrid && g_editMode) {
        ListView_RedrawItems(g_hwndGrid, g_lastResultRows, g_lastResultRows);
    }
}

static void InitInsertMode(void) {
    if (g_insertMode) return;  /* Already in insert mode */
    if (g_colMetaCount < 1) return;
    
    /* Allocate pending values array */
    g_pendingValues = ALLOC_ZERO(char *, g_colMetaCount);
    if (!g_pendingValues) return;
    
    g_insertMode = 1;
}

static void StorePendingValue(int col, const char *value) {
    int len;
    const char *s;
    char *d;
    
    if (!g_pendingValues || col < 0 || col >= g_colMetaCount) return;
    
    /* Free old value */
    FREE(g_pendingValues[col]);
    
    /* Store new value */
    /* NULL pointer = not set, "\x01" = explicit NULL, "" = empty string, else = value */
    if (value) {
        len = 0;
        s = value;
        while (*s++) len++;
        g_pendingValues[col] = ALLOC(char, len + 1);
        if (g_pendingValues[col]) {
            s = value;
            d = g_pendingValues[col];
            STR_COPY(d, s);
            *d = '\0';
        }
    }
}

static void CommitInsert(void) {
    char sql[2048];
    char *p;
    const char *s;
    char *errmsg = NULL;
    int rc, i, first;
    char tableName[128];
    
    if (!g_insertMode || !g_pendingValues) return;
    
    /* Save table name */
    p = tableName;
    STR_COPY(p, g_editTableName);
    *p = '\0';

    /* Build INSERT statement */
    p = sql;
    STR_COPY(p, "INSERT INTO \"");
    STR_COPY(p, tableName);
    STR_COPY(p, "\" (");

    /* Column list - skip autoincrement, only include columns with values */
    first = 1;
    for (i = 0; i < g_colMetaCount; i++) {
        if (g_colMeta[i].isAutoInc) continue;
        if (!g_pendingValues[i]) continue;

        if (!first) *p++ = ',';
        first = 0;
        *p++ = '"';
        STR_COPY(p, g_colMeta[i].name);
        *p++ = '"';
    }

    /* If no columns have values, can't insert */
    if (first) {
        MessageBoxW(g_hwndMain, L"No values entered.", L"Insert", MB_OK | MB_ICONWARNING);
        return;
    }

    STR_COPY(p, ") VALUES (");

    /* Values list */
    first = 1;
    for (i = 0; i < g_colMetaCount; i++) {
        if (g_colMeta[i].isAutoInc) continue;
        if (!g_pendingValues[i]) continue;

        if (!first) *p++ = ',';
        first = 0;

        /* Check for explicit NULL marker */
        if (g_pendingValues[i][0] == '\x01' && g_pendingValues[i][1] == '\0') {
            STR_COPY(p, "NULL");
        } else {
            *p++ = '\'';
            /* Escape single quotes */
            s = g_pendingValues[i];
            while (*s) {
                if (*s == '\'') *p++ = '\'';
                *p++ = *s++;
            }
            *p++ = '\'';
        }
    }
    
    STR_COPY(p, ");");
    *p = '\0';

    /* Execute INSERT */
    rc = sqlite_exec(g_db, sql, NULL, NULL, &errmsg);
    
    if (rc != SQLITE_OK) {
        wchar_t wmsg[256];
        MultiByteToWideChar(CP_ACP, 0, errmsg ? errmsg : "Unknown error", -1, wmsg, 256);
        ReportError(ERR_ERROR, L"Insert Failed", wmsg, 1);
        SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)wmsg);
        if (errmsg) sqlite_freemem(errmsg);
        return;
    }
    
    /* Clear insert mode and refresh */
    ClearInsertMode();
    OpenTableForEditing(tableName);
    
    SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)L"Row inserted");
}

/*
** CommitCellEdit - Save cell changes to database
**
** This function handles multiple concerns and could be refactored into:
** - HandleInsertModeCommit() - store pending values for new row
** - BuildUpdateSQL() - construct UPDATE statement with escaping
** - ExecuteUpdateWithFallback() - run UPDATE, retry with empty on NOT NULL fail
** - UpdateResultCache() - update in-memory result after successful UPDATE
** - CleanupCellEdit() - destroy overlay and reset state
**
** See OPTIMIZATION_PLAN.md D-006 for details.
*/
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
    
    /* Guard against re-entry from WM_KILLFOCUS */
    if (g_inCommit) return;
    g_inCommit = 1;
    
    if (!g_hwndEditOverlay || g_editRow < 0 || g_editCol < 0) {
        g_inCommit = 0;
        CancelCellEdit();
        return;
    }
    
    /* Check and reset NULL flag */
    setNull = g_commitNull;
    g_commitNull = 0;
    
    /* Get new value */
    GetWindowTextW(g_hwndEditOverlay, wval, 256);
    WideCharToMultiByte(CP_ACP, 0, wval, -1, newVal, 512, NULL, NULL);
    
    /* Insert mode - store to pending values, don't execute UPDATE */
    if (g_insertMode && g_editRow == g_lastResultRows) {
        if (setNull) {
            /* Explicit NULL - use marker */
            StorePendingValue(g_editCol, "\x01");
        } else if (!g_cellDirty && newVal[0] == '\0') {
            /* Cell untouched and empty - don't store anything (will be NULL) */
            /* StorePendingValue not called - leaves as NULL */
        } else {
            /* Store value (including empty string if user typed then deleted) */
            StorePendingValue(g_editCol, newVal);
        }
        
        /* Refresh the cell display */
        ListView_RedrawItems(g_hwndGrid, g_editRow, g_editRow);
        
        /* Close edit overlay */
        DestroyWindow(g_hwndEditOverlay);
        g_hwndEditOverlay = NULL;
        g_editRow = -1;
        g_editCol = -1;
        g_inCommit = 0;
        SetFocus(g_hwndGrid);
        return;
    }
    
    /* For existing rows: empty string = NULL unless explicit */
    if (newVal[0] == '\0' && !setNull) {
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
    STR_COPY(p, "UPDATE \"");
    STR_COPY(p, g_editTableName);
    STR_COPY(p, "\" SET \"");
    STR_COPY(p, colName);

    if (setNull) {
        /* SET column = NULL */
        STR_COPY(p, "\" = NULL WHERE rowid = ");
    } else {
        /* SET column = 'value' */
        STR_COPY(p, "\" = '");
        /* Escape single quotes in value */
        s = newVal;
        while (*s) {
            if (*s == '\'') *p++ = '\'';  /* Double up quotes */
            *p++ = *s++;
        }
        STR_COPY(p, "' WHERE rowid = ");
    }
    STR_COPY(p, rowid);
    *p++ = ';';
    *p = 0;
    
    /* Execute UPDATE */
    rc = sqlite_exec(g_db, sql, NULL, NULL, &errmsg);
    
    /* If NULL failed (likely NOT NULL constraint), retry with empty string */
    if (rc != SQLITE_OK && setNull) {
        if (errmsg) { sqlite_freemem(errmsg); errmsg = NULL; }
        
        /* Rebuild as empty string: SET "col" = '' WHERE rowid = N */
        p = sql;
        STR_COPY(p, "UPDATE \"");
        STR_COPY(p, g_editTableName);
        STR_COPY(p, "\" SET \"");
        STR_COPY(p, colName);
        STR_COPY(p, "\" = '' WHERE rowid = ");
        STR_COPY(p, rowid);
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
                ALLOC(char, len + 1);
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
    g_inCommit = 0;
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
