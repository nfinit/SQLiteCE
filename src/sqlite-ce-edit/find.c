/*
** SQLiteCEdit - Find/Search functionality
*/

#include "globals.h"
#include "allocators.h"

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

    buf = ALLOC(wchar_t, len + 1);
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
                if (g_viewMode == 1 && g_gridView) {
                    SetFocus(g_hwndGrid);
                    if (g_findText[0]) GridFindNext();
                } else {
                    SetFocus(g_viewMode == 0 ? g_hwndQuery : g_hwndResult);
                    if (g_findText[0]) {
                        g_searchMode = 1;
                        DoFindNext();
                    }
                }
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            g_hwndFindDlg = NULL;
            if (g_viewMode == 1 && g_gridView)
                SetFocus(g_hwndGrid);
            else
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

/*============================================================================
** Find and Replace (Ctrl+H) - Query view only
**============================================================================*/

static HWND g_hwndReplaceDlg = NULL;
static HWND g_hwndReplFind = NULL;
static HWND g_hwndReplWith = NULL;
static wchar_t g_replaceText[128] = L"";

#define ID_REPL_FIND    201
#define ID_REPL_REPLACE 202
#define ID_REPL_ALL     203

static void DoReplaceOne(void) {
    DWORD selStart, selEnd;

    if (!g_findText[0]) return;
    if (g_viewMode != 0) return;  /* Query view only */

    SendMessage(g_hwndQuery, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);

    /* If we have a selection matching find text, replace it */
    if (selEnd > selStart) {
        int selLen = selEnd - selStart;
        int findLen = lstrlenW(g_findText);
        if (selLen == findLen) {
            int len = GetWindowTextLengthW(g_hwndQuery);
            wchar_t *buf = ALLOC(wchar_t, len + 1);
            if (buf) {
                int i, match = 1;
                GetWindowTextW(g_hwndQuery, buf, len + 1);
                /* Case-insensitive compare of selection */
                for (i = 0; i < selLen && match; i++) {
                    wchar_t c1 = buf[selStart + i], c2 = g_findText[i];
                    if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
                    if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
                    if (c1 != c2) match = 0;
                }
                LocalFree(buf);
                if (match) {
                    SendMessage(g_hwndQuery, EM_REPLACESEL, TRUE, (LPARAM)g_replaceText);
                    g_queryDirty = 1;
                }
            }
        }
    }
    /* Find next */
    DoFindNext();
}

static int DoReplaceAll(void) {
    int len, findLen, replLen, count = 0, i, j;
    wchar_t *buf, *newBuf, *p;

    if (!g_findText[0]) return 0;
    if (g_viewMode != 0) return 0;

    findLen = lstrlenW(g_findText);
    replLen = lstrlenW(g_replaceText);
    len = GetWindowTextLengthW(g_hwndQuery);
    if (len == 0) return 0;

    buf = ALLOC(wchar_t, len + 1);
    if (!buf) return 0;
    GetWindowTextW(g_hwndQuery, buf, len + 1);

    /* Count matches first */
    for (i = 0; i <= len - findLen; i++) {
        for (j = 0; j < findLen; j++) {
            wchar_t c1 = buf[i + j], c2 = g_findText[j];
            if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
            if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
            if (c1 != c2) break;
        }
        if (j == findLen) count++;
    }

    if (count == 0) {
        LocalFree(buf);
        return 0;
    }

    /* Build new string */
    newBuf = ALLOC(wchar_t, len + count * (replLen - findLen) + 1);
    if (!newBuf) {
        LocalFree(buf);
        return 0;
    }

    p = newBuf;
    for (i = 0; i <= len; i++) {
        if (i <= len - findLen) {
            for (j = 0; j < findLen; j++) {
                wchar_t c1 = buf[i + j], c2 = g_findText[j];
                if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
                if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
                if (c1 != c2) break;
            }
            if (j == findLen) {
                for (j = 0; j < replLen; j++) *p++ = g_replaceText[j];
                i += findLen - 1;
                continue;
            }
        }
        *p++ = buf[i];
    }

    SetWindowTextW(g_hwndQuery, newBuf);
    g_queryDirty = 1;

    LocalFree(newBuf);
    LocalFree(buf);
    return count;
}

static WNDPROC g_pfnReplFindProc, g_pfnReplWithProc;

