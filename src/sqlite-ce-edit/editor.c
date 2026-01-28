/*
** SQLiteCEdit - Editor pane management
*/

#include "globals.h"

/*
** Centralized keyboard handler for app-wide shortcuts.
** Call from each subclass proc; returns 1 if key was handled.
*/
int HandleGlobalKeys(UINT msg, WPARAM wParam) {
    int ctrl = GetKeyState(VK_CONTROL) < 0;
    int alt = GetKeyState(VK_MENU) < 0;
    
    if (msg == WM_SYSKEYDOWN || msg == WM_SYSCHAR) {
        if ((wParam == 'X' || wParam == 'x') && alt) {
            if (msg == WM_SYSKEYDOWN)
                SendMessage(g_hwndMain, WM_CLOSE, 0, 0);
            return 1;
        }
        if ((wParam == VK_RETURN || wParam == '\r') && alt) {
            if (msg == WM_SYSKEYDOWN)
                SendMessage(g_hwndMain, WM_COMMAND, IDM_FULLSCREEN, 0);
            return 1;
        }
    }
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_ESCAPE && g_fullScreen) {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_FULLSCREEN, 0);
            return 1;
        }
        if (ctrl) {
            if (wParam == '1') { SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWQUERY, 0); return 1; }
            if (wParam == '2') { SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWRESULT, 0); return 1; }
            if (wParam == '3') { SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWSCHEMA, 0); return 1; }
            if (wParam == 'G' && g_viewMode == 0) { DoGotoLine(); return 1; }
        }
        if (wParam == VK_F5) { ExecuteQuery(); return 1; }
        if (wParam == VK_F6) { SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWRESULT, 0); return 1; }
        if (wParam == VK_F7) { SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWSCHEMA, 0); return 1; }
    }
    return 0;
}

/* Line count cache - file scope for reset access */
static DWORD s_lastSel = (DWORD)-1;
static int s_lastTotal = -1;

void ResetLineCountCache(void) {
    s_lastSel = (DWORD)-1;
    s_lastTotal = -1;
}

void UpdateLineCount(void) {
    wchar_t buf[32];
    DWORD sel;
    int cur, total, textLen, i;
    wchar_t *text;
    if (g_suppressLineCount) {
        g_suppressLineCount--;
        return;
    }
    SendMessage(g_hwndQuery, EM_GETSEL, (WPARAM)&sel, 0);
    textLen = GetWindowTextLengthW(g_hwndQuery);
    
    /* Count logical lines by counting newlines */
    cur = 1;
    total = 1;
    if (textLen > 0) {
        text = (wchar_t*)LocalAlloc(LMEM_FIXED, (textLen + 1) * sizeof(wchar_t));
        if (text) {
            GetWindowTextW(g_hwndQuery, text, textLen + 1);
            for (i = 0; i < textLen; i++) {
                if (text[i] == '\n') {
                    total++;
                    if (i < (int)sel) cur++;
                }
            }
            LocalFree(text);
        }
    }
    /* Only update status bar if cursor moved or line count changed */
    if (sel != s_lastSel || total != s_lastTotal) {
        s_lastSel = sel;
        s_lastTotal = total;
        wsprintfW(buf, L"Ln %d of %d", cur, total);
        SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)buf);
    }
}

