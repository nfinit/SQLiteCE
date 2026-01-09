/*
** SQLiteCEdit - Editor pane management
*/

#include "globals.h"

void UpdateLineCount(void) {
    wchar_t buf[32];
    DWORD sel;
    int cur, total;
    if (g_suppressLineCount) {
        g_suppressLineCount--;
        return;
    }
    SendMessage(g_hwndQuery, EM_GETSEL, (WPARAM)&sel, 0);
    cur = (int)SendMessage(g_hwndQuery, EM_LINEFROMCHAR, sel, 0) + 1;
    total = (int)SendMessage(g_hwndQuery, EM_GETLINECOUNT, 0, 0);
    wsprintfW(buf, L"Ln %d of %d", cur, total);
    SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)buf);
}

void UpdateLineNumbers(void) {
    wchar_t buf[4096];
    int i, total, firstVisible, pos = 0;
    if (!g_showLineNumbers || !g_hwndLineNum || !g_hFontQuery) return;
    total = (int)SendMessage(g_hwndQuery, EM_GETLINECOUNT, 0, 0);
    
    /* Auto-size gutter width based on line count */
    {
        HDC hdc = GetDC(g_hwndLineNum);
        HFONT hOld = (HFONT)SelectObject(hdc, g_hFontQuery);
        SIZE sz;
        wchar_t numBuf[16];
        int newWidth;
        wsprintfW(numBuf, L"%d", total);
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
    
    firstVisible = (int)SendMessage(g_hwndQuery, EM_GETFIRSTVISIBLELINE, 0, 0);
    for (i = firstVisible + 1; i <= total && pos < 4000; i++) {
        pos += wsprintfW(buf + pos, L"%d\r\n", i);
    }
    buf[pos] = 0;
    SetWindowTextW(g_hwndLineNum, buf);
}

void SyncLineNumScroll(void) {
    if (!g_showLineNumbers || !g_hwndLineNum) return;
    UpdateLineNumbers();
}

void SwitchView(int mode) {
    g_viewMode = mode;
    ShowWindow(g_hwndQuery, mode == 0 ? SW_SHOW : SW_HIDE);
    if (g_hwndLineNum) ShowWindow(g_hwndLineNum, (mode == 0 && g_showLineNumbers) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndResult, mode == 1 ? SW_SHOW : SW_HIDE);
    SetFocus(mode == 0 ? g_hwndQuery : g_hwndResult);
    if (mode == 0) {
        UpdateLineCount();
        UpdateLineNumbers();
    } else
        SendMessageW(g_hwndStatus, SB_SETTEXTW, 1, (LPARAM)g_lastResultStatus);
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
    LOGFONTW lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = g_fontSizes[g_fontSizeResult];
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    lstrcpyW(lf.lfFaceName, L"Courier New");
    g_hFontResult = CreateFontIndirectW(&lf);
    SendMessage(g_hwndResult, WM_SETFONT, (WPARAM)g_hFontResult, TRUE);
    if (hOld) DeleteObject(hOld);
}

void CycleFontSize(void) {
    if (g_viewMode == 0) {
        g_fontSizeQuery = (g_fontSizeQuery + 1) % 4;
        UpdateQueryFont();
        SendMessage(g_hwndQuery, EM_SCROLLCARET, 0, 0);
        UpdateLineNumbers();
    } else {
        g_fontSizeResult = (g_fontSizeResult + 1) % 4;
        UpdateResultFont();
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
    /* Alt+X - Exit (must handle here when edit has focus) */
    if (msg == WM_SYSKEYDOWN && (wParam == 'X' || wParam == 'x')) {
        DestroyWindow(g_hwndMain);
        return 0;
    }
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
        /* Ctrl+Enter or F5 - Execute */
        if ((wParam == VK_RETURN && ctrl) || wParam == VK_F5) {
            ExecuteQuery();
            return 0;
        }
        /* Ctrl+O - Open Query */
        if (ctrl && wParam == 'O') {
            DoOpenQuery();
            return 0;
        }
        /* Ctrl+N - New */
        if (ctrl && wParam == 'N') {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_NEW, 0);
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
        /* F6 - Toggle view, Ctrl+1 - Query, Ctrl+2 - Results */
        if (wParam == VK_F6) {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWRESULT, 0);
            return 0;
        }
        if (ctrl && wParam == '1') {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWQUERY, 0);
            return 0;
        }
        if (ctrl && wParam == '2') {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWRESULT, 0);
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
    if (msg == WM_KEYDOWN) {
        int ctrl = GetKeyState(VK_CONTROL) < 0;
        /* Ctrl+A - Select all */
        if (ctrl && wParam == 'A') {
            SendMessage(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
        /* Ctrl+C - Copy (pass through) */
        if (ctrl && wParam == 'C')
            return CallWindowProc(g_pfnResultProc, hwnd, msg, wParam, lParam);
        /* F5 - Execute */
        if (wParam == VK_F5) {
            ExecuteQuery();
            return 0;
        }
        /* Ctrl+O - Open Query */
        if (ctrl && wParam == 'O') {
            DoOpenQuery();
            return 0;
        }
        /* Ctrl+N - New */
        if (ctrl && wParam == 'N') {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_NEW, 0);
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
        /* F6/Escape - back to query, Ctrl+1 - Query, Ctrl+2 - Results */
        if (wParam == VK_F6 || wParam == VK_ESCAPE || wParam == VK_BACK) {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWQUERY, 0);
            return 0;
        }
        if (ctrl && wParam == '1') {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWQUERY, 0);
            return 0;
        }
        if (ctrl && wParam == '2') {
            SendMessage(g_hwndMain, WM_COMMAND, IDM_VIEWRESULT, 0);
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
