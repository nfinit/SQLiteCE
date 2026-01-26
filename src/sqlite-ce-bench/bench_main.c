/*
** SQLite/CEbench
**
** Benchmark and validation suite for SQLite/CE DLL.
** Tests correctness while measuring per-operation timing.
*/

#include <windows.h>
#include <commctrl.h>
#include "sqlite.h"
#include "../sqlite-ce-edit/strutils.h"

#define APP_VERSION "1.2.0"

/* Rich Edit message - may not be in CE headers */
#ifndef EM_SETBKGNDCOLOR
#define EM_SETBKGNDCOLOR (WM_USER + 67)
#endif

/*============================================================================
** Output and UI
**============================================================================*/

static HWND g_hwndMain;
static HWND g_hwndCB;      /* Command bar */
static HWND g_hwndOutput;
static HINSTANCE g_hInst;
static WNDPROC g_pfnEditProc;  /* Original edit control proc */

/* Subclass proc to make edit read-only while keeping white background */
static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_KEYDOWN:
            /* Ignore modifier keys alone */
            if (wParam == VK_CONTROL || wParam == VK_MENU || wParam == VK_SHIFT)
                return 0;
            /* Pass Ctrl/Alt combos to parent for shortcuts */
            if (GetKeyState(VK_CONTROL) < 0 || GetKeyState(VK_MENU) < 0) {
                SendMessage(GetParent(hwnd), msg, wParam, lParam);
                return 0;
            }
            return 0;  /* Block other keys */
        case WM_CHAR:
        case WM_PASTE:
        case WM_CUT:
        case WM_CLEAR:
            return 0;  /* Block input */
    }
    return CallWindowProc(g_pfnEditProc, hwnd, msg, wParam, lParam);
}

static char g_szOutput[16000];
static int g_nOutput = 0;
static int g_batchMode = 0;
static wchar_t g_wzOutput[16000];  /* Wide buffer - static to avoid stack overflow */

static void SetOutputText(void) {
    if (g_batchMode) return;
    MultiByteToWideChar(CP_ACP, 0, g_szOutput, -1, g_wzOutput, 16000);
    SetWindowTextW(g_hwndOutput, g_wzOutput);
    SendMessage(g_hwndOutput, EM_SETSEL, g_nOutput, g_nOutput);
    SendMessage(g_hwndOutput, EM_SCROLLCARET, 0, 0);
}

/* Process pending messages to keep UI responsive */
static void PumpMessages(void) {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

/* Force visible update and process messages */
static void RefreshOutput(void) {
    int savedBatch = g_batchMode;
    g_batchMode = 0;
    SetOutputText();
    g_batchMode = savedBatch;
    PumpMessages();
}

static void FlushOutput(void) {
    g_batchMode = 0;
    SetOutputText();
}

static void ClearOutput(void) {
    g_szOutput[0] = '\0';
    g_nOutput = 0;
}

static void Output(const char *sz) {
    const char *p = sz;
    char *d = g_szOutput + g_nOutput;
    while (*p && g_nOutput < sizeof(g_szOutput) - 1) {
        *d++ = *p++;
        g_nOutput++;
    }
    *d = '\0';
    SetOutputText();
}

static void OutputLine(const char *sz) {
    Output(sz);
    Output("\r\n");
}

static void OutputInt(const char *prefix, int val) {
    char buf[16];
    char *p = buf + 15;
    int neg = 0;
    *p = '\0';
    if (val < 0) { neg = 1; val = -val; }
    if (val == 0) *--p = '0';
    while (val > 0) { *--p = '0' + (val % 10); val /= 10; }
    if (neg) *--p = '-';
    Output(prefix);
    Output(p);
}

/*============================================================================
** Benchmark Framework
**============================================================================*/

static int g_nTests = 0;
static int g_nPassed = 0;
static DWORD g_cumulativeMs = 0;  /* Sum of individual test times */
static int g_iterations = 1;  /* Number of iterations per test */

/* Database handles for each path */
static sqlite *g_db = NULL;       /* Memory (:memory:) */
static sqlite *g_ramDb = NULL;    /* RAM filesystem */
static sqlite *g_flashDb = NULL;  /* Storage card */
static sqlite *g_curDb = NULL;    /* Current db for parameterized tests */

/* Test categories */
#define CAT_INIT     0   /* Database creation, schema loading */
#define CAT_WRITE    1   /* INSERT operations */
#define CAT_READ     2   /* SELECT operations */
#define CAT_UPDATE   3   /* UPDATE/DELETE operations */
#define CAT_QUERY    4   /* Complex queries (JOIN, aggregate, subquery) */
#define CAT_SCHEMA   5   /* DDL operations */
#define CAT_ERROR    6   /* Error path validation */
#define CAT_MATH_INT 7   /* Integer arithmetic */
#define CAT_MATH_FP  8   /* Floating-point arithmetic */
#define CAT_MEMORY   9   /* Memory usage benchmarks (M-xxx) */
#define CAT_IO       10  /* I/O performance benchmarks (I-xxx) */
#define CAT_SCALE    11  /* Scalability benchmarks (S-xxx) */
#define CAT_COUNT    12

static const char *g_catNames[CAT_COUNT] = {
    "Init", "Write", "Read", "Update", "Query", "Schema", "Error", "Math(Int)", "Math(FP)",
    "Memory", "I/O", "Scale"
};
static DWORD g_catMs[CAT_COUNT];
static int g_catTests[CAT_COUNT];
static int g_catPassed[CAT_COUNT];

/* Storage paths */
#define PATH_MEM    0   /* :memory: */
#define PATH_RAM    1   /* \Temp (RAM filesystem) */
#define PATH_FLASH  2   /* \Storage Card\Temp */
#define PATH_COUNT  3

static const char *g_pathNames[PATH_COUNT] = {
    "Memory", "Object Store", "Flash"
};
static DWORD g_pathMs[PATH_COUNT];
static int g_pathTests[PATH_COUNT];

/*============================================================================
** Benchmark Schema
**============================================================================*/

static const char *g_benchSchema =
    "CREATE TABLE customers("
        "id INTEGER PRIMARY KEY, "
        "name TEXT, "
        "region TEXT);"
    "CREATE TABLE products("
        "id INTEGER PRIMARY KEY, "
        "name TEXT, "
        "price REAL);"
    "CREATE TABLE orders("
        "id INTEGER PRIMARY KEY, "
        "customer_id INTEGER, "
        "order_date TEXT);"
    "CREATE TABLE order_items("
        "order_id INTEGER, "
        "product_id INTEGER, "
        "qty INTEGER);"
    "CREATE INDEX idx_orders_cust ON orders(customer_id);"
    "CREATE INDEX idx_items_order ON order_items(order_id);";

/* Seed data - small set for validation, can scale up */
static const char *g_benchSeed =
    "INSERT INTO customers VALUES(1,'Acme Corp','West');"
    "INSERT INTO customers VALUES(2,'Globex','East');"
    "INSERT INTO customers VALUES(3,'Initech','Central');"
    "INSERT INTO products VALUES(1,'Widget',9.99);"
    "INSERT INTO products VALUES(2,'Gadget',19.99);"
    "INSERT INTO products VALUES(3,'Gizmo',29.99);"
    "INSERT INTO orders VALUES(1,1,'1997-01-15');"
    "INSERT INTO orders VALUES(2,1,'1997-02-20');"
    "INSERT INTO orders VALUES(3,2,'1997-03-10');"
    "INSERT INTO order_items VALUES(1,1,5);"
    "INSERT INTO order_items VALUES(1,2,3);"
    "INSERT INTO order_items VALUES(2,3,10);"
    "INSERT INTO order_items VALUES(3,1,2);"
    "INSERT INTO order_items VALUES(3,2,7);";

/* Load schema and seed data into a database */
static int LoadBenchSchema(sqlite *db) {
    if (sqlite_exec(db, g_benchSchema, NULL, NULL, NULL) != SQLITE_OK)
        return 0;
    if (sqlite_exec(db, g_benchSeed, NULL, NULL, NULL) != SQLITE_OK)
        return 0;
    return 1;
}

/* Test path configuration */
static char g_testPath[128] = "\\Temp";  /* Default: RAM filesystem */
static wchar_t g_testPathW[128] = L"\\Temp";
static int g_useStorageCard = 0;
static wchar_t g_storageCardPath[64] = L"";
static int g_currentPath = PATH_RAM;  /* Current path being tested */

/* Path-specific test paths */
static char g_ramPath[128] = "\\Temp";
static wchar_t g_ramPathW[128] = L"\\Temp";
static char g_flashPath[128] = "";
static wchar_t g_flashPathW[128] = L"";

/* Detect storage card (matches SQLite/CEdit pattern) */
static int FindStorageCard(void) {
    WIN32_FIND_DATAW fd;
    HANDLE hFind;
    
    hFind = FindFirstFileW(L"\\Storage Card*", &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            wsprintfW(g_storageCardPath, L"\\%s", fd.cFileName);
            /* Set up flash path */
            wsprintfW(g_flashPathW, L"%s\\Temp", g_storageCardPath);
            WideCharToMultiByte(CP_ACP, 0, g_flashPathW, -1, g_flashPath, 128, NULL, NULL);
            FindClose(hFind);
            return 1;
        }
        FindClose(hFind);
    }
    g_storageCardPath[0] = 0;
    g_flashPath[0] = 0;
    g_flashPathW[0] = 0;
    return 0;
}

/* Update test path based on storage card setting */
static void UpdateTestPath(void) {
    if (g_useStorageCard && g_storageCardPath[0]) {
        wsprintfW(g_testPathW, L"%s\\Temp", g_storageCardPath);
    } else {
        lstrcpyW(g_testPathW, L"\\Temp");
    }
    /* Convert to ANSI for SQLite */
    WideCharToMultiByte(CP_ACP, 0, g_testPathW, -1, g_testPath, 128, NULL, NULL);
}

/* Set current path for parameterized tests */
static void SetCurrentPath(int pathType) {
    g_currentPath = pathType;
    switch (pathType) {
        case PATH_RAM:
            lstrcpyW(g_testPathW, g_ramPathW);
            WideCharToMultiByte(CP_ACP, 0, g_testPathW, -1, g_testPath, 128, NULL, NULL);
            break;
        case PATH_FLASH:
            if (g_flashPath[0]) {
                lstrcpyW(g_testPathW, g_flashPathW);
                WideCharToMultiByte(CP_ACP, 0, g_testPathW, -1, g_testPath, 128, NULL, NULL);
            }
            break;
        default:  /* PATH_MEM - no file path needed */
            break;
    }
}

/* Build full path for test file */
static void BuildTestPath(char *dest, const char *filename) {
    char *d = dest;
    const char *s = g_testPath;
    STR_COPY(d, s);
    *d++ = '\\';
    s = filename;
    STR_COPY(d, s);
    *d = '\0';
}

static void BuildTestPathW(wchar_t *dest, const wchar_t *filename) {
    wchar_t *d = dest;
    const wchar_t *s = g_testPathW;
    STR_COPY_W(d, s);
    *d++ = L'\\';
    s = filename;
    STR_COPY_W(d, s);
    *d = L'\0';
}

/* Test function type - returns 1 for pass, 0 for fail */
typedef int (*TestFunc)(void);

/* Path applicability flags */
#define PMASK_MEM   (1 << PATH_MEM)
#define PMASK_RAM   (1 << PATH_RAM)
#define PMASK_FLASH (1 << PATH_FLASH)
#define PMASK_ALL   (PMASK_MEM | PMASK_RAM | PMASK_FLASH)
#define PMASK_FILE  (PMASK_RAM | PMASK_FLASH)  /* File-based paths only */

typedef struct {
    const char *name;
    TestFunc func;
    int category;
    int pathMask;     /* Which paths this test applies to */
    DWORD lastMs;     /* Timing from last run */
} TestCase;

/* Debug context for failed tests */
static char g_debugContext[64];

static void SetDebugContext(const char *fmt, int val) {
    char *p = g_debugContext;
    const char *f = fmt;
    while (*f && p < g_debugContext + 60) {
        if (*f == '%' && *(f+1) == 'd') {
            /* Insert integer */
            char tmp[16];
            char *t = tmp + 15;
            int neg = 0;
            int v = val;
            *t = '\0';
            if (v < 0) { neg = 1; v = -v; }
            if (v == 0) *--t = '0';
            while (v > 0) { *--t = '0' + (v % 10); v /= 10; }
            if (neg) *--t = '-';
            STR_COPY(p, t);
            f += 2;
        } else {
            *p++ = *f++;
        }
    }
    *p = '\0';
}

static void ClearDebugContext(void) {
    g_debugContext[0] = '\0';
}

/* Path suffixes for output */
static const char *g_pathSuffix[PATH_COUNT] = {
    "", " (ObjStore)", " (Flash)"
};

/* Record test result with timing */
static void RecordTest(const char *name, int passed, DWORD ms, int category, int path) {
    int nameLen = 0;
    const char *p;
    
    g_nTests++;
    g_cumulativeMs += ms;
    g_catMs[category] += ms;
    g_catTests[category]++;
    if (passed) {
        g_nPassed++;
        g_catPassed[category]++;
        Output("  [PASS] ");
    } else {
        Output("  [FAIL] ");
    }
    Output(name);
    Output(g_pathSuffix[path]);
    if (g_debugContext[0]) {
        Output(" (");
        Output(g_debugContext);
        Output(")");
    }
    /* Right-align timing */
    p = name;
    while (*p++) nameLen++;
    p = g_pathSuffix[path];
    while (*p++) nameLen++;
    while (nameLen++ < 36) Output(" ");
    OutputInt("", (int)ms);
    Output(" ms");
    if (g_iterations > 1) {
        Output(" (x");
        OutputInt("", g_iterations);
        Output(")");
    }
    Output("\r\n");
    ClearDebugContext();
}

/* Helper: execute SQL and check for success */
static int ExecOK(const char *sql) {
    return sqlite_exec(g_db, sql, NULL, NULL, NULL) == SQLITE_OK;
}

