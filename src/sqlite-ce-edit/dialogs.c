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
#define IDC_OPT_LOCALPATH    1010
#define IDC_OPT_CARDPATH     1011
#define IDC_OPT_REMEMBERQDIR 1013
#define IDC_OPT_GRIDAUTOSIZE 1014
#define IDC_OPT_STARTLASTDB  1015
#define IDC_OPT_CARDCOMBO    1016
#define IDC_OPT_CUSTOMCARD   1017
#define IDC_OPT_CUSTOMPATH   1018
#define IDC_OPT_RETENTION    1019

static int g_optClearExec, g_optLineNums, g_optErrorMsgBox, g_optRememberQueryDir;
static int g_optStorageCard, g_optStorageCardData, g_optGridAutoSize, g_optStartLastDb;
static int g_optCustomCard, g_optRetention;
static wchar_t g_optLocalPath[MAX_PATH];
static wchar_t g_optCardPath[MAX_PATH];
static wchar_t g_optCardRoot[MAX_PATH];
static wchar_t g_detectedCards[8][MAX_PATH];
static int g_detectedCardCount = 0;
static HWND g_hwndOptions = NULL;
static HWND g_hwndOptTab = NULL;
static int g_optResult = 0;

/* Control arrays for tab visibility */
static HWND g_optGeneralCtrls[2];
static HWND g_optStorageCtrls[12];  /* Expanded for retention controls */
static HWND g_optEditorCtrls[3];
static HWND g_optResultsCtrls[2];

static void UpdateCardPathVisibility(void) {
    int custom = SendMessage(g_optStorageCtrls[4], BM_GETCHECK, 0, 0);
    ShowWindow(g_optStorageCtrls[3], custom ? SW_HIDE : SW_SHOW);  /* combo */
    ShowWindow(g_optStorageCtrls[5], custom ? SW_SHOW : SW_HIDE);  /* custom edit */
}

static void ApplyOptions(HWND hwnd) {
    int sel;
    g_optClearExec = !SendMessage(g_optResultsCtrls[0], BM_GETCHECK, 0, 0);
    g_optGridAutoSize = SendMessage(g_optResultsCtrls[1], BM_GETCHECK, 0, 0);
    g_optLineNums = SendMessage(g_optEditorCtrls[0], BM_GETCHECK, 0, 0);
    g_optErrorMsgBox = SendMessage(g_optEditorCtrls[1], BM_GETCHECK, 0, 0);
    g_optRememberQueryDir = SendMessage(g_optEditorCtrls[2], BM_GETCHECK, 0, 0);
    g_optStartLastDb = SendMessage(g_optGeneralCtrls[0], BM_GETCHECK, 0, 0);
    GetWindowTextW(g_optStorageCtrls[1], g_optLocalPath, MAX_PATH);
    g_optCustomCard = SendMessage(g_optStorageCtrls[4], BM_GETCHECK, 0, 0);
    GetWindowTextW(g_optStorageCtrls[6], g_optCardPath, MAX_PATH);
    g_optStorageCardData = SendMessage(g_optStorageCtrls[7], BM_GETCHECK, 0, 0);
    g_optStorageCard = SendMessage(g_optStorageCtrls[8], BM_GETCHECK, 0, 0);
    /* Read retention value */
    {
        wchar_t buf[16];
        wchar_t *p;
        g_optRetention = 0;
        GetWindowTextW(g_optStorageCtrls[10], buf, 16);
        for (p = buf; *p >= '0' && *p <= '9'; p++) g_optRetention = g_optRetention * 10 + (*p - '0');
    }
    /* Get card root from combo or custom field */
    if (g_optCustomCard) {
        GetWindowTextW(g_optStorageCtrls[5], g_optCardRoot, MAX_PATH);
    } else {
        sel = SendMessage(g_optStorageCtrls[3], CB_GETCURSEL, 0, 0);
        if (sel == 0) {
            /* First available - store empty string to trigger auto-detect */
            g_optCardRoot[0] = 0;
        } else if (sel > 0 && sel <= g_detectedCardCount) {
            lstrcpyW(g_optCardRoot, g_detectedCards[sel - 1]);
        } else {
            g_optCardRoot[0] = 0;
        }
    }
    g_optResult = 1;
}

