/*
** SQLiteCEdit - Windows CE Compatibility Defines
**
** This header provides defines and structures that may not be present
** in older CE SDK versions (CE 2.0, 2.11). Include this before using
** extended window styles or common file dialog structures.
**
** Note: These are compatibility shims for building against older SDKs.
** Modern CE SDKs (3.0+, PocketPC) define most of these.
*/

#ifndef SQLITE_CE_WINCE_COMPAT_H
#define SQLITE_CE_WINCE_COMPAT_H

#include <windows.h>

/*============================================================================
** Extended Window Styles (may not be in CE 2.0 SDK headers)
**============================================================================*/

#ifndef WS_EX_CAPTIONOKBTN
#define WS_EX_CAPTIONOKBTN 0x80000000L
#endif

#ifndef WM_CONTEXTMENU
#define WM_CONTEXTMENU 0x007B
#endif

/*============================================================================
** TreeView Notifications (may not be in CE 2.0 SDK headers)
**============================================================================*/

#ifndef TVN_FIRST
#define TVN_FIRST (0U - 400U)
#endif

#ifndef TVN_KEYDOWN
#define TVN_KEYDOWN (TVN_FIRST - 12)
typedef struct tagTVKEYDOWN {
    NMHDR hdr;
    WORD wVKey;
    UINT flags;
} TV_KEYDOWN;
#endif

/*============================================================================
** Common File Dialog Flags and Structure
**
** Windows CE 2.0/2.11 may not have comdlg32.dll or the OPENFILENAME
** structure. We define our own minimal version.
**============================================================================*/

/* Common flags used with GetOpenFileName/GetSaveFileName */
#ifndef OFN_FILEMUSTEXIST
#define OFN_FILEMUSTEXIST   0x00001000
#endif

#ifndef OFN_PATHMUSTEXIST
#define OFN_PATHMUSTEXIST   0x00000800
#endif

#ifndef OFN_HIDEREADONLY
#define OFN_HIDEREADONLY    0x00000004
#endif

#ifndef OFN_OVERWRITEPROMPT
#define OFN_OVERWRITEPROMPT 0x00000002
#endif

/* CE-specific OPENFILENAME structure */
typedef struct tagCE_OPENFILENAME {
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

/* Import functions from comdlg32.dll if available */
BOOL WINAPI GetOpenFileNameW(CE_OPENFILENAME*);
BOOL WINAPI GetSaveFileNameW(CE_OPENFILENAME*);

/*============================================================================
** View Toolbar Icons (may not be in CE 2.0 headers)
**============================================================================*/

#ifndef IDB_VIEW_SMALL_COLOR
#define IDB_VIEW_SMALL_COLOR 4
#define VIEW_LIST    2
#define VIEW_DETAILS 3
#endif

/*============================================================================
** ListView Extended Styles (CE 3.0+)
**============================================================================*/

#ifndef LVS_EX_FULLROWSELECT
#define LVS_EX_FULLROWSELECT 0x00000020
#endif

#ifndef LVS_EX_GRIDLINES
#define LVS_EX_GRIDLINES 0x00000001
#endif

/*============================================================================
** Edit Control Styles
**============================================================================*/

#ifndef EM_SETMARGINS
#define EM_SETMARGINS 0x00D3
#define EC_LEFTMARGIN  0x0001
#define EC_RIGHTMARGIN 0x0002
#endif

#endif /* SQLITE_CE_WINCE_COMPAT_H */