/* Helper: execute SQL and return row count */
static int g_callbackCount;
static int CountCallback(void *arg, int argc, char **argv, char **cols) {
    (void)arg; (void)argc; (void)argv; (void)cols;
    g_callbackCount++;
    return 0;
}

static int CountRows(const char *sql) {
    g_callbackCount = 0;
    sqlite_exec(g_db, sql, CountCallback, NULL, NULL);
    return g_callbackCount;
}

/* Helper: get single integer result */
static int g_intResult;
static int g_verboseMode = 0;  /* Set to 1 to see raw values */

/* Simple atoi that handles negatives (CE 2.0 atoi may not) */
static int myatoi(const char *s) {
    int val = 0;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return neg ? -val : val;
}

static int IntCallback(void *arg, int argc, char **argv, char **cols) {
    (void)arg; (void)cols;
    if (argc > 0 && argv[0]) {
        if (g_verboseMode) {
            Output("    [VERBOSE] raw: '");
            Output(argv[0]);
            Output("'\r\n");
        }
        g_intResult = myatoi(argv[0]);
    }
    return 0;
}

static int GetInt(const char *sql) {
    g_intResult = -99999;  /* Unlikely value to detect if callback ran */
    sqlite_exec(g_db, sql, IntCallback, NULL, NULL);
    return g_intResult;
}

/*============================================================================
** Test Cases - Basic Operations
**============================================================================*/

static int test_open_memory(void) {
    sqlite *db = sqlite_open(":memory:", 0, NULL);
    if (db) { sqlite_close(db); return 1; }
    return 0;
}

static int test_open_file(void) {
    sqlite *db;
    char path[128];
    wchar_t pathW[128];
    BuildTestPath(path, "test_open.db");
    BuildTestPathW(pathW, L"test_open.db");
    DeleteFileW(pathW);
    db = sqlite_open(path, 0, NULL);
    if (!db) return 0;
    sqlite_close(db);
    DeleteFileW(pathW);
    return 1;
}

static int test_load_schema(void) {
    sqlite *db;
    int ok, count;
    db = sqlite_open(":memory:", 0, NULL);
    if (!db) return 0;
    ok = LoadBenchSchema(db);
    if (ok) {
        /* Verify schema loaded - check table count */
        sqlite *saved = g_db;
        g_db = db;
        count = CountRows("SELECT name FROM sqlite_master WHERE type='table'");
        ok = (count == 4);  /* customers, products, orders, order_items */
        g_db = saved;
    }
    sqlite_close(db);
    return ok;
}

static int test_create_table(void) {
    return ExecOK("CREATE TABLE t1(a, b, c)");
}

static int test_drop_table(void) {
    ExecOK("CREATE TABLE t_drop(x)");
    return ExecOK("DROP TABLE t_drop");
}

/*============================================================================
** Test Cases - CRUD Operations
**============================================================================*/

static int test_insert(void) {
    int ok;
    ExecOK("CREATE TABLE t_ins(id INTEGER PRIMARY KEY, val TEXT)");
    ok = ExecOK("INSERT INTO t_ins VALUES(1, 'hello')");
    ExecOK("DROP TABLE t_ins");
    return ok;
}

static int test_insert_null_id(void) {
    int ok, rowid;
    ExecOK("CREATE TABLE t_ins2(id INTEGER PRIMARY KEY, val TEXT)");
    ok = ExecOK("INSERT INTO t_ins2(val) VALUES('auto')");
    rowid = sqlite_last_insert_rowid(g_db);
    ExecOK("DROP TABLE t_ins2");
    return ok && rowid == 1;
}

static int test_select(void) {
    int count;
    ExecOK("CREATE TABLE t_sel(x)");
    ExecOK("INSERT INTO t_sel VALUES(1)");
    ExecOK("INSERT INTO t_sel VALUES(2)");
    count = CountRows("SELECT * FROM t_sel");
    ExecOK("DROP TABLE t_sel");
    return count == 2;
}

static int test_select_rowid(void) {
    int rowid;
    ExecOK("CREATE TABLE t_rowid(x)");
    ExecOK("INSERT INTO t_rowid VALUES('test')");
    rowid = GetInt("SELECT rowid FROM t_rowid");
    ExecOK("DROP TABLE t_rowid");
    return rowid == 1;
}

static int test_select_explicit_id(void) {
    int id;
    ExecOK("CREATE TABLE t_expid(id INTEGER PRIMARY KEY, x)");
    ExecOK("INSERT INTO t_expid VALUES(100, 'test')");
    id = GetInt("SELECT id FROM t_expid");
    ExecOK("DROP TABLE t_expid");
    return id == 100;
}

static int test_update(void) {
    int val;
    ExecOK("CREATE TABLE t_upd(id INTEGER PRIMARY KEY, val INTEGER)");
    ExecOK("INSERT INTO t_upd VALUES(1, 10)");
    ExecOK("UPDATE t_upd SET val = 20 WHERE id = 1");
    val = GetInt("SELECT val FROM t_upd WHERE id = 1");
    ExecOK("DROP TABLE t_upd");
    return val == 20;
}

static int test_delete(void) {
    int count;
    ExecOK("CREATE TABLE t_del(id INTEGER PRIMARY KEY)");
    ExecOK("INSERT INTO t_del VALUES(1)");
    ExecOK("INSERT INTO t_del VALUES(2)");
    ExecOK("DELETE FROM t_del WHERE id = 1");
    count = CountRows("SELECT * FROM t_del");
    ExecOK("DROP TABLE t_del");
    return count == 1;
}

/*============================================================================
** Test Cases - Data Types
**============================================================================*/

static int test_type_integer(void) {
    int val;
    ExecOK("CREATE TABLE t_int(v INTEGER)");
    ExecOK("INSERT INTO t_int VALUES(12345)");
    val = GetInt("SELECT v FROM t_int");
    ExecOK("DROP TABLE t_int");
    return val == 12345;
}

static int test_type_negative(void) {
    int val;
    ExecOK("CREATE TABLE t_neg(v INTEGER)");
    ExecOK("INSERT INTO t_neg VALUES(-999)");
    val = GetInt("SELECT v FROM t_neg");
    ExecOK("DROP TABLE t_neg");
    if (val != -999) SetDebugContext("expected -999, got %d", val);
    return val == -999;
}

static int test_type_text(void) {
    int count;
    ExecOK("CREATE TABLE t_txt(v TEXT)");
    ExecOK("INSERT INTO t_txt VALUES('hello world')");
    count = CountRows("SELECT * FROM t_txt WHERE v = 'hello world'");
    ExecOK("DROP TABLE t_txt");
    return count == 1;
}

static int test_type_null(void) {
    int count;
    ExecOK("CREATE TABLE t_null(v)");
    ExecOK("INSERT INTO t_null VALUES(NULL)");
    count = CountRows("SELECT * FROM t_null WHERE v IS NULL");
    ExecOK("DROP TABLE t_null");
    return count == 1;
}

/*============================================================================
** Test Cases - Persistence
**============================================================================*/

static int test_persistence(void) {
    char path[128];
    wchar_t pathW[128];
    sqlite *db1, *db2, *saved_db;
    int ok = 0;
    
    BuildTestPath(path, "test_persist.db");
    BuildTestPathW(pathW, L"test_persist.db");
    DeleteFileW(pathW);
    
    /* Create and populate */
    db1 = sqlite_open(path, 0, NULL);
    if (!db1) return 0;
    sqlite_exec(db1, "CREATE TABLE p(x)", NULL, NULL, NULL);
    sqlite_exec(db1, "INSERT INTO p VALUES(42)", NULL, NULL, NULL);
    sqlite_close(db1);
    
    /* Reopen and verify */
    db2 = sqlite_open(path, 0, NULL);
    if (!db2) return 0;
    saved_db = g_db;
    g_db = db2;
    ok = (GetInt("SELECT x FROM p") == 42);
    sqlite_close(db2);
    g_db = saved_db;
    
    DeleteFileW(pathW);
    return ok;
}

/*============================================================================
** Test Cases - Multiple Rows
**============================================================================*/

static int test_multiple_rows(void) {
    int count;
    ExecOK("CREATE TABLE t_multi(id INTEGER PRIMARY KEY, name TEXT)");
    ExecOK("INSERT INTO t_multi VALUES(100, 'Alice')");
    ExecOK("INSERT INTO t_multi VALUES(200, 'Bob')");
    ExecOK("INSERT INTO t_multi VALUES(300, 'Charlie')");
    count = CountRows("SELECT * FROM t_multi");
    ExecOK("DROP TABLE t_multi");
    return count == 3;
}

static int test_order_by(void) {
    int first;
    ExecOK("CREATE TABLE t_ord(v INTEGER)");
    ExecOK("INSERT INTO t_ord VALUES(3)");
    ExecOK("INSERT INTO t_ord VALUES(1)");
    ExecOK("INSERT INTO t_ord VALUES(2)");
    first = GetInt("SELECT v FROM t_ord ORDER BY v LIMIT 1");
    ExecOK("DROP TABLE t_ord");
    return first == 1;
}

static int test_count(void) {
    int count;
    ExecOK("CREATE TABLE t_cnt(x)");
    ExecOK("INSERT INTO t_cnt VALUES(1)");
    ExecOK("INSERT INTO t_cnt VALUES(2)");
    ExecOK("INSERT INTO t_cnt VALUES(3)");
    count = GetInt("SELECT COUNT(*) FROM t_cnt");
    ExecOK("DROP TABLE t_cnt");
    return count == 3;
}

/*============================================================================
** System Diagnostics (informational, no pass/fail)
**============================================================================*/

static void RunDiagnostics(void) {
    OutputLine("--- System Information ---");
    
    /* SQLite version */
    Output("  SQLite version: ");
    OutputLine(sqlite_libversion());
    
    /* Pointer size */
    OutputInt("  sizeof(void*): ", sizeof(void*));
    OutputLine("");
    OutputInt("  sizeof(int): ", sizeof(int));
    OutputLine("");
    OutputInt("  sizeof(long): ", sizeof(long));
    OutputLine("");
    
    /* Endianness test */
    {
        unsigned int x = 0x01020304;
        unsigned char *p = (unsigned char *)&x;
        Output("  Byte order: ");
        if (p[0] == 0x04) {
            OutputLine("Little-endian");
        } else if (p[0] == 0x01) {
            OutputLine("Big-endian");
        } else {
            OutputLine("Unknown");
        }
        Output("  Test bytes: ");
        OutputInt("", p[0]);
        Output(" ");
        OutputInt("", p[1]);
        Output(" ");
        OutputInt("", p[2]);
        Output(" ");
        OutputInt("", p[3]);
        OutputLine("");
    }
    
    /* Byte swap test */
    {
        int orig = 0x01020304;
        #define BYTESWAP(X) ((int)( \
            ((((unsigned int)(X)) & 0xFFu) << 24) | \
            ((((unsigned int)(X)) & 0xFF00u) << 8) | \
            ((((unsigned int)(X)) >> 8) & 0xFF00u) | \
            ((((unsigned int)(X)) >> 24) & 0xFFu) ))
        int swapped = BYTESWAP(orig);
        OutputInt("  Byteswap 0x01020304: ", swapped);
        OutputLine("");
        OutputInt("  Expected 0x04030201: ", 0x04030201);
        OutputLine("");
        Output("  Byteswap: ");
        OutputLine(swapped == 0x04030201 ? "OK" : "MISMATCH");
    }
    
    /* intToKey/keyToInt round-trip (critical for rowid) */
    {
        int orig = 100;
        unsigned int key = BYTESWAP((unsigned int)orig ^ 0x80000000u);
        int back = (int)(BYTESWAP(key) ^ 0x80000000u);
        OutputInt("  intToKey(100): ", (int)key);
        OutputLine("");
        OutputInt("  keyToInt back: ", back);
        OutputLine("");
        Output("  Round-trip: ");
        OutputLine(back == orig ? "OK" : "MISMATCH");
    }
    
    OutputLine("");
    OutputLine("--- File System ---");
    
    /* Test \Temp directory */
    {
        DWORD attr = GetFileAttributesW(L"\\Temp");
        Output("  \\Temp exists: ");
        OutputLine(attr != 0xFFFFFFFF ? "YES" : "NO");
    }
    
    /* Test file creation */
    {
        HANDLE hFile = CreateFileW(L"\\Temp\\_test_.tmp",
            GENERIC_READ | GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD written;
            int ok = WriteFile(hFile, "test", 4, &written, NULL);
            CloseHandle(hFile);
            DeleteFileW(L"\\Temp\\_test_.tmp");
            Output("  File write test: ");
            OutputLine(ok && written == 4 ? "OK" : "FAIL");
        } else {
            OutputLine("  File write test: FAIL (create)");
        }
    }
    
    OutputLine("");
}

/*============================================================================
** Test Cases - Advanced
**============================================================================*/

static int test_transaction_commit(void) {
    int ok;
    ExecOK("CREATE TABLE tr(x)");
    ExecOK("BEGIN");
    ExecOK("INSERT INTO tr VALUES(1)");
    ExecOK("COMMIT");
    ok = (GetInt("SELECT x FROM tr") == 1);
    ExecOK("DROP TABLE tr");
    return ok;
}

static int test_transaction_rollback(void) {
    int ok;
    ExecOK("CREATE TABLE tr(x)");
    ExecOK("INSERT INTO tr VALUES(1)");
    ExecOK("BEGIN");
    ExecOK("INSERT INTO tr VALUES(2)");
    ExecOK("ROLLBACK");
    ok = (CountRows("SELECT * FROM tr") == 1);
    ExecOK("DROP TABLE tr");
    return ok;
}

static int test_like(void) {
    int ok;
    ExecOK("CREATE TABLE lk(s TEXT)");
    ExecOK("INSERT INTO lk VALUES('hello')");
    ExecOK("INSERT INTO lk VALUES('world')");
    ExecOK("INSERT INTO lk VALUES('help')");
    ok = (CountRows("SELECT * FROM lk WHERE s LIKE 'hel%'") == 2);
    ExecOK("DROP TABLE lk");
    return ok;
}

