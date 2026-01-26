/*
** SQLiteCEdit - Custom File Picker
** Replaces GetOpenFileNameW/GetSaveFileNameW with keyboard-friendly dialog
*/

#include "globals.h"
#include "constants.h"

/*============================================================================
** File Picker State
**============================================================================*/

static HWND g_hwndPicker = NULL;
static HWND g_hwndList = NULL;
static HWND g_hwndPath = NULL;
static HWND g_hwndFilename = NULL;
static HWND g_hwndOK = NULL;
static HWND g_hwndCancel = NULL;
static wchar_t g_pickerDir[MAX_PATH];
static wchar_t g_pickerResult[MAX_PATH];
static const wchar_t *g_pickerFilter = NULL;
static const wchar_t *g_pickerDefExt = NULL;
static int g_pickerSaveMode = 0;
static int g_pickerOK = 0;
static int g_pickerDone = 0;
static WNDPROC g_pfnListProc = NULL;
static WNDPROC g_pfnEditProc = NULL;
static HWND g_hwndFilter = NULL;

/* Type-ahead buffer - constants defined in constants.h */
static wchar_t g_typeAhead[TYPEAHEAD_MAX + 1];
static int g_typeAheadLen = 0;

/* Forward declarations */
static void PopulateFilterCombo(const wchar_t *filter);

/*============================================================================
** Helper: Get extension from current filter selection
**============================================================================*/

static void GetCurrentFilterExt(wchar_t *ext, int maxLen) {
    wchar_t item[64];
    ext[0] = 0;
    if (!g_hwndFilter) return;

    GetWindowTextW(g_hwndFilter, item, 64);
    /* Item is like "*.csv" or "*.*" */
    if (item[0] == '*' && item[1] == '.') {
        int i = 0;
        const wchar_t *p = item + 2;
        while (*p && i < maxLen - 1) {
            if (*p == '*') { ext[0] = 0; return; }  /* *.* means all */
            ext[i++] = *p++;
        }
        ext[i] = 0;
    }
}

/*============================================================================
** Populate filter combobox from filter string
**============================================================================*/

static void PopulateFilterCombo(const wchar_t *filter) {
    const wchar_t *p = filter;
    if (!filter || !g_hwndFilter) return;

    SendMessageW(g_hwndFilter, CB_RESETCONTENT, 0, 0);

    /* Filter format: "Desc\0*.ext\0Desc2\0*.ext2\0\0" */
    while (*p) {
        /* Skip description */
        while (*p) p++;
        p++;
        if (!*p) break;
        /* Add pattern (e.g., "*.csv") */
        SendMessageW(g_hwndFilter, CB_ADDSTRING, 0, (LPARAM)p);
        while (*p) p++;
        p++;
    }

    /* Add "All files" option */
    SendMessageW(g_hwndFilter, CB_ADDSTRING, 0, (LPARAM)L"*.*");

    /* Select first item */
    SendMessageW(g_hwndFilter, CB_SETCURSEL, 0, 0);
}

/*============================================================================
** Populate file list for current directory
** PICKER_PICKER_MAX_ENTRIES defined in constants.h
**============================================================================*/

static void SortAndAdd(wchar_t entries[][MAX_PATH], int count, int bracket) {
    int i, j;
    wchar_t temp[MAX_PATH];
    /* Simple insertion sort */
    for (i = 1; i < count; i++) {
        lstrcpyW(temp, entries[i]);
        j = i - 1;
        while (j >= 0 && lstrcmpiW(entries[j], temp) > 0) {
            lstrcpyW(entries[j + 1], entries[j]);
            j--;
        }
        lstrcpyW(entries[j + 1], temp);
    }
    /* Add to listbox */
    for (i = 0; i < count; i++) {
        if (bracket) {
            wchar_t item[MAX_PATH];
            wsprintfW(item, L"[%s]", entries[i]);
            SendMessageW(g_hwndList, LB_ADDSTRING, 0, (LPARAM)item);
        } else {
            SendMessageW(g_hwndList, LB_ADDSTRING, 0, (LPARAM)entries[i]);
        }
    }
}

