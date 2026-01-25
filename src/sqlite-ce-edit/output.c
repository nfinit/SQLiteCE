/*
** SQLiteCEdit - Output buffer management
*/

#include "globals.h"
#include "allocators.h"

void ClearOutput(void) {
    g_szOutput[0] = '\0';
    g_nOutput = 0;
}

void Output(const char *sz) {
    while (*sz && g_nOutput < sizeof(g_szOutput) - 1) {
        g_szOutput[g_nOutput++] = *sz++;
    }
    g_szOutput[g_nOutput] = '\0';
}

void OutputLine(const char *sz) {
    Output(sz);
    Output("\r\n");
}

void FlushOutput(void) {
    wchar_t *wz = ALLOC(wchar_t, g_nOutput + 1);
    if (wz) {
        MultiByteToWideChar(CP_ACP, 0, g_szOutput, -1, wz, g_nOutput + 1);
        SetWindowTextW(g_hwndResult, wz);
        LocalFree(wz);
    }
}

void ShowResultMessage(LPCWSTR msg, int clear) {
    if (clear) {
        g_lastResultRows = 0;
        SetWindowTextW(g_hwndResult, msg);
    } else {
        /* Append to existing text */
        int existingLen = GetWindowTextLengthW(g_hwndResult);
        int msgLen = lstrlenW(msg);
        wchar_t *buf = ALLOC(wchar_t, existingLen + msgLen + 3);
        if (buf) {
            GetWindowTextW(g_hwndResult, buf, existingLen + 1);
            if (existingLen > 0) {
                buf[existingLen++] = '\r';
                buf[existingLen++] = '\n';
            }
            lstrcpyW(buf + existingLen, msg);
            SetWindowTextW(g_hwndResult, buf);
            LocalFree(buf);
        }
    }
    /* Switch to text results view */
    SwitchView(VIEW_RESULT);
    if (g_gridView && g_hwndGrid) {
        ShowWindow(g_hwndGrid, SW_HIDE);
        ShowWindow(g_hwndResult, SW_SHOW);
    }
    SendMessage(g_hwndCB, TB_ENABLEBUTTON, IDM_EXECATCURSOR, FALSE);
}

/*
** Centralized error reporting.
** Severity levels:
**   ERR_INFO    - Informational message (no icon in msgbox, or log only)
**   ERR_WARNING - Warning (exclamation icon)
**   ERR_ERROR   - Error (stop icon)
**   ERR_FATAL   - Fatal error (stop icon, always shows msgbox)
**
** showMsgBox controls whether to show a message box:
**   0 = log to result pane only
**   1 = show message box
*/
void ReportError(int severity, const wchar_t *context, const wchar_t *message, int showMsgBox) {
    wchar_t buf[512];
    UINT mbIcon;
    const wchar_t *prefix;

    /* Determine icon and prefix based on severity */
    switch (severity) {
        case ERR_INFO:
            mbIcon = MB_ICONINFORMATION;
            prefix = L"";
            break;
        case ERR_WARNING:
            mbIcon = MB_ICONWARNING;
            prefix = L"Warning: ";
            break;
        case ERR_FATAL:
            showMsgBox = 1;  /* Always show msgbox for fatal errors */
            /* Fall through */
        case ERR_ERROR:
        default:
            mbIcon = MB_ICONERROR;
            prefix = L"Error: ";
            break;
    }

    /* Build message */
    if (message && message[0]) {
        wsprintfW(buf, L"%s%s", prefix, message);
    } else {
        wsprintfW(buf, L"%sUnknown error", prefix);
    }

    /* Show message box if requested */
    if (showMsgBox) {
        MessageBoxW(g_hwndMain, message, context ? context : L"SQLite/CE", MB_OK | mbIcon);
    }

    /* Also log to result pane for reference */
    if (severity >= ERR_WARNING) {
        ShowResultMessage(buf, 0);
    }
}
