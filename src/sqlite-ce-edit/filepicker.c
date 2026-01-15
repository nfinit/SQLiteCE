/*
** SQLiteCEdit - Custom File Picker
** Replaces GetOpenFileNameW/GetSaveFileNameW with keyboard-friendly dialog
*/

#include "globals.h"

/*============================================================================
** File Picker State
**============================================================================*/

static HWND g_hwndPicker = NULL;
static HWND g_hwndList = NULL;
static HWND g_hwndPath = NULL;
static HWND g_hwndFilename = NULL;
static wchar_t g_pickerDir[MAX_PATH];
static wchar_t g_pickerResult[MAX_PATH];
static const wchar_t *g_pickerFilter = NULL;
static const wchar_t *g_pickerDefExt = NULL;
static int g_pickerSaveMode = 0;
static int g_pickerOK = 0;
static int g_pickerDone = 0;
static WNDPROC g_pfnListProc = NULL;

/*============================================================================
** Helper: Extract extension filter (e.g., "*.db" from filter string)
**============================================================================*/

static void GetFilterExt(const wchar_t *filter, wchar_t *ext, int maxLen) {
    /* Filter format: "Description\0*.ext\0..." - find first *.ext */
    const wchar_t *p = filter;
    ext[0] = 0;
    if (!p) return;
    
    /* Skip description */
    while (*p) p++;
    p++;
    
    /* Copy extension pattern */
    if (*p == '*' && *(p+1) == '.') {
        int i = 0;
        p += 2;  /* Skip *. */
        while (*p && *p != '\0' && i < maxLen - 1) {
            ext[i++] = *p++;
        }
        ext[i] = 0;
    }
}

/*============================================================================
** Populate file list for current directory
**============================================================================*/

