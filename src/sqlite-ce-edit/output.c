/*
** SQLiteCEdit - Output buffer management
*/

#include "globals.h"

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
    wchar_t *wz = (wchar_t *)LocalAlloc(LMEM_FIXED, (g_nOutput + 1) * sizeof(wchar_t));
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
        wchar_t *buf = (wchar_t *)LocalAlloc(LMEM_FIXED, (existingLen + msgLen + 3) * sizeof(wchar_t));
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
