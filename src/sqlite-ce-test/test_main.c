/*
** SQLite/CE Test Harness
**
** Simple GUI app to validate SQLite/CE DLL functionality.
** Creates a window with test output and runs basic database operations.
*/

#include <windows.h>
#include "sqlite.h"

/* Window handles */
static HWND g_hwndMain;
static HWND g_hwndOutput;
static HWND g_hwndRunBtn;

/* Debug function from DLL */
__declspec(dllimport) void sqliteRbtreeGetDebugInfo(int *pNode, int *pKey, int *nKey, unsigned char *bytes);

/* Output buffer */
static char g_szOutput[16000];
static int g_nOutput = 0;

/* Test counters */
static int g_nTests = 0;
static int g_nPassed = 0;

/*
** Convert UTF-8/ANSI to wide string and set window text
*/
static void SetOutputText(void) {
    wchar_t wzBuf[16000];
    MultiByteToWideChar(CP_ACP, 0, g_szOutput, -1, wzBuf, 16000);
    SetWindowTextW(g_hwndOutput, wzBuf);
    SendMessage(g_hwndOutput, EM_SETSEL, g_nOutput, g_nOutput);
    SendMessage(g_hwndOutput, EM_SCROLLCARET, 0, 0);
}

/*
** Append text to output buffer and update display
*/
static void Output(const char *sz) {
    int len = 0;
    const char *p = sz;
    while (*p++) len++;
    
    if (g_nOutput + len < sizeof(g_szOutput) - 1) {
        char *d = g_szOutput + g_nOutput;
        p = sz;
        while (*p) *d++ = *p++;
        *d = '\0';
        g_nOutput += len;
    }
    SetOutputText();
}

static void OutputLine(const char *sz) {
    Output(sz);
    Output("\r\n");
}

/*
** Simple integer to string
*/
static void IntToStr(int val, char *buf) {
    char tmp[16];
    char *p = tmp + 15;
    int neg = 0;
    *p = '\0';
    if (val < 0) { neg = 1; val = -val; }
    if (val == 0) *--p = '0';
    while (val > 0) { *--p = '0' + (val % 10); val /= 10; }
    if (neg) *--p = '-';
    while (*p) *buf++ = *p++;
    *buf = '\0';
}

/*
** Test assertion
*/
static void Test(const char *name, int condition) {
    g_nTests++;
    if (condition) {
        g_nPassed++;
        Output("[PASS] ");
    } else {
        Output("[FAIL] ");
    }
    OutputLine(name);
}

/*
** Callback for sqlite_exec - just count rows and display
*/
static int QueryCallback(void *pArg, int argc, char **argv, char **colNames) {
    int *pCount = (int *)pArg;
    int i;
    (*pCount)++;
    
    for (i = 0; i < argc; i++) {
        if (i > 0) Output(", ");
        Output(colNames[i]);
        Output("=");
        Output(argv[i] ? argv[i] : "NULL");
    }
    Output("\r\n");
    return 0;
}