static void PopulateFileList(void) {
    WIN32_FIND_DATAW fd;
    HANDLE hFind;
    wchar_t pattern[MAX_PATH];
    wchar_t ext[32];
    int atRoot;
    static wchar_t entries[PICKER_MAX_ENTRIES][MAX_PATH];
    int count;

    /* Reset type-ahead on directory change */
    g_typeAheadLen = 0;
    g_typeAhead[0] = 0;

    SendMessageW(g_hwndList, LB_RESETCONTENT, 0, 0);

    atRoot = (lstrcmpW(g_pickerDir, L"\\") == 0);

    /* Add [.] for save mode - confirms with current filename */
    if (g_pickerSaveMode) {
        SendMessageW(g_hwndList, LB_ADDSTRING, 0, (LPARAM)L"[.]");
    }

    /* Add parent directory entry if not at root */
    if (!atRoot) {
        SendMessageW(g_hwndList, LB_ADDSTRING, 0, (LPARAM)L"[..]");
    }

    /* Collect subdirectories */
    count = 0;
    if (atRoot) {
        lstrcpyW(pattern, L"\\*");
    } else {
        wsprintfW(pattern, L"%s\\*", g_pickerDir);
    }
    hFind = FindFirstFileW(pattern, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                fd.cFileName[0] != '.' && count < PICKER_MAX_ENTRIES) {
                lstrcpyW(entries[count++], fd.cFileName);
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    SortAndAdd(entries, count, 1);

    /* Collect files matching filter */
    count = 0;
    GetCurrentFilterExt(ext, 32);
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
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && count < PICKER_MAX_ENTRIES) {
                lstrcpyW(entries[count++], fd.cFileName);
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    SortAndAdd(entries, count, 0);

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
        /* Special entries */
        if (lstrcmpW(item, L"[.]") == 0) {
            /* Save to current directory - simulate OK button */
            SendMessageW(g_hwndPicker, WM_COMMAND, IDOK, 0);
            return;
        }
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
** Type-ahead: jump to first file matching typed prefix
**============================================================================*/

static int MatchPrefix(const wchar_t *item, const wchar_t *prefix, int prefixLen) {
    const wchar_t *p = item;
    int i;
    wchar_t ic, pc;

    /* Skip directory brackets */
    if (*p == '[') p++;

    for (i = 0; i < prefixLen && *p; i++, p++) {
        ic = *p; pc = prefix[i];
        if (ic >= 'a' && ic <= 'z') ic -= 32;
        if (pc >= 'a' && pc <= 'z') pc -= 32;
        if (ic != pc) return 0;
    }
    return (i == prefixLen);
}

static void OnTypeAhead(wchar_t ch) {
    int count, i, start;
    wchar_t item[MAX_PATH];

    /* Append to buffer */
    if (g_typeAheadLen < TYPEAHEAD_MAX) {
        g_typeAhead[g_typeAheadLen++] = ch;
        g_typeAhead[g_typeAheadLen] = 0;
    }

    /* Reset timer */
    KillTimer(g_hwndPicker, TYPEAHEAD_TIMER_ID);
    SetTimer(g_hwndPicker, TYPEAHEAD_TIMER_ID, TYPEAHEAD_TIMEOUT, NULL);

    count = (int)SendMessageW(g_hwndList, LB_GETCOUNT, 0, 0);
    start = (int)SendMessageW(g_hwndList, LB_GETCURSEL, 0, 0);
    if (start < 0) start = 0;

    /* Search from start, then wrap */
    for (i = 0; i < count; i++) {
        int idx = (start + i) % count;
        SendMessageW(g_hwndList, LB_GETTEXT, idx, (LPARAM)item);
        if (MatchPrefix(item, g_typeAhead, g_typeAheadLen)) {
            SendMessageW(g_hwndList, LB_SETCURSEL, idx, 0);
            return;
        }
    }
}

/*============================================================================
** Tab navigation helper
**============================================================================*/

static void TabNext(HWND from) {
    if (from == g_hwndList) SetFocus(g_hwndFilename);
    else if (from == g_hwndFilename) SetFocus(g_hwndFilter);
    else if (from == g_hwndFilter) SetFocus(g_hwndOK);
    else if (from == g_hwndOK) SetFocus(g_hwndCancel);
    else SetFocus(g_hwndList);
}

static void TabPrev(HWND from) {
    if (from == g_hwndList) SetFocus(g_hwndCancel);
    else if (from == g_hwndFilename) SetFocus(g_hwndList);
    else if (from == g_hwndFilter) SetFocus(g_hwndFilename);
    else if (from == g_hwndOK) SetFocus(g_hwndFilter);
    else SetFocus(g_hwndOK);
}

/*============================================================================
** Combobox subclass for Tab/Enter/Escape handling
**============================================================================*/

static WNDPROC g_pfnComboProc = NULL;

static LRESULT CALLBACK PickerComboProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_TAB) {
            if (GetKeyState(VK_SHIFT) & 0x8000)
                TabPrev(hwnd);
            else
                TabNext(hwnd);
            return 0;
        }
        if (wParam == VK_RETURN) {
            SendMessageW(g_hwndPicker, WM_COMMAND, IDOK, 0);
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            g_pickerOK = 0;
            PostMessage(g_hwndPicker, WM_CLOSE, 0, 0);
            return 0;
        }
    }
    if (msg == WM_CHAR && (wParam == '\t' || wParam == '\r'))
        return 0;
    return CallWindowProc(g_pfnComboProc, hwnd, msg, wParam, lParam);
}

/*============================================================================
** Edit subclass for Tab/Enter handling
**============================================================================*/

