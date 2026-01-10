/*
** SQLiteCEdit - Shared globals and declarations
*/

#ifndef GLOBALS_H
#define GLOBALS_H

#include <windows.h>
#include <commctrl.h>
#define WINCEOEM 1
#include <windbase.h>
#include "sqlite.h"

/*============================================================================
** Common File Dialog (may not be in CE 2.0 SDK headers)
**============================================================================*/

#define OFN_FILEMUSTEXIST   0x00001000
#define OFN_PATHMUSTEXIST   0x00000800
#define OFN_HIDEREADONLY    0x00000004
#define OFN_OVERWRITEPROMPT 0x00000002

typedef struct tagOFN {
    DWORD         lStructSize;
    HWND          hwndOwner;
    HINSTANCE     hInstance;
    LPCWSTR       lpstrFilter;
    LPWSTR        lpstrCustomFilter;
    DWORD         nMaxCustFilter;
    DWORD         nFilterIndex;
    LPWSTR        lpstrFile;
    DWORD         nMaxFile;
    LPWSTR        lpstrFileTitle;
    DWORD         nMaxFileTitle;
    LPCWSTR       lpstrInitialDir;
    LPCWSTR       lpstrTitle;
    DWORD         Flags;
    WORD          nFileOffset;
    WORD          nFileExtension;
    LPCWSTR       lpstrDefExt;
    LPARAM        lCustData;
    void*         lpfnHook;
    LPCWSTR       lpTemplateName;
} CE_OPENFILENAME;

BOOL WINAPI GetOpenFileNameW(CE_OPENFILENAME*);
BOOL WINAPI GetSaveFileNameW(CE_OPENFILENAME*);

/* View toolbar icons (may not be in CE 2.0 headers) */
#ifndef IDB_VIEW_SMALL_COLOR
#define IDB_VIEW_SMALL_COLOR 4
#define VIEW_LIST    2
#define VIEW_DETAILS 3
#endif

/*============================================================================
** Version
**============================================================================*/

#define SQLITECEDIT_VERSION L"0.4.0"

/*============================================================================
** Menu IDs
**============================================================================*/

#define IDM_NEW      101
#define IDM_OPEN     102
#define IDM_CLOSE    103
#define IDM_EXIT     104
#define IDM_OPENQUERY  105
#define IDM_SAVEQUERY  106
#define IDM_EXPORTCSV  107
#define IDM_EXPORTDB   108
#define IDM_IMPORTCSV  109
#define IDM_IMPORTCEDB 110
#define IDM_EXECUTE  201
#define IDM_FIND     202
#define IDM_FINDNEXT 203
#define IDM_EXECATCURSOR 204
#define IDM_CLEAR    301
#define IDM_VIEWQUERY  401
#define IDM_VIEWRESULT 402
#define IDM_FONTSIZE   403
#define IDM_ABOUT    501

/*============================================================================
** Resource IDs
**============================================================================*/

#define IDI_MAIN    1
#define IDB_TOOLBAR 100
#define IDB_LOGO    101

/*============================================================================
** Toolbar bitmap indices
**============================================================================*/

#define TB_PLAY    0
#define TB_QUERY   1
#define TB_RESULTS 2
#define TB_GRID    3
#define TB_OPEN    4
#define TB_CLOSE   5
#define TB_NEW     6
#define TB_STD_BASE 7
#define TB_VIEW_BASE (TB_STD_BASE + 15)

/*============================================================================
** Global Variables (defined in globals.c)
**============================================================================*/

extern HINSTANCE g_hInst;
extern HWND g_hwndMain;
extern HWND g_hwndCB;
extern HWND g_hwndStatus;
extern HMENU g_hMenu;
extern HACCEL g_hAccel;
extern HBRUSH g_hBrushWhite;
extern HWND g_hwndQuery;
extern HWND g_hwndResult;
extern HWND g_hwndLineNum;
extern sqlite *g_db;
extern wchar_t g_szDbPath[MAX_PATH];
extern HFONT g_hFontQuery;
extern HFONT g_hFontResult;
extern int g_fontSizes[4];
extern int g_fontSizeQuery;
extern int g_fontSizeResult;
extern int g_showLineNumbers;
extern int g_lineNumWidth;
extern WNDPROC g_pfnQueryProc;
extern WNDPROC g_pfnResultProc;
extern WNDPROC g_pfnLineNumProc;

/* Output buffer */
extern char g_szOutput[32000];
extern int g_nOutput;

/* State flags */
extern int g_clearOnExec;
extern int g_viewMode;
extern int g_showingHint;
extern int g_suppressLineCount;
extern int g_execAtCursor;
extern wchar_t g_lastResultStatus[64];
extern wchar_t g_findText[128];
extern int g_searchMode;
extern wchar_t g_szQueryPath[MAX_PATH];
extern int g_queryDirty;

/*============================================================================
** Function Declarations - Output (output.c)
**============================================================================*/

void ClearOutput(void);
void Output(const char *sz);
void OutputLine(const char *sz);
void FlushOutput(void);

/*============================================================================
** Function Declarations - Editor (editor.c)
**============================================================================*/

void UpdateLineCount(void);
void UpdateLineNumbers(void);
void SyncLineNumScroll(void);
void SwitchView(int mode);
void UpdateQueryFont(void);
void UpdateResultFont(void);
void CycleFontSize(void);
LRESULT CALLBACK LineNumProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK QueryEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ResultEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/*============================================================================
** Function Declarations - Database (database.c)
**============================================================================*/

void SetStatusDb(const wchar_t *sz);
void SetStatusResult(const wchar_t *sz);
void UpdateDbSize(void);
void UpdateTitle(void);
void CloseDatabase(void);
int OpenDatabase(const wchar_t *path);
const wchar_t *GetFilename(const wchar_t *path);

/*============================================================================
** Function Declarations - Execute (execute.c)
**============================================================================*/

void ExecuteQuery(void);

/*============================================================================
** Function Declarations - File Operations (fileops.c)
**============================================================================*/

void DoFileNew(void);
void DoFileOpen(void);
void DoOpenQuery(void);
void DoSaveQuery(void);
void DoExportCSV(void);
void DoExportDb(void);
void DoImportCSV(void);
void DoImportCEDB(void);

/*============================================================================
** Function Declarations - Find (find.c)
**============================================================================*/

void DoFind(void);
void DoFindNext(void);

/*============================================================================
** Function Declarations - Dialogs (dialogs.c)
**============================================================================*/

void DoAbout(void);
int PromptForPath(const wchar_t *title, const wchar_t *defPath);

#endif /* GLOBALS_H */
