/*
** SQLiteCEdit - Centralized Constants
**
** This header consolidates magic numbers and configurable limits
** to enable easy tuning and documentation of system constraints.
*/

#ifndef CONSTANTS_H
#define CONSTANTS_H

/*============================================================================
** CSV Import Limits (import.c)
**============================================================================*/

/* Maximum number of columns that can be imported from a CSV file */
#define CSV_MAX_COLS        64

/* Maximum line length in bytes for CSV parsing */
#define CSV_MAX_LINE        8192

/* Maximum CSV file size in bytes (1MB) - prevents memory exhaustion */
#define CSV_MAX_FILE_SIZE   (1024 * 1024)

/*============================================================================
** Query Result Limits (execute.c)
**============================================================================*/

/* Maximum columns in a single query result set */
#define MAX_RESULT_COLS     32

/* Maximum rows stored in the legacy text-mode result buffer */
#define MAX_RESULT_ROWS     500

/* Maximum cell display length in legacy text mode */
#define MAX_CELL_LEN        64

/*============================================================================
** Grid/Undo Limits (grid.c)
**============================================================================*/

/* Maximum bytes allocated for undo cache (64KB) */
#define UNDO_MAX_BYTES      65536

/*============================================================================
** File Picker Limits (filepicker.c)
**============================================================================*/

/* Maximum directory entries shown in file picker */
#define PICKER_MAX_ENTRIES  256

/* Type-ahead buffer size for keyboard navigation */
#define TYPEAHEAD_MAX       16

/* Type-ahead timeout in milliseconds */
#define TYPEAHEAD_TIMEOUT   800

/* Timer ID for type-ahead */
#define TYPEAHEAD_TIMER_ID  1

/*============================================================================
** UI Dimensions and Layout
**============================================================================*/

/* Line number column width in pixels */
#define LINE_NUM_WIDTH_DEFAULT  32

/* Minimum line number column width */
#define LINE_NUM_WIDTH_MIN      24

/* Maximum line number column width */
#define LINE_NUM_WIDTH_MAX      48

/*============================================================================
** Buffer Sizes
**============================================================================*/

/* Output buffer for result text (32KB) */
#define OUTPUT_BUFFER_SIZE      32000

/* SQL statement buffer size */
#define SQL_BUFFER_SIZE         4096

/* Find/Replace text buffer size */
#define FIND_BUFFER_SIZE        128

/* Table/column name buffer size */
#define NAME_BUFFER_SIZE        128

/* Column metadata name field size */
#define COL_NAME_SIZE           64

/* Column metadata type field size */
#define COL_TYPE_SIZE           32

/*============================================================================
** Timing Constants
**============================================================================*/

/* Tap-and-hold timeout for context menu (milliseconds) */
#define TAP_HOLD_TIMEOUT        500

/*============================================================================
** Registry/Settings
**============================================================================*/

/* Maximum recent files tracked */
#define MAX_RECENT_FILES        5

/*============================================================================
** Schema Tree Image Indices
**============================================================================*/

#define IMG_DATABASE    0
#define IMG_TABLE       1
#define IMG_COLUMN      2
#define IMG_KEY         3
#define IMG_VIEW        4
#define IMG_TRIGGER     5
#define IMG_INDEX       6

#endif /* CONSTANTS_H */
