/*
** SQLiteCEdit - SQL Query Editor for Windows CE
** Main entry point and window procedure
*/

#include "globals.h"

#ifndef ICON_SMALL
#define ICON_SMALL 0
#endif

/*============================================================================
** Window Procedure
**============================================================================*/

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            RECT rc;
            int cbHeight;
            HMENU hMenu, hFile, hView;
            TBBUTTON tbButtons[15];
            
            g_hBrushWhite = CreateSolidBrush(RGB(255, 255, 255));
            
            /* Command bar with menus */
            g_hwndCB = CommandBar_Create(g_hInst, hwnd, 1);
            
            hMenu = CreateMenu();
            hFile = CreatePopupMenu();
            {
                HMENU hDatabase, hQueryFile, hExport, hImport;
                hDatabase = CreatePopupMenu();
                AppendMenuW(hDatabase, MF_STRING, IDM_NEW, L"&New...");
                AppendMenuW(hDatabase, MF_STRING, IDM_OPEN, L"&Open...");
                g_hRecentDbMenu = CreatePopupMenu();
                AppendMenuW(g_hRecentDbMenu, MF_STRING | MF_GRAYED, 0, L"(none)");
                AppendMenuW(hDatabase, MF_POPUP, (UINT)g_hRecentDbMenu, L"&Recent");
                AppendMenuW(hDatabase, MF_STRING, IDM_CLOSE, L"&Close");
                AppendMenuW(hDatabase, MF_STRING, IDM_BACKUP, L"&Backup...");
                AppendMenuW(hFile, MF_POPUP, (UINT)hDatabase, L"&Database");
                hQueryFile = CreatePopupMenu();
                AppendMenuW(hQueryFile, MF_STRING, IDM_OPENQUERY, L"&Open...\tCtrl+O");
                AppendMenuW(hQueryFile, MF_STRING, IDM_SAVEQUERY, L"&Save...\tCtrl+S");
                g_hRecentQueryMenu = CreatePopupMenu();
                AppendMenuW(g_hRecentQueryMenu, MF_STRING | MF_GRAYED, 0, L"(none)");
                AppendMenuW(hQueryFile, MF_POPUP, (UINT)g_hRecentQueryMenu, L"&Recent");
                AppendMenuW(hFile, MF_POPUP, (UINT)hQueryFile, L"&Query");
                AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
                hExport = CreatePopupMenu();
                AppendMenuW(hExport, MF_STRING, IDM_EXPORTRESULTS, L"&Results...");
                AppendMenuW(hExport, MF_STRING, IDM_EXPORTTABLE, L"&Table...");
                AppendMenuW(hExport, MF_STRING, IDM_EXPORTHTML, L"&HTML Table...");
                AppendMenuW(hExport, MF_STRING, IDM_EXPORTDB, L"&Database...");
                AppendMenuW(hFile, MF_POPUP, (UINT)hExport, L"&Export");
                hImport = CreatePopupMenu();
                AppendMenuW(hImport, MF_STRING, IDM_IMPORTCSV, L"&CSV...");
                AppendMenuW(hImport, MF_STRING, IDM_IMPORTCEDB, L"CE &Database...");
                AppendMenuW(hFile, MF_POPUP, (UINT)hImport, L"&Import");
                AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
                AppendMenuW(hFile, MF_STRING, IDM_OPTIONS, L"&Options...");
                AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
                AppendMenuW(hFile, MF_STRING, IDM_ABOUT, L"&About SQLite/CEdit...");
                AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
                AppendMenuW(hFile, MF_STRING, IDM_EXIT, L"E&xit\tAlt+X");
            }
            AppendMenuW(hMenu, MF_POPUP, (UINT)hFile, L"&File");
            g_hFileMenu = hFile;
            
            /* Create all three context menus upfront */
            g_hQueryCtx = CreatePopupMenu();
            AppendMenuW(g_hQueryCtx, MF_STRING, IDM_EXECUTE, L"&Execute\tCtrl+Enter");
            AppendMenuW(g_hQueryCtx, MF_SEPARATOR, 0, NULL);
            AppendMenuW(g_hQueryCtx, MF_STRING, IDM_FIND, L"&Find...\tCtrl+F");
            AppendMenuW(g_hQueryCtx, MF_STRING, IDM_FINDNEXT, L"Find &Next\tF3");
            AppendMenuW(g_hQueryCtx, MF_STRING, IDM_REPLACE, L"&Replace...\tCtrl+H");
            
            g_hResultCtx = CreatePopupMenu();
            AppendMenuW(g_hResultCtx, MF_STRING, IDM_VIEWGRID, L"&Grid View\tCtrl+G");
            AppendMenuW(g_hResultCtx, MF_SEPARATOR, 0, NULL);
            AppendMenuW(g_hResultCtx, MF_STRING, IDM_EXPORTRESULTS, L"&Export Results...");
            AppendMenuW(g_hResultCtx, MF_STRING, IDM_EXPORTHTMLRES, L"Export &HTML...");
            AppendMenuW(g_hResultCtx, MF_SEPARATOR, 0, NULL);
            AppendMenuW(g_hResultCtx, MF_STRING, IDM_FIND, L"&Find...\tCtrl+F");
            AppendMenuW(g_hResultCtx, MF_STRING, IDM_FINDNEXT, L"Find &Next\tF3");
            
            {
                HMENU hSelObj = CreatePopupMenu();
                AppendMenuW(hSelObj, MF_STRING, IDM_SCHEMA_SELECT, L"&Select");
                AppendMenuW(hSelObj, MF_STRING, IDM_EXPORTDDL, L"Export &DDL...");
                AppendMenuW(hSelObj, MF_SEPARATOR, 0, NULL);
                AppendMenuW(hSelObj, MF_STRING, IDM_SCHEMA_DROP, L"&Drop");
                g_hSchemaCtx = CreatePopupMenu();
                AppendMenuW(g_hSchemaCtx, MF_POPUP, (UINT)hSelObj, L"Selected &Object");
                AppendMenuW(g_hSchemaCtx, MF_SEPARATOR, 0, NULL);
                AppendMenuW(g_hSchemaCtx, MF_STRING, IDM_REFRESH, L"&Refresh");
                AppendMenuW(g_hSchemaCtx, MF_STRING, IDM_EXPORTALLDDL, L"Export &All DDL...");
                AppendMenuW(g_hSchemaCtx, MF_SEPARATOR, 0, NULL);
                AppendMenuW(g_hSchemaCtx, MF_STRING, IDM_SHOWSIZES, L"Show &Details");
            }
            
            /* Add only Query context menu initially - others added on view switch */
            AppendMenuW(hMenu, MF_POPUP, (UINT)g_hQueryCtx, L"&Query");
            
            hView = CreatePopupMenu();
            AppendMenuW(hView, MF_STRING, IDM_VIEWQUERY, L"&Query\tCtrl+1, Esc");
            AppendMenuW(hView, MF_STRING, IDM_VIEWRESULT, L"&Results\tCtrl+2, F6");
            AppendMenuW(hView, MF_STRING, IDM_VIEWSCHEMA, L"&Schema\tCtrl+3, F7");
            g_hViewMenu = hView;
            AppendMenuW(hMenu, MF_POPUP, (UINT)hView, L"&View");
            
            g_hMenu = hMenu;
            CommandBar_InsertMenubarEx(g_hwndCB, NULL, (LPTSTR)hMenu, 0);
            
            /* Add toolbar bitmaps */
            CommandBar_AddBitmap(g_hwndCB, g_hInst, IDB_TOOLBAR, 11, 0, 0);
            CommandBar_AddBitmap(g_hwndCB, HINST_COMMCTRL, IDB_STD_SMALL_COLOR, 15, 0, 0);
            CommandBar_AddBitmap(g_hwndCB, HINST_COMMCTRL, IDB_VIEW_SMALL_COLOR, 12, 0, 0);
            
            memset(tbButtons, 0, sizeof(tbButtons));
            
            tbButtons[0].fsStyle = TBSTYLE_SEP;
            tbButtons[0].iBitmap = 8;
            
            tbButtons[1].iBitmap = TB_QUERY;
            tbButtons[1].idCommand = IDM_VIEWQUERY;
            tbButtons[1].fsState = TBSTATE_ENABLED | TBSTATE_CHECKED;
            tbButtons[1].fsStyle = TBSTYLE_CHECK | TBSTYLE_GROUP;
            
            tbButtons[2].iBitmap = TB_RESULTS;
            tbButtons[2].idCommand = IDM_VIEWRESULT;
            tbButtons[2].fsState = TBSTATE_ENABLED;
            tbButtons[2].fsStyle = TBSTYLE_CHECK | TBSTYLE_GROUP;
            
            tbButtons[3].iBitmap = TB_SCHEMA;
            tbButtons[3].idCommand = IDM_VIEWSCHEMA;
            tbButtons[3].fsState = TBSTATE_ENABLED;
            tbButtons[3].fsStyle = TBSTYLE_CHECK | TBSTYLE_GROUP;
            
            tbButtons[4].fsStyle = TBSTYLE_SEP;
            
            tbButtons[5].iBitmap = TB_EXECAT;
            tbButtons[5].idCommand = IDM_EXECATCURSOR;
            tbButtons[5].fsState = TBSTATE_ENABLED | (g_execAtCursor ? TBSTATE_CHECKED : 0);
            tbButtons[5].fsStyle = TBSTYLE_CHECK;
            
            tbButtons[6].fsStyle = TBSTYLE_SEP;
            
            tbButtons[7].iBitmap = TB_STD_BASE + STD_FIND;
            tbButtons[7].idCommand = IDM_FONTSIZE;
            tbButtons[7].fsState = TBSTATE_ENABLED;
            tbButtons[7].fsStyle = TBSTYLE_BUTTON;
            
            tbButtons[8].fsStyle = TBSTYLE_SEP;
            
            tbButtons[9].iBitmap = TB_OPEN;
            tbButtons[9].idCommand = IDM_OPEN;
            tbButtons[9].fsState = TBSTATE_ENABLED;
            tbButtons[9].fsStyle = TBSTYLE_BUTTON;
            
            tbButtons[10].iBitmap = TB_CLOSE;
            tbButtons[10].idCommand = IDM_CLOSE;
            tbButtons[10].fsState = TBSTATE_ENABLED;
            tbButtons[10].fsStyle = TBSTYLE_BUTTON;
            
            tbButtons[11].iBitmap = TB_NEW;
            tbButtons[11].idCommand = IDM_NEW;
            tbButtons[11].fsState = TBSTATE_ENABLED;
            tbButtons[11].fsStyle = TBSTYLE_BUTTON;
            
            tbButtons[12].fsStyle = TBSTYLE_SEP;
            
            tbButtons[13].iBitmap = TB_PLAY;
            tbButtons[13].idCommand = IDM_EXECUTE;
            tbButtons[13].fsState = TBSTATE_ENABLED;
            tbButtons[13].fsStyle = TBSTYLE_BUTTON;
            
            tbButtons[14].iBitmap = TB_STOP;
            tbButtons[14].idCommand = IDM_STOP;
            tbButtons[14].fsState = 0;
            tbButtons[14].fsStyle = TBSTYLE_BUTTON;
            
            CommandBar_AddButtons(g_hwndCB, 15, tbButtons);
            
            CommandBar_AddAdornments(g_hwndCB, CMDBAR_HELP, 0);
            cbHeight = CommandBar_Height(g_hwndCB);
            
            GetClientRect(hwnd, &rc);
            
            /* Status bar with two panes */
            g_hwndStatus = CreateWindowW(STATUSCLASSNAMEW, NULL,
                WS_CHILD | WS_VISIBLE,
                0, 0, 0, 0, hwnd, (HMENU)1003, g_hInst, NULL);
            {
                int parts[2] = {120, -1};
                RECT rcStatus;
                int sbHeight, editHeight, queryLeft;
                SendMessage(g_hwndStatus, SB_SETPARTS, 2, (LPARAM)parts);
                GetWindowRect(g_hwndStatus, &rcStatus);
                sbHeight = rcStatus.bottom - rcStatus.top;
                editHeight = rc.bottom - cbHeight - sbHeight;
                queryLeft = g_showLineNumbers ? g_lineNumWidth : 0;
            
            /* Line number gutter */
            g_hwndLineNum = CreateWindowW(
                L"EDIT", L"",
                WS_CHILD | WS_BORDER | (g_showLineNumbers ? WS_VISIBLE : 0) | ES_MULTILINE | ES_RIGHT,
                0, cbHeight, g_lineNumWidth, editHeight,
                hwnd, (HMENU)1004, g_hInst, NULL);
            SendMessage(g_hwndLineNum, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(0, 2));
            g_pfnLineNumProc = (WNDPROC)SetWindowLong(g_hwndLineNum, GWL_WNDPROC, (LONG)LineNumProc);
            
            /* Query input */
            g_hwndQuery = CreateWindowW(
                L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
                ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_NOHIDESEL,
                queryLeft, cbHeight, rc.right - queryLeft, editHeight,
                hwnd, (HMENU)1001, g_hInst, NULL);
            
            g_pfnQueryProc = (WNDPROC)SetWindowLong(g_hwndQuery, GWL_WNDPROC, (LONG)QueryEditProc);
            
            /* Results output */
            g_hwndResult = CreateWindowW(
                L"EDIT", L"",
                WS_CHILD | WS_BORDER | WS_VSCROLL | WS_HSCROLL |
                ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                0, cbHeight, rc.right, editHeight,
                hwnd, (HMENU)1002, g_hInst, NULL);
            
            /* Grid view */
            CreateGridView(hwnd, 0, cbHeight, rc.right, editHeight);
            
            /* Schema view */
            CreateSchemaView(hwnd, 0, cbHeight, rc.right, editHeight);
            }
            
            g_pfnResultProc = (WNDPROC)SetWindowLong(g_hwndResult, GWL_WNDPROC, (LONG)ResultEditProc);
            
            UpdateQueryFont();
            UpdateResultFont();
            
            g_viewMode = 0;
            CheckMenuRadioItem(g_hViewMenu, IDM_VIEWQUERY, IDM_VIEWSCHEMA, IDM_VIEWQUERY, MF_BYCOMMAND);
            UpdateLineNumbers();
            SendMessage(hwnd, WM_SIZE, 0, 0);
            
            return 0;
        }
        
        case WM_SIZE: {
            RECT rc, rcStatus;
            int cbHeight, sbHeight, editHeight, queryLeft;
            
            SendMessage(g_hwndStatus, WM_SIZE, 0, 0);
            GetWindowRect(g_hwndStatus, &rcStatus);
            sbHeight = rcStatus.bottom - rcStatus.top;
            
            cbHeight = CommandBar_Height(g_hwndCB);
            GetClientRect(hwnd, &rc);
            editHeight = rc.bottom - cbHeight - sbHeight;
            queryLeft = g_showLineNumbers ? g_lineNumWidth : 0;
            
            if (g_hwndLineNum)
                MoveWindow(g_hwndLineNum, 0, cbHeight, g_lineNumWidth, editHeight, TRUE);
            
            MoveWindow(g_hwndQuery, queryLeft, cbHeight, rc.right - queryLeft, editHeight, TRUE);
            MoveWindow(g_hwndResult, 0, cbHeight, rc.right, editHeight, TRUE);
            if (g_hwndGrid)
                MoveWindow(g_hwndGrid, 0, cbHeight, rc.right, editHeight, TRUE);
            if (g_hwndSchema)
                MoveWindow(g_hwndSchema, 0, cbHeight, rc.right, editHeight, TRUE);
            return 0;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDM_NEW:     DoFileNew(); break;
                case IDM_OPEN:    DoFileOpen(); break;
                case IDM_CLOSE:   CloseDatabase(); break;
                case IDM_BACKUP:  DoBackupDatabase(); break;
                case IDM_OPENQUERY: DoOpenQuery(); break;
                case IDM_SAVEQUERY: DoSaveQuery(); break;
                case IDM_EXPORTCSV: DoExportCSV(); break;
                case IDM_EXPORTTXT: DoExportTxt(); break;
                case IDM_EXPORTRESULTS: DoExportResults(); break;
                case IDM_EXPORTTABLE: DoExportTable(); break;
                case IDM_EXPORTHTML: DoExportHTML(); break;
                case IDM_EXPORTHTMLRES: DoExportHTMLResults(); break;
                case IDM_EXPORTDB: DoExportDb(); break;
                case IDM_IMPORTCSV: DoImportCSV(); break;
                case IDM_IMPORTCEDB: DoImportCEDB(); break;
                case IDM_EXIT:    SendMessage(hwnd, WM_CLOSE, 0, 0); break;
                case IDM_EXECUTE: ExecuteQuery(); break;
                case IDM_STOP: g_abortQuery = 1; break;
                case IDM_EXECATCURSOR:
                    if (g_viewMode == 0) {
                        /* Query view: toggle exec-at-cursor */
                        g_execAtCursor = !g_execAtCursor;
                        SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_EXECATCURSOR, g_execAtCursor);
                    } else if (g_viewMode == 1) {
                        /* Results view: toggle grid/text - but not in edit mode */
                        if (g_editMode) {
                            MessageBeep(MB_OK);  /* Beep to indicate locked */
                            break;
                        }
                        g_gridView = !g_gridView;
                        ShowWindow(g_hwndResult, g_gridView ? SW_HIDE : SW_SHOW);
                        if (g_hwndGrid) ShowWindow(g_hwndGrid, g_gridView ? SW_SHOW : SW_HIDE);
                        SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_EXECATCURSOR, g_gridView);
                        SetFocus(g_gridView ? g_hwndGrid : g_hwndResult);
                        if (g_gridView) PopulateGrid();
                    } else if (g_viewMode == 2) {
                        /* Schema view: toggle show details */
                        g_showSizes = !g_showSizes;
                        SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_EXECATCURSOR, g_showSizes);
                        ForceMenuRebuild();
                        RefreshSchema();
                    }
                    break;
                case IDM_FIND:    DoFind(); break;
                case IDM_FINDNEXT: DoFindNext(); break;
                case IDM_REPLACE: DoReplace(); break;
                case IDM_ABOUT:
                    DoAbout();
                    break;
                case IDM_HELP:
                    DoAbout();
                    break;
                case IDM_OPTIONS:
                    DoOptions();
                    break;
                case IDM_VIEWQUERY:
                    SwitchView(0);
                    SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_VIEWQUERY, TRUE);
                    SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_VIEWRESULT, FALSE);
                    SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_VIEWSCHEMA, FALSE);
                    CheckMenuRadioItem(g_hViewMenu, IDM_VIEWQUERY, IDM_VIEWSCHEMA, IDM_VIEWQUERY, MF_BYCOMMAND);
                    break;
                case IDM_VIEWRESULT:
                    SwitchView(1);
                    SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_VIEWQUERY, FALSE);
                    SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_VIEWRESULT, TRUE);
                    SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_VIEWSCHEMA, FALSE);
                    CheckMenuRadioItem(g_hViewMenu, IDM_VIEWQUERY, IDM_VIEWSCHEMA, IDM_VIEWRESULT, MF_BYCOMMAND);
                    break;
                case IDM_VIEWSCHEMA:
                    SwitchView(2);
                    SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_VIEWQUERY, FALSE);
                    SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_VIEWRESULT, FALSE);
                    SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_VIEWSCHEMA, TRUE);
                    CheckMenuRadioItem(g_hViewMenu, IDM_VIEWQUERY, IDM_VIEWSCHEMA, IDM_VIEWSCHEMA, MF_BYCOMMAND);
                    break;
                case IDM_VIEWGRID:
                    /* Toggle grid view - same as IDM_EXECATCURSOR in results mode */
                    if (g_viewMode == 1) {
                        SendMessage(hwnd, WM_COMMAND, IDM_EXECATCURSOR, 0);
                        ForceMenuRebuild();  /* Update checkmark */
                    }
                    break;
                case IDM_REFRESH:
                    if (g_viewMode == 2) RefreshSchema();
                    break;
                case IDM_SHOWSIZES:
                    g_showSizes = !g_showSizes;
                    ForceMenuRebuild();
                    RefreshSchema();
                    break;
                case IDM_SCHEMA_SELECT:
                    OnSchemaDoubleClick();
                    break;
                case IDM_SCHEMA_DROP:
                    OnSchemaDelete();
                    break;
                case IDM_EXPORTDDL:
                    ExportSelectedDDL();
                    break;
                case IDM_EXPORTALLDDL:
                    ExportAllDDL();
                    break;
                case IDM_FONTSIZE:
                    CycleFontSize();
                    break;
                case IDOK:        SendMessage(hwnd, WM_CLOSE, 0, 0); break;
                case 1001:
                    if (HIWORD(wParam) == EN_CHANGE && g_viewMode == 0) {
                        UpdateLineCount();
                        if (!g_showingHint) g_queryDirty = 1;
                    }
                    break;
                default:
                    /* Handle recent files */
                    if (LOWORD(wParam) >= IDM_RECENT_BASE && LOWORD(wParam) <= IDM_RECENT_MAX) {
                        int idx = LOWORD(wParam) - IDM_RECENT_BASE;
                        if (idx < g_recentCount && g_recentFiles[idx][0])
                            OpenDatabase(g_recentFiles[idx]);
                    }
                    /* Handle recent queries */
                    if (LOWORD(wParam) >= IDM_RECENT_QUERY_BASE && LOWORD(wParam) <= IDM_RECENT_QUERY_MAX) {
                        int idx = LOWORD(wParam) - IDM_RECENT_QUERY_BASE;
                        if (idx < g_recentQueryCount && g_recentQueries[idx][0])
                            OpenQueryFile(g_recentQueries[idx]);
                    }
                    break;
            }
            return 0;
        
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC:
            if ((HWND)lParam == g_hwndLineNum || (HWND)lParam == g_hwndQuery || (HWND)lParam == g_hwndResult) {
                SetBkColor((HDC)wParam, RGB(255, 255, 255));
                return (LRESULT)g_hBrushWhite;
            }
            break;
        
        case WM_SYSKEYDOWN:
            if (wParam == 'X' && GetKeyState(VK_MENU) < 0) {
                SendMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            break;
        
        case WM_KEYDOWN:
            if (GetKeyState(VK_CONTROL) < 0) {
                if (wParam == 'O') {
                    if ((GetWindowTextLengthW(g_hwndQuery) == 0 || g_showingHint) &&
                        lstrcmpW(g_szDbPath, L":memory:") == 0)
                        DoFileOpen();
                    else
                        DoOpenQuery();
                    return 0;
                }
                if (wParam == 'N') {
                    if ((GetWindowTextLengthW(g_hwndQuery) == 0 || g_showingHint) &&
                        lstrcmpW(g_szDbPath, L":memory:") == 0)
                        DoFileNew();
                    else
                        DoNewQuery();
                    return 0;
                }
                if (wParam == 'C') { g_abortQuery = 1; return 0; }
                if (wParam == 'F') { DoFind(); return 0; }
                if (wParam == 'H') { DoReplace(); return 0; }
                if (wParam == '1') { SendMessage(hwnd, WM_COMMAND, IDM_VIEWQUERY, 0); return 0; }
                if (wParam == '2') { SendMessage(hwnd, WM_COMMAND, IDM_VIEWRESULT, 0); return 0; }
                if (wParam == '3') { SendMessage(hwnd, WM_COMMAND, IDM_VIEWSCHEMA, 0); return 0; }
            }
            if (wParam == VK_F5) { ExecuteQuery(); return 0; }
            if (wParam == VK_F6) { SendMessage(hwnd, WM_COMMAND, IDM_VIEWRESULT, 0); return 0; }
            if (wParam == VK_F7) { SendMessage(hwnd, WM_COMMAND, IDM_VIEWSCHEMA, 0); return 0; }
            if (wParam == VK_UP || wParam == VK_DOWN || wParam == VK_LEFT || wParam == VK_RIGHT) {
                if (g_viewMode == 0) SetFocus(g_hwndQuery);
                else if (g_viewMode == 1) SetFocus(g_gridView ? g_hwndGrid : g_hwndResult);
                else if (g_viewMode == 2 && g_hwndSchema) SetFocus(g_hwndSchema);
                return 0;
            }
            if (wParam == VK_RETURN && g_viewMode != 2) {
                if (g_viewMode == 0) SetFocus(g_hwndQuery);
                else if (g_viewMode == 1) SetFocus(g_gridView ? g_hwndGrid : g_hwndResult);
                return 0;
            }
            break;
        
        case WM_NOTIFY: {
            NMHDR *pnm = (NMHDR*)lParam;
            if (pnm->hwndFrom == g_hwndGrid && pnm->code == LVN_GETDISPINFOW) {
                OnGridGetDispInfo((NMLVDISPINFOW*)lParam);
                return 0;
            }
            if (pnm->hwndFrom == g_hwndGrid && pnm->code == LVN_COLUMNCLICK) {
                NMLISTVIEW *pnmlv = (NMLISTVIEW*)lParam;
                OnGridColumnClick(pnmlv->iSubItem);
                return 0;
            }
            if (pnm->hwndFrom == g_hwndGrid && pnm->code == NM_DBLCLK) {
                /* Double-click on grid cell - start edit if in edit mode */
                NMLISTVIEW *pnmlv = (NMLISTVIEW*)lParam;
                OnGridDoubleClick(pnmlv->iItem, pnmlv->iSubItem);
                return 0;
            }
            /* Header double-click to auto-size column */
            if (pnm->code == HDN_ITEMDBLCLICKW || pnm->code == HDN_ITEMDBLCLICKA) {
                NMHEADERW *phdr = (NMHEADERW*)lParam;
                if (GetParent(pnm->hwndFrom) == g_hwndGrid) {
                    int col = phdr->iItem;
                    int contentWidth, headerWidth;
                    ListView_SetColumnWidth(g_hwndGrid, col, LVSCW_AUTOSIZE);
                    contentWidth = ListView_GetColumnWidth(g_hwndGrid, col);
                    ListView_SetColumnWidth(g_hwndGrid, col, LVSCW_AUTOSIZE_USEHEADER);
                    headerWidth = ListView_GetColumnWidth(g_hwndGrid, col);
                    if (contentWidth > headerWidth)
                        ListView_SetColumnWidth(g_hwndGrid, col, contentWidth);
                    return 0;
                }
            }
            if (pnm->hwndFrom == g_hwndSchema) {
                if (pnm->code == TVN_ITEMEXPANDINGW) {
                    OnSchemaExpanding((NMTREEVIEWW*)lParam);
                } else if (pnm->code == NM_DBLCLK) {
                    OnSchemaDoubleClick();
                } else if (pnm->code == TVN_KEYDOWN) {
                    TV_KEYDOWN *pKey = (TV_KEYDOWN*)lParam;
                    if (pKey->wVKey == VK_DELETE) {
                        OnSchemaDelete();
                        return TRUE;
                    } else if (pKey->wVKey == VK_RETURN) {
                        OnSchemaDoubleClick();
                        return TRUE;
                    } else if (pKey->wVKey == VK_ESCAPE) {
                        SendMessage(hwnd, WM_COMMAND, IDM_VIEWQUERY, 0);
                        return TRUE;
                    } else if (GetKeyState(VK_CONTROL) < 0) {
                        if (pKey->wVKey == '1') SendMessage(hwnd, WM_COMMAND, IDM_VIEWQUERY, 0);
                        else if (pKey->wVKey == '2') SendMessage(hwnd, WM_COMMAND, IDM_VIEWRESULT, 0);
                        else if (pKey->wVKey == '3') SendMessage(hwnd, WM_COMMAND, IDM_VIEWSCHEMA, 0);
                        return TRUE;
                    } else if (pKey->wVKey == VK_F5) {
                        ExecuteQuery();
                        return TRUE;
                    } else if (pKey->wVKey == VK_F6) {
                        SendMessage(hwnd, WM_COMMAND, IDM_VIEWRESULT, 0);
                        return TRUE;
                    } else if (pKey->wVKey == VK_F7) {
                        SendMessage(hwnd, WM_COMMAND, IDM_VIEWSCHEMA, 0);
                        return TRUE;
                    }
                }
            }
            break;
        }
        
        case WM_INITMENUPOPUP: {
            HMENU hMenu = (HMENU)wParam;
            /* Check if this is the Selected Object submenu */
            if (g_viewMode == 2) {
                int objType = GetSelectedObjectType();
                int canSelect = (objType == 1 || objType == 4);  /* TABLE or VIEW */
                int canDrop = (objType >= 0);  /* TABLE, VIEW, or TRIGGER */
                EnableMenuItem(hMenu, IDM_SCHEMA_SELECT, canSelect ? MF_ENABLED : MF_GRAYED);
                EnableMenuItem(hMenu, IDM_EXPORTDDL, canDrop ? MF_ENABLED : MF_GRAYED);
                EnableMenuItem(hMenu, IDM_SCHEMA_DROP, canDrop ? MF_ENABLED : MF_GRAYED);
            }
            break;
        }
        
        case WM_HELP:
            DoAbout();
            return TRUE;
        
        case WM_CLOSE:
            if (g_queryDirty) {
                int r = MessageBoxW(hwnd, L"Save changes to query?", L"SQLite/CE", 
                                    MB_YESNOCANCEL | MB_ICONQUESTION);
                if (r == IDCANCEL) return 0;
                if (r == IDYES) DoSaveQuery();
            }
            SaveSettings();
            DestroyWindow(hwnd);
            return 0;
        
        case WM_DESTROY:
            CloseDatabase();
            if (g_hFontQuery) DeleteObject(g_hFontQuery);
            if (g_hFontResult) DeleteObject(g_hFontResult);
            if (g_hBrushWhite) DeleteObject(g_hBrushWhite);
            CommandBar_Destroy(g_hwndCB);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/*============================================================================
** Entry Point
**============================================================================*/

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR lpCmd, int nShow) {
    WNDCLASSW wc = {0};
    MSG msg;
    RECT rcWork;
    ACCEL accel[15];
    int nAccel = 0;
    
    (void)hPrev; (void)lpCmd; (void)nShow;
    
    g_hInst = hInst;
    InitCommonControls();
    LoadSettings();
    
    /* Build accelerator table */
    accel[nAccel].fVirt = FCONTROL | FVIRTKEY; accel[nAccel].key = 'O'; accel[nAccel].cmd = IDM_OPENQUERY; nAccel++;
    accel[nAccel].fVirt = FCONTROL | FVIRTKEY; accel[nAccel].key = 'S'; accel[nAccel].cmd = IDM_SAVEQUERY; nAccel++;
    accel[nAccel].fVirt = FCONTROL | FVIRTKEY; accel[nAccel].key = 'F'; accel[nAccel].cmd = IDM_FIND; nAccel++;
    accel[nAccel].fVirt = FCONTROL | FVIRTKEY; accel[nAccel].key = 'H'; accel[nAccel].cmd = IDM_REPLACE; nAccel++;
    accel[nAccel].fVirt = FCONTROL | FVIRTKEY; accel[nAccel].key = '1'; accel[nAccel].cmd = IDM_VIEWQUERY; nAccel++;
    accel[nAccel].fVirt = FCONTROL | FVIRTKEY; accel[nAccel].key = '2'; accel[nAccel].cmd = IDM_VIEWRESULT; nAccel++;
    accel[nAccel].fVirt = FCONTROL | FVIRTKEY; accel[nAccel].key = '3'; accel[nAccel].cmd = IDM_VIEWSCHEMA; nAccel++;
    accel[nAccel].fVirt = FVIRTKEY; accel[nAccel].key = VK_F3; accel[nAccel].cmd = IDM_FINDNEXT; nAccel++;
    accel[nAccel].fVirt = FVIRTKEY; accel[nAccel].key = VK_F5; accel[nAccel].cmd = IDM_EXECUTE; nAccel++;
    accel[nAccel].fVirt = FVIRTKEY; accel[nAccel].key = VK_F6; accel[nAccel].cmd = IDM_VIEWRESULT; nAccel++;
    accel[nAccel].fVirt = FVIRTKEY; accel[nAccel].key = VK_F7; accel[nAccel].cmd = IDM_VIEWSCHEMA; nAccel++;
    accel[nAccel].fVirt = FALT | FVIRTKEY; accel[nAccel].key = 'X'; accel[nAccel].cmd = IDM_EXIT; nAccel++;
    g_hAccel = CreateAcceleratorTableW(accel, nAccel);
    
    SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWork, 0);
    
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_MAIN));
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"SQLiteCEdit";
    RegisterClassW(&wc);
    
    g_hwndMain = CreateWindowW(L"SQLiteCEdit", L"SQLite/CE",
        WS_VISIBLE,
        rcWork.left, rcWork.top,
        rcWork.right - rcWork.left, rcWork.bottom - rcWork.top,
        NULL, NULL, hInst, NULL);
    
    SendMessage(g_hwndMain, WM_SETICON, ICON_SMALL, 
        (LPARAM)LoadImage(hInst, MAKEINTRESOURCE(IDI_MAIN), IMAGE_ICON, 16, 16, 0));
    
    /* Update menus with loaded recent files */
    UpdateRecentMenu();
    UpdateRecentQueryMenu();
    
    OpenDatabase(L":memory:");
    
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAccelerator(g_hwndMain, g_hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    if (g_hAccel) DestroyAcceleratorTable(g_hAccel);
    return (int)msg.wParam;
}