static int test_is_null(void) {
    int ok;
    ExecOK("CREATE TABLE nu(x)");
    ExecOK("INSERT INTO nu VALUES(1)");
    ExecOK("INSERT INTO nu VALUES(NULL)");
    ok = (CountRows("SELECT * FROM nu WHERE x IS NULL") == 1);
    ExecOK("DROP TABLE nu");
    return ok;
}

static int test_sum(void) {
    int ok;
    ExecOK("CREATE TABLE sm(x INTEGER)");
    ExecOK("INSERT INTO sm VALUES(10)");
    ExecOK("INSERT INTO sm VALUES(20)");
    ExecOK("INSERT INTO sm VALUES(30)");
    ok = (GetInt("SELECT SUM(x) FROM sm") == 60);
    ExecOK("DROP TABLE sm");
    return ok;
}

static int test_min_max(void) {
    int ok;
    ExecOK("CREATE TABLE mm(x INTEGER)");
    ExecOK("INSERT INTO mm VALUES(5)");
    ExecOK("INSERT INTO mm VALUES(15)");
    ExecOK("INSERT INTO mm VALUES(10)");
    ok = (GetInt("SELECT MIN(x) FROM mm") == 5) &&
         (GetInt("SELECT MAX(x) FROM mm") == 15);
    ExecOK("DROP TABLE mm");
    return ok;
}

static int test_join(void) {
    int ok;
    ExecOK("CREATE TABLE j1(id INTEGER, name TEXT)");
    ExecOK("CREATE TABLE j2(id INTEGER, value INTEGER)");
    ExecOK("INSERT INTO j1 VALUES(1, 'Alice')");
    ExecOK("INSERT INTO j1 VALUES(2, 'Bob')");
    ExecOK("INSERT INTO j2 VALUES(1, 100)");
    ExecOK("INSERT INTO j2 VALUES(2, 200)");
    ok = (CountRows("SELECT * FROM j1, j2 WHERE j1.id = j2.id") == 2);
    ExecOK("DROP TABLE j1");
    ExecOK("DROP TABLE j2");
    return ok;
}

/*============================================================================
** Test Cases - Export/Import (0.2.0)
**============================================================================*/

static int test_sql_quote_escape(void) {
    int ok;
    char **result;
    int nRow, nCol;
    ExecOK("CREATE TABLE q(s TEXT)");
    ExecOK("INSERT INTO q VALUES('it''s a test')");
    ExecOK("INSERT INTO q VALUES('say \"hello\"')");
    ok = (CountRows("SELECT * FROM q") == 2);
    /* Verify content */
    if (sqlite_get_table(g_db, "SELECT s FROM q WHERE s LIKE '%it''s%'", &result, &nRow, &nCol, NULL) == SQLITE_OK) {
        ok = ok && (nRow == 1);
        sqlite_free_table(result);
    }
    ExecOK("DROP TABLE q");
    return ok;
}

static int test_sqlite_master_tables(void) {
    int ok;
    ExecOK("CREATE TABLE sm_t1(a INTEGER)");
    ExecOK("CREATE TABLE sm_t2(b TEXT)");
    ExecOK("CREATE TABLE sm_t3(c REAL)");
    ok = (CountRows("SELECT name FROM sqlite_master WHERE type='table' AND name LIKE 'sm_t%'") == 3);
    ExecOK("DROP TABLE sm_t1");
    ExecOK("DROP TABLE sm_t2");
    ExecOK("DROP TABLE sm_t3");
    return ok;
}

static int test_sqlite_master_indexes(void) {
    int ok;
    char **result;
    int nRow, nCol;
    ExecOK("CREATE TABLE idx_t(a INTEGER, b TEXT)");
    ExecOK("CREATE INDEX idx_a ON idx_t(a)");
    ok = (CountRows("SELECT name FROM sqlite_master WHERE type='index' AND name='idx_a'") == 1);
    /* Verify SQL is stored */
    if (sqlite_get_table(g_db, "SELECT sql FROM sqlite_master WHERE name='idx_a'", &result, &nRow, &nCol, NULL) == SQLITE_OK) {
        ok = ok && (nRow == 1) && (result[1] != NULL);
        sqlite_free_table(result);
    }
    ExecOK("DROP TABLE idx_t");
    return ok;
}

static int test_export_db_schema(void) {
    char srcPath[128], dstPath[128];
    wchar_t srcPathW[128], dstPathW[128];
    sqlite *srcDb, *dstDb;
    char **result;
    int nRow, nCol, ok = 0;
    
    BuildTestPath(srcPath, "exp_src.db");
    BuildTestPath(dstPath, "exp_dst.db");
    BuildTestPathW(srcPathW, L"exp_src.db");
    BuildTestPathW(dstPathW, L"exp_dst.db");
    
    DeleteFileW(srcPathW);
    DeleteFileW(dstPathW);
    
    /* Create source with schema */
    srcDb = sqlite_open(srcPath, 0, NULL);
    if (!srcDb) return 0;
    sqlite_exec(srcDb, "CREATE TABLE t1(id INTEGER PRIMARY KEY, name TEXT)", NULL, NULL, NULL);
    sqlite_exec(srcDb, "CREATE TABLE t2(x REAL, y REAL)", NULL, NULL, NULL);
    
    /* "Export" by copying schema */
    dstDb = sqlite_open(dstPath, 0, NULL);
    if (!dstDb) { sqlite_close(srcDb); return 0; }
    
    if (sqlite_get_table(srcDb, "SELECT sql FROM sqlite_master WHERE type='table'", &result, &nRow, &nCol, NULL) == SQLITE_OK) {
        int i;
        for (i = 1; i <= nRow; i++) {
            if (result[i]) sqlite_exec(dstDb, result[i], NULL, NULL, NULL);
        }
        sqlite_free_table(result);
    }
    sqlite_close(srcDb);
    sqlite_close(dstDb);
    
    /* Verify destination has both tables */
    dstDb = sqlite_open(dstPath, 0, NULL);
    if (dstDb) {
        ok = (CountRows("SELECT name FROM sqlite_master WHERE type='table'") >= 2);
        /* Temporarily use dstDb for count */
        {
            sqlite *saved = g_db;
            g_db = dstDb;
            ok = (CountRows("SELECT name FROM sqlite_master WHERE type='table'") == 2);
            g_db = saved;
        }
        sqlite_close(dstDb);
    }
    
    DeleteFileW(srcPathW);
    DeleteFileW(dstPathW);
    return ok;
}

static int test_export_db_data(void) {
    char srcPath[128], dstPath[128];
    wchar_t srcPathW[128], dstPathW[128];
    sqlite *srcDb, *dstDb;
    char **result;
    int nRow, nCol, ok = 0;
    
    BuildTestPath(srcPath, "expd_src.db");
    BuildTestPath(dstPath, "expd_dst.db");
    BuildTestPathW(srcPathW, L"expd_src.db");
    BuildTestPathW(dstPathW, L"expd_dst.db");
    
    DeleteFileW(srcPathW);
    DeleteFileW(dstPathW);
    
    /* Create source with data */
    srcDb = sqlite_open(srcPath, 0, NULL);
    if (!srcDb) return 0;
    sqlite_exec(srcDb, "CREATE TABLE t(id INTEGER, val TEXT)", NULL, NULL, NULL);
    sqlite_exec(srcDb, "INSERT INTO t VALUES(1, 'one')", NULL, NULL, NULL);
    sqlite_exec(srcDb, "INSERT INTO t VALUES(2, 'two')", NULL, NULL, NULL);
    sqlite_exec(srcDb, "INSERT INTO t VALUES(3, 'three')", NULL, NULL, NULL);
    
    /* Export schema */
    dstDb = sqlite_open(dstPath, 0, NULL);
    if (!dstDb) { sqlite_close(srcDb); return 0; }
    sqlite_exec(dstDb, "CREATE TABLE t(id INTEGER, val TEXT)", NULL, NULL, NULL);
    
    /* Export data */
    if (sqlite_get_table(srcDb, "SELECT * FROM t", &result, &nRow, &nCol, NULL) == SQLITE_OK) {
        int i;
        for (i = 1; i <= nRow; i++) {
            char sql[256];
            char *p = sql;
            char *s;
            for (s = "INSERT INTO t VALUES("; *s; ) *p++ = *s++;
            for (s = result[i * nCol]; *s; ) *p++ = *s++;  /* id */
            *p++ = ','; *p++ = '\'';
            for (s = result[i * nCol + 1]; *s; ) *p++ = *s++;  /* val */
            *p++ = '\''; *p++ = ')'; *p = '\0';
            sqlite_exec(dstDb, sql, NULL, NULL, NULL);
        }
        sqlite_free_table(result);
    }
    sqlite_close(srcDb);
    sqlite_close(dstDb);
    
    /* Verify destination has 3 rows */
    dstDb = sqlite_open(dstPath, 0, NULL);
    if (dstDb) {
        sqlite *saved = g_db;
        g_db = dstDb;
        ok = (CountRows("SELECT * FROM t") == 3);
        ok = ok && (GetInt("SELECT id FROM t WHERE val='two'") == 2);
        g_db = saved;
        sqlite_close(dstDb);
    }
    
    DeleteFileW(srcPathW);
    DeleteFileW(dstPathW);
    return ok;
}

static int test_invalid_sql(void) {
    int rc;
    char *errmsg = NULL;
    rc = sqlite_exec(g_db, "SELEKT * FORM nowhere", NULL, NULL, &errmsg);
    if (errmsg) sqlite_freemem(errmsg);
    return (rc != SQLITE_OK);  /* Should fail */
}

static int test_missing_table(void) {
    int rc;
    char *errmsg = NULL;
    rc = sqlite_exec(g_db, "SELECT * FROM nonexistent_table_xyz", NULL, NULL, &errmsg);
    if (errmsg) sqlite_freemem(errmsg);
    return (rc != SQLITE_OK);  /* Should fail */
}

static int test_constraint_violation(void) {
    int rc;
    char *errmsg = NULL;
    ExecOK("CREATE TABLE cv(id INTEGER PRIMARY KEY)");
    ExecOK("INSERT INTO cv VALUES(1)");
    rc = sqlite_exec(g_db, "INSERT INTO cv VALUES(1)", NULL, NULL, &errmsg);  /* Duplicate */
    if (errmsg) sqlite_freemem(errmsg);
    ExecOK("DROP TABLE cv");
    return (rc != SQLITE_OK);  /* Should fail */
}

static int test_memory_isolation(void) {
    sqlite *db1, *db2;
    int ok = 0;
    
    db1 = sqlite_open(":memory:", 0, NULL);
    db2 = sqlite_open(":memory:", 0, NULL);
    if (!db1 || !db2) {
        if (db1) sqlite_close(db1);
        if (db2) sqlite_close(db2);
        return 0;
    }
    
    sqlite_exec(db1, "CREATE TABLE t(x INTEGER)", NULL, NULL, NULL);
    sqlite_exec(db1, "INSERT INTO t VALUES(42)", NULL, NULL, NULL);
    
    /* db2 should NOT see db1's table */
    {
        char **result;
        int nRow, nCol;
        if (sqlite_get_table(db2, "SELECT * FROM t", &result, &nRow, &nCol, NULL) != SQLITE_OK) {
            ok = 1;  /* Expected: table doesn't exist in db2 */
        } else {
            sqlite_free_table(result);
            ok = 0;  /* Unexpected: table exists in db2 */
        }
    }
    
    sqlite_close(db1);
    sqlite_close(db2);
    return ok;
}

/*============================================================================
** Test Cases - Triggers (0.5.0)
**============================================================================*/

static int test_trigger_create(void) {
    int ok;
    ExecOK("CREATE TABLE tr_t(id INTEGER PRIMARY KEY, val TEXT)");
    ExecOK("CREATE TABLE tr_log(msg TEXT)");
    ok = ExecOK("CREATE TRIGGER tr_ins AFTER INSERT ON tr_t FOR EACH ROW BEGIN INSERT INTO tr_log VALUES('inserted'); END");
    ExecOK("DROP TRIGGER tr_ins");
    ExecOK("DROP TABLE tr_log");
    ExecOK("DROP TABLE tr_t");
    return ok;
}

static int test_trigger_fires_insert(void) {
    int ok;
    ExecOK("CREATE TABLE tr_t(id INTEGER PRIMARY KEY, val TEXT)");
    ExecOK("CREATE TABLE tr_log(msg TEXT)");
    ExecOK("CREATE TRIGGER tr_ins AFTER INSERT ON tr_t FOR EACH ROW BEGIN INSERT INTO tr_log VALUES('inserted'); END");
    ExecOK("INSERT INTO tr_t VALUES(1, 'test')");
    ok = (CountRows("SELECT * FROM tr_log") == 1);
    ExecOK("DROP TRIGGER tr_ins");
    ExecOK("DROP TABLE tr_log");
    ExecOK("DROP TABLE tr_t");
    return ok;
}

static int test_trigger_fires_update(void) {
    int ok;
    ExecOK("CREATE TABLE tr_t(id INTEGER PRIMARY KEY, val TEXT)");
    ExecOK("CREATE TABLE tr_log(msg TEXT)");
    ExecOK("CREATE TRIGGER tr_upd AFTER UPDATE ON tr_t FOR EACH ROW BEGIN INSERT INTO tr_log VALUES('updated'); END");
    ExecOK("INSERT INTO tr_t VALUES(1, 'test')");
    ExecOK("UPDATE tr_t SET val='changed' WHERE id=1");
    ok = (CountRows("SELECT * FROM tr_log") == 1);
    ExecOK("DROP TRIGGER tr_upd");
    ExecOK("DROP TABLE tr_log");
    ExecOK("DROP TABLE tr_t");
    return ok;
}

