# SQLite/CEbench

Benchmark and validation suite for SQLite/CE DLL on Windows CE devices.

## Features

- **Validation**: 60+ tests covering CRUD, transactions, triggers, views, indexes, and more
- **Benchmarking**: Per-operation timing with configurable iteration counts (1x, 10x, 100x)
- **Memory reporting**: Shows free/total memory before and after benchmark run
- **Timing breakdown**: Cumulative time (sum of operations) vs total time (wall clock) with overhead percentage

## Setting up in VS6

1. Open the SQLiteCE workspace (`Workspace/SQLiteCE.dsw`)

2. Add a new project: File → New → Projects tab
   - Select "WCE Application"
   - Project name: `SQLiteCEBench`
   - Add to current workspace: Yes
   - Location: Should default to `Workspace/SQLiteCEBench`
   - Click OK
   - Select "A typical 'Hello World' application" then Finish

3. Remove the generated source files from the project (the Hello World template files)

4. Add our source file:
   - Project → Add To Project → Files
   - Navigate to `src/sqlite-ce-bench/bench_main.c`
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
   - Select SQLiteCEBench
   - Check SQLiteCE (so DLL builds first)

9. Build for x86em target first for emulator testing

## Running

1. Build both SQLiteCE and SQLiteCEBench
2. Deploy to emulator or device
3. Run SQLiteCEBench.exe
4. Select iteration count from Options → Iterations menu
5. Tap "Run" to start benchmark
6. Save results with the Save button (syncs to desktop via ActiveSync)

## Output Example

```
=== SQLite/CEbench ===
SQLite version: 2.8.17
Memory: 4,128 KB free of 8,192 KB
Iterations: 10

  [PASS] Open :memory: database          12 ms (x10)
  [PASS] CREATE TABLE                     3 ms (x10)
  ...

--- Summary ---
Tests:      60
Passed:     60
Cumulative: 1,234 ms
Total:      1,456 ms
Overhead:   222 ms (15%)

*** ALL TESTS PASSED ***

Memory: 4,096 KB free of 8,192 KB
```

## Menu Options

- **Run**: Execute benchmark with current settings
- **Options → Iterations**: Set 1x (validation), 10x, or 100x iterations
- **Options → Verbose**: Show raw callback values for debugging
- **Options → Save to Sync Folder**: Save logs to Synchronized Files (syncs to desktop)
- **Options → Diagnostics**: Show system info (byte order, file system tests, etc.)
