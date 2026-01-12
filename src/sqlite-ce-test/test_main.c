/*
** SQLite/CE Test Harness
**
** Modular test framework for validating SQLite/CE DLL functionality.
*/

#include <windows.h>
#include <commctrl.h>
#include "sqlite.h"

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
    OutputLine(p);
}

/*============================================================================
** Test Framework
**============================================================================*/

static int g_nTests = 0;
static int g_nPassed = 0;
static sqlite *g_db = NULL;  /* Shared database handle for tests */

/* Test function type - returns 1 for pass, 0 for fail */
typedef int (*TestFunc)(void);

typedef struct {
    const char *name;
    TestFunc func;
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
            while (*t) *p++ = *t++;
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

/* Record test result */
static void RecordTest(const char *name, int passed) {
    g_nTests++;
    if (passed) {
        g_nPassed++;
        Output("  [PASS] ");
        OutputLine(name);
    } else {
        Output("  [FAIL] ");
        Output(name);
        if (g_debugContext[0]) {
            Output(" (");
            Output(g_debugContext);
            Output(")");
        }
        Output("\r\n");
    }
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
    DeleteFileW(L"\\Temp\\test_open.db");
    db = sqlite_open("\\Temp\\test_open.db", 0, NULL);
    if (!db) return 0;
    sqlite_close(db);
    DeleteFileW(L"\\Temp\\test_open.db");
    return 1;
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
    const char *path = "\\Temp\\test_persist.db";
    sqlite *db1, *db2, *saved_db;
    int ok = 0;
    
    DeleteFileW(L"\\Temp\\test_persist.db");
    
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
    
    DeleteFileW(L"\\Temp\\test_persist.db");
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
    OutputInt("  sizeof(int): ", sizeof(int));
    OutputInt("  sizeof(long): ", sizeof(long));
    
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
        OutputInt("  Expected 0x04030201: ", 0x04030201);
        Output("  Byteswap: ");
        OutputLine(swapped == 0x04030201 ? "OK" : "MISMATCH");
    }
    
    /* intToKey/keyToInt round-trip (critical for rowid) */
    {
        int orig = 100;
        unsigned int key = BYTESWAP((unsigned int)orig ^ 0x80000000u);
        int back = (int)(BYTESWAP(key) ^ 0x80000000u);
        OutputInt("  intToKey(100): ", (int)key);
        OutputInt("  keyToInt back: ", back);
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
    const char *srcPath = "\\Temp\\exp_src.db";
    const char *dstPath = "\\Temp\\exp_dst.db";
    sqlite *srcDb, *dstDb;
    char **result;
    int nRow, nCol, ok = 0;
    
    DeleteFileW(L"\\Temp\\exp_src.db");
    DeleteFileW(L"\\Temp\\exp_dst.db");
    
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
    
    DeleteFileW(L"\\Temp\\exp_src.db");
    DeleteFileW(L"\\Temp\\exp_dst.db");
    return ok;
}

static int test_export_db_data(void) {
    const char *srcPath = "\\Temp\\expd_src.db";
    const char *dstPath = "\\Temp\\expd_dst.db";
    sqlite *srcDb, *dstDb;
    char **result;
    int nRow, nCol, ok = 0;
    
    DeleteFileW(L"\\Temp\\expd_src.db");
    DeleteFileW(L"\\Temp\\expd_dst.db");
    
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
    
    DeleteFileW(L"\\Temp\\expd_src.db");
    DeleteFileW(L"\\Temp\\expd_dst.db");
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
** Test Registry
**============================================================================*/

static TestCase g_tests[] = {
    /* Basic operations */
    { "Open :memory: database",     test_open_memory },
    { "Open file database",         test_open_file },
    { "CREATE TABLE",               test_create_table },
    { "DROP TABLE",                 test_drop_table },
    
    /* CRUD */
    { "INSERT with explicit id",    test_insert },
    { "INSERT with auto id",        test_insert_null_id },
    { "SELECT rows",                test_select },
    { "SELECT rowid",               test_select_rowid },
    { "SELECT explicit INTEGER PK", test_select_explicit_id },
    { "UPDATE",                     test_update },
    { "DELETE",                     test_delete },
    
    /* Data types */
    { "INTEGER type",               test_type_integer },
    { "Negative INTEGER",           test_type_negative },
    { "TEXT type",                  test_type_text },
    { "NULL value",                 test_type_null },
    
    /* Multiple rows */
    { "Multiple row insert",        test_multiple_rows },
    { "ORDER BY",                   test_order_by },
    { "COUNT(*)",                   test_count },
    
    /* Persistence */
    { "File persistence",           test_persistence },
    
    /* Advanced */
    { "Transaction COMMIT",         test_transaction_commit },
    { "Transaction ROLLBACK",       test_transaction_rollback },
    { "LIKE pattern",               test_like },
    { "IS NULL",                    test_is_null },
    { "SUM aggregate",              test_sum },
    { "MIN/MAX aggregate",          test_min_max },
    { "JOIN",                       test_join },
    
    /* Export/Import (0.2.0) */
    { "SQL quote escaping",         test_sql_quote_escape },
    { "sqlite_master tables",       test_sqlite_master_tables },
    { "sqlite_master indexes",      test_sqlite_master_indexes },
    { "Export DB schema",           test_export_db_schema },
    { "Export DB data",             test_export_db_data },
    
    /* Error handling */
    { "Invalid SQL error",          test_invalid_sql },
    { "Missing table error",        test_missing_table },
    { "Constraint violation",       test_constraint_violation },
    
    /* Memory databases */
    { "Memory DB isolation",        test_memory_isolation },
    
    /* Triggers (0.5.0) */
    { "CREATE TRIGGER",             test_trigger_create },
    { "Trigger fires on INSERT",    test_trigger_fires_insert },
    { "Trigger fires on UPDATE",    test_trigger_fires_update },
    { "Trigger fires on DELETE",    test_trigger_fires_delete },
    { "Trigger NEW.column ref",     test_trigger_new_reference },
    { "DROP TRIGGER",               test_trigger_drop },
    
    /* Views (0.5.0) */
    { "CREATE VIEW",                test_view_create },
    { "SELECT from VIEW",           test_view_select },
    { "VIEW with JOIN",             test_view_with_join },
    { "DROP VIEW",                  test_view_drop },
    { "VIEW in sqlite_master",      test_view_in_sqlite_master },
    
    /* Indexes (0.5.0) */
    { "CREATE INDEX",               test_index_create },
    { "UNIQUE INDEX constraint",    test_index_unique },
    { "DROP INDEX",                 test_index_drop },
    { "INDEX in sqlite_master",     test_index_in_sqlite_master },
    
    /* Complex queries (0.5.0) */
    { "Subquery in WHERE",          test_subquery_where },
    { "Subquery in FROM",           test_subquery_from },
    { "UNION",                      test_union },
    { "UNION ALL",                  test_union_all },
    { "GROUP BY with HAVING",       test_group_by_having },
    { "ORDER BY multiple cols",     test_order_by_multiple },
    { "LIMIT and OFFSET",           test_limit_offset },
    
    /* Edge cases (0.5.0) */
    { "Empty table SELECT",         test_empty_table_select },
    { "NULL comparisons",           test_null_comparisons },
    { "String with quotes",         test_string_embedded_quotes },
    { "Large integer (32-bit max)", test_large_integer },
    
    /* Date/Time functions (0.5.0) */
    { "datetime('now')",            test_datetime_now },
    { "date('now')",                test_date_now },
    { "time('now')",                test_time_now },
    
    { NULL, NULL }
};

/*============================================================================
** Test Runner
**============================================================================*/

static void RunTests(void) {
    TestCase *t;
    int result;
    DWORD startTick, endTick;
    
    ClearOutput();
    g_nTests = 0;
    g_nPassed = 0;
    
    /* Show immediate feedback before batch mode */
    OutputLine("Running tests...");
    FlushOutput();
    
    ClearOutput();
    g_batchMode = 1;
    
    OutputLine("=== SQLite/CE Test Suite ===");
    Output("SQLite version: ");
    OutputLine(sqlite_libversion());
    OutputLine("");
    
    startTick = GetTickCount();
    
    g_db = sqlite_open(":memory:", 0, NULL);
    if (!g_db) {
        OutputLine("Failed to open test database");
        FlushOutput();
        return;
    }
    
    for (t = g_tests; t->name; t++) {
        result = t->func();
        RecordTest(t->name, result);
    }
    
    sqlite_close(g_db);
    g_db = NULL;
    
    endTick = GetTickCount();
    
    OutputLine("");
    OutputInt("Tests:  ", g_nTests);
    OutputInt("Passed: ", g_nPassed);
    OutputInt("Failed: ", g_nTests - g_nPassed);
    Output("Time:   ");
    {
        char buf[24];
        char *p;
        int ms = (int)(endTick - startTick);
        buf[20] = ' '; buf[21] = 'm'; buf[22] = 's'; buf[23] = '\0';
        p = buf + 20;
        if (ms == 0) *--p = '0';
        while (ms > 0) { *--p = '0' + (ms % 10); ms /= 10; }
        OutputLine(p);
    }
    
    if (g_nPassed == g_nTests) {
        OutputLine("");
        OutputLine("*** ALL TESTS PASSED ***");
    }
    
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
            HMENU hMenuBar, hMenuOpt;
            TBBUTTON tbButtons[2];
            
            /* Create command bar */
            g_hwndCB = CommandBar_Create(g_hInst, hwnd, 1);
            
            /* Add menu bar with Run (direct action) and Options (popup) */
            hMenuBar = CreateMenu();
            AppendMenuW(hMenuBar, MF_STRING, IDM_RUN, L"&Run");
            hMenuOpt = CreatePopupMenu();
            AppendMenuW(hMenuOpt, MF_STRING, IDM_VERBOSE, L"&Verbose\tAlt+V");
            AppendMenuW(hMenuOpt, MF_STRING | MF_CHECKED, IDM_SYNCFOLD, L"Save to &Sync Folder");
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
                L"EDIT", L"Tap Run to begin.",
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
                        wsprintfW(path, L"\\My Documents\\Synchronized Files\\sqlite_%04d%02d%02d_%02d%02d%02d.log",
                            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
                        hFile = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                        if (hFile == INVALID_HANDLE_VALUE) {
                            wsprintfW(path, L"\\Temp\\sqlite_%04d%02d%02d_%02d%02d%02d.log",
                                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
                            hFile = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                            usedAlt = 1;
                        }
                    } else {
                        wsprintfW(path, L"\\Temp\\sqlite_%04d%02d%02d_%02d%02d%02d.log",
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
    wc.lpszClassName = L"SQLiteCETest";
    RegisterClassW(&wc);
    
    /* Full-screen style typical for CE apps - no caption/border */
    g_hwndMain = CreateWindowW(L"SQLiteCETest", L"SQLite/CE Test",
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
