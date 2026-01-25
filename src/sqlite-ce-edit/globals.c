/*
** SQLiteCEdit - Global variable definitions
**
** NOTE: These globals are organized into logical groups. Future refactoring
** should consolidate each group into a struct for better cache locality.
** See OPTIMIZATION_PLAN.md A-001 for details.
*/

#include "globals.h"
#include "constants.h"

/*============================================================================
** Application State - Core app handles and instance
**============================================================================*/

HINSTANCE g_hInst;
HWND g_hwndMain;
HWND g_hwndCB;           /* Command bar / toolbar */
HWND g_hwndStatus;       /* Status bar */
HACCEL g_hAccel;
HBRUSH g_hBrushWhite = NULL;

/*============================================================================
** Menu Handles
**============================================================================*/

HMENU g_hMenu;
HMENU g_hViewMenu;
HMENU g_hEditMenu;
HMENU g_hFileMenu;
HMENU g_hRecentDbMenu;
HMENU g_hRecentQueryMenu;
HMENU g_hQueryCtx;       /* Query editor context menu */
HMENU g_hResultCtx;      /* Results context menu */
HMENU g_hSchemaCtx;      /* Schema tree context menu */

/*============================================================================
** View Windows
**============================================================================*/

HWND g_hwndQuery;        /* Query editor edit control */
HWND g_hwndResult;       /* Results text control */
HWND g_hwndGrid;         /* Grid list view */
HWND g_hwndSchema;       /* Schema tree view */
HWND g_hwndLineNum;      /* Line number display */

/* Original window procedures for subclassing */
WNDPROC g_pfnQueryProc;
WNDPROC g_pfnResultProc;
WNDPROC g_pfnGridProc;
WNDPROC g_pfnLineNumProc;

/*============================================================================
** Database State
**============================================================================*/

sqlite *g_db = NULL;
wchar_t g_szDbPath[MAX_PATH] = {0};

/* Last query results (for copy, export, etc.) */
char **g_lastResult = NULL;
int g_lastResultRows = 0;
int g_lastResultCols = 0;

/*============================================================================
** Font Settings
**============================================================================*/

HFONT g_hFontQuery = NULL;
HFONT g_hFontResult = NULL;
HFONT g_hFontSchema = NULL;
HFONT g_hFontGrid = NULL;
int g_fontSizes[] = {10, 12, 14, 16};
int g_fontSizeQuery = 2;   /* Index into g_fontSizes */
int g_fontSizeResult = 2;
int g_fontSizeSchema = 2;

/*============================================================================
** Editor Settings
**============================================================================*/

int g_showLineNumbers = 1;
int g_lineNumWidth = 0;
int g_showStatusBar = 1;
int g_fullScreen = 0;

/*============================================================================
** Output Buffer
**============================================================================*/

char g_szOutput[OUTPUT_BUFFER_SIZE];
int g_nOutput = 0;

/*============================================================================
** Query Execution State
**============================================================================*/

int g_clearOnExec = 1;       /* Clear output before executing */
int g_viewMode = 0;          /* Current view: VIEW_QUERY, VIEW_RESULT, VIEW_SCHEMA */
int g_showingHint = 0;       /* Showing placeholder hint text */
int g_suppressLineCount = 0; /* Don't update line count temporarily */
int g_execAtCursor = 0;      /* Execute statement at cursor only */
int g_abortQuery = 0;        /* User requested query abort */
wchar_t g_lastResultStatus[64] = L"";
DWORD g_lastQueryTime = 0;   /* Execution time in ms */
int g_showErrorMsgBox = 0;   /* Show errors in message box */

/*============================================================================
** Query File State
**============================================================================*/

wchar_t g_szQueryPath[MAX_PATH] = L"";
wchar_t g_szLastQueryDir[MAX_PATH] = L"\\";
int g_queryDirty = 0;        /* Query has unsaved changes */
wchar_t g_szDefaultDbPath[MAX_PATH] = L"\\My Documents\\Data";

/*============================================================================
** Find/Replace State
**============================================================================*/

wchar_t g_findText[128] = L"";
int g_searchMode = 0;        /* Incremental search active */

/*============================================================================
** Recent Files Lists
**============================================================================*/

wchar_t g_recentFiles[MAX_RECENT_FILES][MAX_PATH] = {0};
int g_recentCount = 0;
wchar_t g_recentQueries[MAX_RECENT_FILES][MAX_PATH] = {0};
int g_recentQueryCount = 0;

/*============================================================================
** Grid Edit Mode State
**============================================================================*/

int g_editMode = 0;                  /* 1 = grid is editable */
char g_editTableName[128] = {0};     /* Table being edited */
int g_gridViewBeforeEdit = 0;        /* Saved g_gridView before edit mode */
int g_gridView = 0;                  /* 1 = showing grid, 0 = showing text */

/* Column metadata for edit mode */
ColumnMeta *g_colMeta = NULL;
int g_colMetaCount = 0;

/* Insert mode (editing placeholder row) */
int g_insertMode = 0;
char **g_pendingValues = NULL;

/*============================================================================
** Schema View Options
**============================================================================*/

int g_showSizes = 0;         /* Show table sizes in schema tree */
int g_gridAutoSize = 1;      /* Auto-size grid columns */

/*============================================================================
** Backup/Storage Options
**============================================================================*/

int g_useStorageCard = 0;    /* Use storage card for backups */
int g_useStorageCardData = 0; /* Use storage card for data */
wchar_t g_szDataRelPath[MAX_PATH] = L"\\Data";
wchar_t g_szLocalBasePath[MAX_PATH] = L"\\My Documents";
wchar_t g_szCardBasePath[MAX_PATH] = L"";