static LRESULT CALLBACK ReplEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WNDPROC origProc = (hwnd == g_hwndReplFind) ? g_pfnReplFindProc : g_pfnReplWithProc;
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_TAB) {
            SetFocus((hwnd == g_hwndReplFind) ? g_hwndReplWith : g_hwndReplFind);
            return 0;
        }
        if (wParam == VK_RETURN) {
            SendMessage(GetParent(hwnd), WM_COMMAND, ID_REPL_FIND, 0);
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            SendMessage(GetParent(hwnd), WM_CLOSE, 0, 0);
            return 0;
        }
    }
    if (msg == WM_CHAR && wParam == 27) {  /* Escape as WM_CHAR */
        SendMessage(GetParent(hwnd), WM_CLOSE, 0, 0);
        return 0;
    }
    return CallWindowProc(origProc, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK ReplaceWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            CreateWindowW(L"STATIC", L"Find:",
                WS_CHILD | WS_VISIBLE, 5, 7, 45, 16, hwnd, NULL, g_hInst, NULL);
            g_hwndReplFind = CreateWindowW(L"EDIT", g_findText,
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP,
                55, 5, 175, 20, hwnd, (HMENU)101, g_hInst, NULL);
            CreateWindowW(L"STATIC", L"Replace:",
                WS_CHILD | WS_VISIBLE, 5, 32, 50, 16, hwnd, NULL, g_hInst, NULL);
            g_hwndReplWith = CreateWindowW(L"EDIT", g_replaceText,
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP,
                55, 30, 175, 20, hwnd, (HMENU)102, g_hInst, NULL);
            CreateWindowW(L"BUTTON", L"Find",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                5, 58, 55, 22, hwnd, (HMENU)ID_REPL_FIND, g_hInst, NULL);
            CreateWindowW(L"BUTTON", L"Replace",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                65, 58, 60, 22, hwnd, (HMENU)ID_REPL_REPLACE, g_hInst, NULL);
            CreateWindowW(L"BUTTON", L"All",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                175, 58, 55, 22, hwnd, (HMENU)ID_REPL_ALL, g_hInst, NULL);
            g_pfnReplFindProc = (WNDPROC)SetWindowLong(g_hwndReplFind, GWL_WNDPROC, (LONG)ReplEditProc);
            g_pfnReplWithProc = (WNDPROC)SetWindowLong(g_hwndReplWith, GWL_WNDPROC, (LONG)ReplEditProc);
            SetFocus(g_hwndReplFind);
            SendMessage(g_hwndReplFind, EM_SETSEL, 0, -1);
            return 0;
        }
        case WM_COMMAND:
            GetWindowTextW(g_hwndReplFind, g_findText, 128);
            GetWindowTextW(g_hwndReplWith, g_replaceText, 128);
            if (LOWORD(wParam) == ID_REPL_FIND) {
                if (g_findText[0]) DoFindNext();
            } else if (LOWORD(wParam) == ID_REPL_REPLACE) {
                if (g_findText[0]) DoReplaceOne();
            } else if (LOWORD(wParam) == ID_REPL_ALL) {
                if (g_findText[0]) {
                    int n = DoReplaceAll();
                    wchar_t buf[64];
                    wsprintfW(buf, L"%d replacement%s made.", n, n == 1 ? L"" : L"s");
                    MessageBoxW(hwnd, buf, L"Replace All", MB_OK);
                }
            }
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            g_hwndReplaceDlg = NULL;
            SetFocus(g_hwndQuery);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void DoReplace(void) {
    WNDCLASSW wc = {0};
    RECT rc;

    /* Only available in Query view */
    if (g_viewMode != 0) {
        MessageBoxW(g_hwndMain, L"Replace is only available in Query view.", L"Replace", MB_OK);
        return;
    }

    if (g_hwndReplaceDlg) {
        SetFocus(g_hwndReplFind);
        return;
    }

    /* Close find dialog if open */
    if (g_hwndFindDlg) {
        DestroyWindow(g_hwndFindDlg);
        g_hwndFindDlg = NULL;
    }

    wc.lpfnWndProc = ReplaceWndProc;
    wc.hInstance = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"SQLiteCEReplace";
    RegisterClassW(&wc);

    GetWindowRect(g_hwndMain, &rc);
    g_hwndReplaceDlg = CreateWindowExW(WS_EX_TOOLWINDOW, L"SQLiteCEReplace", L"Replace",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        rc.left + 20, rc.top + 50, 240, 108,
        g_hwndMain, NULL, g_hInst, NULL);
    ShowWindow(g_hwndReplaceDlg, SW_SHOW);
}