void UpdateLineNumbers(void) {
    static wchar_t *cachedText = NULL;
    static int cachedLen = -1;
    wchar_t buf[4096];
    wchar_t *text = NULL;
    int i, visLines, firstVisible, pos = 0, textLen;
    int charIdx, logicalLine;
    if (!g_showLineNumbers || !g_hwndLineNum || !g_hFontQuery) return;
    
    visLines = (int)SendMessage(g_hwndQuery, EM_GETLINECOUNT, 0, 0);
    textLen = GetWindowTextLengthW(g_hwndQuery);
    
    /* Use cached text if length unchanged */
    if (textLen == cachedLen && cachedText) {
        text = cachedText;
    } else {
        /* Text changed - re-fetch and cache */
        if (cachedText) { LocalFree(cachedText); cachedText = NULL; }
        cachedLen = textLen;
        if (textLen > 0) {
            cachedText = (wchar_t*)LocalAlloc(LMEM_FIXED, (textLen + 1) * sizeof(wchar_t));
            if (cachedText) GetWindowTextW(g_hwndQuery, cachedText, textLen + 1);
        }
        text = cachedText;
    }
    
    /* Count logical lines for gutter width */
    {
        int logicalTotal = 1;
        if (text) {
            for (i = 0; i < textLen; i++)
                if (text[i] == '\n') logicalTotal++;
        }
        /* Auto-size gutter width */
        {
            HDC hdc = GetDC(g_hwndLineNum);
            HFONT hOld = (HFONT)SelectObject(hdc, g_hFontQuery);
            SIZE sz;
            wchar_t numBuf[16];
            int newWidth;
            wsprintfW(numBuf, L"%d", logicalTotal);
            GetTextExtentPoint32W(hdc, numBuf, lstrlenW(numBuf), &sz);
            newWidth = sz.cx + 10;
            if (newWidth < 20) newWidth = 20;
            SelectObject(hdc, hOld);
            ReleaseDC(g_hwndLineNum, hdc);
            if (newWidth != g_lineNumWidth) {
                g_lineNumWidth = newWidth;
                SendMessage(g_hwndMain, WM_SIZE, 0, 0);
                UpdateWindow(g_hwndMain);
            }
        }
    }
    
    firstVisible = (int)SendMessage(g_hwndQuery, EM_GETFIRSTVISIBLELINE, 0, 0);
    
    /* Count logical line number at first visible line */
    charIdx = (int)SendMessage(g_hwndQuery, EM_LINEINDEX, firstVisible, 0);
    logicalLine = 1;
    if (text) {
        for (i = 0; i < charIdx && i < textLen; i++)
            if (text[i] == '\n') logicalLine++;
    }
    
    /* For each visible line, check if it starts a new logical line */
    for (i = firstVisible; i < visLines && pos < 4000; i++) {
        charIdx = (int)SendMessage(g_hwndQuery, EM_LINEINDEX, i, 0);
        if (i == 0 || (text && charIdx > 0 && text[charIdx - 1] == '\n')) {
            /* Start of logical line */
            pos += wsprintfW(buf + pos, L"%d\r\n", logicalLine);
            logicalLine++;
        } else {
            /* Wrapped continuation - blank */
            pos += wsprintfW(buf + pos, L"\r\n");
        }
    }
    buf[pos] = 0;
    SetWindowTextW(g_hwndLineNum, buf);
}

void SyncLineNumScroll(void) {
    if (!g_showLineNumbers || !g_hwndLineNum) return;
    UpdateLineNumbers();
}

static int g_lastMenuMode = 0;  /* Start with Query menu (mode 0) */

