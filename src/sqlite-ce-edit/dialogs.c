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
                76, 10, 128, 64, hwnd, (HMENU)100, g_hInst, NULL);
            SendDlgItemMessage(hwnd, 100, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)g_hLogo);
            CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_CENTER,
                5, 80, 270, 50, hwnd, NULL, g_hInst, NULL);
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
        rc.left + 30, rc.top + 30, 280, 160,
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

#define IDC_OPT_TAB          1000
#define IDC_OPT_CLEAREXEC    1001
#define IDC_OPT_EXECATCURSOR 1002
#define IDC_OPT_LINENUMS     1003
#define IDC_OPT_ERRORMSGBOX  1004
#define IDC_OPT_DBPATH       1005
#define IDC_OPT_CLEARREG     1006
#define IDC_OPT_STORAGECARD  1007
#define IDC_OPT_STORAGECARDDATA 1008
#define IDC_OPT_DBPATHLABEL  1009

static int g_optClearExec, g_optExecAtCursor, g_optLineNums, g_optErrorMsgBox;
static int g_optStorageCard, g_optStorageCardData;
static wchar_t g_optDbPath[MAX_PATH];
static HWND g_hwndOptions = NULL;
static HWND g_hwndOptTab = NULL;
static int g_optResult = 0;

/* Control arrays for tab visibility */
static HWND g_optGeneralCtrls[5];
static HWND g_optStorageCtrls[4];

static void ApplyOptions(HWND hwnd) {
    g_optClearExec = SendMessage(g_optGeneralCtrls[0], BM_GETCHECK, 0, 0);
    g_optExecAtCursor = SendMessage(g_optGeneralCtrls[1], BM_GETCHECK, 0, 0);
    g_optLineNums = SendMessage(g_optGeneralCtrls[2], BM_GETCHECK, 0, 0);
    g_optErrorMsgBox = SendMessage(g_optGeneralCtrls[3], BM_GETCHECK, 0, 0);
    g_optStorageCardData = SendMessage(g_optStorageCtrls[0], BM_GETCHECK, 0, 0);
    g_optStorageCard = SendMessage(g_optStorageCtrls[1], BM_GETCHECK, 0, 0);
    GetWindowTextW(g_optStorageCtrls[3], g_optDbPath, MAX_PATH);
    g_optResult = 1;
}