static void PopulateFileList(void) {
    WIN32_FIND_DATAW fd;
    HANDLE hFind;
    wchar_t pattern[MAX_PATH];
    wchar_t ext[32];
    int atRoot;
    
    SendMessageW(g_hwndList, LB_RESETCONTENT, 0, 0);
    
    atRoot = (lstrcmpW(g_pickerDir, L"\\") == 0);
    
    /* Add parent directory entry if not at root */
    if (!atRoot) {
        SendMessageW(g_hwndList, LB_ADDSTRING, 0, (LPARAM)L"[..]");
    }
    
    /* Add subdirectories */
    if (atRoot) {
        lstrcpyW(pattern, L"\\*");
    } else {
        wsprintfW(pattern, L"%s\\*", g_pickerDir);
    }
    hFind = FindFirstFileW(pattern, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                fd.cFileName[0] != '.') {
                wchar_t item[MAX_PATH];
                wsprintfW(item, L"[%s]", fd.cFileName);
                SendMessageW(g_hwndList, LB_ADDSTRING, 0, (LPARAM)item);
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    
    /* Add files matching filter */
    GetFilterExt(g_pickerFilter, ext, 32);
    if (atRoot) {
        if (ext[0]) {
            wsprintfW(pattern, L"\\*.%s", ext);
        } else {
            lstrcpyW(pattern, L"\\*.*");
        }
    } else {
        if (ext[0]) {
            wsprintfW(pattern, L"%s\\*.%s", g_pickerDir, ext);
        } else {
            wsprintfW(pattern, L"%s\\*.*", g_pickerDir);
        }
    }
    
    hFind = FindFirstFileW(pattern, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                SendMessageW(g_hwndList, LB_ADDSTRING, 0, (LPARAM)fd.cFileName);
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    
    /* Update path display */
    SetWindowTextW(g_hwndPath, g_pickerDir);
    
    /* Select first item */
    if (SendMessageW(g_hwndList, LB_GETCOUNT, 0, 0) > 0) {
        SendMessageW(g_hwndList, LB_SETCURSEL, 0, 0);
    }
}

/*============================================================================
** Navigate to selected item (folder) or select file
**============================================================================*/

static void OnItemActivate(void) {
    int sel;
    wchar_t item[MAX_PATH];
    wchar_t newPath[MAX_PATH];
    
    sel = (int)SendMessageW(g_hwndList, LB_GETCURSEL, 0, 0);
    if (sel < 0) return;
    
    SendMessageW(g_hwndList, LB_GETTEXT, sel, (LPARAM)item);
    
    if (item[0] == '[') {
        /* Directory - navigate into it */
        if (lstrcmpW(item, L"[..]") == 0) {
            /* Go up one level */
            wchar_t *p = g_pickerDir + lstrlenW(g_pickerDir) - 1;
            while (p > g_pickerDir && *p != '\\') p--;
            if (p == g_pickerDir) {
                lstrcpyW(g_pickerDir, L"\\");
            } else {
                *p = 0;
            }
        } else {
            /* Enter subdirectory - strip brackets */
            wchar_t dirName[MAX_PATH];
            int i = 0;
            const wchar_t *p = item + 1;
            while (*p && *p != ']') dirName[i++] = *p++;
            dirName[i] = 0;
            
            if (lstrcmpW(g_pickerDir, L"\\") == 0) {
                wsprintfW(newPath, L"\\%s", dirName);
            } else {
                wsprintfW(newPath, L"%s\\%s", g_pickerDir, dirName);
            }
            lstrcpyW(g_pickerDir, newPath);
        }
        PopulateFileList();
    } else {
        /* File - put in filename field and build full path */
        SetWindowTextW(g_hwndFilename, item);
        
        /* Build full path in g_pickerResult */
        if (lstrcmpW(g_pickerDir, L"\\") == 0) {
            wsprintfW(g_pickerResult, L"\\%s", item);
        } else {
            wsprintfW(g_pickerResult, L"%s\\%s", g_pickerDir, item);
        }
        
        /* In save mode, confirm overwrite */
        if (g_pickerSaveMode) {
            if (MessageBoxW(g_hwndPicker, L"File exists. Overwrite?",
                    L"Confirm", MB_YESNO | MB_ICONQUESTION) != IDYES) {
                SetFocus(g_hwndList);
                return;
            }
        }
        
        /* File selected - confirm dialog */
        g_pickerOK = 1;
        PostMessage(g_hwndPicker, WM_CLOSE, 0, 0);
    }
}

/*============================================================================
** Type-ahead: jump to first file starting with typed character
**============================================================================*/

static void OnTypeAhead(wchar_t ch) {
    int count, i, start, idx;
    wchar_t item[MAX_PATH];
    wchar_t upper = ch;
    wchar_t first;
    
    /* Convert to uppercase for comparison */
    if (upper >= 'a' && upper <= 'z') upper -= 32;
    
    count = (int)SendMessageW(g_hwndList, LB_GETCOUNT, 0, 0);
    start = (int)SendMessageW(g_hwndList, LB_GETCURSEL, 0, 0);
    if (start < 0) start = 0;
    
    /* Search from current position + 1, wrapping around */
    for (i = 1; i <= count; i++) {
        idx = (start + i) % count;
        SendMessageW(g_hwndList, LB_GETTEXT, idx, (LPARAM)item);
        
        /* Skip directory brackets */
        first = item[0];
        if (first == '[' && item[1]) first = item[1];
        if (first >= 'a' && first <= 'z') first -= 32;
        
        if (first == upper) {
            SendMessageW(g_hwndList, LB_SETCURSEL, idx, 0);
            return;
        }
    }
}

/*============================================================================
** List subclass for keyboard handling
**============================================================================*/

static LRESULT CALLBACK PickerListProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            OnItemActivate();
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            g_pickerOK = 0;
            PostMessage(g_hwndPicker, WM_CLOSE, 0, 0);
            return 0;
        }
        if (wParam == VK_BACK) {
            /* Backspace goes up one directory */
            if (lstrcmpW(g_pickerDir, L"\\") != 0) {
                wchar_t *p = g_pickerDir + lstrlenW(g_pickerDir) - 1;
                while (p > g_pickerDir && *p != '\\') p--;
                if (p == g_pickerDir) {
                    lstrcpyW(g_pickerDir, L"\\");
                } else {
                    *p = 0;
                }
                PopulateFileList();
            }
            return 0;
        }
    }
    if (msg == WM_CHAR) {
        /* Type-ahead for alphanumeric */
        if ((wParam >= 'A' && wParam <= 'Z') ||
            (wParam >= 'a' && wParam <= 'z') ||
            (wParam >= '0' && wParam <= '9')) {
            OnTypeAhead((wchar_t)wParam);
            return 0;
        }
        if (wParam == '\r') return 0;  /* Suppress beep */
    }
    return CallWindowProc(g_pfnListProc, hwnd, msg, wParam, lParam);
}

/*============================================================================
** Dialog window procedure
**============================================================================*/