static void ShowOptionsTab(int tab) {
    int i;
    for (i = 0; i < 2; i++) ShowWindow(g_optGeneralCtrls[i], tab == 0 ? SW_SHOW : SW_HIDE);
    for (i = 0; i < 12; i++) {
        if (i == 4) continue;  /* Custom checkbox handled separately (child of dialog) */
        ShowWindow(g_optStorageCtrls[i], tab == 1 ? SW_SHOW : SW_HIDE);
    }
    if (tab == 1) {
        ShowWindow(g_optStorageCtrls[4], SW_SHOW);
        SetWindowPos(g_optStorageCtrls[4], HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        UpdateCardPathVisibility();
    } else {
        ShowWindow(g_optStorageCtrls[4], SW_HIDE);
    }
    for (i = 0; i < 3; i++) ShowWindow(g_optEditorCtrls[i], tab == 2 ? SW_SHOW : SW_HIDE);
    for (i = 0; i < 2; i++) ShowWindow(g_optResultsCtrls[i], tab == 3 ? SW_SHOW : SW_HIDE);
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
            tci.pszText = L"Editor";
            SendMessage(g_hwndOptTab, TCM_INSERTITEMW, 2, (LPARAM)&tci);
            tci.pszText = L"Results";
            SendMessage(g_hwndOptTab, TCM_INSERTITEMW, 3, (LPARAM)&tci);
            
            /* Get tab content area (relative to tab control) */
            SetRect(&tabRc, 0, 0, 424, 130);
            SendMessage(g_hwndOptTab, TCM_ADJUSTRECT, FALSE, (LPARAM)&tabRc);
            x = tabRc.left + 4;
            y = tabRc.top + 2;
            
            /* General tab controls */
            g_optGeneralCtrls[0] = CreateWindowW(L"BUTTON", L"Start with last opened database",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                x, y, 200, 20, g_hwndOptTab, (HMENU)IDC_OPT_STARTLASTDB, g_hInst, NULL);
            g_optGeneralCtrls[1] = CreateWindowW(L"BUTTON", L"Clear registry settings...",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                x, y + 24, 160, 22, g_hwndOptTab, (HMENU)IDC_OPT_CLEARREG, g_hInst, NULL);
            
            /* Storage tab controls */
            /* Row 1: device path */
            g_optStorageCtrls[0] = CreateWindowW(L"STATIC", L"Device path:",
                WS_CHILD, x, y + 2, 75, 16, g_hwndOptTab, (HMENU)-1, g_hInst, NULL);
            g_optStorageCtrls[1] = CreateWindowW(L"EDIT", g_optLocalPath,
                WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                x + 75, y, 200, 20, g_hwndOptTab, (HMENU)IDC_OPT_LOCALPATH, g_hInst, NULL);
            /* Row 2: storage card selection */
            g_optStorageCtrls[2] = CreateWindowW(L"STATIC", L"Storage card:",
                WS_CHILD, x, y + 26, 75, 16, g_hwndOptTab, (HMENU)-1, g_hInst, NULL);
            g_optStorageCtrls[4] = CreateWindowW(L"BUTTON", L"Custom",
                WS_CHILD | WS_CLIPSIBLINGS | BS_AUTOCHECKBOX,
                4 + x + 75, 4 + y + 24, 60, 20, hwnd, (HMENU)IDC_OPT_CUSTOMCARD, g_hInst, NULL);
            g_optStorageCtrls[3] = CreateWindowW(L"COMBOBOX", NULL,
                WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                x + 140, y + 22, 140, 100, g_hwndOptTab, (HMENU)IDC_OPT_CARDCOMBO, g_hInst, NULL);
            g_optStorageCtrls[5] = CreateWindowW(L"EDIT", g_optCardRoot,
                WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                x + 140, y + 22, 140, 20, g_hwndOptTab, (HMENU)IDC_OPT_CUSTOMPATH, g_hInst, NULL);
            /* Relative path field */
            g_optStorageCtrls[6] = CreateWindowW(L"EDIT", g_optCardPath,
                WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                x + 285, y + 22, 105, 20, g_hwndOptTab, (HMENU)IDC_OPT_CARDPATH, g_hInst, NULL);
            /* Row 3: checkboxes */
            g_optStorageCtrls[7] = CreateWindowW(L"BUTTON", L"Prefer card for data",
                WS_CHILD | BS_AUTOCHECKBOX,
                x, y + 48, 150, 20, g_hwndOptTab, (HMENU)IDC_OPT_STORAGECARDDATA, g_hInst, NULL);
            g_optStorageCtrls[8] = CreateWindowW(L"BUTTON", L"Prefer card for backups",
                WS_CHILD | BS_AUTOCHECKBOX,
                x + 160, y + 48, 160, 20, g_hwndOptTab, (HMENU)IDC_OPT_STORAGECARD, g_hInst, NULL);
            /* Row 4: backup retention */
            g_optStorageCtrls[9] = CreateWindowW(L"STATIC", L"Keep backups:",
                WS_CHILD, x, y + 72, 80, 16, g_hwndOptTab, (HMENU)-1, g_hInst, NULL);
            {
                wchar_t buf[16];
                wsprintfW(buf, L"%d", g_optRetention);
                g_optStorageCtrls[10] = CreateWindowW(L"EDIT", buf,
                    WS_CHILD | WS_BORDER | ES_NUMBER,
                    x + 80, y + 70, 30, 20, g_hwndOptTab, (HMENU)IDC_OPT_RETENTION, g_hInst, NULL);
            }
            g_optStorageCtrls[11] = CreateWindowW(L"STATIC", L"(0 = unlimited)",
                WS_CHILD, x + 115, y + 72, 100, 16, g_hwndOptTab, (HMENU)-1, g_hInst, NULL);
            
            /* Populate card combo - first item is auto-select */
            SendMessageW(g_optStorageCtrls[3], CB_ADDSTRING, 0, (LPARAM)L"(first available)");
            g_detectedCardCount = DetectStorageCards(g_detectedCards, 8);
            {
                int i, sel = 0;  /* Default to first available */
                for (i = 0; i < g_detectedCardCount; i++) {
                    SendMessageW(g_optStorageCtrls[3], CB_ADDSTRING, 0, (LPARAM)(g_detectedCards[i] + 1));
                    if (lstrcmpiW(g_detectedCards[i], g_optCardRoot) == 0) sel = i + 1;
                }
                SendMessage(g_optStorageCtrls[3], CB_SETCURSEL, sel, 0);
            }
            
            /* Editor tab controls */
            g_optEditorCtrls[0] = CreateWindowW(L"BUTTON", L"Show line numbers",
                WS_CHILD | BS_AUTOCHECKBOX,
                x, y, 200, 20, g_hwndOptTab, (HMENU)IDC_OPT_LINENUMS, g_hInst, NULL);
            g_optEditorCtrls[1] = CreateWindowW(L"BUTTON", L"Popup on SQL error",
                WS_CHILD | BS_AUTOCHECKBOX,
                x, y + 22, 200, 20, g_hwndOptTab, (HMENU)IDC_OPT_ERRORMSGBOX, g_hInst, NULL);
            g_optEditorCtrls[2] = CreateWindowW(L"BUTTON", L"Remember last query path on exit",
                WS_CHILD | BS_AUTOCHECKBOX,
                x, y + 44, 220, 20, g_hwndOptTab, (HMENU)IDC_OPT_REMEMBERQDIR, g_hInst, NULL);
            
            /* Results tab controls */
            g_optResultsCtrls[0] = CreateWindowW(L"BUTTON", L"Append text output",
                WS_CHILD | BS_AUTOCHECKBOX,
                x, y, 200, 20, g_hwndOptTab, (HMENU)IDC_OPT_CLEAREXEC, g_hInst, NULL);
            g_optResultsCtrls[1] = CreateWindowW(L"BUTTON", L"Auto-size grid columns",
                WS_CHILD | BS_AUTOCHECKBOX,
                x, y + 22, 200, 20, g_hwndOptTab, (HMENU)IDC_OPT_GRIDAUTOSIZE, g_hInst, NULL);
            
            /* Set initial values */
            SendMessage(g_optGeneralCtrls[0], BM_SETCHECK, g_optStartLastDb, 0);
            SendMessage(g_optResultsCtrls[0], BM_SETCHECK, !g_optClearExec, 0);
            SendMessage(g_optResultsCtrls[1], BM_SETCHECK, g_optGridAutoSize, 0);
            SendMessage(g_optEditorCtrls[0], BM_SETCHECK, g_optLineNums, 0);
            SendMessage(g_optEditorCtrls[1], BM_SETCHECK, g_optErrorMsgBox, 0);
            SendMessage(g_optEditorCtrls[2], BM_SETCHECK, g_optRememberQueryDir, 0);
            SendMessage(g_optStorageCtrls[4], BM_SETCHECK, g_optCustomCard, 0);
            SendMessage(g_optStorageCtrls[7], BM_SETCHECK, g_optStorageCardData, 0);
            SendMessage(g_optStorageCtrls[8], BM_SETCHECK, g_optStorageCard, 0);
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
            if (LOWORD(wParam) == IDC_OPT_CUSTOMCARD) {
                UpdateCardPathVisibility();
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
    g_optGridAutoSize = g_gridAutoSize;
    g_optLineNums = g_showLineNumbers;
    g_optErrorMsgBox = g_showErrorMsgBox;
    g_optRememberQueryDir = g_rememberQueryDir;
    g_optStartLastDb = g_startWithLastDb;
    g_optStorageCard = g_useStorageCard;
    g_optStorageCardData = g_useStorageCardData;
    g_optCustomCard = g_useCustomCardPath;
    g_optRetention = g_backupRetention;
    lstrcpyW(g_optLocalPath, g_szLocalBasePath);
    lstrcpyW(g_optCardPath, g_szCardBasePath);
    lstrcpyW(g_optCardRoot, g_szStorageCardRoot);
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
        g_gridAutoSize = g_optGridAutoSize;
        g_showErrorMsgBox = g_optErrorMsgBox;
        g_startWithLastDb = g_optStartLastDb;
        g_useStorageCard = g_optStorageCard;
        g_useStorageCardData = g_optStorageCardData;
        g_useCustomCardPath = g_optCustomCard;
        g_backupRetention = g_optRetention;
        lstrcpyW(g_szLocalBasePath, g_optLocalPath);
        lstrcpyW(g_szCardBasePath, g_optCardPath);
        lstrcpyW(g_szStorageCardRoot, g_optCardRoot);
        
        /* Reset query dir to default if option turned off */
        if (g_rememberQueryDir && !g_optRememberQueryDir) {
            if (GetFileAttributesW(L"\\My Documents") != 0xFFFFFFFF)
                lstrcpyW(g_szLastQueryDir, L"\\My Documents");
            else
                lstrcpyW(g_szLastQueryDir, L"\\");
        }
        g_rememberQueryDir = g_optRememberQueryDir;
        
        /* Handle line numbers toggle */
        if (g_optLineNums != g_showLineNumbers) {
            g_showLineNumbers = g_optLineNums;
            ShowWindow(g_hwndLineNum, (g_viewMode == 0 && g_showLineNumbers) ? SW_SHOW : SW_HIDE);
            SendMessage(g_hwndMain, WM_SIZE, 0, 0);
        }
    }
}