static void UpdateContextMenu(int mode) {
    HMENU hNewMenu, hCtx, hOldMenu;
    
    /* Only update if mode changed */
    if (mode == g_lastMenuMode) return;
    
    /* Get old menu - we'll detach submenus before destroying */
    hOldMenu = CommandBar_GetMenu(g_hwndCB, 0);
    
    /* Detach reusable submenus from old menu so DestroyMenu won't kill them */
    if (hOldMenu) {
        RemoveMenu(hOldMenu, 0, MF_BYPOSITION);  /* File */
        RemoveMenu(hOldMenu, 0, MF_BYPOSITION);  /* Edit */
        RemoveMenu(hOldMenu, 0, MF_BYPOSITION);  /* Context (Query/Results/Schema) */
        RemoveMenu(hOldMenu, 0, MF_BYPOSITION);  /* View */
    }
    
    /* Build a fresh menu bar */
    hNewMenu = CreateMenu();
    AppendMenuW(hNewMenu, MF_POPUP, (UINT)g_hFileMenu, L"&File");
    AppendMenuW(hNewMenu, MF_POPUP, (UINT)g_hEditMenu, L"&Edit");
    
    /* Add only the active context menu */
    if (mode == 0) {
        hCtx = g_hQueryCtx;
        CheckMenuItem(hCtx, IDM_EXECATCURSOR, g_execAtCursor ? MF_CHECKED : MF_UNCHECKED);
        AppendMenuW(hNewMenu, MF_POPUP, (UINT)hCtx, L"&Query");
    } else if (mode == 1) {
        hCtx = g_hResultCtx;
        CheckMenuItem(hCtx, IDM_VIEWGRID, g_gridView ? MF_CHECKED : MF_UNCHECKED);
        AppendMenuW(hNewMenu, MF_POPUP, (UINT)hCtx, L"&Results");
    } else {
        hCtx = g_hSchemaCtx;
        CheckMenuItem(hCtx, IDM_SHOWSIZES, g_showSizes ? MF_CHECKED : MF_UNCHECKED);
        AppendMenuW(hNewMenu, MF_POPUP, (UINT)hCtx, L"&Schema");
    }
    
    AppendMenuW(hNewMenu, MF_POPUP, (UINT)g_hViewMenu, L"&View");
    CheckMenuRadioItem(g_hViewMenu, IDM_VIEWQUERY, IDM_VIEWSCHEMA,
        mode == 0 ? IDM_VIEWQUERY : (mode == 1 ? IDM_VIEWRESULT : IDM_VIEWSCHEMA), MF_BYCOMMAND);
    
    /* Update Edit menu state - basic state on view switch, WM_INITMENUPOPUP refines */
    EnableMenuItem(g_hEditMenu, IDM_REPLACE, (mode == VIEW_QUERY) ? MF_ENABLED : MF_GRAYED);
    
    /* Remove old menu from CommandBar (index 0) */
    SendMessage(g_hwndCB, TB_DELETEBUTTON, 0, 0);
    
    /* Insert new menu */
    CommandBar_InsertMenubarEx(g_hwndCB, NULL, (LPTSTR)hNewMenu, 0);
    
    /* Force CommandBar to recalculate layout */
    CommandBar_DrawMenuBar(g_hwndCB, 0);
    
    /* Now safe to destroy old shell (submenus already detached) */
    if (hOldMenu) DestroyMenu(hOldMenu);
    
    g_lastMenuMode = mode;
    g_hMenu = hNewMenu;
}

void ForceMenuRebuild(void) {
    g_lastMenuMode = -1;
    UpdateContextMenu(g_viewMode);
}

void SwitchView(int mode) {
    g_viewMode = mode;
    ShowWindow(g_hwndQuery, mode == VIEW_QUERY ? SW_SHOW : SW_HIDE);
    if (g_hwndLineNum) ShowWindow(g_hwndLineNum, (mode == VIEW_QUERY && g_showLineNumbers) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndResult, mode == VIEW_RESULT && !g_gridView ? SW_SHOW : SW_HIDE);
    if (g_hwndGrid) ShowWindow(g_hwndGrid, mode == VIEW_RESULT && g_gridView ? SW_SHOW : SW_HIDE);
    if (g_hwndSchema) ShowWindow(g_hwndSchema, mode == VIEW_SCHEMA ? SW_SHOW : SW_HIDE);
    
    /* Update context menu first, before toolbar changes */
    UpdateContextMenu(mode);
    
    /* Update view toggle buttons */
    SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_VIEWQUERY, mode == VIEW_QUERY);
    SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_VIEWRESULT, mode == VIEW_RESULT);
    SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_VIEWSCHEMA, mode == VIEW_SCHEMA);
    
    /* Swap button bitmap/state based on view mode */
    if (mode == VIEW_QUERY) {
        /* Query view: exec-at-cursor toggle */
        SendMessage(g_hwndCB, TB_CHANGEBITMAP, IDM_EXECATCURSOR, TB_EXECAT);
        SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_EXECATCURSOR, TRUE);
        SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_EXECATCURSOR, g_execAtCursor);
    } else if (mode == VIEW_RESULT) {
        /* Results view: grid toggle - checked = grid mode */
        /* Disabled in edit mode (can't switch away from grid while editing table) */
        SendMessage(g_hwndCB, TB_CHANGEBITMAP, IDM_EXECATCURSOR, TB_GRID);
        SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_EXECATCURSOR, !g_editMode && g_lastResultRows > 0);
        SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_EXECATCURSOR, g_gridView);
    } else {
        /* Schema view: show details toggle - checked = details shown */
        SendMessage(g_hwndCB, TB_CHANGEBITMAP, IDM_EXECATCURSOR, TB_DETAIL);
        SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_EXECATCURSOR, TRUE);
        SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_EXECATCURSOR, g_showSizes);
    }
    
    if (mode == VIEW_QUERY) {
        SetFocus(g_hwndQuery);
        UpdateLineCount();
        UpdateLineNumbers();
    } else if (mode == VIEW_RESULT) {
        SetFocus(g_gridView ? g_hwndGrid : g_hwndResult);
        SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)g_lastResultStatus);
    } else if (mode == VIEW_SCHEMA) {
        wchar_t statusBuf[64];
        SetFocus(g_hwndSchema);
        RefreshSchema();
        GetSchemaStatus(statusBuf, 64);
        SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)statusBuf);
    }
}

