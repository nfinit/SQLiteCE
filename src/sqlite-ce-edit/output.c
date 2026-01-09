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
