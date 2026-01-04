# SQLite/CE Test Harness

Simple GUI application to test SQLite/CE DLL functionality on Windows CE devices.

## Setting up in VS6

1. Open the SQLiteCE workspace (`Workspace/SQLiteCE.dsw`)

2. Add a new project: File → New → Projects tab
   - Select "WCE Application"
   - Project name: `SQLiteCETest`
   - Add to current workspace: Yes
   - Location: Should default to `Workspace/SQLiteCETest`
   - Click OK
   - Select "A typical 'Hello World' application" then Finish

3. Remove the generated source files from the project (the Hello World template files)

4. Add our source file:
   - Project → Add To Project → Files
   - Navigate to `src/sqlite-ce-test/test_main.c`
   - Add it

5. Configure include paths (Project → Settings → C/C++ → Preprocessor):
   - Additional include directories: `..\..\src\sqlite-ce`

6. Configure linker (Project → Settings → Link):
   - Object/library modules: Add `SQLiteCE.lib` 
   - Or add dependency: Project → Dependencies → check SQLiteCE

7. Add the /FI flag (Project → Settings → C/C++ → Project Options):
   - Add: `/FI"..\..\src\sqlite-ce\config.h"`

8. Set project dependency:
   - Project → Dependencies
   - Select SQLiteCETest
   - Check SQLiteCE (so DLL builds first)

9. Build for x86em target first for emulator testing

## Running

1. Build both SQLiteCE and SQLiteCETest
2. Deploy to emulator or device
3. Run SQLiteCETest.exe
4. Click "Run Tests" button
5. Results appear in the text area

## What it tests

- sqlite_open / sqlite_close
- CREATE TABLE
- INSERT
- SELECT with callback
- UPDATE
- DELETE
- Data persistence (close and reopen)