void UpdateQueryFont(void) {
    HFONT hOld = g_hFontQuery;
    LOGFONTW lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = g_fontSizes[g_fontSizeQuery];
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    lstrcpyW(lf.lfFaceName, L"Courier New");
    g_hFontQuery = CreateFontIndirectW(&lf);
    SendMessage(g_hwndQuery, WM_SETFONT, (WPARAM)g_hFontQuery, TRUE);
    if (g_hwndLineNum)
        SendMessage(g_hwndLineNum, WM_SETFONT, (WPARAM)g_hFontQuery, TRUE);
    if (hOld) DeleteObject(hOld);
}

void UpdateResultFont(void) {
    HFONT hOld = g_hFontResult;
    HFONT hOldGrid = g_hFontGrid;
    LOGFONTW lf;
    
    /* Text results - Courier New */
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = g_fontSizes[g_fontSizeResult];
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    lstrcpyW(lf.lfFaceName, L"Courier New");
    g_hFontResult = CreateFontIndirectW(&lf);
    SendMessage(g_hwndResult, WM_SETFONT, (WPARAM)g_hFontResult, TRUE);
    
    /* Grid - Tahoma */
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = g_fontSizes[g_fontSizeResult];
    lstrcpyW(lf.lfFaceName, L"Tahoma");
    g_hFontGrid = CreateFontIndirectW(&lf);
    if (g_hwndGrid)
        SendMessage(g_hwndGrid, WM_SETFONT, (WPARAM)g_hFontGrid, TRUE);
    
    if (hOld) DeleteObject(hOld);
    if (hOldGrid) DeleteObject(hOldGrid);
}

void UpdateSchemaFont(void) {
    HFONT hOld = g_hFontSchema;
    LOGFONTW lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = g_fontSizes[g_fontSizeSchema];
    lstrcpyW(lf.lfFaceName, L"Tahoma");
    g_hFontSchema = CreateFontIndirectW(&lf);
    if (g_hwndSchema)
        SendMessage(g_hwndSchema, WM_SETFONT, (WPARAM)g_hFontSchema, TRUE);
    if (hOld) DeleteObject(hOld);
}

void CycleFontSize(void) {
    if (g_viewMode == 0) {
        g_fontSizeQuery = (g_fontSizeQuery + 1) % 4;
        UpdateQueryFont();
        SendMessage(g_hwndQuery, EM_SCROLLCARET, 0, 0);
        UpdateLineNumbers();
    } else if (g_viewMode == 1) {
        g_fontSizeResult = (g_fontSizeResult + 1) % 4;
        UpdateResultFont();
        if (g_gridView) PopulateGrid();
    } else if (g_viewMode == 2) {
        g_fontSizeSchema = (g_fontSizeSchema + 1) % 4;
        UpdateSchemaFont();
    }
}

/* Subclass proc for line number gutter - blocks all input */
LRESULT CALLBACK LineNumProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CHAR || msg == WM_KEYDOWN || msg == WM_LBUTTONDOWN || 
        msg == WM_RBUTTONDOWN || msg == WM_LBUTTONDBLCLK)
        return 0;
    return CallWindowProc(g_pfnLineNumProc, hwnd, msg, wParam, lParam);
}

