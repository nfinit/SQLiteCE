/*
** SQLiteCEdit - Centralized settings (registry persistence)
**
** All persistent settings are defined here. To add a new setting:
** 1. Add the global variable to globals.c/h
** 2. Add an entry to the appropriate SettingDef array below
** 3. Call LoadSettings() at startup, SaveSettings() on exit
*/

#include "globals.h"

#define REG_KEY L"Software\\IntermountainSystems\\SQLiteCEdit"

static int g_settingsCleared = 0;  /* Block save after clear */

/*============================================================================
** Setting Definitions
**============================================================================*/

/*
** Setting definition structure
** Note: We use void* for pValue since VC++ 6.0 doesn't support C99 unions.
** Type safety is enforced by separating int and string settings into
** different arrays and using the helper macros below.
*/
typedef enum { SET_INT, SET_STR } SettingType;

typedef struct {
    const wchar_t *name;
    SettingType type;
    void *pValue;       /* Pointer to global: int* for SET_INT, wchar_t* for SET_STR */
    int defaultInt;     /* Default for SET_INT */
    const wchar_t *defaultStr;  /* Default for SET_STR */
} SettingDef;

/*
** Helper macros for type-safe setting definitions
** INT_SETTING: defines an integer setting (pValue must be int*)
** STR_SETTING: defines a string setting (pValue must be wchar_t[])
*/
#define INT_SETTING(regname, var, def) \
    { L##regname, SET_INT, (void*)&(var), (def), NULL }

#define STR_SETTING(regname, var, def) \
    { L##regname, SET_STR, (void*)(var), 0, L##def }

#define END_SETTINGS \
    { NULL, 0, NULL, 0, NULL }

/* Integer settings - all pValue entries are int* */
static SettingDef g_intSettings[] = {
    INT_SETTING("ClearOnExec",      g_clearOnExec,      1),
    INT_SETTING("ExecAtCursor",     g_execAtCursor,     0),
    INT_SETTING("ShowLineNumbers",  g_showLineNumbers,  1),
    INT_SETTING("ShowStatusBar",    g_showStatusBar,    1),
    INT_SETTING("ShowErrorMsgBox",  g_showErrorMsgBox,  0),
    INT_SETTING("ShowSizes",        g_showSizes,        0),
    INT_SETTING("GridAutoSize",     g_gridAutoSize,     1),
    INT_SETTING("UseStorageCard",   g_useStorageCard,   0),
    INT_SETTING("UseStorageCardData", g_useStorageCardData, 0),
    END_SETTINGS
};

/* String settings - all pValue entries are wchar_t[] */
static SettingDef g_strSettings[] = {
    STR_SETTING("DefaultDbPath",  g_szDefaultDbPath,  "\\My Documents\\Data"),
    STR_SETTING("DataRelPath",    g_szDataRelPath,    "\\Data"),
    STR_SETTING("LocalBasePath",  g_szLocalBasePath,  "\\My Documents"),
    STR_SETTING("CardBasePath",   g_szCardBasePath,   ""),
    END_SETTINGS
};

/*============================================================================
** Registry Helpers
** Note: CE 2.0 uses simplified registry API without security attributes
**============================================================================*/

static HKEY OpenSettingsKey(int forWrite) {
    HKEY hKey = NULL;
    DWORD disp;
    if (forWrite) {
        RegCreateKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, NULL, 0, 0, NULL, &hKey, &disp);
    } else {
        RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, 0, &hKey);
    }
    return hKey;
}