static void UpdatePathDisplay(HWND hwnd) {
    wchar_t label[128];
    WIN32_FIND_DATAW fd;
    HANDLE hFind;
    int useCard = SendMessage(g_optStorageCtrls[0], BM_GETCHECK, 0, 0);
    
    if (useCard) {
        hFind = FindFirstFileW(L"\\Storage Card*", &fd);
        if (hFind != INVALID_HANDLE_VALUE && (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            wsprintfW(label, L"Data path (in \\%s%s):", fd.cFileName, g_szCardBasePath);
            FindClose(hFind);
        } else {
            if (hFind != INVALID_HANDLE_VALUE) FindClose(hFind);
            lstrcpyW(label, L"Data path (no card found):");
        }
    } else {
        wsprintfW(label, L"Data path (in %s):", g_szLocalBasePath);
    }
    SetWindowTextW(g_optStorageCtrls[2], label);
}

static void ShowOptionsTab(int tab) {
    int i, showGen = (tab == 0), showSto = (tab == 1);
    for (i = 0; i < 5; i++) ShowWindow(g_optGeneralCtrls[i], showGen ? SW_SHOW : SW_HIDE);
    for (i = 0; i < 4; i++) ShowWindow(g_optStorageCtrls[i], showSto ? SW_SHOW : SW_HIDE);
}

static LRESULT CALLBACK OptionsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            TCITEMW tci = {0};
            RECT tabRc;
            int x, y;
            
            /* Tab control */
            g_hwndOptTab = CreateWindowW(WC_TABCONTROLW, NULL,
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                4, 4, 424, 130, hwnd, (HMENU)IDC_OPT_TAB, g_hInst, NULL);
            tci.mask = TCIF_TEXT;
            tci.pszText = L"General";
            SendMessage(g_hwndOptTab, TCM_INSERTITEMW, 0, (LPARAM)&tci);
            tci.pszText = L"Storage";
            SendMessage(g_hwndOptTab, TCM_INSERTITEMW, 1, (LPARAM)&tci);
            
            /* Get tab content area (relative to tab control) */
            SetRect(&tabRc, 0, 0, 424, 130);
            SendMessage(g_hwndOptTab, TCM_ADJUSTRECT, FALSE, (LPARAM)&tabRc);
            x = tabRc.left + 4;
            y = tabRc.top + 2;
            
            /* General tab controls - children of tab control */
            g_optGeneralCtrls[0] = CreateWindowW(L"BUTTON", L"Clear results on execute",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                x, y, 200, 20, g_hwndOptTab, (HMENU)IDC_OPT_CLEAREXEC, g_hInst, NULL);
            g_optGeneralCtrls[1] = CreateWindowW(L"BUTTON", L"Execute at cursor",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                x, y + 22, 200, 20, g_hwndOptTab, (HMENU)IDC_OPT_EXECATCURSOR, g_hInst, NULL);
            g_optGeneralCtrls[2] = CreateWindowW(L"BUTTON", L"Show line numbers",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                x, y + 44, 200, 20, g_hwndOptTab, (HMENU)IDC_OPT_LINENUMS, g_hInst, NULL);
            g_optGeneralCtrls[3] = CreateWindowW(L"BUTTON", L"Message box on error",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                x, y + 66, 200, 20, g_hwndOptTab, (HMENU)IDC_OPT_ERRORMSGBOX, g_hInst, NULL);
            g_optGeneralCtrls[4] = CreateWindowW(L"BUTTON", L"Clear All Settings...",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                x + 210, y + 66, 140, 22, g_hwndOptTab, (HMENU)IDC_OPT_CLEARREG, g_hInst, NULL);
            
            /* Storage tab controls - children of tab control */
            g_optStorageCtrls[0] = CreateWindowW(L"BUTTON", L"Use storage card for data",
                WS_CHILD | BS_AUTOCHECKBOX,
                x, y, 220, 20, g_hwndOptTab, (HMENU)IDC_OPT_STORAGECARDDATA, g_hInst, NULL);
            g_optStorageCtrls[1] = CreateWindowW(L"BUTTON", L"Use storage card for backups",
                WS_CHILD | BS_AUTOCHECKBOX,
                x, y + 22, 220, 20, g_hwndOptTab, (HMENU)IDC_OPT_STORAGECARD, g_hInst, NULL);
            g_optStorageCtrls[2] = CreateWindowW(L"STATIC", L"Data path:",
                WS_CHILD, x, y + 50, 400, 16, g_hwndOptTab, (HMENU)IDC_OPT_DBPATHLABEL, g_hInst, NULL);
            g_optStorageCtrls[3] = CreateWindowW(L"EDIT", g_optDbPath,
                WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                x, y + 68, 394, 22, g_hwndOptTab, (HMENU)IDC_OPT_DBPATH, g_hInst, NULL);
            
            /* Set initial values */
            SendMessage(g_optGeneralCtrls[0], BM_SETCHECK, g_optClearExec, 0);
            SendMessage(g_optGeneralCtrls[1], BM_SETCHECK, g_optExecAtCursor, 0);
            SendMessage(g_optGeneralCtrls[2], BM_SETCHECK, g_optLineNums, 0);
            SendMessage(g_optGeneralCtrls[3], BM_SETCHECK, g_optErrorMsgBox, 0);
            SendMessage(g_optStorageCtrls[0], BM_SETCHECK, g_optStorageCardData, 0);
            SendMessage(g_optStorageCtrls[1], BM_SETCHECK, g_optStorageCard, 0);
            UpdatePathDisplay(hwnd);
            ShowOptionsTab(0);
            return 0;
        }
        case WM_NOTIFY: {
            NMHDR *nmh = (NMHDR *)lParam;
            if (nmh->idFrom == IDC_OPT_TAB && nmh->code == TCN_SELCHANGE) {
                ShowOptionsTab(TabCtrl_GetCurSel(g_hwndOptTab));
            }
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                ApplyOptions(hwnd);
                DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == IDC_OPT_STORAGECARDDATA) {
                UpdatePathDisplay(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == IDC_OPT_CLEARREG) {
                if (MessageBoxW(hwnd, L"Clear all saved settings and recent files?", 
                                L"Confirm", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    ClearSettings();
                    MessageBoxW(hwnd, L"Settings cleared. Restart to use defaults.", 
                                L"Settings", MB_OK | MB_ICONINFORMATION);
                }
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            g_hwndOptions = NULL;
            g_hwndOptTab = NULL;
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
    g_optStorageCard = g_useStorageCard;
    g_optStorageCardData = g_useStorageCardData;
    lstrcpyW(g_optDbPath, g_szDataRelPath);
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
        rc.left + 16, rc.top + 30, 440, 170,
        g_hwndMain, NULL, g_hInst, NULL);
    ShowWindow(g_hwndOptions, SW_SHOW);
    
    /* Modal message loop */
    while (g_hwndOptions && GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            SendMessage(g_hwndOptions, WM_CLOSE, 0, 0);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    if (g_optResult) {
        g_clearOnExec = g_optClearExec;
        g_execAtCursor = g_optExecAtCursor;
        g_showErrorMsgBox = g_optErrorMsgBox;
        g_useStorageCard = g_optStorageCard;
        g_useStorageCardData = g_optStorageCardData;
        lstrcpyW(g_szDataRelPath, g_optDbPath);
        
        /* Sync toolbar button state */
        SendMessage(g_hwndCB, TB_CHECKBUTTON, IDM_EXECATCURSOR, g_execAtCursor);
        
        /* Handle line numbers toggle */
        if (g_optLineNums != g_showLineNumbers) {
            g_showLineNumbers = g_optLineNums;
            ShowWindow(g_hwndLineNum, (g_viewMode == 0 && g_showLineNumbers) ? SW_SHOW : SW_HIDE);
            SendMessage(g_hwndMain, WM_SIZE, 0, 0);
        }
    }
}
