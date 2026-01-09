/*
** SQLiteCEdit - Find/Search functionality
*/

#include "globals.h"

static HWND g_hwndFindDlg = NULL;
static HWND g_hwndFindEdit = NULL;
static WNDPROC g_pfnFindEditProc = NULL;

void DoFindNext(void) {
    HWND hwndEdit = g_viewMode == 0 ? g_hwndQuery : g_hwndResult;
    int len, findLen, start, i, j;
    wchar_t *buf;
    DWORD sel;
    
    if (!g_findText[0]) return;
    
    findLen = lstrlenW(g_findText);
    len = GetWindowTextLengthW(hwndEdit);
    if (len == 0) return;
    
    buf = (wchar_t *)LocalAlloc(LMEM_FIXED, (len + 1) * sizeof(wchar_t));
    if (!buf) return;
    GetWindowTextW(hwndEdit, buf, len + 1);
    
    /* Get current position */
    SendMessage(hwndEdit, EM_GETSEL, (WPARAM)&sel, 0);
    start = sel + 1;
    if (start > len) start = 0;
    
    /* Search forward from cursor */
    for (i = start; i <= len - findLen; i++) {
        for (j = 0; j < findLen; j++) {
            wchar_t c1 = buf[i + j], c2 = g_findText[j];
            if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
            if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
            if (c1 != c2) break;
        }
        if (j == findLen) {
            SendMessage(hwndEdit, EM_SETSEL, i, i + findLen);
            SendMessage(hwndEdit, EM_SCROLLCARET, 0, 0);
            LocalFree(buf);
            return;
        }
    }
    /* Wrap around */
    for (i = 0; i < start && i <= len - findLen; i++) {
        for (j = 0; j < findLen; j++) {
            wchar_t c1 = buf[i + j], c2 = g_findText[j];
            if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
            if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
            if (c1 != c2) break;
        }
        if (j == findLen) {
            SendMessage(hwndEdit, EM_SETSEL, i, i + findLen);
            SendMessage(hwndEdit, EM_SCROLLCARET, 0, 0);
            LocalFree(buf);
            return;
        }
    }
    LocalFree(buf);
    MessageBoxW(g_hwndMain, L"Text not found.", L"Find", MB_OK);
}

static LRESULT CALLBACK FindEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            SendMessage(g_hwndFindDlg, WM_COMMAND, IDOK, 0);
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            SendMessage(g_hwndFindDlg, WM_CLOSE, 0, 0);
            return 0;
        }
    }
    return CallWindowProc(g_pfnFindEditProc, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK FindWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hwndFindEdit = CreateWindowW(L"EDIT", g_findText,
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP,
                5, 5, 133, 20, hwnd, (HMENU)101, g_hInst, NULL);
            CreateWindowW(L"BUTTON", L"Find",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                145, 4, 45, 22, hwnd, (HMENU)IDOK, g_hInst, NULL);
            g_pfnFindEditProc = (WNDPROC)SetWindowLong(g_hwndFindEdit, GWL_WNDPROC, (LONG)FindEditProc);
            SetFocus(g_hwndFindEdit);
            SendMessage(g_hwndFindEdit, EM_SETSEL, 0, -1);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                GetWindowTextW(g_hwndFindEdit, g_findText, 128);
                DestroyWindow(hwnd);
                g_hwndFindDlg = NULL;
                SetFocus(g_viewMode == 0 ? g_hwndQuery : g_hwndResult);
                if (g_findText[0]) {
                    g_searchMode = 1;
                    DoFindNext();
                }
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            g_hwndFindDlg = NULL;
            SetFocus(g_viewMode == 0 ? g_hwndQuery : g_hwndResult);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void DoFind(void) {
    WNDCLASSW wc = {0};
    RECT rc;
    
    if (g_hwndFindDlg) {
        SetFocus(g_hwndFindEdit);
        return;
    }
    
    wc.lpfnWndProc = FindWndProc;
    wc.hInstance = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"SQLiteCEFind";
    RegisterClassW(&wc);
    
    GetWindowRect(g_hwndMain, &rc);
    #ifndef WS_EX_TOOLWINDOW
    #define WS_EX_TOOLWINDOW 0x00000080L
    #endif
    g_hwndFindDlg = CreateWindowExW(WS_EX_TOOLWINDOW, L"SQLiteCEFind", L"Find",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        rc.left + 20, rc.top + 50, 200, 52,
        g_hwndMain, NULL, g_hInst, NULL);
    ShowWindow(g_hwndFindDlg, SW_SHOW);
}
