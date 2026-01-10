/*
** SQLiteCEdit - Dialogs (About, Path prompt)
*/

#include "globals.h"

/*============================================================================
** About Dialog
**============================================================================*/

static HBITMAP g_hLogo = NULL;

static LRESULT CALLBACK AboutWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            wchar_t text[256];
            wchar_t ver[32], built[32];
            const char *sv = sqlite_libversion();
            const char *bd = __DATE__;
            int i, j;
            for (i = 0; sv[i] && i < 31; i++) ver[i] = sv[i];
            ver[i] = 0;
            for (i = 0, j = 0; bd[i] && j < 31; i++) {
                if (bd[i] == ' ' && bd[i+1] == ' ') continue;
                built[j++] = bd[i];
            }
            built[j] = 0;
            wsprintfW(text,
                L"SQLite/CEdit " SQLITECEDIT_VERSION L" (using SQLite %s)\n"
                L"(C) Intermountain Systems\n"
                L"Build date: %s", ver, built);
            CreateWindowW(L"STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_BITMAP,
                61, 10, 128, 64, hwnd, (HMENU)100, g_hInst, NULL);
            SendDlgItemMessage(hwnd, 100, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)g_hLogo);
            CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_CENTER,
                5, 80, 240, 50, hwnd, NULL, g_hInst, NULL);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hwnd);
                SetFocus(g_viewMode == 0 ? g_hwndQuery : g_hwndResult);
                return 0;
            }
            break;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE || wParam == VK_RETURN) {
                DestroyWindow(hwnd);
                SetFocus(g_viewMode == 0 ? g_hwndQuery : g_hwndResult);
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            SetFocus(g_viewMode == 0 ? g_hwndQuery : g_hwndResult);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void DoAbout(void) {
    WNDCLASSW wc = {0};
    RECT rc;
    HWND hwndAbout;
    
    if (!g_hLogo)
        g_hLogo = LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_LOGO));
    
    wc.lpfnWndProc = AboutWndProc;
    wc.hInstance = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"SQLiteCEAbout";
    RegisterClassW(&wc);
    
    GetWindowRect(g_hwndMain, &rc);
    hwndAbout = CreateWindowW(L"SQLiteCEAbout", L"About",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        rc.left + 30, rc.top + 30, 250, 160,
        g_hwndMain, NULL, g_hInst, NULL);
    ShowWindow(hwndAbout, SW_SHOW);
}

/*============================================================================
** Simple Path Input Dialog
**============================================================================*/

static wchar_t g_szPathBuf[MAX_PATH];
static HWND g_hwndPathEdit;

static LRESULT CALLBACK PathDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            g_hwndPathEdit = CreateWindowW(L"EDIT", g_szPathBuf,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                10, 10, rc.right - 20, 26,
                hwnd, (HMENU)101, g_hInst, NULL);
            CreateWindowW(L"BUTTON", L"OK",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                10, 46, 60, 26, hwnd, (HMENU)IDOK, g_hInst, NULL);
            CreateWindowW(L"BUTTON", L"Cancel",
                WS_CHILD | WS_VISIBLE,
                80, 46, 60, 26, hwnd, (HMENU)IDCANCEL, g_hInst, NULL);
            SetFocus(g_hwndPathEdit);
            return FALSE;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                GetWindowTextW(g_hwndPathEdit, g_szPathBuf, MAX_PATH);
                EndDialog(hwnd, IDOK);
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwnd, IDCANCEL);
            }
            return TRUE;
        case WM_CLOSE:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

int PromptForPath(const wchar_t *title, const wchar_t *defPath) {
    struct {
        DLGTEMPLATE tmpl;
        WORD menu, wndclass, title;
    } dlg;
    
    (void)title;  /* TODO: use title in dialog */
    lstrcpyW(g_szPathBuf, defPath);
    
    memset(&dlg, 0, sizeof(dlg));
    dlg.tmpl.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
    dlg.tmpl.cx = 160;
    dlg.tmpl.cy = 50;
    dlg.tmpl.x = 20;
    dlg.tmpl.y = 20;
    
    return DialogBoxIndirectW(g_hInst, &dlg.tmpl, g_hwndMain, PathDlgProc) == IDOK;
}

/*============================================================================
** Options Dialog
**============================================================================*/

#define IDC_OPT_CLEAREXEC    1001
#define IDC_OPT_EXECATCURSOR 1002
#define IDC_OPT_LINENUMS     1003
#define IDC_OPT_ERRORMSGBOX  1004
#define IDC_OPT_DBPATH       1005

static int g_optClearExec, g_optExecAtCursor, g_optLineNums, g_optErrorMsgBox;
static wchar_t g_optDbPath[MAX_PATH];
static HWND g_hwndOptions = NULL;
static int g_optResult = 0;