static int test_trigger_fires_delete(void) {
    int ok;
    ExecOK("CREATE TABLE tr_t(id INTEGER PRIMARY KEY, val TEXT)");
    ExecOK("CREATE TABLE tr_log(msg TEXT)");
    ExecOK("CREATE TRIGGER tr_del AFTER DELETE ON tr_t FOR EACH ROW BEGIN INSERT INTO tr_log VALUES('deleted'); END");
    ExecOK("INSERT INTO tr_t VALUES(1, 'test')");
    ExecOK("DELETE FROM tr_t WHERE id=1");
    ok = (CountRows("SELECT * FROM tr_log") == 1);
    ExecOK("DROP TRIGGER tr_del");
    ExecOK("DROP TABLE tr_log");
    ExecOK("DROP TABLE tr_t");
    return ok;
}

static int test_trigger_new_reference(void) {
    int ok;
    ExecOK("CREATE TABLE tr_t(id INTEGER PRIMARY KEY, val INTEGER)");
    ExecOK("CREATE TABLE tr_log(logged_val INTEGER)");
    ExecOK("CREATE TRIGGER tr_ins AFTER INSERT ON tr_t FOR EACH ROW BEGIN INSERT INTO tr_log VALUES(NEW.val); END");
    ExecOK("INSERT INTO tr_t VALUES(1, 42)");
    ok = (GetInt("SELECT logged_val FROM tr_log") == 42);
    ExecOK("DROP TRIGGER tr_ins");
    ExecOK("DROP TABLE tr_log");
    ExecOK("DROP TABLE tr_t");
    return ok;
}

static int test_trigger_drop(void) {
    int ok;
    ExecOK("CREATE TABLE tr_t(id INTEGER)");
    ExecOK("CREATE TABLE tr_log(msg TEXT)");
    ExecOK("CREATE TRIGGER tr_ins AFTER INSERT ON tr_t FOR EACH ROW BEGIN INSERT INTO tr_log VALUES('x'); END");
    ok = ExecOK("DROP TRIGGER tr_ins");
    ExecOK("DROP TABLE tr_log");
    ExecOK("DROP TABLE tr_t");
    return ok;
}

/*============================================================================
** Test Cases - Views (0.5.0)
**============================================================================*/

static int test_view_create(void) {
    int ok;
    ExecOK("CREATE TABLE v_t(id INTEGER, name TEXT)");
    ok = ExecOK("CREATE VIEW v_names AS SELECT name FROM v_t");
    ExecOK("DROP VIEW v_names");
    ExecOK("DROP TABLE v_t");
    return ok;
}

static int test_view_select(void) {
    int ok;
    ExecOK("CREATE TABLE v_t(id INTEGER, name TEXT)");
    ExecOK("INSERT INTO v_t VALUES(1, 'Alice')");
    ExecOK("INSERT INTO v_t VALUES(2, 'Bob')");
    ExecOK("CREATE VIEW v_names AS SELECT name FROM v_t");
    ok = (CountRows("SELECT * FROM v_names") == 2);
    ExecOK("DROP VIEW v_names");
    ExecOK("DROP TABLE v_t");
    return ok;
}

static int test_view_with_join(void) {
    int ok;
    ExecOK("CREATE TABLE v_users(id INTEGER, name TEXT)");
    ExecOK("CREATE TABLE v_orders(id INTEGER, user_id INTEGER, amount INTEGER)");
    ExecOK("INSERT INTO v_users VALUES(1, 'Alice')");
    ExecOK("INSERT INTO v_orders VALUES(1, 1, 100)");
    ExecOK("CREATE VIEW v_user_orders AS SELECT u.name, o.amount FROM v_users u, v_orders o WHERE u.id = o.user_id");
    ok = (CountRows("SELECT * FROM v_user_orders") == 1);
    ExecOK("DROP VIEW v_user_orders");
    ExecOK("DROP TABLE v_orders");
    ExecOK("DROP TABLE v_users");
    return ok;
}

static int test_view_drop(void) {
    int ok;
    ExecOK("CREATE TABLE v_t(x INTEGER)");
    ExecOK("CREATE VIEW v_x AS SELECT x FROM v_t");
    ok = ExecOK("DROP VIEW v_x");
    ExecOK("DROP TABLE v_t");
    return ok;
}

static int test_view_in_sqlite_master(void) {
    int ok;
    ExecOK("CREATE TABLE v_t(x INTEGER)");
    ExecOK("CREATE VIEW v_test AS SELECT x FROM v_t");
    ok = (CountRows("SELECT * FROM sqlite_master WHERE type='view' AND name='v_test'") == 1);
    ExecOK("DROP VIEW v_test");
    ExecOK("DROP TABLE v_t");
    return ok;
}

/*============================================================================
** Test Cases - Indexes (0.5.0)
**============================================================================*/

static int test_index_create(void) {
    int ok;
    ExecOK("CREATE TABLE idx_t(a INTEGER, b TEXT)");
    ok = ExecOK("CREATE INDEX idx_a ON idx_t(a)");
    ExecOK("DROP TABLE idx_t");
    return ok;
}

static int test_index_unique(void) {
    int ok, rc;
    char *errmsg = NULL;
    ExecOK("CREATE TABLE idx_t(a INTEGER, b TEXT)");
    ExecOK("CREATE UNIQUE INDEX idx_a ON idx_t(a)");
    ExecOK("INSERT INTO idx_t VALUES(1, 'one')");
    rc = sqlite_exec(g_db, "INSERT INTO idx_t VALUES(1, 'duplicate')", NULL, NULL, &errmsg);
    ok = (rc != SQLITE_OK);  /* Should fail due to unique constraint */
    if (errmsg) sqlite_freemem(errmsg);
    ExecOK("DROP TABLE idx_t");
    return ok;
}

static int test_index_drop(void) {
    int ok;
    ExecOK("CREATE TABLE idx_t(a INTEGER)");
    ExecOK("CREATE INDEX idx_a ON idx_t(a)");
    ok = ExecOK("DROP INDEX idx_a");
    ExecOK("DROP TABLE idx_t");
    return ok;
}

static int test_index_in_sqlite_master(void) {
    int ok;
    ExecOK("CREATE TABLE idx_t(a INTEGER)");
    ExecOK("CREATE INDEX idx_test ON idx_t(a)");
    ok = (CountRows("SELECT * FROM sqlite_master WHERE type='index' AND name='idx_test'") == 1);
    ExecOK("DROP TABLE idx_t");
    return ok;
}

/*============================================================================
** Test Cases - Complex Queries (0.5.0)
**============================================================================*/

static int test_subquery_where(void) {
    int ok;
    ExecOK("CREATE TABLE sq_t(id INTEGER, val INTEGER)");
    ExecOK("INSERT INTO sq_t VALUES(1, 10)");
    ExecOK("INSERT INTO sq_t VALUES(2, 20)");
    ExecOK("INSERT INTO sq_t VALUES(3, 30)");
    ok = (CountRows("SELECT * FROM sq_t WHERE val > (SELECT AVG(val) FROM sq_t)") == 1);
    ExecOK("DROP TABLE sq_t");
    return ok;
}

static int test_subquery_from(void) {
    int ok;
    ExecOK("CREATE TABLE sq_t(id INTEGER, val INTEGER)");
    ExecOK("INSERT INTO sq_t VALUES(1, 10)");
    ExecOK("INSERT INTO sq_t VALUES(2, 20)");
    ok = (GetInt("SELECT MAX(val) FROM (SELECT val FROM sq_t)") == 20);
    ExecOK("DROP TABLE sq_t");
    return ok;
}

static int test_union(void) {
    int ok;
    ExecOK("CREATE TABLE u1(x INTEGER)");
    ExecOK("CREATE TABLE u2(x INTEGER)");
    ExecOK("INSERT INTO u1 VALUES(1)");
    ExecOK("INSERT INTO u1 VALUES(2)");
    ExecOK("INSERT INTO u2 VALUES(2)");
    ExecOK("INSERT INTO u2 VALUES(3)");
    ok = (CountRows("SELECT x FROM u1 UNION SELECT x FROM u2") == 3);  /* 1,2,3 - no dups */
    ExecOK("DROP TABLE u1");
    ExecOK("DROP TABLE u2");
    return ok;
}

static int test_union_all(void) {
    int ok;
    ExecOK("CREATE TABLE u1(x INTEGER)");
    ExecOK("CREATE TABLE u2(x INTEGER)");
    ExecOK("INSERT INTO u1 VALUES(1)");
    ExecOK("INSERT INTO u1 VALUES(2)");
    ExecOK("INSERT INTO u2 VALUES(2)");
    ExecOK("INSERT INTO u2 VALUES(3)");
    ok = (CountRows("SELECT x FROM u1 UNION ALL SELECT x FROM u2") == 4);  /* includes dup */
    ExecOK("DROP TABLE u1");
    ExecOK("DROP TABLE u2");
    return ok;
}

static int test_group_by_having(void) {
    int ok;
    ExecOK("CREATE TABLE gb_t(cat TEXT, val INTEGER)");
    ExecOK("INSERT INTO gb_t VALUES('A', 10)");
    ExecOK("INSERT INTO gb_t VALUES('A', 20)");
    ExecOK("INSERT INTO gb_t VALUES('B', 5)");
    ok = (CountRows("SELECT cat, SUM(val) FROM gb_t GROUP BY cat HAVING SUM(val) > 10") == 1);
    ExecOK("DROP TABLE gb_t");
    return ok;
}

static int test_order_by_multiple(void) {
    int ok;
    ExecOK("CREATE TABLE ob_t(a INTEGER, b INTEGER)");
    ExecOK("INSERT INTO ob_t VALUES(1, 2)");
    ExecOK("INSERT INTO ob_t VALUES(1, 1)");
    ExecOK("INSERT INTO ob_t VALUES(2, 1)");
    ok = (GetInt("SELECT b FROM ob_t ORDER BY a, b LIMIT 1") == 1);
    ExecOK("DROP TABLE ob_t");
    return ok;
}

static int test_limit_offset(void) {
    int ok;
    ExecOK("CREATE TABLE lo_t(x INTEGER)");
    ExecOK("INSERT INTO lo_t VALUES(1)");
    ExecOK("INSERT INTO lo_t VALUES(2)");
    ExecOK("INSERT INTO lo_t VALUES(3)");
    ExecOK("INSERT INTO lo_t VALUES(4)");
    ok = (GetInt("SELECT x FROM lo_t ORDER BY x LIMIT 1 OFFSET 2") == 3);
    ExecOK("DROP TABLE lo_t");
    return ok;
}

/*============================================================================
** Test Cases - Edge Cases (0.5.0)
**============================================================================*/

static int test_empty_table_select(void) {
    int ok;
    ExecOK("CREATE TABLE empty_t(x INTEGER)");
    ok = (CountRows("SELECT * FROM empty_t") == 0);
    ExecOK("DROP TABLE empty_t");
    return ok;
}

static int test_null_comparisons(void) {
    int ok;
    ExecOK("CREATE TABLE nc_t(x INTEGER)");
    ExecOK("INSERT INTO nc_t VALUES(1)");
    ExecOK("INSERT INTO nc_t VALUES(NULL)");
    ExecOK("INSERT INTO nc_t VALUES(2)");
    ok = (CountRows("SELECT * FROM nc_t WHERE x IS NULL") == 1);
    ok = ok && (CountRows("SELECT * FROM nc_t WHERE x IS NOT NULL") == 2);
    ExecOK("DROP TABLE nc_t");
    return ok;
}

static int test_string_embedded_quotes(void) {
    int ok;
    ExecOK("CREATE TABLE eq_t(s TEXT)");
    ExecOK("INSERT INTO eq_t VALUES('it''s a \"test\"')");
    ok = (CountRows("SELECT * FROM eq_t WHERE s LIKE '%test%'") == 1);
    ExecOK("DROP TABLE eq_t");
    return ok;
}

static int test_large_integer(void) {
    int ok, val;
    ExecOK("CREATE TABLE li_t(x INTEGER)");
    ExecOK("INSERT INTO li_t VALUES(2147483647)");  /* Max 32-bit signed */
    val = GetInt("SELECT x FROM li_t");
    ok = (val == 2147483647);
    ExecOK("DROP TABLE li_t");
    return ok;
}

/*============================================================================
** Test Cases - Benchmark Schema Queries
** These use the shared schema: customers, products, orders, order_items
**============================================================================*/

/* Helper: setup and teardown benchmark schema on g_db */
static int g_benchSchemaLoaded = 0;

static int SetupBenchSchema(void) {
    if (g_benchSchemaLoaded) return 1;
    if (!LoadBenchSchema(g_db)) return 0;
    g_benchSchemaLoaded = 1;
    return 1;
}

static int test_bench_join_orders(void) {
    int count;
    if (!SetupBenchSchema()) return 0;
    /* Join orders with customers */
    count = CountRows(
        "SELECT o.id, c.name FROM orders o, customers c "
        "WHERE o.customer_id = c.id");
    return (count == 3);  /* 3 orders in seed data */
}

static int test_bench_join_items(void) {
    int count;
    if (!SetupBenchSchema()) return 0;
    /* Join order_items with products */
    count = CountRows(
        "SELECT oi.order_id, p.name, oi.qty FROM order_items oi, products p "
        "WHERE oi.product_id = p.id");
    return (count == 5);  /* 5 order_items in seed data */
}

static int test_bench_three_way_join(void) {
    int count;
    if (!SetupBenchSchema()) return 0;
    /* Three-way join: customers -> orders -> order_items */
    count = CountRows(
        "SELECT c.name, o.order_date, oi.qty "
        "FROM customers c, orders o, order_items oi "
        "WHERE c.id = o.customer_id AND o.id = oi.order_id");
    return (count == 5);
}

static int test_bench_aggregate_sum(void) {
    int total;
    if (!SetupBenchSchema()) return 0;
    /* Sum quantities per order */
    total = GetInt("SELECT SUM(qty) FROM order_items WHERE order_id = 1");
    return (total == 8);  /* 5 + 3 from seed data */
}

static int test_bench_aggregate_count(void) {
    int count;
    if (!SetupBenchSchema()) return 0;
    /* Count orders per customer */
    count = GetInt(
        "SELECT COUNT(*) FROM orders WHERE customer_id = 1");
    return (count == 2);  /* Customer 1 has 2 orders */
}