static int ReadRegInt(HKEY hKey, const wchar_t *name, int defaultVal) {
    DWORD val, size = sizeof(DWORD), type;
    if (RegQueryValueExW(hKey, name, NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS && type == REG_DWORD)
        return (int)val;
    return defaultVal;
}

static void WriteRegInt(HKEY hKey, const wchar_t *name, int val) {
    DWORD dw = (DWORD)val;
    RegSetValueExW(hKey, name, 0, REG_DWORD, (LPBYTE)&dw, sizeof(DWORD));
}

static void ReadRegStr(HKEY hKey, const wchar_t *name, wchar_t *buf, int bufLen, const wchar_t *defaultVal) {
    DWORD size = bufLen * sizeof(wchar_t), type;
    if (RegQueryValueExW(hKey, name, NULL, &type, (LPBYTE)buf, &size) != ERROR_SUCCESS || type != REG_SZ) {
        if (defaultVal) {
            int i;
            for (i = 0; i < bufLen - 1 && defaultVal[i]; i++) buf[i] = defaultVal[i];
            buf[i] = 0;
        } else {
            buf[0] = 0;
        }
    }
}

static void WriteRegStr(HKEY hKey, const wchar_t *name, const wchar_t *val) {
    RegSetValueExW(hKey, name, 0, REG_SZ, (LPBYTE)val, (lstrlenW(val) + 1) * sizeof(wchar_t));
}

/*============================================================================
** Recent Files Persistence
**============================================================================*/

static void LoadRecentList(HKEY hKey, const wchar_t *prefix, wchar_t list[][MAX_PATH], int *count, int max) {
    wchar_t name[32];
    int i;
    *count = 0;
    for (i = 0; i < max; i++) {
        wsprintfW(name, L"%s%d", prefix, i);
        ReadRegStr(hKey, name, list[i], MAX_PATH, NULL);
        if (list[i][0]) (*count)++;
        else break;
    }
}

static void SaveRecentList(HKEY hKey, const wchar_t *prefix, wchar_t list[][MAX_PATH], int count, int max) {
    wchar_t name[32];
    int i;
    for (i = 0; i < max; i++) {
        wsprintfW(name, L"%s%d", prefix, i);
        if (i < count && list[i][0])
            WriteRegStr(hKey, name, list[i]);
        else
            RegDeleteValueW(hKey, name);
    }
}

/*============================================================================
** Public API
**============================================================================*/

void LoadSettings(void) {
    HKEY hKey;
    SettingDef *s;
    
    hKey = OpenSettingsKey(0);  /* Read */
    if (!hKey) return;
    
    /* Load integer settings */
    for (s = g_intSettings; s->name; s++) {
        *(int *)s->pValue = ReadRegInt(hKey, s->name, s->defaultInt);
    }
    
    /* Load string settings */
    for (s = g_strSettings; s->name; s++) {
        ReadRegStr(hKey, s->name, (wchar_t *)s->pValue, MAX_PATH, s->defaultStr);
    }
    
    /* Load recent files */
    LoadRecentList(hKey, L"RecentDb", g_recentFiles, &g_recentCount, MAX_RECENT_FILES);
    LoadRecentList(hKey, L"RecentQuery", g_recentQueries, &g_recentQueryCount, MAX_RECENT_FILES);
    
    RegCloseKey(hKey);
}

void SaveSettings(void) {
    HKEY hKey;
    SettingDef *s;
    
    if (g_settingsCleared) return;  /* Don't re-save after clear */
    
    hKey = OpenSettingsKey(1);  /* Write */
    if (!hKey) return;
    
    /* Save integer settings */
    for (s = g_intSettings; s->name; s++) {
        WriteRegInt(hKey, s->name, *(int *)s->pValue);
    }
    
    /* Save string settings */
    for (s = g_strSettings; s->name; s++) {
        WriteRegStr(hKey, s->name, (wchar_t *)s->pValue);
    }
    
    /* Save recent files */
    SaveRecentList(hKey, L"RecentDb", g_recentFiles, g_recentCount, MAX_RECENT_FILES);
    SaveRecentList(hKey, L"RecentQuery", g_recentQueries, g_recentQueryCount, MAX_RECENT_FILES);
    
    RegCloseKey(hKey);
}

#define REG_PARENT L"Software\\IntermountainSystems"

void ClearSettings(void) {
    HKEY hKey;
    wchar_t name[256];
    DWORD nameLen;
    
    /* Open and enumerate all values to delete them */
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, 0, &hKey) == ERROR_SUCCESS) {
        /* Delete values one at a time (index 0 each time since list shrinks) */
        while (1) {
            nameLen = 256;
            if (RegEnumValueW(hKey, 0, name, &nameLen, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
                break;
            RegDeleteValueW(hKey, name);
        }
        RegCloseKey(hKey);
    }
    
    /* Now delete the empty key */
    RegDeleteKeyW(HKEY_CURRENT_USER, REG_KEY);
    
    /* Try to delete parent if empty (will fail silently if not empty) */
    RegDeleteKeyW(HKEY_CURRENT_USER, REG_PARENT);
    
    g_settingsCleared = 1;  /* Block save on exit */
}
