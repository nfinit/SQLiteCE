# SQLite/CE Build Guide

This document describes how to build SQLite/CE for different platforms.

## Project Structure

```
src/
├── sqlite/           # SQLite 2.8.17 core (portable C)
├── sqlite-ce/        # Windows CE platform layer
├── sqlite-ce-edit/   # GUI editor application
├── sqlite-ce-test/   # Unit test framework
└── sqlite-ce-bench/  # Benchmark tool
```

## Build Targets

| Target | Description | Primary Use |
|--------|-------------|-------------|
| sqlite-ce.dll | SQLite library for Windows CE | Production |
| sqlitece-edit.exe | Database editor GUI | Production |
| sqlite_test | Unit tests | Development |
| sqlite_bench | Performance benchmarks | Development |

---

## Windows CE Build (Primary Target)

SQLite/CE targets Windows CE 2.0+ devices using Visual C++ 6.0 with the
Windows CE Toolkit. This is the primary production target.

### Prerequisites

- Microsoft Visual C++ 6.0
- Windows CE Toolkit for VC++ 6.0
- Target SDK (HP 620LX, Generic CE, etc.)

### Creating VC++ 6.0 Projects

#### SQLite DLL Project (sqlite-ce.dll)

1. **Create new project**: File > New > Projects > "WCE Dynamic-Link Library"
   - Project name: `sqlite-ce`
   - Select target SDK (e.g., "H/PC Ver 2.00")
   - Select target CPU (e.g., SH3)

2. **Add source files** to project:
   ```
   From src/sqlite/:
     attach.c, auth.c, btree.c, btree_rb.c, build.c, copy.c, date.c,
     delete.c, encode.c, expr.c, func.c, hash.c, insert.c, main.c,
     pager.c, parse.c, pragma.c, printf.c, random.c, select.c,
     table.c, tokenize.c, trigger.c, update.c, util.c, vacuum.c,
     vdbe.c, vdbeaux.c, where.c

   From src/sqlite-ce/:
     os.c, mempool.c, log.c
   ```

3. **Configure include paths**: Project > Settings > C/C++ > Preprocessor
   - Additional include directories: `../sqlite,../sqlite-ce`

4. **Configure preprocessor**: Project > Settings > C/C++ > Preprocessor
   - Preprocessor definitions: Add `SQLITE_EXPORT`

5. **Build**: Build > Build sqlite-ce.dll

#### Editor Application (sqlitece-edit.exe)

1. **Create new project**: File > New > Projects > "WCE Application"
   - Project name: `sqlite-ce-edit`
   - Select same SDK and CPU as DLL

2. **Add source files** from `src/sqlite-ce-edit/`:
   ```
   main.c, database.c, dialogs.c, editor.c, execute.c, fileops.c,
   filepicker.c, find.c, globals.c, grid.c, import.c, output.c,
   schema.c, settings.c, strpool.c
   ```

3. **Configure include paths**:
   - Additional include directories: `../sqlite,../sqlite-ce`

4. **Link to SQLite DLL**: Project > Settings > Link
   - Object/library modules: Add `sqlite-ce.lib`
   - Additional library path: `../sqlite-ce/WCE{SDK}{CPU}/Release`

5. **Build**: Build > Build sqlite-ce-edit.exe

### Compiler Settings for CE

Recommended settings for memory-constrained devices:

```
# C/C++ > Code Generation
- Use runtime library: Multithreaded DLL
- Struct member alignment: 4 bytes

# C/C++ > Optimizations (Release)
- Optimization: Minimize Size (/O1)
- Favor: Small Code (/Os)

# C/C++ > Preprocessor
- Defines for CE 2.0: (automatic from SDK)
- Optional: SQLITE_DEFAULT_CACHE_SIZE=64
```

### Target CPUs

| CPU | Macro | Notes |
|-----|-------|-------|
| SH3 | `SH3`, `SHx` | HP 620LX, Jornada 680 |
| MIPS | `MIPS` | NEC MobilePro, etc. |
| ARM | `ARM` | iPAQ, etc. |
| x86 | `_X86_` | Emulator |

---

## Desktop Linux Build (Development)

The included Makefile builds SQLite for desktop Linux. Use this for
development, testing, and debugging before deploying to CE devices.

### Prerequisites

- GCC or Clang
- GNU Make

### Quick Start

```bash
# Build everything
make

# Build with debug symbols
make DEBUG=1

# Run unit tests
make test

# Run benchmarks
make bench

# Clean build artifacts
make clean

# Show help
make help
```

### Build Output

```
build/
├── bin/
│   ├── sqlite_test    # Unit test runner
│   └── sqlite_bench   # Benchmark tool
├── lib/
│   └── libsqlite.a    # Static library
└── obj/               # Object files
```

### Verbose Build

```bash
make VERBOSE=1
```

---

## Desktop Windows Build (Development)

For development on Windows without CE, use MinGW or MSVC.

### MinGW

```bash
# Same as Linux
make CC=gcc
```

### MSVC (nmake)

Create a simple nmake file or use the Makefile with GNU make for Windows.

---

## Configuration Options

These preprocessor defines customize the build:

| Define | Default | Description |
|--------|---------|-------------|
| `SQLITE_DEFAULT_CACHE_SIZE` | 64 | Page cache size (pages) |
| `SQLITE_DEFAULT_PAGE_SIZE` | 1024 | Database page size (bytes) |
| `THREADSAFE` | 1 | Enable thread safety |
| `SQLITE_DEBUG` | (off) | Enable debug assertions |
| `SQLITE_OMIT_VACUUM` | (off) | Omit VACUUM for smaller code |
| `SQLITE_CE_LOG_LEVEL` | 3 | Log level (1=error, 5=trace) |

Example:
```
CFLAGS += -DSQLITE_DEFAULT_CACHE_SIZE=128
```

---

## Testing

### Unit Tests

The test framework in `src/sqlite-ce-test/` provides assertion macros:

```c
#include "test_macros.h"

static int test_example(void) {
    TEST_ASSERT(1 == 1);
    TEST_ASSERT_EQ(5, 5);
    TEST_ASSERT_STR_EQ("hello", "hello");
    return TEST_PASS;
}
```

Run tests:
```bash
make test
```

### Benchmarks

```bash
make bench
./build/bin/sqlite_bench
```

---

## Troubleshooting

### CE Build Issues

**Link error: unresolved external symbol**
- Ensure all source files are added to the project
- Check that sqlite-ce.lib is in the library path

**Runtime crash on device**
- Check SQLITE_DEFAULT_CACHE_SIZE isn't too large for device RAM
- Enable SQLITE_DEBUG to get assertions
- Use CE Remote Tools to debug

### Desktop Build Issues

**Missing parse.c**
- The parser is pre-generated; ensure src/sqlite/parse.c exists
- If missing, generate with: `lemon parse.y`

**Undefined reference to `sqliteOsXxx`**
- Desktop builds use OS_UNIX; ensure os_unix.c is included
- Or use the sqlite-ce os.c with desktop stubs

---

## Version History

See OPTIMIZATION_PLAN.md for detailed change tracking.