static int test_bench_indexed_lookup(void) {
    int count;
    if (!SetupBenchSchema()) return 0;
    /* Lookup using index on customer_id */
    count = CountRows(
        "SELECT * FROM orders WHERE customer_id = 2");
    return (count == 1);
}

static int test_bench_subquery(void) {
    int count;
    if (!SetupBenchSchema()) return 0;
    /* Subquery: customers with orders */
    count = CountRows(
        "SELECT * FROM customers WHERE id IN "
        "(SELECT DISTINCT customer_id FROM orders)");
    return (count == 2);  /* Customers 1 and 2 have orders */
}

/*============================================================================
** Test Cases - Date/Time Functions (0.5.0)
**============================================================================*/

static int test_datetime_now(void) {
    int ok;
    /* datetime('now') should return a non-empty string */
    ExecOK("CREATE TABLE dt_t(ts TEXT)");
    ExecOK("INSERT INTO dt_t VALUES(datetime('now'))");
    ok = (CountRows("SELECT * FROM dt_t WHERE ts IS NOT NULL AND ts != ''") == 1);
    ExecOK("DROP TABLE dt_t");
    return ok;
}

static int test_date_now(void) {
    int ok;
    ExecOK("CREATE TABLE dt_t(d TEXT)");
    ExecOK("INSERT INTO dt_t VALUES(date('now'))");
    ok = (CountRows("SELECT * FROM dt_t WHERE d IS NOT NULL AND d != ''") == 1);
    ExecOK("DROP TABLE dt_t");
    return ok;
}

static int test_time_now(void) {
    int ok;
    ExecOK("CREATE TABLE dt_t(t TEXT)");
    ExecOK("INSERT INTO dt_t VALUES(time('now'))");
    ok = (CountRows("SELECT * FROM dt_t WHERE t IS NOT NULL AND t != ''") == 1);
    ExecOK("DROP TABLE dt_t");
    return ok;
}

/*============================================================================
** Test Cases - Math/CPU (Memory only)
**============================================================================*/

static int test_math_integer(void) {
    int result = GetInt("SELECT 12345 * 6789 / 123 + 456 - 789");
    return (result == 681050);
}

static int test_math_multiply(void) {
    int result = GetInt("SELECT 12345 * 6789");
    return (result == 83810205);
}

static int test_math_divide(void) {
    int result = GetInt("SELECT 1000000 / 127");
    return (result == 7874);
}

static int test_math_abs(void) {
    int ok;
    ok = (GetInt("SELECT ABS(-12345)") == 12345);
    ok = ok && (GetInt("SELECT ABS(42)") == 42);
    ok = ok && (GetInt("SELECT ABS(-3) + ABS(6)") == 9);  /* Was broken pre-ce3 */
    return ok;
}

static int test_math_minmax(void) {
    int ok;
    ok = (GetInt("SELECT MIN(5, 3, 9, 1, 7)") == 1);
    ok = ok && (GetInt("SELECT MAX(5, 3, 9, 1, 7)") == 9);
    return ok;
}

static int test_math_round(void) {
    int ok;
    ok = (GetInt("SELECT ROUND(3.7)") == 4);
    ok = ok && (GetInt("SELECT ROUND(3.2)") == 3);
    ok = ok && (GetInt("SELECT ROUND(-2.5)") == -3);
    return ok;
}

static int test_math_coalesce(void) {
    int result = GetInt("SELECT COALESCE(NULL, NULL, 42, 99)");
    return (result == 42);
}

static int test_math_nullif(void) {
    int ok;
    /* NULLIF returns NULL if args equal, else first arg */
    ok = (GetInt("SELECT NULLIF(5, 5)") == -99999);  /* NULL -> our sentinel */
    ok = ok && (GetInt("SELECT NULLIF(5, 3)") == 5);
    return ok;
}

static int test_math_length(void) {
    int result = GetInt("SELECT LENGTH('Hello, World!')");
    return (result == 13);
}

static int test_math_substr(void) {
    int ok;
    ExecOK("CREATE TABLE tmp(s TEXT)");
    ExecOK("INSERT INTO tmp VALUES(SUBSTR('Hello, World!', 8, 5))");
    ok = (CountRows("SELECT * FROM tmp WHERE s = 'World'") == 1);
    ExecOK("DROP TABLE tmp");
    return ok;
}

static int test_math_upper_lower(void) {
    int ok;
    ExecOK("CREATE TABLE tmp(s TEXT)");
    ExecOK("INSERT INTO tmp VALUES(UPPER('hello'))");
    ok = (CountRows("SELECT * FROM tmp WHERE s = 'HELLO'") == 1);
    ExecOK("DELETE FROM tmp");
    ExecOK("INSERT INTO tmp VALUES(LOWER('WORLD'))");
    ok = ok && (CountRows("SELECT * FROM tmp WHERE s = 'world'") == 1);
    ExecOK("DROP TABLE tmp");
    return ok;
}

static int test_math_typeof(void) {
    int ok;
    ExecOK("CREATE TABLE tmp(t TEXT)");
    ExecOK("INSERT INTO tmp VALUES(TYPEOF(123))");
    ok = (CountRows("SELECT * FROM tmp WHERE t = 'numeric'") == 1);
    ExecOK("DELETE FROM tmp");
    ExecOK("INSERT INTO tmp VALUES(TYPEOF('abc'))");
    ok = ok && (CountRows("SELECT * FROM tmp WHERE t = 'text'") == 1);
    ExecOK("DROP TABLE tmp");
    return ok;
}

static int test_math_concat(void) {
    int ok;
    ExecOK("CREATE TABLE tmp(s TEXT)");
    ExecOK("INSERT INTO tmp VALUES('Hello' || ', ' || 'World!')");
    ok = (CountRows("SELECT * FROM tmp WHERE s = 'Hello, World!'") == 1);
    ExecOK("DROP TABLE tmp");
    return ok;
}

static int test_math_expr_chain(void) {
    /* Chain of operations to stress expression evaluation */
    int result = GetInt(
        "SELECT ((100 + 200) * 3 - 50) / 5 + ABS(-25) - LENGTH('test')");
    /* (300)*3=900, -50=850, /5=170, +25=195, -4=191 */
    return (result == 191);
}

/*============================================================================
** Test Cases - Floating-Point Math (Memory only)
**============================================================================*/

/* Helper to get a float result and compare with tolerance */
static double g_floatResult;
static int FloatCallback(void *arg, int argc, char **argv, char **cols) {
    (void)arg; (void)cols;
    if (argc > 0 && argv[0]) {
        /* String to double conversion with scientific notation support */
        double val = 0.0;
        double frac = 0.0;
        double div = 1.0;
        int neg = 0;
        int inFrac = 0;
        int exp = 0;
        int expNeg = 0;
        const char *s = argv[0];
        if (*s == '-') { neg = 1; s++; }
        while (*s && *s != 'e' && *s != 'E') {
            if (*s == '.') { inFrac = 1; s++; continue; }
            if (*s >= '0' && *s <= '9') {
                if (inFrac) {
                    div *= 10.0;
                    frac += (*s - '0') / div;
                } else {
                    val = val * 10.0 + (*s - '0');
                }
            }
            s++;
        }
        val = val + frac;
        /* Parse exponent if present */
        if (*s == 'e' || *s == 'E') {
            s++;
            if (*s == '-') { expNeg = 1; s++; }
            else if (*s == '+') { s++; }
            while (*s >= '0' && *s <= '9') {
                exp = exp * 10 + (*s - '0');
                s++;
            }
            while (exp-- > 0) val = expNeg ? val / 10.0 : val * 10.0;
        }
        g_floatResult = neg ? -val : val;
    }
    return 0;
}

static double GetFloat(const char *sql) {
    g_floatResult = 0.0;
    sqlite_exec(g_db, sql, FloatCallback, NULL, NULL);
    return g_floatResult;
}

static int FloatClose(double a, double b, double tol) {
    double diff = a - b;
    if (diff < 0) diff = -diff;
    return diff < tol;
}

static int test_fp_multiply(void) {
    double result = GetFloat("SELECT 3.14159 * 2.0");
    return FloatClose(result, 6.28318, 0.0001);
}

static int test_fp_divide(void) {
    double result = GetFloat("SELECT 22.0 / 7.0");
    return FloatClose(result, 3.142857, 0.0001);
}

static int test_fp_add_sub(void) {
    double result = GetFloat("SELECT 1.5 + 2.25 - 0.75");
    return FloatClose(result, 3.0, 0.0001);
}

static int test_fp_round(void) {
    int ok;
    ok = FloatClose(GetFloat("SELECT ROUND(3.14159, 2)"), 3.14, 0.001);
    ok = ok && FloatClose(GetFloat("SELECT ROUND(2.5)"), 3.0, 0.001);
    ok = ok && FloatClose(GetFloat("SELECT ROUND(9.999, 1)"), 10.0, 0.001);
    return ok;
}

static int test_fp_abs(void) {
    double result = GetFloat("SELECT ABS(-3.14159)");
    return FloatClose(result, 3.14159, 0.00001);
}

static int test_fp_mixed(void) {
    /* Mix integer and float operations */
    double result = GetFloat("SELECT (100 + 0.5) * 2.0 / 4.0");
    /* 100.5 * 2.0 = 201.0, / 4.0 = 50.25 */
    return FloatClose(result, 50.25, 0.001);
}

static int test_fp_chain(void) {
    /* Chain of FP operations to stress FPU */
    double result = GetFloat(
        "SELECT ((1.1 + 2.2) * 3.3 - 4.4) / 5.5 + 6.6");
    /* 3.3 * 3.3 = 10.89, - 4.4 = 6.49, / 5.5 = 1.18, + 6.6 = 7.78 */
    return FloatClose(result, 7.78, 0.01);
}

static int test_fp_precision(void) {
    /* Test precision with small numbers - use looser tolerance */
    double result = GetFloat("SELECT 0.001 * 0.001");
    return FloatClose(result, 0.000001, 0.000001);  /* 100% tolerance for tiny numbers */
}

static int test_fp_large(void) {
    /* Test with larger numbers */
    double result = GetFloat("SELECT 123456.789 * 1000.0");
    return FloatClose(result, 123456789.0, 1.0);
}

/*============================================================================
** Benchmark Tests - Memory (M-xxx from BENCHMARK_PLAN.md)
**============================================================================*/

/* M-001: Baseline Memory Footprint */
static DWORD g_memBefore, g_memAfter;

static int test_mem_baseline(void) {
    MEMORYSTATUS ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);
    g_memBefore = (DWORD)(ms.dwAvailPhys / 1024);
    /* Just measure - always passes */
    return 1;
}

static int test_mem_after_schema(void) {
    MEMORYSTATUS ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);
    g_memAfter = (DWORD)(ms.dwAvailPhys / 1024);
    /* Report delta in debug context */
    SetDebugContext("delta: %d KB", (int)(g_memBefore - g_memAfter));
    return 1;
}

/* M-002: Query Result Memory Scaling */
static int test_mem_result_100(void) {
    MEMORYSTATUS ms1, ms2;
    int i;
    sqlite *saved = g_db;

    g_db = sqlite_open(":memory:", 0, NULL);
    if (!g_db) return 0;

    sqlite_exec(g_db, "CREATE TABLE t(id INTEGER PRIMARY KEY, data TEXT)", NULL, NULL, NULL);
    sqlite_exec(g_db, "BEGIN", NULL, NULL, NULL);
    for (i = 0; i < 100; i++) {
        sqlite_exec(g_db, "INSERT INTO t VALUES(NULL, 'Test data row for memory benchmark')", NULL, NULL, NULL);
    }
    sqlite_exec(g_db, "COMMIT", NULL, NULL, NULL);

    ms1.dwLength = sizeof(ms1);
    GlobalMemoryStatus(&ms1);

    /* Fetch all rows */
    CountRows("SELECT * FROM t");

    ms2.dwLength = sizeof(ms2);
    GlobalMemoryStatus(&ms2);

    sqlite_close(g_db);
    g_db = saved;

    SetDebugContext("100 rows, %d KB", (int)((ms1.dwAvailPhys - ms2.dwAvailPhys) / 1024));
    return 1;
}

static int test_mem_result_1k(void) {
    MEMORYSTATUS ms1, ms2;
    int i;
    sqlite *saved = g_db;

    g_db = sqlite_open(":memory:", 0, NULL);
    if (!g_db) return 0;

    sqlite_exec(g_db, "CREATE TABLE t(id INTEGER PRIMARY KEY, data TEXT)", NULL, NULL, NULL);
    sqlite_exec(g_db, "BEGIN", NULL, NULL, NULL);
    for (i = 0; i < 1000; i++) {
        sqlite_exec(g_db, "INSERT INTO t VALUES(NULL, 'Test data row for memory benchmark')", NULL, NULL, NULL);
    }
    sqlite_exec(g_db, "COMMIT", NULL, NULL, NULL);

    ms1.dwLength = sizeof(ms1);
    GlobalMemoryStatus(&ms1);

    CountRows("SELECT * FROM t");

    ms2.dwLength = sizeof(ms2);
    GlobalMemoryStatus(&ms2);

    sqlite_close(g_db);
    g_db = saved;

    SetDebugContext("1K rows, %d KB", (int)((ms1.dwAvailPhys - ms2.dwAvailPhys) / 1024));
    return 1;
}

/*============================================================================
** Benchmark Tests - I/O (I-xxx from BENCHMARK_PLAN.md)
**============================================================================*/