static LRESULT CALLBACK PickerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND: {
            WORD cmd = LOWORD(wParam);
            WORD notify = HIWORD(wParam);
            
            if (cmd == IDOK) {
                wchar_t filename[MAX_PATH];
                GetWindowTextW(g_hwndFilename, filename, MAX_PATH);
                
                if (filename[0]) {
                    /* Build full path */
                    if (lstrcmpW(g_pickerDir, L"\\") == 0) {
                        wsprintfW(g_pickerResult, L"\\%s", filename);
                    } else {
                        wsprintfW(g_pickerResult, L"%s\\%s", g_pickerDir, filename);
                    }
                    
                    /* Add default extension if missing */
                    if (g_pickerDefExt && g_pickerDefExt[0]) {
                        wchar_t *p = g_pickerResult + lstrlenW(g_pickerResult);
                        int hasExt = 0;
                        while (p > g_pickerResult && *p != '\\') {
                            if (*p == '.') { hasExt = 1; break; }
                            p--;
                        }
                        if (!hasExt) {
                            lstrcatW(g_pickerResult, L".");
                            lstrcatW(g_pickerResult, g_pickerDefExt);
                        }
                    }
                    
                    /* In save mode, confirm overwrite if file exists */
                    if (g_pickerSaveMode) {
                        HANDLE hTest = CreateFileW(g_pickerResult, 0, 0, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                        if (hTest != INVALID_HANDLE_VALUE) {
                            CloseHandle(hTest);
                            if (MessageBoxW(hwnd, L"File exists. Overwrite?",
                                    L"Confirm", MB_YESNO | MB_ICONQUESTION) != IDYES) {
                                SetFocus(g_hwndFilename);
                                return 0;
                            }
                        }
                    }
                    
                    g_pickerOK = 1;
                }
                PostMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            if (cmd == IDCANCEL) {
                g_pickerOK = 0;
                PostMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            /* Listbox double-click */
            if (cmd == 101 && notify == LBN_DBLCLK) {
                OnItemActivate();
                return 0;
            }
            /* Listbox selection change - update filename for files */
            if (cmd == 101 && notify == LBN_SELCHANGE) {
                int sel = (int)SendMessageW(g_hwndList, LB_GETCURSEL, 0, 0);
                if (sel >= 0) {
                    wchar_t item[MAX_PATH];
                    SendMessageW(g_hwndList, LB_GETTEXT, sel, (LPARAM)item);
                    if (item[0] != '[') {
                        SetWindowTextW(g_hwndFilename, item);
                    }
                }
                return 0;
            }
            break;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                g_pickerOK = 0;
                PostMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            g_hwndPicker = NULL;
            g_pickerDone = 1;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

/*============================================================================
** Public API: Custom file picker
**============================================================================*/

int CustomFilePicker(HWND hwndOwner, wchar_t *filePath, int maxPath,
                     const wchar_t *title, const wchar_t *filter,
                     const wchar_t *defExt, const wchar_t *initialDir,
                     int saveMode) {
    WNDCLASSW wc = {0};
    MSG msg;
    RECT rc;
    int dlgW = 300, dlgH = 195;
    
    /* Initialize state */
    g_pickerResult[0] = 0;
    g_pickerFilter = filter;
    g_pickerDefExt = defExt;
    g_pickerSaveMode = saveMode;
    g_pickerOK = 0;
    g_pickerDone = 0;
    
    /* Set initial directory */
    if (initialDir && initialDir[0]) {
        lstrcpyW(g_pickerDir, initialDir);
    } else {
        lstrcpyW(g_pickerDir, L"\\");
    }
    
    /* Pre-fill filename if provided */
    if (filePath && filePath[0]) {
        /* Extract just filename if full path given */
        const wchar_t *fn = filePath;
        const wchar_t *p = filePath;
        while (*p) {
            if (*p == '\\') fn = p + 1;
            p++;
        }
        lstrcpyW(g_pickerResult, fn);
    }
    
    /* Register window class once */
    {
        static int classRegistered = 0;
        if (!classRegistered) {
            wc.lpfnWndProc = PickerWndProc;
            wc.hInstance = g_hInst;
            wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
            wc.lpszClassName = L"SQLiteCEFilePicker";
            RegisterClassW(&wc);
            classRegistered = 1;
        }
    }
    
    /* Create dialog */
    GetWindowRect(hwndOwner, &rc);
    g_hwndPicker = CreateWindowExW(0,
        L"SQLiteCEFilePicker", title ? title : L"Select File",
        WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU,
        rc.left + 20, rc.top + 10, dlgW, dlgH,
        hwndOwner, NULL, g_hInst, NULL);
    
    /* Path display (read-only) */
    g_hwndPath = CreateWindowW(L"EDIT", g_pickerDir,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY | ES_AUTOHSCROLL,
        10, 10, dlgW - 20, 22, g_hwndPicker, NULL, g_hInst, NULL);
    
    /* File listbox */
    g_hwndList = CreateWindowW(L"LISTBOX", NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_SORT,
        10, 34, dlgW - 20, 80, g_hwndPicker, (HMENU)101, g_hInst, NULL);
    
    /* Subclass listbox */
    g_pfnListProc = (WNDPROC)SetWindowLong(g_hwndList, GWL_WNDPROC, (LONG)PickerListProc);
    
    /* Filename edit */
    g_hwndFilename = CreateWindowW(L"EDIT", g_pickerResult,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        10, 110, dlgW - 20, 22, g_hwndPicker, (HMENU)102, g_hInst, NULL);
    
    /* Buttons */
    CreateWindowW(L"BUTTON", saveMode ? L"Save" : L"Open",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        dlgW - 160, 140, 70, 22, g_hwndPicker, (HMENU)IDOK, g_hInst, NULL);
    CreateWindowW(L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE,
        dlgW - 80, 140, 70, 22, g_hwndPicker, (HMENU)IDCANCEL, g_hInst, NULL);
    
    /* Populate list */
    PopulateFileList();
    
    /* Focus listbox */
    SetFocus(g_hwndList);
    
    /* Modal loop */
    EnableWindow(hwndOwner, FALSE);
    
    while (!g_pickerDone && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    
    EnableWindow(hwndOwner, TRUE);
    ShowWindow(hwndOwner, SW_SHOWNORMAL);
    SetForegroundWindow(hwndOwner);
    
    /* Copy result */
    if (g_pickerOK && g_pickerResult[0]) {
        lstrcpyW(filePath, g_pickerResult);
        return 1;
    }
    return 0;
}