/*
** Run all tests
*/
static void RunTests(void) {
    sqlite *db = NULL;
    char *zErr = NULL;
    int rc;
    int rowCount;
    char buf[64];
    const char *zTestDb = "\\Temp\\sqlitetest.db";
    HANDLE hFile;
    DWORD dwWritten;
    
    g_szOutput[0] = '\0';
    g_nOutput = 0;
    g_nTests = 0;
    g_nPassed = 0;
    
    OutputLine("=== SQLite/CE Test Harness ===");
    OutputLine("");
    
    /* Diagnostic: Test raw file creation first */
    OutputLine("--- Diagnostics ---");
    
    /* Test atoi */
    Output("atoi(\"100\")=");
    IntToStr(atoi("100"), buf);
    OutputLine(buf);
    
    /* Test intToKey/keyToInt round-trip - this is what SQLite uses for INTEGER PRIMARY KEY */
    {
        int orig = 100;
        unsigned int key = (orig) ^ 0x80000000;  /* intToKey without swap */
        int back = (key) ^ 0x80000000;           /* keyToInt without swap */
        Output("intToKey(100)="); IntToStr((int)key, buf); OutputLine(buf);
        Output("keyToInt back="); IntToStr(back, buf); OutputLine(buf);
    }
    
    /* Test CreateDirectoryW */
    rc = CreateDirectoryW(L"\\Temp", NULL);
    Output("CreateDirectory \\Temp: ");
    if (rc) {
        OutputLine("Created");
    } else {
        DWORD err = GetLastError();
        if (err == ERROR_ALREADY_EXISTS || err == 6) {
            OutputLine("OK (exists)");
        } else {
            IntToStr(err, buf);
            Output("err=");
            OutputLine(buf);
        }
    }
    
    /* Test raw CreateFileW */
    hFile = CreateFileW(L"\\Temp\\test.tmp",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    Output("CreateFileW: ");
    if (hFile != INVALID_HANDLE_VALUE) {
        OutputLine("OK");
        /* Try writing */
        rc = WriteFile(hFile, "test", 4, &dwWritten, NULL);
        Output("WriteFile: ");
        if (rc && dwWritten == 4) {
            OutputLine("OK");
        } else {
            IntToStr(GetLastError(), buf);
            Output("err=");
            OutputLine(buf);
        }
        CloseHandle(hFile);
        DeleteFileW(L"\\Temp\\test.tmp");
    } else {
        IntToStr(GetLastError(), buf);
        Output("err=");
        OutputLine(buf);
    }
    
    /* Show sqlite version */
    Output("SQLite version: ");
    OutputLine(sqlite_libversion());
    
    /* Test in-memory database first */
    OutputLine("");
    OutputLine("Testing :memory: database...");
    db = sqlite_open(":memory:", 0, &zErr);
    Output(":memory: open: ");
    if (db) {
        OutputLine("OK");
        rc = sqlite_exec(db, "CREATE TABLE t(x)", NULL, NULL, &zErr);
        Output("CREATE TABLE: ");
        OutputLine(rc == SQLITE_OK ? "OK" : "FAIL");
        sqlite_close(db);
        db = NULL;
    } else {
        Output("FAIL - ");
        OutputLine(zErr ? zErr : "unknown");
        if (zErr) sqlite_freemem(zErr);
    }
    
    OutputLine("");
    OutputLine("--- SQLite File Tests ---");
    
    /* Delete any existing test database */
    if (DeleteFileW(L"\\Temp\\sqlitetest.db")) {
        OutputLine("Deleted old test database");
    }
    DeleteFileW(L"\\Temp\\sqlitetest.db-journal");
    
    /* Also try root - in case path handling puts it there */
    DeleteFileW(L"\\sqlitetest.db");
    DeleteFileW(L"\\sqlitetest.db-journal");
    
    /* Test 1: Open database */
    OutputLine("Opening database: \\Temp\\sqlitetest.db");
    db = sqlite_open(zTestDb, 0, &zErr);
    Test("sqlite_open returns non-NULL", db != NULL);
    if (!db) {
        Output("Error: ");
        OutputLine(zErr ? zErr : "unknown");
        if (zErr) sqlite_freemem(zErr);
        goto done;
    }
    
    /* Check if file was actually created */
    {
        DWORD attrs = GetFileAttributesW(L"\\Temp\\sqlitetest.db");
        Output("File exists after open: ");
        OutputLine(attrs != 0xFFFFFFFF ? "YES" : "NO");
    }
    
    /* Test 2: Create table - drop first if exists */
    OutputLine("");
    OutputLine("Creating table...");
    rc = sqlite_exec(db, "DROP TABLE test", NULL, NULL, NULL);
    Output("DROP TABLE rc="); IntToStr(rc, buf); OutputLine(buf);
    
    rc = sqlite_exec(db, "CREATE TABLE test(id INTEGER PRIMARY KEY, name TEXT, value REAL)", NULL, NULL, &zErr);
    Output("CREATE TABLE rc="); IntToStr(rc, buf); OutputLine(buf);
    Test("CREATE TABLE", rc == SQLITE_OK);
    if (rc != SQLITE_OK) {
        Output("Error: ");
        OutputLine(zErr ? zErr : "unknown");
        if (zErr) sqlite_freemem(zErr);
    }
    
    /* Show table schema */
    OutputLine("Table info:");
    sqlite_exec(db, "PRAGMA table_info(test)", QueryCallback, &rowCount, NULL);
    
    /* Also create a table without INTEGER PRIMARY KEY for comparison */
    sqlite_exec(db, "DROP TABLE test2", NULL, NULL, NULL);
    sqlite_exec(db, "CREATE TABLE test2(id INTEGER, name TEXT)", NULL, NULL, NULL);
    
    /* Verify table is empty */
    rowCount = 0;
    sqlite_exec(db, "SELECT * FROM test", QueryCallback, &rowCount, NULL);
    Output("Rows after CREATE: "); IntToStr(rowCount, buf); OutputLine(buf);
    
    /* Test 3: Insert data - use very different IDs */
    OutputLine("");
    OutputLine("Inserting rows...");
    
    /* First test: auto-generated rowid */
    rc = sqlite_exec(db, "INSERT INTO test(name,value) VALUES('Test', 999)", NULL, NULL, &zErr);
    Output("  auto-id rc="); IntToStr(rc, buf); OutputLine(buf);
    if (zErr) { Output("  err="); OutputLine(zErr); sqlite_freemem(zErr); zErr = NULL; }
    Output("  auto last_insert_rowid="); IntToStr(sqlite_last_insert_rowid(db), buf); OutputLine(buf);
    sqlite_exec(db, "DELETE FROM test", NULL, NULL, NULL);
    
    /* Test intToKey/keyToInt at runtime with actual values */
    {
        int orig = 100;
        unsigned int key;
        int back;
        unsigned char *kb;
        
        key = (unsigned int)orig ^ 0x80000000u;
        kb = (unsigned char *)&key;
        Output("  100 as key bytes: ");
        IntToStr(kb[0], buf); Output(buf); Output(" ");
        IntToStr(kb[1], buf); Output(buf); Output(" ");
        IntToStr(kb[2], buf); Output(buf); Output(" ");
        IntToStr(kb[3], buf); OutputLine(buf);
        
        back = (int)(key ^ 0x80000000u);
        Output("  round-trip: "); IntToStr(back, buf); OutputLine(buf);
    }
    
    /* Check struct packing - mimic CellHdr */
    {
        struct TestCellHdr {
            unsigned int leftChild;
            unsigned short nKey;
            unsigned short iNext;
            unsigned char nKeyHi;
            unsigned char nDataHi;
            unsigned short nData;
        };
        int testVal, swapped;
        
        Output("  sizeof(TestCellHdr)="); IntToStr(sizeof(struct TestCellHdr), buf); OutputLine(buf);
        
        /* Test byte swap locally using same macro as DLL */
        #define TEST_BYTESWAP(X) \
            (((((int)(X)) & 0xFF) << 24) | ((((int)(X)) & 0xFF00) << 8) | \
             ((((int)(X)) >> 8) & 0xFF00) | ((((int)(X)) >> 24) & 0xFF))
        
        testVal = 0x80000001;
        swapped = TEST_BYTESWAP(testVal);
        Output("  local byteswap(0x80000001)="); IntToStr(swapped, buf); OutputLine(buf);
        Output("  expected 0x01000080="); IntToStr(0x01000080, buf); OutputLine(buf);
    }
    
    /* Minimal rowid test - fresh db, single insert, immediate query */
    OutputLine("");
    OutputLine("Minimal rowid test:");
    {
        sqlite *testdb;
        char *err = NULL;
        testdb = sqlite_open(":memory:", 0, &err);
        if (testdb) {
            sqlite_exec(testdb, "CREATE TABLE t(x)", NULL, NULL, NULL);
            sqlite_exec(testdb, "INSERT INTO t VALUES('hello')", NULL, NULL, NULL);
            Output("  last_insert_rowid="); IntToStr(sqlite_last_insert_rowid(testdb), buf); OutputLine(buf);
            OutputLine("  SELECT rowid,x FROM t:");
            sqlite_exec(testdb, "SELECT rowid,x FROM t", QueryCallback, &rowCount, NULL);
            
            /* Get debug info via function call */
            {
                int dbg_pNode, dbg_pKey, dbg_nKey;
                unsigned char dbg_bytes[4];
                sqliteRbtreeGetDebugInfo(&dbg_pNode, &dbg_pKey, &dbg_nKey, dbg_bytes);
                Output("  rbtree pNode="); IntToStr(dbg_pNode, buf); OutputLine(buf);
                Output("  rbtree pKey="); IntToStr(dbg_pKey, buf); OutputLine(buf);
                Output("  rbtree nKey="); IntToStr(dbg_nKey, buf); OutputLine(buf);
                Output("  rbtree bytes: ");
                IntToStr(dbg_bytes[0], buf); Output(buf); Output(" ");
                IntToStr(dbg_bytes[1], buf); Output(buf); Output(" ");
                IntToStr(dbg_bytes[2], buf); Output(buf); Output(" ");
                IntToStr(dbg_bytes[3], buf); OutputLine(buf);
            }
            
            sqlite_close(testdb);
        } else {
            Output("  open failed: "); OutputLine(err ? err : "unknown");
        }
    }
    
    /* Test regular INTEGER column (not PRIMARY KEY) */
    sqlite_exec(db, "INSERT INTO test2(id,name) VALUES(100,'Test')", NULL, NULL, NULL);
    OutputLine("test2 (regular INTEGER):");
    sqlite_exec(db, "SELECT * FROM test2", QueryCallback, &rowCount, NULL);
    OutputLine("test2 rowid (auto-generated):");
    Output("  C API last_insert_rowid: "); IntToStr(sqlite_last_insert_rowid(db), buf); OutputLine(buf);
    sqlite_exec(db, "SELECT last_insert_rowid() as lirid", QueryCallback, &rowCount, NULL);
    sqlite_exec(db, "SELECT rowid, * FROM test2", QueryCallback, &rowCount, NULL);
    
    sqlite_exec(db, "DELETE FROM test2", NULL, NULL, NULL);
    
    rc = sqlite_exec(db, "INSERT INTO test(id,name,value) VALUES(100, 'Alice', 314)", NULL, NULL, &zErr);
    Output("  row1 rc="); IntToStr(rc, buf); OutputLine(buf);
    Test("INSERT row 1", rc == SQLITE_OK);
    if (zErr) { Output("  err="); OutputLine(zErr); sqlite_freemem(zErr); zErr = NULL; }
    Output("  last_insert_rowid="); IntToStr(sqlite_last_insert_rowid(db), buf); OutputLine(buf);
    rowCount = 0;
    sqlite_exec(db, "SELECT * FROM test", QueryCallback, &rowCount, NULL);
    Output("  count="); IntToStr(rowCount, buf); OutputLine(buf);
    
    rc = sqlite_exec(db, "INSERT INTO test(id,name,value) VALUES(200, 'Bob', 271)", NULL, NULL, &zErr);
    Output("  row2 rc="); IntToStr(rc, buf); OutputLine(buf);
    Test("INSERT row 2", rc == SQLITE_OK);
    if (zErr) { Output("  err="); OutputLine(zErr); sqlite_freemem(zErr); zErr = NULL; }
    rowCount = 0;
    sqlite_exec(db, "SELECT * FROM test", QueryCallback, &rowCount, NULL);
    Output("  count="); IntToStr(rowCount, buf); OutputLine(buf);
    
    rc = sqlite_exec(db, "INSERT INTO test(id,name,value) VALUES(300, 'Charlie', 141)", NULL, NULL, &zErr);
    Output("  row3 rc="); IntToStr(rc, buf); OutputLine(buf);
    Test("INSERT row 3", rc == SQLITE_OK);
    if (zErr) { Output("  err="); OutputLine(zErr); sqlite_freemem(zErr); zErr = NULL; }
    rowCount = 0;
    sqlite_exec(db, "SELECT * FROM test", QueryCallback, &rowCount, NULL);
    Output("  count="); IntToStr(rowCount, buf); OutputLine(buf);
    
    /* Test 4: Select data */
    OutputLine("");
    OutputLine("Selecting rows...");
    rowCount = 0;
    rc = sqlite_exec(db, "SELECT * FROM test ORDER BY id", QueryCallback, &rowCount, &zErr);
    Test("SELECT returns SQLITE_OK", rc == SQLITE_OK);
    Test("SELECT returns 3 rows", rowCount == 3);
    
    /* Test 5: Update - use id=200 which exists */
    OutputLine("");
    OutputLine("Updating row...");
    rc = sqlite_exec(db, "UPDATE test SET value = 9.99 WHERE id = 200", NULL, NULL, &zErr);
    Test("UPDATE", rc == SQLITE_OK);
    
    /* Test 6: Verify update */
    rowCount = 0;
    rc = sqlite_exec(db, "SELECT * FROM test WHERE id = 200", QueryCallback, &rowCount, &zErr);
    Test("SELECT after UPDATE", rc == SQLITE_OK && rowCount == 1);
    
    /* Test 7: Delete - use id=300 which exists */
    OutputLine("");
    OutputLine("Deleting row...");
    rc = sqlite_exec(db, "DELETE FROM test WHERE id = 300", NULL, NULL, &zErr);
    Test("DELETE", rc == SQLITE_OK);
    
    /* Test 8: Count remaining - should be 2 */
    rowCount = 0;
    rc = sqlite_exec(db, "SELECT COUNT(*) as cnt FROM test", QueryCallback, &rowCount, &zErr);
    Test("COUNT after DELETE", rc == SQLITE_OK);
    
    /* Test 9: Close database */
    OutputLine("");
    OutputLine("Closing database...");
    sqlite_close(db);
    db = NULL;
    Test("sqlite_close", 1); /* If we get here, it worked */
    
    /* Test 10: Reopen and verify persistence */
    OutputLine("");
    OutputLine("Reopening to verify persistence...");
    db = sqlite_open(zTestDb, 0, &zErr);
    Test("Reopen database", db != NULL);
    if (db) {
        rowCount = 0;
        rc = sqlite_exec(db, "SELECT * FROM test", QueryCallback, &rowCount, &zErr);
        Test("Data persisted (2 rows)", rc == SQLITE_OK && rowCount == 2);
        sqlite_close(db);
        db = NULL;
    }
    
done:
    if (db) sqlite_close(db);
    
    /* Summary */
    OutputLine("");
    OutputLine("=== Summary ===");
    Output("Tests: ");
    IntToStr(g_nTests, buf);
    OutputLine(buf);
    Output("Passed: ");
    IntToStr(g_nPassed, buf);
    OutputLine(buf);
    Output("Failed: ");
    IntToStr(g_nTests - g_nPassed, buf);
    OutputLine(buf);
    
    if (g_nPassed == g_nTests) {
        OutputLine("");
        OutputLine("*** ALL TESTS PASSED ***");
    }
    
    /* Cleanup test file */
    DeleteFileW(L"\\Temp\\sqlitetest.db");
}

/*
** Persistent database test - creates/updates a database that survives across runs
*/
static void RunPersistentTest(void) {
    sqlite *db = NULL;
    char *zErr = NULL;
    int rc;
    int rowCount;
    int runCount = 0;
    char buf[64];
    const char *zPersistDb = "\\My Documents\\persistent.db";
    
    g_szOutput[0] = '\0';
    g_nOutput = 0;
    
    OutputLine("=== Persistent Database Test ===");
    OutputLine("");
    Output("Database: ");
    OutputLine(zPersistDb);
    OutputLine("");
    
    /* Open or create the database */
    db = sqlite_open(zPersistDb, 0, &zErr);
    if (!db) {
        Output("Failed to open: ");
        OutputLine(zErr ? zErr : "unknown");
        if (zErr) sqlite_freemem(zErr);
        return;
    }
    OutputLine("Database opened OK");
    
    /* Try to create table - ignore error if it already exists */
    rc = sqlite_exec(db, 
        "CREATE TABLE runs(id INTEGER PRIMARY KEY, timestamp TEXT)",
        NULL, NULL, &zErr);
    if (rc != SQLITE_OK && zErr) {
        /* Check if it's just "table already exists" - that's OK */
        if (zErr[0] == 't' && zErr[1] == 'a') {
            /* "table runs already exists" - ignore */
            sqlite_freemem(zErr);
            zErr = NULL;
        } else {
            Output("CREATE TABLE error: ");
            OutputLine(zErr);
            sqlite_freemem(zErr);
        }
    }
    
    /* Insert a new run record with simple counter */
    rc = sqlite_exec(db, 
        "INSERT INTO runs(timestamp) VALUES('run')",
        NULL, NULL, &zErr);
    if (rc == SQLITE_OK) {
        OutputLine("Inserted new run record");
    } else {
        Output("INSERT error: ");
        OutputLine(zErr ? zErr : "unknown");
        if (zErr) sqlite_freemem(zErr);
    }
    
    /* Count total runs */
    OutputLine("");
    OutputLine("All runs recorded in this database:");
    rowCount = 0;
    rc = sqlite_exec(db, "SELECT id, timestamp FROM runs ORDER BY id", 
        QueryCallback, &rowCount, &zErr);
    
    OutputLine("");
    Output("Total runs: ");
    IntToStr(rowCount, buf);
    OutputLine(buf);
    
    sqlite_close(db);
    
    OutputLine("");
    OutputLine("Database saved. Run test again to see count increase!");
    OutputLine("Check \\My Documents\\persistent.db in File Explorer.");
}

/*
** Window procedure
*/
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            /* Create output edit control */
            g_hwndOutput = CreateWindowW(L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_LEFT,
                5, 5, rc.right - 10, rc.bottom - 40,
                hwnd, (HMENU)101, NULL, NULL);
            
            /* Create Run Tests button */
            g_hwndRunBtn = CreateWindowW(L"BUTTON", L"Run Tests",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                5, rc.bottom - 30, 80, 25,
                hwnd, (HMENU)102, NULL, NULL);
            
            /* Create Persistent Test button */
            CreateWindowW(L"BUTTON", L"Persistent",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                90, rc.bottom - 30, 75, 25,
                hwnd, (HMENU)103, NULL, NULL);
            
            /* Create Save Log button */
            CreateWindowW(L"BUTTON", L"Save Log",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                170, rc.bottom - 30, 70, 25,
                hwnd, (HMENU)104, NULL, NULL);
            
            return 0;
        }
        
        case WM_SIZE: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            MoveWindow(g_hwndOutput, 5, 5, rc.right - 10, rc.bottom - 40, TRUE);
            MoveWindow(g_hwndRunBtn, 5, rc.bottom - 30, 80, 25, TRUE);
            MoveWindow(GetDlgItem(hwnd, 103), 90, rc.bottom - 30, 75, 25, TRUE);
            MoveWindow(GetDlgItem(hwnd, 104), 170, rc.bottom - 30, 70, 25, TRUE);
            return 0;
        }
        
        case WM_COMMAND:
            if (LOWORD(wParam) == 102) { /* Run Tests button */
                wchar_t wzRunning[] = L"Running tests...";
                SetWindowTextW(g_hwndOutput, wzRunning);
                UpdateWindow(g_hwndOutput);
                RunTests();
            } else if (LOWORD(wParam) == 103) { /* Persistent DB button */
                wchar_t wzRunning[] = L"Running persistent test...";
                SetWindowTextW(g_hwndOutput, wzRunning);
                UpdateWindow(g_hwndOutput);
                RunPersistentTest();
            } else if (LOWORD(wParam) == 104) { /* Save Log button */
                HANDLE hFile;
                const wchar_t *logPath = L"\\My Documents\\Synchronized Files\\sqlite_test.log";
                const wchar_t *logPathAlt = L"\\Temp\\sqlite_test.log";
                const char *savedMsg = "Log saved to \\My Documents\\Synchronized Files\\";
                
                hFile = CreateFileW(logPath,
                    GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile == INVALID_HANDLE_VALUE) {
                    hFile = CreateFileW(logPathAlt,
                        GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                    savedMsg = "Log saved to \\Temp\\";
                }
                if (hFile != INVALID_HANDLE_VALUE) {
                    DWORD written;
                    WriteFile(hFile, g_szOutput, g_nOutput, &written, NULL);
                    CloseHandle(hFile);
                    OutputLine("");
                    OutputLine(savedMsg);
                }
            }
            return 0;
        
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/*
** Entry point
*/
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR lpCmd, int nShow) {
    WNDCLASSW wc;
    MSG msg;
    int screenW, screenH;
    
    /* Ensure \Temp exists */
    CreateDirectoryW(L"\\Temp", NULL);
    
    /* Get screen size */
    screenW = GetSystemMetrics(SM_CXSCREEN);
    screenH = GetSystemMetrics(SM_CYSCREEN);
    
    /* Register window class */
    wc.style = 0;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInst;
    wc.hIcon = NULL;
    wc.hCursor = NULL;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"SQLiteCETest";
    RegisterClassW(&wc);
    
    /* Create main window - use explicit size, not CW_USEDEFAULT */
    g_hwndMain = CreateWindowW(L"SQLiteCETest", L"SQLite/CE Test",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        0, 0, screenW, screenH - 26,  /* Leave room for taskbar */
        NULL, NULL, hInst, NULL);
    
    if (!g_hwndMain) return 1;
    
    ShowWindow(g_hwndMain, nShow);
    UpdateWindow(g_hwndMain);
    
    /* Message loop */
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}