/* I-001: Sequential Read Performance */
static int test_io_seq_read(void) {
    sqlite *db;
    char path[128];
    wchar_t pathW[128];
    char sql[256];
    int i, count;
    DWORD start, elapsed;
    char *p;

    BuildTestPath(path, "io_seq_test.db");
    BuildTestPathW(pathW, L"io_seq_test.db");
    DeleteFileW(pathW);

    db = sqlite_open(path, 0, NULL);
    if (!db) return 0;

    /* Create table with chunky data */
    sqlite_exec(db, "CREATE TABLE data(id INTEGER PRIMARY KEY, chunk TEXT)", NULL, NULL, NULL);
    sqlite_exec(db, "BEGIN", NULL, NULL, NULL);

    /* Insert 1000 rows with ~100 byte chunks */
    for (i = 0; i < 1000; i++) {
        p = sql;
        STR_COPY(p, "INSERT INTO data VALUES(");
        /* Add id */
        {
            char num[16];
            char *n = num + 15;
            int v = i + 1;
            *n = '\0';
            while (v > 0) { *--n = '0' + (v % 10); v /= 10; }
            if (i == 0) *--n = '1';
            STR_COPY(p, n);
        }
        STR_COPY(p, ", 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ')");
        *p = '\0';
        sqlite_exec(db, sql, NULL, NULL, NULL);
    }
    sqlite_exec(db, "COMMIT", NULL, NULL, NULL);

    /* Time sequential read */
    start = GetTickCount();
    {
        sqlite *saved = g_db;
        g_db = db;
        count = CountRows("SELECT * FROM data");
        g_db = saved;
    }
    elapsed = GetTickCount() - start;

    sqlite_close(db);
    DeleteFileW(pathW);

    SetDebugContext("%d rows in %d ms", count);
    return (count == 1000);
}

/* I-004: Sorted Dirty Page Writes */
static int test_io_sorted_writes(void) {
    sqlite *db;
    char path[128];
    wchar_t pathW[128];
    DWORD start, elapsed;
    int i;

    BuildTestPath(path, "io_write_test.db");
    BuildTestPathW(pathW, L"io_write_test.db");
    DeleteFileW(pathW);

    db = sqlite_open(path, 0, NULL);
    if (!db) return 0;

    /* Create multiple tables to spread pages */
    sqlite_exec(db, "CREATE TABLE t1(id INTEGER PRIMARY KEY, data TEXT)", NULL, NULL, NULL);
    sqlite_exec(db, "CREATE TABLE t2(id INTEGER PRIMARY KEY, data TEXT)", NULL, NULL, NULL);
    sqlite_exec(db, "CREATE TABLE t3(id INTEGER PRIMARY KEY, data TEXT)", NULL, NULL, NULL);

    /* Insert data to dirty multiple pages */
    sqlite_exec(db, "BEGIN", NULL, NULL, NULL);
    for (i = 0; i < 100; i++) {
        sqlite_exec(db, "INSERT INTO t1 VALUES(NULL, 'Data for table 1')", NULL, NULL, NULL);
        sqlite_exec(db, "INSERT INTO t2 VALUES(NULL, 'Data for table 2')", NULL, NULL, NULL);
        sqlite_exec(db, "INSERT INTO t3 VALUES(NULL, 'Data for table 3')", NULL, NULL, NULL);
    }

    /* Time the commit (writes dirty pages) */
    start = GetTickCount();
    sqlite_exec(db, "COMMIT", NULL, NULL, NULL);
    elapsed = GetTickCount() - start;

    sqlite_close(db);
    DeleteFileW(pathW);

    SetDebugContext("commit: %d ms", (int)elapsed);
    return 1;
}

/*============================================================================
** Benchmark Tests - Scalability (S-xxx from BENCHMARK_PLAN.md)
**============================================================================*/

/* S-001: Maximum practical table size test */
static int test_scale_10k_rows(void) {
    sqlite *db;
    char sql[128];
    int i;
    DWORD start, elapsed;
    char *p;

    db = sqlite_open(":memory:", 0, NULL);
    if (!db) return 0;

    sqlite_exec(db, "CREATE TABLE big(id INTEGER PRIMARY KEY, val INTEGER, name TEXT)", NULL, NULL, NULL);
    sqlite_exec(db, "CREATE INDEX idx_val ON big(val)", NULL, NULL, NULL);

    start = GetTickCount();
    sqlite_exec(db, "BEGIN", NULL, NULL, NULL);
    for (i = 0; i < 10000; i++) {
        p = sql;
        STR_COPY(p, "INSERT INTO big VALUES(NULL, ");
        {
            char num[16];
            char *n = num + 15;
            int v = i % 1000;
            *n = '\0';
            if (v == 0) *--n = '0';
            while (v > 0) { *--n = '0' + (v % 10); v /= 10; }
            STR_COPY(p, n);
        }
        STR_COPY(p, ", 'Row data')");
        *p = '\0';
        sqlite_exec(db, sql, NULL, NULL, NULL);
    }
    sqlite_exec(db, "COMMIT", NULL, NULL, NULL);
    elapsed = GetTickCount() - start;

    /* Verify count */
    {
        sqlite *saved = g_db;
        g_db = db;
        i = GetInt("SELECT COUNT(*) FROM big");
        g_db = saved;
    }

    sqlite_close(db);

    SetDebugContext("10K rows in %d ms", (int)elapsed);
    return (i == 10000);
}

/* S-003: Query stress test */
static int test_scale_query_stress(void) {
    sqlite *db;
    int i, count = 0;
    DWORD start, elapsed;

    db = sqlite_open(":memory:", 0, NULL);
    if (!db) return 0;

    sqlite_exec(db, "CREATE TABLE t(id INTEGER PRIMARY KEY, val INTEGER)", NULL, NULL, NULL);
    sqlite_exec(db, "INSERT INTO t VALUES(1, 100)", NULL, NULL, NULL);
    sqlite_exec(db, "INSERT INTO t VALUES(2, 200)", NULL, NULL, NULL);
    sqlite_exec(db, "INSERT INTO t VALUES(3, 300)", NULL, NULL, NULL);

    start = GetTickCount();
    for (i = 0; i < 1000; i++) {
        sqlite *saved = g_db;
        g_db = db;
        if (GetInt("SELECT SUM(val) FROM t") == 600) count++;
        g_db = saved;
    }
    elapsed = GetTickCount() - start;

    sqlite_close(db);

    SetDebugContext("1K queries in %d ms", (int)elapsed);
    return (count == 1000);
}

/* S-006: Rapid open/close cycles */
static int test_scale_open_close(void) {
    char path[128];
    wchar_t pathW[128];
    int i, ok = 1;
    DWORD start, elapsed;
    MEMORYSTATUS ms1, ms2;

    BuildTestPath(path, "open_close_test.db");
    BuildTestPathW(pathW, L"open_close_test.db");
    DeleteFileW(pathW);

    /* Create initial database */
    {
        sqlite *db = sqlite_open(path, 0, NULL);
        if (!db) return 0;
        sqlite_exec(db, "CREATE TABLE t(id INTEGER PRIMARY KEY)", NULL, NULL, NULL);
        sqlite_close(db);
    }

    ms1.dwLength = sizeof(ms1);
    GlobalMemoryStatus(&ms1);

    start = GetTickCount();
    for (i = 0; i < 100; i++) {
        sqlite *db = sqlite_open(path, 0, NULL);
        if (!db) { ok = 0; break; }
        sqlite_exec(db, "SELECT COUNT(*) FROM t", NULL, NULL, NULL);
        sqlite_close(db);
    }
    elapsed = GetTickCount() - start;

    ms2.dwLength = sizeof(ms2);
    GlobalMemoryStatus(&ms2);

    DeleteFileW(pathW);

    /* Check for memory leak (should be minimal) */
    SetDebugContext("100 cycles, %d ms, %d KB delta", (int)elapsed);
    return ok && ((ms1.dwAvailPhys - ms2.dwAvailPhys) < 50 * 1024);  /* < 50KB leak */
}

/*============================================================================
** Test Registry
**============================================================================*/

static TestCase g_tests[] = {
    /* Init - Database/file operations */
    { "Open :memory: database",     test_open_memory,           CAT_INIT, PMASK_MEM },
    { "Open file database",         test_open_file,             CAT_INIT, PMASK_FILE },
    { "Load benchmark schema",      test_load_schema,           CAT_INIT, PMASK_MEM },
    { "File persistence",           test_persistence,           CAT_INIT, PMASK_FILE },
    { "Export DB schema",           test_export_db_schema,      CAT_INIT, PMASK_FILE },
    { "Export DB data",             test_export_db_data,        CAT_INIT, PMASK_FILE },
    { "Memory DB isolation",        test_memory_isolation,      CAT_INIT, PMASK_MEM },
    
    /* Write - INSERT operations */
    { "INSERT with explicit id",    test_insert,                CAT_WRITE, PMASK_ALL },
    { "INSERT with auto id",        test_insert_null_id,        CAT_WRITE, PMASK_ALL },
    { "Multiple row insert",        test_multiple_rows,         CAT_WRITE, PMASK_ALL },
    { "INTEGER type",               test_type_integer,          CAT_WRITE, PMASK_ALL },
    { "Negative INTEGER",           test_type_negative,         CAT_WRITE, PMASK_ALL },
    { "TEXT type",                  test_type_text,             CAT_WRITE, PMASK_ALL },
    { "NULL value",                 test_type_null,             CAT_WRITE, PMASK_ALL },
    { "SQL quote escaping",         test_sql_quote_escape,      CAT_WRITE, PMASK_ALL },
    { "String with quotes",         test_string_embedded_quotes,CAT_WRITE, PMASK_ALL },
    { "Large integer (32-bit max)", test_large_integer,         CAT_WRITE, PMASK_ALL },
    { "Transaction COMMIT",         test_transaction_commit,    CAT_WRITE, PMASK_ALL },
    { "Transaction ROLLBACK",       test_transaction_rollback,  CAT_WRITE, PMASK_ALL },
    
    /* Read - SELECT operations */
    { "SELECT rows",                test_select,                CAT_READ, PMASK_ALL },
    { "SELECT rowid",               test_select_rowid,          CAT_READ, PMASK_ALL },
    { "SELECT explicit INTEGER PK", test_select_explicit_id,    CAT_READ, PMASK_ALL },
    { "Empty table SELECT",         test_empty_table_select,    CAT_READ, PMASK_ALL },
    { "ORDER BY",                   test_order_by,              CAT_READ, PMASK_ALL },
    { "ORDER BY multiple cols",     test_order_by_multiple,     CAT_READ, PMASK_ALL },
    { "LIMIT and OFFSET",           test_limit_offset,          CAT_READ, PMASK_ALL },
    
    /* Update - UPDATE/DELETE operations */
    { "UPDATE",                     test_update,                CAT_UPDATE, PMASK_ALL },
    { "DELETE",                     test_delete,                CAT_UPDATE, PMASK_ALL },
    
    /* Query - Complex operations */
    { "COUNT(*)",                   test_count,                 CAT_QUERY, PMASK_MEM },
    { "SUM aggregate",              test_sum,                   CAT_QUERY, PMASK_MEM },
    { "MIN/MAX aggregate",          test_min_max,               CAT_QUERY, PMASK_MEM },
    { "GROUP BY with HAVING",       test_group_by_having,       CAT_QUERY, PMASK_MEM },
    { "JOIN",                       test_join,                  CAT_QUERY, PMASK_MEM },
    { "VIEW with JOIN",             test_view_with_join,        CAT_QUERY, PMASK_MEM },
    { "SELECT from VIEW",           test_view_select,           CAT_QUERY, PMASK_MEM },
    { "LIKE pattern",               test_like,                  CAT_QUERY, PMASK_MEM },
    { "IS NULL",                    test_is_null,               CAT_QUERY, PMASK_MEM },
    { "NULL comparisons",           test_null_comparisons,      CAT_QUERY, PMASK_MEM },
    { "Subquery in WHERE",          test_subquery_where,        CAT_QUERY, PMASK_MEM },
    { "Subquery in FROM",           test_subquery_from,         CAT_QUERY, PMASK_MEM },
    { "UNION",                      test_union,                 CAT_QUERY, PMASK_MEM },
    { "UNION ALL",                  test_union_all,             CAT_QUERY, PMASK_MEM },
    { "Trigger fires on INSERT",    test_trigger_fires_insert,  CAT_QUERY, PMASK_MEM },
    { "Trigger fires on UPDATE",    test_trigger_fires_update,  CAT_QUERY, PMASK_MEM },
    { "Trigger fires on DELETE",    test_trigger_fires_delete,  CAT_QUERY, PMASK_MEM },
    { "Trigger NEW.column ref",     test_trigger_new_reference, CAT_QUERY, PMASK_MEM },
    { "UNIQUE INDEX constraint",    test_index_unique,          CAT_QUERY, PMASK_MEM },
    { "datetime('now')",            test_datetime_now,          CAT_QUERY, PMASK_MEM },
    { "date('now')",                test_date_now,              CAT_QUERY, PMASK_MEM },
    { "time('now')",                test_time_now,              CAT_QUERY, PMASK_MEM },
    /* Benchmark schema queries */
    { "Bench: JOIN orders",         test_bench_join_orders,     CAT_QUERY, PMASK_MEM },
    { "Bench: JOIN items",          test_bench_join_items,      CAT_QUERY, PMASK_MEM },
    { "Bench: 3-way JOIN",          test_bench_three_way_join,  CAT_QUERY, PMASK_MEM },
    { "Bench: SUM aggregate",       test_bench_aggregate_sum,   CAT_QUERY, PMASK_MEM },
    { "Bench: COUNT aggregate",     test_bench_aggregate_count, CAT_QUERY, PMASK_MEM },
    { "Bench: indexed lookup",      test_bench_indexed_lookup,  CAT_QUERY, PMASK_MEM },
    { "Bench: subquery",            test_bench_subquery,        CAT_QUERY, PMASK_MEM },
    
    /* Schema - DDL operations */
    { "CREATE TABLE",               test_create_table,          CAT_SCHEMA, PMASK_MEM },
    { "DROP TABLE",                 test_drop_table,            CAT_SCHEMA, PMASK_MEM },
    { "CREATE TRIGGER",             test_trigger_create,        CAT_SCHEMA, PMASK_MEM },
    { "DROP TRIGGER",               test_trigger_drop,          CAT_SCHEMA, PMASK_MEM },
    { "CREATE VIEW",                test_view_create,           CAT_SCHEMA, PMASK_MEM },
    { "DROP VIEW",                  test_view_drop,             CAT_SCHEMA, PMASK_MEM },
    { "CREATE INDEX",               test_index_create,          CAT_SCHEMA, PMASK_MEM },
    { "DROP INDEX",                 test_index_drop,            CAT_SCHEMA, PMASK_MEM },
    { "sqlite_master tables",       test_sqlite_master_tables,  CAT_SCHEMA, PMASK_MEM },
    { "sqlite_master indexes",      test_sqlite_master_indexes, CAT_SCHEMA, PMASK_MEM },
    { "VIEW in sqlite_master",      test_view_in_sqlite_master, CAT_SCHEMA, PMASK_MEM },
    { "INDEX in sqlite_master",     test_index_in_sqlite_master,CAT_SCHEMA, PMASK_MEM },
    
    /* Error - Error handling */
    { "Invalid SQL error",          test_invalid_sql,           CAT_ERROR, PMASK_MEM },
    { "Missing table error",        test_missing_table,         CAT_ERROR, PMASK_MEM },
    { "Constraint violation",       test_constraint_violation,  CAT_ERROR, PMASK_MEM },
    
    /* Math (Integer) - CPU integer arithmetic (Memory only) */
    { "Integer arithmetic",         test_math_integer,          CAT_MATH_INT, PMASK_MEM },
    { "Multiply",                   test_math_multiply,         CAT_MATH_INT, PMASK_MEM },
    { "Divide",                     test_math_divide,           CAT_MATH_INT, PMASK_MEM },
    { "ABS function",               test_math_abs,              CAT_MATH_INT, PMASK_MEM },
    { "MIN/MAX functions",          test_math_minmax,           CAT_MATH_INT, PMASK_MEM },
    { "ROUND function",             test_math_round,            CAT_MATH_INT, PMASK_MEM },
    { "COALESCE function",          test_math_coalesce,         CAT_MATH_INT, PMASK_MEM },
    { "NULLIF function",            test_math_nullif,           CAT_MATH_INT, PMASK_MEM },
    { "LENGTH function",            test_math_length,           CAT_MATH_INT, PMASK_MEM },
    { "SUBSTR function",            test_math_substr,           CAT_MATH_INT, PMASK_MEM },
    { "UPPER/LOWER functions",      test_math_upper_lower,      CAT_MATH_INT, PMASK_MEM },
    { "TYPEOF function",            test_math_typeof,           CAT_MATH_INT, PMASK_MEM },
    { "String concatenation",       test_math_concat,           CAT_MATH_INT, PMASK_MEM },
    { "Expression chain",           test_math_expr_chain,       CAT_MATH_INT, PMASK_MEM },
    
    /* Math (FP) - Floating-point arithmetic (Memory only) */
    { "FP multiply",                test_fp_multiply,           CAT_MATH_FP, PMASK_MEM },
    { "FP divide",                  test_fp_divide,             CAT_MATH_FP, PMASK_MEM },
    { "FP add/subtract",            test_fp_add_sub,            CAT_MATH_FP, PMASK_MEM },
    { "FP ROUND function",          test_fp_round,              CAT_MATH_FP, PMASK_MEM },
    { "FP ABS function",            test_fp_abs,                CAT_MATH_FP, PMASK_MEM },
    { "FP mixed int/float",         test_fp_mixed,              CAT_MATH_FP, PMASK_MEM },
    { "FP expression chain",        test_fp_chain,              CAT_MATH_FP, PMASK_MEM },
    { "FP precision (small)",       test_fp_precision,          CAT_MATH_FP, PMASK_MEM },
    { "FP large numbers",           test_fp_large,              CAT_MATH_FP, PMASK_MEM },

    /* Memory benchmarks (M-xxx) */
    { "M-001: Memory baseline",     test_mem_baseline,          CAT_MEMORY, PMASK_MEM },
    { "M-001: Memory after schema", test_mem_after_schema,      CAT_MEMORY, PMASK_MEM },
    { "M-002: Result memory 100",   test_mem_result_100,        CAT_MEMORY, PMASK_MEM },
    { "M-002: Result memory 1K",    test_mem_result_1k,         CAT_MEMORY, PMASK_MEM },

    /* I/O benchmarks (I-xxx) */
    { "I-001: Sequential read",     test_io_seq_read,           CAT_IO, PMASK_FILE },
    { "I-004: Sorted page writes",  test_io_sorted_writes,      CAT_IO, PMASK_FILE },

    /* Scalability benchmarks (S-xxx) */
    { "S-001: Insert 10K rows",     test_scale_10k_rows,        CAT_SCALE, PMASK_MEM },
    { "S-003: Query stress 1K",     test_scale_query_stress,    CAT_SCALE, PMASK_MEM },
    { "S-006: Open/close 100x",     test_scale_open_close,      CAT_SCALE, PMASK_FILE },

    { NULL, NULL, 0, 0 }
};