/* Subclass proc for query edit - catches Ctrl+Enter */
LRESULT CALLBACK QueryEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    /* Global shortcuts first */
    if (HandleGlobalKeys(msg, wParam))
        return 0;
    /* Clear hint on focus */
    if (msg == WM_SETFOCUS && g_showingHint) {
        SetWindowTextW(hwnd, L"");
        g_showingHint = 0;
        UpdateLineCount();
    }
    /* Clear search mode on click */
    if (msg == WM_LBUTTONDOWN)
        g_searchMode = 0;
    if (msg == WM_KEYDOWN) {
        int ctrl = GetKeyState(VK_CONTROL) < 0;
        /* Arrow keys exit search mode */
        if (wParam == VK_UP || wParam == VK_DOWN || wParam == VK_LEFT || wParam == VK_RIGHT)
            g_searchMode = 0;
        /* Enter in search mode = Find Next */
        if (wParam == VK_RETURN && !ctrl && g_searchMode) {
            DoFindNext();
            return 0;
        }
        /* Ctrl+Enter, Ctrl+E, or F5 - Execute */
        if ((wParam == VK_RETURN && ctrl) || (wParam == 'E' && ctrl) || wParam == VK_F5) {
            ExecuteQuery();
            return 0;
        }
        /* Ctrl+O - Open Query (or Database if editor empty) */
        if (ctrl && wParam == 'O') {
            if ((GetWindowTextLengthW(g_hwndQuery) == 0 || g_showingHint) &&
                lstrcmpW(g_szDbPath, L":memory:") == 0)
                DoFileOpen();
            else
                DoOpenQuery();
            return 0;
        }
        /* Ctrl+N - New Query (or New Database if editor empty and :memory:) */
        if (ctrl && wParam == 'N') {
            if ((GetWindowTextLengthW(g_hwndQuery) == 0 || g_showingHint) &&
                lstrcmpW(g_szDbPath, L":memory:") == 0)
                SendMessage(g_hwndMain, WM_COMMAND, IDM_NEW, 0);
            else
                DoNewQuery();
            return 0;
        }
        /* Ctrl+S - Save Query */
        if (ctrl && wParam == 'S') {
            DoSaveQuery();
            return 0;
        }
        /* Ctrl+F - Find, Ctrl+H - Replace, F3 - Find Next */
        if (ctrl && wParam == 'F') {
            DoFind();
            return 0;
        }
        if (ctrl && wParam == 'H') {
            DoReplace();
            return 0;
        }
        if (wParam == VK_F3) {
            DoFindNext();
            return 0;
        }
        /* Ctrl+A - Select all (CE edit control may not support natively) */
        if (ctrl && wParam == 'A') {
            SendMessage(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
        /* Up arrow on line 1 - go to start of buffer */
        if (wParam == VK_UP) {
            DWORD sel;
            SendMessage(hwnd, EM_GETSEL, (WPARAM)&sel, 0);
            if (SendMessage(hwnd, EM_LINEFROMCHAR, sel, 0) == 0) {
                SendMessage(hwnd, EM_SETSEL, 0, 0);
                return 0;
            }
        }
        /* Home - go to start of buffer */
        if (wParam == VK_HOME) {
            SendMessage(hwnd, EM_SETSEL, 0, 0);
            SendMessage(hwnd, EM_SCROLLCARET, 0, 0);
            return 0;
        }
        /* End - go to end of buffer */
        if (wParam == VK_END) {
            int len = GetWindowTextLengthW(hwnd);
            SendMessage(hwnd, EM_SETSEL, len, len);
            SendMessage(hwnd, EM_SCROLLCARET, 0, 0);
            return 0;
        }
        /* Ctrl+C/V/X - pass through for edit operations */
        if (ctrl && (wParam == 'C' || wParam == 'V' || wParam == 'X'))
            return CallWindowProc(g_pfnQueryProc, hwnd, msg, wParam, lParam);
    }
    /* Update line count on keyup and scroll caret into view for navigation keys */
    if (msg == WM_KEYUP) {
        UpdateLineCount();
        UpdateLineNumbers();
        if (wParam == VK_PRIOR || wParam == VK_NEXT || wParam == VK_HOME || wParam == VK_END)
            SendMessage(hwnd, EM_SCROLLCARET, 0, 0);
    }
    if (msg == WM_LBUTTONUP) {
        UpdateLineCount();
        UpdateLineNumbers();
    }
    if (msg == WM_VSCROLL) {
        LRESULT r = CallWindowProc(g_pfnQueryProc, hwnd, msg, wParam, lParam);
        SyncLineNumScroll();
        return r;
    }
    /* Clear search mode and suppress beeps on typing */
    if (msg == WM_CHAR) {
        if (GetKeyState(VK_CONTROL) < 0) {
            /* Ctrl+V=22 - paste, then scroll caret into view */
            if (wParam == 22) {
                LRESULT r = CallWindowProc(g_pfnQueryProc, hwnd, msg, wParam, lParam);
                SendMessage(hwnd, EM_SCROLLCARET, 0, 0);
                UpdateLineCount();
                UpdateLineNumbers();
                return r;
            }
            /* Ctrl+C=3, Ctrl+X=24 - pass through */
            if (wParam == 3 || wParam == 24)
                return CallWindowProc(g_pfnQueryProc, hwnd, msg, wParam, lParam);
            return 0;
        }
        /* Suppress Enter in search mode (handled in WM_KEYDOWN) */
        if (g_searchMode && (wParam == '\r' || wParam == '\n'))
            return 0;
        g_searchMode = 0;  /* Any typing exits search mode */
    }
    return CallWindowProc(g_pfnQueryProc, hwnd, msg, wParam, lParam);
}

/* Subclass proc for result edit - blocks input but allows copy */
LRESULT CALLBACK ResultEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    /* Global shortcuts first */
    if (HandleGlobalKeys(msg, wParam))
        return 0;
    if (msg == WM_KEYDOWN) {
        int ctrl = GetKeyState(VK_CONTROL) < 0;
        /* Allow navigation keys through */
        if (wParam == VK_UP || wParam == VK_DOWN || wParam == VK_LEFT || wParam == VK_RIGHT ||
            wParam == VK_PRIOR || wParam == VK_NEXT || wParam == VK_HOME || wParam == VK_END)
            return CallWindowProc(g_pfnResultProc, hwnd, msg, wParam, lParam);
        /* Ctrl+A - Select all */
        if (ctrl && wParam == 'A') {
            SendMessage(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
        /* Ctrl+C - Copy (pass through) */
        if (ctrl && wParam == 'C')
            return CallWindowProc(g_pfnResultProc, hwnd, msg, wParam, lParam);
        /* Ctrl+E - Execute */
        if (ctrl && wParam == 'E') {
            ExecuteQuery();
            return 0;
        }
        /* Ctrl+O - Open Query (or Database if editor empty) */
        if (ctrl && wParam == 'O') {
            if ((GetWindowTextLengthW(g_hwndQuery) == 0 || g_showingHint) &&
                lstrcmpW(g_szDbPath, L":memory:") == 0)
                DoFileOpen();
            else
                DoOpenQuery();
            return 0;
        }
        /* Ctrl+N - New Query (or New Database if editor empty and :memory:) */
        if (ctrl && wParam == 'N') {
            if ((GetWindowTextLengthW(g_hwndQuery) == 0 || g_showingHint) &&
                lstrcmpW(g_szDbPath, L":memory:") == 0)
                SendMessage(g_hwndMain, WM_COMMAND, IDM_NEW, 0);
            else
                DoNewQuery();
            return 0;
        }
        /* Ctrl+S - Save Query */
        if (ctrl && wParam == 'S') {
            DoSaveQuery();
            return 0;
        }
        /* Ctrl+F - Find, F3 - Find Next */
        if (ctrl && wParam == 'F') {
            DoFind();
            return 0;
        }
        if (wParam == VK_F3) {
            DoFindNext();
            return 0;
        }
        /* Escape/Backspace - back to query */
        if (wParam == VK_ESCAPE || wParam == VK_BACK) {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWQUERY, 0);
            return 0;
        }
        /* Ctrl+G - Toggle grid view (only if results exist) */
        if (ctrl && wParam == 'G') {
            if (g_lastResultRows > 0)
                SendMessage(g_hwndMain, WM_COMMAND, IDM_EXECATCURSOR, 0);
            return 0;
        }
        return 0;  /* Block all other keys */
    }
    /* Allow Ctrl+C WM_CHAR through */
    if (msg == WM_CHAR) {
        if (GetKeyState(VK_CONTROL) < 0 && wParam == 3)
            return CallWindowProc(g_pfnResultProc, hwnd, msg, wParam, lParam);
        return 0;
    }
    if (msg == WM_KEYUP)
        return 0;
    return CallWindowProc(g_pfnResultProc, hwnd, msg, wParam, lParam);
}