static void ApplyOptions(HWND hwnd) {
    g_optClearExec = SendMessage(GetDlgItem(hwnd, IDC_OPT_CLEAREXEC), BM_GETCHECK, 0, 0);
    g_optExecAtCursor = SendMessage(GetDlgItem(hwnd, IDC_OPT_EXECATCURSOR), BM_GETCHECK, 0, 0);
    g_optLineNums = SendMessage(GetDlgItem(hwnd, IDC_OPT_LINENUMS), BM_GETCHECK, 0, 0);
    g_optErrorMsgBox = SendMessage(GetDlgItem(hwnd, IDC_OPT_ERRORMSGBOX), BM_GETCHECK, 0, 0);
    GetWindowTextW(GetDlgItem(hwnd, IDC_OPT_DBPATH), g_optDbPath, MAX_PATH);
    g_optResult = 1;
}

static LRESULT CALLBACK OptionsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            /* Left column - checkboxes */
            CreateWindowW(L"BUTTON", L"Clear results on execute",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                10, 10, 165, 20, hwnd, (HMENU)IDC_OPT_CLEAREXEC, g_hInst, NULL);
            CreateWindowW(L"BUTTON", L"Execute at cursor",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                10, 32, 165, 20, hwnd, (HMENU)IDC_OPT_EXECATCURSOR, g_hInst, NULL);
            CreateWindowW(L"BUTTON", L"Show line numbers",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                10, 54, 165, 20, hwnd, (HMENU)IDC_OPT_LINENUMS, g_hInst, NULL);
            CreateWindowW(L"BUTTON", L"Message box on error",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                10, 76, 165, 20, hwnd, (HMENU)IDC_OPT_ERRORMSGBOX, g_hInst, NULL);
            
            /* Right column - paths */
            CreateWindowW(L"STATIC", L"Default database path:",
                WS_CHILD | WS_VISIBLE,
                185, 10, 150, 16, hwnd, NULL, g_hInst, NULL);
            CreateWindowW(L"EDIT", g_optDbPath,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                185, 28, 180, 22, hwnd, (HMENU)IDC_OPT_DBPATH, g_hInst, NULL);
            
            SendMessage(GetDlgItem(hwnd, IDC_OPT_CLEAREXEC), BM_SETCHECK, g_optClearExec, 0);
            SendMessage(GetDlgItem(hwnd, IDC_OPT_EXECATCURSOR), BM_SETCHECK, g_optExecAtCursor, 0);
            SendMessage(GetDlgItem(hwnd, IDC_OPT_LINENUMS), BM_SETCHECK, g_optLineNums, 0);
            SendMessage(GetDlgItem(hwnd, IDC_OPT_ERRORMSGBOX), BM_SETCHECK, g_optErrorMsgBox, 0);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                ApplyOptions(hwnd);
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            g_hwndOptions = NULL;
            SetFocus(g_viewMode == 0 ? g_hwndQuery : g_hwndResult);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void DoOptions(void) {
    WNDCLASSW wc = {0};
    RECT rc;
    MSG msg;
    
    if (g_hwndOptions) {
        SetFocus(g_hwndOptions);
        return;
    }
    
    g_optClearExec = g_clearOnExec;
    g_optExecAtCursor = g_execAtCursor;
    g_optLineNums = g_showLineNumbers;
    g_optErrorMsgBox = g_showErrorMsgBox;
    lstrcpyW(g_optDbPath, g_szDefaultDbPath);
    g_optResult = 0;
    
    wc.lpfnWndProc = OptionsWndProc;
    wc.hInstance = g_hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"SQLiteCEOptions";
    RegisterClassW(&wc);
    
    GetWindowRect(g_hwndMain, &rc);
    g_hwndOptions = CreateWindowExW(WS_EX_CAPTIONOKBTN,
        L"SQLiteCEOptions", L"Options",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        rc.left + 20, rc.top + 30, 380, 125,
        g_hwndMain, NULL, g_hInst, NULL);
    ShowWindow(g_hwndOptions, SW_SHOW);
    
    /* Modal message loop */
    while (g_hwndOptions && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    if (g_optResult) {
        g_clearOnExec = g_optClearExec;
        g_execAtCursor = g_optExecAtCursor;
        g_showErrorMsgBox = g_optErrorMsgBox;
        lstrcpyW(g_szDefaultDbPath, g_optDbPath);
        
        /* Handle line numbers toggle */
        if (g_optLineNums != g_showLineNumbers) {
            g_showLineNumbers = g_optLineNums;
            ShowWindow(g_hwndLineNum, (g_viewMode == 0 && g_showLineNumbers) ? SW_SHOW : SW_HIDE);
            SendMessage(g_hwndMain, WM_SIZE, 0, 0);
        }
    }
}