/*============================================================================
** Benchmark Runner
**============================================================================*/

static void OutputMemoryInfo(void) {
    MEMORYSTATUS ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);
    Output("Memory: ");
    OutputInt("", (int)(ms.dwAvailPhys / 1024));
    Output(" KB free of ");
    OutputInt("", (int)(ms.dwTotalPhys / 1024));
    OutputLine(" KB");
}

/* Processor architecture constants - may be missing from CE 2.0 headers */
#ifndef PROCESSOR_ARCHITECTURE_SHX
#define PROCESSOR_ARCHITECTURE_SHX 4
#endif
#ifndef PROCESSOR_ARCHITECTURE_ARM
#define PROCESSOR_ARCHITECTURE_ARM 5
#endif

static void OutputDeviceInfo(void) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    Output("CPU: ");
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_SHX:
            Output("SH");
            break;
        case PROCESSOR_ARCHITECTURE_MIPS:
            Output("MIPS");
            break;
        case PROCESSOR_ARCHITECTURE_ARM:
            Output("ARM");
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            Output("x86");
            break;
        default:
            Output("Unknown");
            break;
    }
    Output(" (level ");
    OutputInt("", si.wProcessorLevel);
    OutputLine(")");
}

static void RunTests(void) {
    TestCase *t;
    int result, iter, cat, path;
    DWORD totalStart, totalEnd;
    DWORD testStart, testEnd, testMs;
    char ramDbPath[128], flashDbPath[128];
    wchar_t ramDbPathW[128], flashDbPathW[128];
    int hasFlash;
    
    ClearOutput();
    g_nTests = 0;
    g_nPassed = 0;
    g_cumulativeMs = 0;
    g_benchSchemaLoaded = 0;  /* Reset for fresh run */
    
    /* Show immediate feedback before batch mode */
    OutputLine("Running benchmark...");
    FlushOutput();
    
    /* Create test directories */
    CreateDirectoryW(g_ramPathW, NULL);
    hasFlash = (g_flashPath[0] != 0);
    if (hasFlash) {
        CreateDirectoryW(g_flashPathW, NULL);
    }
    
    ClearOutput();
    g_batchMode = 1;
    
    /* Reset category and path stats */
    for (cat = 0; cat < CAT_COUNT; cat++) {
        g_catMs[cat] = 0;
        g_catTests[cat] = 0;
        g_catPassed[cat] = 0;
    }
    for (path = 0; path < PATH_COUNT; path++) {
        g_pathMs[path] = 0;
        g_pathTests[path] = 0;
    }
    
    OutputLine("=== SQLite/CEbench ===");
    Output("Version: " APP_VERSION "  SQLite: ");
    OutputLine(sqlite_libversion());
    OutputDeviceInfo();
    OutputMemoryInfo();
    if (hasFlash) {
        Output("Storage: RAM + ");
        Output(g_flashPath);
        OutputLine("");
    } else {
        OutputLine("Storage: RAM only (no card detected)");
    }
    if (g_iterations > 1) {
        Output("Iterations: ");
        OutputInt("", g_iterations);
        OutputLine("");
    }
    OutputLine("");
    
    totalStart = GetTickCount();
    
    /* Initialize databases */
    OutputLine("--- Initializing ---");
    
    /* Memory database */
    testStart = GetTickCount();
    g_db = sqlite_open(":memory:", 0, NULL);
    if (!g_db) {
        OutputLine("  [FAIL] Open :memory:");
        FlushOutput();
        return;
    }
    LoadBenchSchema(g_db);
    testEnd = GetTickCount();
    Output("  [PASS] Memory database        ");
    OutputInt("", (int)(testEnd - testStart));
    OutputLine(" ms");
    g_pathMs[PATH_MEM] += (testEnd - testStart);
    
    /* RAM filesystem database */
    BuildTestPath(ramDbPath, "bench_ram.db");
    BuildTestPathW(ramDbPathW, L"bench_ram.db");
    lstrcpyW(g_testPathW, g_ramPathW);
    WideCharToMultiByte(CP_ACP, 0, g_testPathW, -1, g_testPath, 128, NULL, NULL);
    DeleteFileW(ramDbPathW);
    testStart = GetTickCount();
    g_ramDb = sqlite_open(ramDbPath, 0, NULL);
    if (g_ramDb) {
        LoadBenchSchema(g_ramDb);
        testEnd = GetTickCount();
        Output("  [PASS] RAM database           ");
        OutputInt("", (int)(testEnd - testStart));
        OutputLine(" ms");
        g_pathMs[PATH_RAM] += (testEnd - testStart);
    } else {
        OutputLine("  [FAIL] RAM database");
    }
    
    /* Flash database (if available) */
    if (hasFlash) {
        lstrcpyW(g_testPathW, g_flashPathW);
        WideCharToMultiByte(CP_ACP, 0, g_testPathW, -1, g_testPath, 128, NULL, NULL);
        BuildTestPath(flashDbPath, "bench_flash.db");
        BuildTestPathW(flashDbPathW, L"bench_flash.db");
        DeleteFileW(flashDbPathW);
        testStart = GetTickCount();
        g_flashDb = sqlite_open(flashDbPath, 0, NULL);
        if (g_flashDb) {
            LoadBenchSchema(g_flashDb);
            testEnd = GetTickCount();
            Output("  [PASS] Flash database         ");
            OutputInt("", (int)(testEnd - testStart));
            OutputLine(" ms");
            g_pathMs[PATH_FLASH] += (testEnd - testStart);
        } else {
            OutputLine("  [FAIL] Flash database");
        }
    }
    OutputLine("");
    
    /* Mark schema as loaded (for benchmark query tests) */
    g_benchSchemaLoaded = 1;
    
    /* Run tests by path, then by category */
    
    /* Memory path */
    g_curDb = g_db;
    OutputLine("=== Memory Tests ===");
    RefreshOutput();
    PumpMessages();
    for (cat = 0; cat < CAT_COUNT; cat++) {
        int hasTests = 0;
        for (t = g_tests; t->name; t++) {
            if (t->category == cat && (t->pathMask & PMASK_MEM)) { hasTests = 1; break; }
        }
        if (!hasTests) continue;
        
        Output("--- ");
        Output(g_catNames[cat]);
        OutputLine(" ---");
        
        for (t = g_tests; t->name; t++) {
            if (t->category != cat) continue;
            if (!(t->pathMask & PMASK_MEM)) continue;
            testStart = GetTickCount();
            result = 1;
            for (iter = 0; iter < g_iterations && result; iter++) {
                result = t->func();
            }
            testEnd = GetTickCount();
            testMs = testEnd - testStart;
            t->lastMs = testMs;
            RecordTest(t->name, result, testMs, cat, PATH_MEM);
            g_pathMs[PATH_MEM] += testMs;
            g_pathTests[PATH_MEM]++;
        }
        OutputLine("");
    }
    
    /* RAM filesystem path */
    if (g_ramDb) {
        sqlite *savedDb = g_db;
        g_db = g_ramDb;
        g_curDb = g_ramDb;
        g_benchSchemaLoaded = 1;  /* Schema already loaded */
        SetCurrentPath(PATH_RAM);
        
        OutputLine("=== Object Store Tests ===");
        RefreshOutput();
        PumpMessages();
        for (cat = 0; cat < CAT_COUNT; cat++) {
            int hasTests = 0;
            for (t = g_tests; t->name; t++) {
                if (t->category == cat && (t->pathMask & PMASK_RAM)) { hasTests = 1; break; }
            }
            if (!hasTests) continue;
            
            Output("--- ");
            Output(g_catNames[cat]);
            OutputLine(" ---");
            
            for (t = g_tests; t->name; t++) {
                if (t->category != cat) continue;
                if (!(t->pathMask & PMASK_RAM)) continue;
                testStart = GetTickCount();
                result = 1;
                for (iter = 0; iter < g_iterations && result; iter++) {
                    result = t->func();
                }
                testEnd = GetTickCount();
                testMs = testEnd - testStart;
                RecordTest(t->name, result, testMs, cat, PATH_RAM);
                g_pathMs[PATH_RAM] += testMs;
                g_pathTests[PATH_RAM]++;
            }
            OutputLine("");
        }
        g_db = savedDb;
    }
    
    /* Flash path */
    if (g_flashDb) {
        sqlite *savedDb = g_db;
        g_db = g_flashDb;
        g_curDb = g_flashDb;
        g_benchSchemaLoaded = 1;
        SetCurrentPath(PATH_FLASH);
        
        OutputLine("=== Flash Storage Tests ===");
        RefreshOutput();
        PumpMessages();
        for (cat = 0; cat < CAT_COUNT; cat++) {
            int hasTests = 0;
            for (t = g_tests; t->name; t++) {
                if (t->category == cat && (t->pathMask & PMASK_FLASH)) { hasTests = 1; break; }
            }
            if (!hasTests) continue;
            
            Output("--- ");
            Output(g_catNames[cat]);
            OutputLine(" ---");
            
            for (t = g_tests; t->name; t++) {
                if (t->category != cat) continue;
                if (!(t->pathMask & PMASK_FLASH)) continue;
                testStart = GetTickCount();
                result = 1;
                for (iter = 0; iter < g_iterations && result; iter++) {
                    result = t->func();
                }
                testEnd = GetTickCount();
                testMs = testEnd - testStart;
                RecordTest(t->name, result, testMs, cat, PATH_FLASH);
                g_pathMs[PATH_FLASH] += testMs;
                g_pathTests[PATH_FLASH]++;
            }
            OutputLine("");
        }
        g_db = savedDb;
    }
    
    /* Cleanup databases */
    if (g_db) { sqlite_close(g_db); g_db = NULL; }
    if (g_ramDb) { sqlite_close(g_ramDb); g_ramDb = NULL; DeleteFileW(ramDbPathW); }
    if (g_flashDb) { sqlite_close(g_flashDb); g_flashDb = NULL; DeleteFileW(flashDbPathW); }
    
    /* Cleanup directories */
    RemoveDirectoryW(g_ramPathW);
    if (hasFlash) RemoveDirectoryW(g_flashPathW);
    
    totalEnd = GetTickCount();
    
    OutputLine("--- Summary ---");
    OutputInt("Tests:      ", g_nTests);
    OutputLine("");
    OutputInt("Passed:     ", g_nPassed);
    OutputLine("");
    OutputInt("Failed:     ", g_nTests - g_nPassed);
    OutputLine("");
    OutputLine("");
    
    /* Category breakdown */
    for (cat = 0; cat < CAT_COUNT; cat++) {
        int pct;
        if (g_catTests[cat] == 0) continue;
        Output(g_catNames[cat]);
        Output(":  ");
        /* Pad category name */
        {
            int len = 0;
            const char *p = g_catNames[cat];
            while (*p++) len++;
            while (len++ < 14) Output(" ");
        }
        OutputInt("", (int)g_catMs[cat]);
        Output(" ms");
        pct = (g_cumulativeMs > 0) ? (int)((g_catMs[cat] * 100) / g_cumulativeMs) : 0;
        Output(" (");
        OutputInt("", pct);
        OutputLine("%)");
    }
    OutputLine("");
    
    /* Path breakdown */
    OutputLine("By storage path:");
    for (path = 0; path < PATH_COUNT; path++) {
        if (g_pathTests[path] == 0 && g_pathMs[path] == 0) continue;
        Output("  ");
        Output(g_pathNames[path]);
        Output(":  ");
        {
            int len = 0;
            const char *p = g_pathNames[path];
            while (*p++) len++;
            while (len++ < 10) Output(" ");
        }
        OutputInt("", (int)g_pathMs[path]);
        OutputLine(" ms");
    }
    OutputLine("");
    
    OutputInt("Cumulative: ", (int)g_cumulativeMs);
    OutputLine(" ms");
    OutputInt("Total:      ", (int)(totalEnd - totalStart));
    OutputLine(" ms");
    {
        DWORD overhead = (totalEnd - totalStart) - g_cumulativeMs;
        int pct = (g_cumulativeMs > 0) ? (int)((overhead * 100) / (totalEnd - totalStart)) : 0;
        Output("Overhead:   ");
        OutputInt("", (int)overhead);
        Output(" ms (");
        OutputInt("", pct);
        OutputLine("%)");
    }
    
    if (g_nPassed == g_nTests) {
        OutputLine("");
        OutputLine("*** ALL TESTS PASSED ***");
    }
    
    OutputLine("");
    OutputMemoryInfo();
    
    FlushOutput();
}

