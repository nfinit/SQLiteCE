/*
** SQLiteCEdit - Global variable definitions
*/

#include "globals.h"

/*============================================================================
** Global Variables
**============================================================================*/

HINSTANCE g_hInst;
HWND g_hwndMain;
HWND g_hwndCB;
HWND g_hwndStatus;
HMENU g_hMenu;
HMENU g_hViewMenu;
HMENU g_hRecentDbMenu;
HMENU g_hRecentQueryMenu;
HMENU g_hQueryCtx;
HMENU g_hResultCtx;
HMENU g_hSchemaCtx;
HMENU g_hFileMenu;
HACCEL g_hAccel;
HBRUSH g_hBrushWhite = NULL;
HWND g_hwndQuery;
HWND g_hwndResult;
HWND g_hwndGrid;
HWND g_hwndSchema;
HWND g_hwndLineNum;
sqlite *g_db = NULL;
wchar_t g_szDbPath[MAX_PATH] = {0};
char **g_lastResult = NULL;
int g_lastResultRows = 0;
int g_lastResultCols = 0;
HFONT g_hFontQuery = NULL;
HFONT g_hFontResult = NULL;
HFONT g_hFontSchema = NULL;
HFONT g_hFontGrid = NULL;
int g_fontSizes[] = {10, 12, 14, 16};
int g_fontSizeQuery = 2;
int g_fontSizeResult = 2;
int g_fontSizeSchema = 2;
int g_showLineNumbers = 1;
int g_lineNumWidth = 0;
int g_showStatusBar = 1;
int g_fullScreen = 0;
WNDPROC g_pfnQueryProc;
WNDPROC g_pfnResultProc;
WNDPROC g_pfnGridProc;
WNDPROC g_pfnLineNumProc;

/* Output buffer */
char g_szOutput[32000];
int g_nOutput = 0;

/* State flags */
int g_clearOnExec = 1;
int g_viewMode = 0;
int g_showingHint = 0;
int g_suppressLineCount = 0;
int g_execAtCursor = 0;
int g_gridView = 0;
int g_abortQuery = 0;
wchar_t g_lastResultStatus[64] = L"";
DWORD g_lastQueryTime = 0;
wchar_t g_findText[128] = L"";
int g_searchMode = 0;
wchar_t g_szQueryPath[MAX_PATH] = L"";
wchar_t g_szLastQueryDir[MAX_PATH] = L"\\";
int g_queryDirty = 0;
int g_showErrorMsgBox = 0;
wchar_t g_szDefaultDbPath[MAX_PATH] = L"\\My Documents\\Data";
wchar_t g_recentFiles[MAX_RECENT_FILES][MAX_PATH] = {0};
int g_recentCount = 0;
wchar_t g_recentQueries[MAX_RECENT_FILES][MAX_PATH] = {0};
int g_recentQueryCount = 0;

/* Grid edit mode state */
int g_editMode = 0;                      /* 1 = grid is editable */
char g_editTableName[128] = {0};         /* Table being edited */
int g_gridViewBeforeEdit = 0;            /* Saved g_gridView state before edit mode */

/* Column metadata for edit mode */
ColumnMeta *g_colMeta = NULL;            /* Array of column info */
int g_colMetaCount = 0;                  /* Number of columns */

/* Insert mode state (editing placeholder row) */
int g_insertMode = 0;                    /* 1 = editing the placeholder row */
char **g_pendingValues = NULL;           /* Pending values for new row, one per column */