static LRESULT CALLBACK PickerEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_TAB) {
            if (GetKeyState(VK_SHIFT) & 0x8000)
                TabPrev(hwnd);
            else
                TabNext(hwnd);
            return 0;
        }
        if (wParam == VK_RETURN) {
            SendMessageW(g_hwndPicker, WM_COMMAND, IDOK, 0);
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            g_pickerOK = 0;
            PostMessage(g_hwndPicker, WM_CLOSE, 0, 0);
            return 0;
        }
    }
    if (msg == WM_CHAR && (wParam == '\t' || wParam == '\r'))
        return 0;
    return CallWindowProc(g_pfnEditProc, hwnd, msg, wParam, lParam);
}

/*============================================================================
** Button subclass for Tab handling
**============================================================================*/

static WNDPROC g_pfnBtnProc = NULL;

static LRESULT CALLBACK PickerBtnProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_TAB) {
            if (GetKeyState(VK_SHIFT) & 0x8000)
                TabPrev(hwnd);
            else
                TabNext(hwnd);
            return 0;
        }
        if (wParam == VK_RETURN) {
            /* Trigger the focused button */
            SendMessageW(g_hwndPicker, WM_COMMAND, GetDlgCtrlID(hwnd), 0);
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            g_pickerOK = 0;
            PostMessage(g_hwndPicker, WM_CLOSE, 0, 0);
            return 0;
        }
    }
    if (msg == WM_CHAR && wParam == '\r')
        return 0;
    return CallWindowProc(g_pfnBtnProc, hwnd, msg, wParam, lParam);
}

/*============================================================================
** List subclass for keyboard handling
**============================================================================*/

static LRESULT CALLBACK PickerListProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_TAB) {
            if (GetKeyState(VK_SHIFT) & 0x8000)
                TabPrev(hwnd);
            else
                TabNext(hwnd);
            return 0;
        }
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

                    /* Add extension from filter if missing */
                    {
                        wchar_t *p = g_pickerResult + lstrlenW(g_pickerResult);
                        wchar_t ext[32];
                        int hasExt = 0;
                        while (p > g_pickerResult && *p != '\\') {
                            if (*p == '.') { hasExt = 1; break; }
                            p--;
                        }
                        if (!hasExt) {
                            GetCurrentFilterExt(ext, 32);
                            if (ext[0]) {
                                lstrcatW(g_pickerResult, L".");
                                lstrcatW(g_pickerResult, ext);
                            } else if (g_pickerDefExt && g_pickerDefExt[0]) {
                                lstrcatW(g_pickerResult, L".");
                                lstrcatW(g_pickerResult, g_pickerDefExt);
                            }
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
            /* Filter combobox change - refresh file list */
            if (cmd == 103 && notify == CBN_SELCHANGE) {
                PopulateFileList();
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
        case WM_TIMER:
            if (wParam == TYPEAHEAD_TIMER_ID) {
                KillTimer(hwnd, TYPEAHEAD_TIMER_ID);
                g_typeAheadLen = 0;
                g_typeAhead[0] = 0;
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
    int dlgW = 360, dlgH = 195;
    int filterW = 70;

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

        /* Strip extension from pre-filled filename */
        {
            wchar_t *dot = NULL, *s = g_pickerResult;
            while (*s) {
                if (*s == '.') dot = s;
                s++;
            }
            if (dot) *dot = 0;
        }
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
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
        10, 34, dlgW - 20, 80, g_hwndPicker, (HMENU)101, g_hInst, NULL);

    /* Subclass listbox */
    g_pfnListProc = (WNDPROC)SetWindowLong(g_hwndList, GWL_WNDPROC, (LONG)PickerListProc);

    /* Filename edit */
    g_hwndFilename = CreateWindowW(L"EDIT", g_pickerResult,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        10, 116, dlgW - filterW - 25, 22, g_hwndPicker, (HMENU)102, g_hInst, NULL);

    /* Subclass edit */
    g_pfnEditProc = (WNDPROC)SetWindowLong(g_hwndFilename, GWL_WNDPROC, (LONG)PickerEditProc);

    /* Filter combobox */
    g_hwndFilter = CreateWindowW(L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWNLIST,
        dlgW - filterW - 10, 116, filterW, 100, g_hwndPicker, (HMENU)103, g_hInst, NULL);
    PopulateFilterCombo(filter);

    /* Subclass combobox */
    g_pfnComboProc = (WNDPROC)SetWindowLong(g_hwndFilter, GWL_WNDPROC, (LONG)PickerComboProc);

    /* Buttons */
    g_hwndOK = CreateWindowW(L"BUTTON", saveMode ? L"Save" : L"Open",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        dlgW - 160, 140, 70, 22, g_hwndPicker, (HMENU)IDOK, g_hInst, NULL);
    g_hwndCancel = CreateWindowW(L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE,
        dlgW - 80, 140, 70, 22, g_hwndPicker, (HMENU)IDCANCEL, g_hInst, NULL);

    /* Subclass buttons */
    g_pfnBtnProc = (WNDPROC)SetWindowLong(g_hwndOK, GWL_WNDPROC, (LONG)PickerBtnProc);
    SetWindowLong(g_hwndCancel, GWL_WNDPROC, (LONG)PickerBtnProc);

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