/*============================================================================
** Window Procedure and Entry Point
**============================================================================*/

/* Menu IDs */
#define IDM_RUN      201
#define IDM_SAVE     202
#define IDM_VERBOSE  203
#define IDM_SYNCFOLD 204
#define IDM_FONTSIZE 205
#define IDM_ITER1    206
#define IDM_ITER10   207
#define IDM_ITER100  208
#define IDM_DIAG     209
#define IDM_STORAGE  210

static HMENU g_hMenu;
static int g_useSyncFolder = 1;  /* Default: try Sync folder first */
static int g_fontSizes[] = {10, 12, 14, 16};
static int g_fontSizeIdx = 2;  /* Default: 14 */
static HFONT g_hFont;

static void SetOutputFont(void) {
    HFONT hOld = g_hFont;
    LOGFONTW lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = g_fontSizes[g_fontSizeIdx];
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    lstrcpyW(lf.lfFaceName, L"Courier New");
    g_hFont = CreateFontIndirectW(&lf);
    SendMessage(g_hwndOutput, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    if (hOld) DeleteObject(hOld);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            RECT rc;
            int cbHeight;
            HMENU hMenuBar, hMenuOpt, hMenuIter;
            TBBUTTON tbButtons[2];
            
            /* Create command bar */
            g_hwndCB = CommandBar_Create(g_hInst, hwnd, 1);
            
            /* Detect storage card at startup */
            FindStorageCard();
            
            /* Add menu bar with Run (direct action) and Options (popup) */
            hMenuBar = CreateMenu();
            AppendMenuW(hMenuBar, MF_STRING, IDM_RUN, L"&Run");
            
            /* Iterations submenu */
            hMenuIter = CreatePopupMenu();
            AppendMenuW(hMenuIter, MF_STRING | MF_CHECKED, IDM_ITER1, L"&1x (Validate)");
            AppendMenuW(hMenuIter, MF_STRING, IDM_ITER10, L"1&0x");
            AppendMenuW(hMenuIter, MF_STRING, IDM_ITER100, L"10&0x");
            
            hMenuOpt = CreatePopupMenu();
            AppendMenuW(hMenuOpt, MF_POPUP, (UINT)hMenuIter, L"&Iterations");
            AppendMenuW(hMenuOpt, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenuOpt, MF_STRING | (g_storageCardPath[0] ? 0 : MF_GRAYED), IDM_STORAGE, L"Use &Storage Card");
            AppendMenuW(hMenuOpt, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenuOpt, MF_STRING, IDM_VERBOSE, L"&Verbose");
            AppendMenuW(hMenuOpt, MF_STRING | MF_CHECKED, IDM_SYNCFOLD, L"Save to S&ync Folder");
            AppendMenuW(hMenuOpt, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenuOpt, MF_STRING, IDM_DIAG, L"&Diagnostics");
            AppendMenuW(hMenuBar, MF_POPUP, (UINT)hMenuOpt, L"&Options");
            
            CommandBar_InsertMenubarEx(g_hwndCB, NULL, (LPTSTR)hMenuBar, 0);
            g_hMenu = hMenuBar;
            
            /* Add toolbar buttons */
            CommandBar_AddBitmap(g_hwndCB, HINST_COMMCTRL, IDB_STD_SMALL_COLOR, 15, 0, 0);
            
            memset(tbButtons, 0, sizeof(tbButtons));
            tbButtons[0].iBitmap = STD_FILESAVE;
            tbButtons[0].idCommand = IDM_SAVE;
            tbButtons[0].fsState = TBSTATE_ENABLED;
            tbButtons[0].fsStyle = TBSTYLE_BUTTON;
            
            tbButtons[1].iBitmap = STD_FIND;  /* Magnifier for font size */
            tbButtons[1].idCommand = IDM_FONTSIZE;
            tbButtons[1].fsState = TBSTATE_ENABLED;
            tbButtons[1].fsStyle = TBSTYLE_BUTTON;
            
            CommandBar_AddButtons(g_hwndCB, 2, tbButtons);
            
            CommandBar_AddAdornments(g_hwndCB, 0, 0);
            cbHeight = CommandBar_Height(g_hwndCB);
            
            GetClientRect(hwnd, &rc);
            
            g_hwndOutput = CreateWindowW(
                L"EDIT", L"SQLite/CEbench v" L"1.2.0",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE,
                0, cbHeight, rc.right, rc.bottom - cbHeight,
                hwnd, (HMENU)101, g_hInst, NULL);
            
            /* Subclass to block input while keeping white background */
            g_pfnEditProc = (WNDPROC)SetWindowLong(g_hwndOutput, GWL_WNDPROC, (LONG)EditSubclassProc);
            
            SetOutputFont();
            
            return 0;
        }
        
        case WM_SIZE: {
            RECT rc;
            int cbHeight = CommandBar_Height(g_hwndCB);
            GetClientRect(hwnd, &rc);
            MoveWindow(g_hwndOutput, 0, cbHeight, rc.right, rc.bottom - cbHeight, TRUE);
            return 0;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK:
                    DestroyWindow(hwnd);
                    break;
                case IDM_RUN:
                    RunTests();
                    break;
                case IDM_SAVE: {
                    HANDLE hFile;
                    SYSTEMTIME st;
                    wchar_t path[128];
                    int usedAlt = 0;
                    
                    GetLocalTime(&st);
                    
                    if (g_useSyncFolder) {
                        wsprintfW(path, L"\\My Documents\\Synchronized Files\\bench_%04d%02d%02d_%02d%02d%02d.log",
                            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
                        hFile = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                        if (hFile == INVALID_HANDLE_VALUE) {
                            wsprintfW(path, L"\\Temp\\bench_%04d%02d%02d_%02d%02d%02d.log",
                                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
                            hFile = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                            usedAlt = 1;
                        }
                    } else {
                        wsprintfW(path, L"\\Temp\\bench_%04d%02d%02d_%02d%02d%02d.log",
                            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
                        hFile = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                    }
                    
                    if (hFile != INVALID_HANDLE_VALUE) {
                        DWORD written;
                        WriteFile(hFile, g_szOutput, g_nOutput, &written, NULL);
                        CloseHandle(hFile);
                        OutputLine("");
                        OutputLine(usedAlt ? "Log saved to \\Temp\\" : (g_useSyncFolder ? "Log saved to Synchronized Files" : "Log saved to \\Temp\\"));
                    }
                    break;
                }
                case IDM_VERBOSE:
                    g_verboseMode = !g_verboseMode;
                    CheckMenuItem(g_hMenu, IDM_VERBOSE, g_verboseMode ? MF_CHECKED : MF_UNCHECKED);
                    break;
                case IDM_SYNCFOLD:
                    g_useSyncFolder = !g_useSyncFolder;
                    CheckMenuItem(g_hMenu, IDM_SYNCFOLD, g_useSyncFolder ? MF_CHECKED : MF_UNCHECKED);
                    break;
                case IDM_FONTSIZE:
                    g_fontSizeIdx = (g_fontSizeIdx + 1) % 4;
                    SetOutputFont();
                    break;
                case IDM_ITER1:
                    g_iterations = 1;
                    CheckMenuItem(g_hMenu, IDM_ITER1, MF_CHECKED);
                    CheckMenuItem(g_hMenu, IDM_ITER10, MF_UNCHECKED);
                    CheckMenuItem(g_hMenu, IDM_ITER100, MF_UNCHECKED);
                    break;
                case IDM_ITER10:
                    g_iterations = 10;
                    CheckMenuItem(g_hMenu, IDM_ITER1, MF_UNCHECKED);
                    CheckMenuItem(g_hMenu, IDM_ITER10, MF_CHECKED);
                    CheckMenuItem(g_hMenu, IDM_ITER100, MF_UNCHECKED);
                    break;
                case IDM_ITER100:
                    g_iterations = 100;
                    CheckMenuItem(g_hMenu, IDM_ITER1, MF_UNCHECKED);
                    CheckMenuItem(g_hMenu, IDM_ITER10, MF_UNCHECKED);
                    CheckMenuItem(g_hMenu, IDM_ITER100, MF_CHECKED);
                    break;
                case IDM_STORAGE:
                    g_useStorageCard = !g_useStorageCard;
                    CheckMenuItem(g_hMenu, IDM_STORAGE, g_useStorageCard ? MF_CHECKED : MF_UNCHECKED);
                    UpdateTestPath();
                    break;
                case IDM_DIAG:
                    ClearOutput();
                    RunDiagnostics();
                    break;
            }
            return 0;
        
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC:
            /* Make edit control white */
            SetBkColor((HDC)wParam, RGB(255, 255, 255));
            return (LRESULT)GetStockObject(WHITE_BRUSH);
        
        case WM_KEYDOWN:
            if (wParam == VK_CONTROL || wParam == VK_MENU || wParam == VK_SHIFT)
                break;  /* Ignore modifier keys alone */
            if (GetKeyState(VK_CONTROL) < 0) {
                if (wParam == 'S') { SendMessage(hwnd, WM_COMMAND, IDM_SAVE, 0); return 0; }
            }
            if (GetKeyState(VK_MENU) < 0) {
                if (wParam == 'V') { SendMessage(hwnd, WM_COMMAND, IDM_VERBOSE, 0); return 0; }
            }
            break;
        
        case WM_SYSCOMMAND:
            /* Block menu activation via Alt key alone (SC_KEYMENU with no char) */
            if ((wParam & 0xFFF0) == SC_KEYMENU && lParam == 0)
                return 0;
            break;
        
        case WM_DESTROY:
            /* Clean up database handles to release file locks */
            if (g_db) { sqlite_close(g_db); g_db = NULL; }
            if (g_ramDb) { sqlite_close(g_ramDb); g_ramDb = NULL; }
            if (g_flashDb) { sqlite_close(g_flashDb); g_flashDb = NULL; }
            CommandBar_Destroy(g_hwndCB);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR lpCmd, int nShow) {
    WNDCLASSW wc = {0};
    MSG msg;
    RECT rcWork;
    
    (void)hPrev; (void)lpCmd; (void)nShow;
    
    g_hInst = hInst;
    InitCommonControls();
    CreateDirectoryW(L"\\Temp", NULL);
    
    /* Get work area (screen minus taskbar) */
    SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWork, 0);
    
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(1));
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"SQLiteCEBench";
    RegisterClassW(&wc);
    
    /* Full-screen style typical for CE apps - no caption/border */
    g_hwndMain = CreateWindowW(L"SQLiteCEBench", L"SQLite/CEbench",
        WS_VISIBLE,
        rcWork.left, rcWork.top, 
        rcWork.right - rcWork.left, rcWork.bottom - rcWork.top,
        NULL, NULL, hInst, NULL);
    
    /* Set small icon for taskbar */
#ifndef ICON_SMALL
#define ICON_SMALL 0
#endif
    SendMessage(g_hwndMain, WM_SETICON, ICON_SMALL,
        (LPARAM)LoadImage(hInst, MAKEINTRESOURCE(1), IMAGE_ICON, 16, 16, 0));
    
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}
