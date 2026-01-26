# SQLite/CE Performance Benchmark Plan

**Project**: SQLite/CE - Transactional Relational Database for Windows CE
**Baseline**: SQLite 2.8.17 core + CE port layer + Query Editor UI
**Created**: 2026-01-25
**Last Updated**: 2026-01-25

---

## Table of Contents

1. [Overview](#overview)
2. [Benchmark Environment](#benchmark-environment)
3. [Status Legend](#status-legend)
4. [Benchmark Categories](#benchmark-categories)
   - [Category M: Memory Benchmarks](#category-m-memory-benchmarks)
   - [Category Q: Query Performance Benchmarks](#category-q-query-performance-benchmarks)
   - [Category I: I/O Performance Benchmarks](#category-i-io-performance-benchmarks)
   - [Category U: UI Performance Benchmarks](#category-u-ui-performance-benchmarks)
   - [Category S: Stress & Scalability Benchmarks](#category-s-stress--scalability-benchmarks)
   - [Category C: Comparison Benchmarks](#category-c-comparison-benchmarks)
5. [Benchmark Execution Checklist](#benchmark-execution-checklist)
6. [Results Tracking](#results-tracking)
7. [Change Log](#change-log)

---

## Overview

This document defines a comprehensive benchmark suite for measuring SQLite/CE performance across memory usage, query execution, I/O operations, and UI responsiveness. Each benchmark is designed to:

1. Establish baseline performance metrics
2. Validate optimization improvements
3. Detect performance regressions
4. Guide future optimization priorities

### Key Principles

- **Reproducibility**: All benchmarks must be repeatable with consistent results
- **Isolation**: Each benchmark tests a specific subsystem in isolation
- **Realistic Workloads**: Tests reflect actual usage patterns on CE devices
- **Measurable Metrics**: Clear, quantifiable success criteria

---

## Benchmark Environment

### Target Hardware Profiles

| Profile | Device Type | CPU | RAM | Storage | Use Case |
|---------|-------------|-----|-----|---------|----------|
| **CE-Low** | Entry PDA | ARM 200MHz | 16MB | 32MB Flash | Minimum spec |
| **CE-Mid** | Standard PDA | ARM 400MHz | 32MB | 64MB Flash | Typical device |
| **CE-High** | Premium PDA | ARM 624MHz | 64MB | 128MB Flash | High-end device |
| **Emulator** | VS2005 Emulator | Host CPU | 64MB | Virtual | Development testing |

### Test Database Profiles

| Profile | Tables | Rows/Table | Indexes | Total Size | Description |
|---------|--------|------------|---------|------------|-------------|
| **Tiny** | 3 | 100 | 2 | ~50KB | Quick smoke tests |
| **Small** | 5 | 1,000 | 5 | ~500KB | Typical small app |
| **Medium** | 10 | 10,000 | 10 | ~5MB | Moderate workload |
| **Large** | 20 | 100,000 | 20 | ~50MB | Stress testing |
| **Huge** | 50 | 1,000,000 | 50 | ~500MB | Scalability limits |

### Measurement Tools

| Tool | Purpose | Location |
|------|---------|----------|
| `GetTickCount()` | Elapsed time (ms resolution) | Windows CE API |
| `GlobalMemoryStatus()` | Memory usage | Windows CE API |
| `GetProcessHeap()` + `HeapWalk()` | Heap analysis | Windows CE API |
| Custom allocator stats | Pool/intern metrics | `mempool.c`, `strpool.c`, `strintern.c` |
| `PRAGMA cache_stats` | Page cache metrics | SQLite extension |

---

## Status Legend

| Status | Meaning |
|--------|---------|
| `PENDING` | Benchmark defined but not yet implemented |
| `READY` | Benchmark implemented, awaiting execution |
| `IN_PROGRESS` | Currently being run |
| `COMPLETE` | Executed with results recorded |
| `BLOCKED` | Cannot run due to dependency or issue |
| `DEFERRED` | Postponed for future release |

---

## Benchmark Categories

---

### Category M: Memory Benchmarks

#### M-001: Baseline Memory Footprint
| Field | Value |
|-------|-------|
| **Name** | Baseline Memory Footprint |
| **Status** | `PENDING` |
| **Reason for Change** | Establish baseline memory usage for empty database and idle state |
| **Description** | Measure memory consumption at application startup with no database loaded, then with empty database, then with database containing schema only (no data). |
| **Metrics** | - Process working set (KB)<br>- Heap allocated (KB)<br>- Global variables footprint (KB)<br>- Static buffers (KB) |
| **Methodology** | 1. Launch app, measure before any action<br>2. Create new in-memory database<br>3. Create 10-table schema<br>4. Measure at each step |
| **Expected Baseline** | < 500KB idle, < 1MB with schema |
| **Test Script** | `bench/m001_baseline_memory.sql` |

#### M-002: Query Result Memory Scaling
| Field | Value |
|-------|-------|
| **Name** | Query Result Memory Scaling |
| **Status** | `PENDING` |
| **Reason for Change** | Validate A-002 (string pool) and A-003 (string interning) optimizations |
| **Description** | Execute queries returning increasing row counts and measure memory growth. Compare with/without string pool and interning. |
| **Metrics** | - Memory per 1K rows (KB)<br>- String pool efficiency (%)<br>- Intern hit rate (%)<br>- Peak memory usage (KB) |
| **Methodology** | 1. Query 100, 1K, 10K, 100K rows<br>2. Measure memory after each<br>3. Record pool/intern statistics<br>4. Calculate memory per row |
| **Expected Baseline** | < 100 bytes/row average, > 30% intern hit rate |
| **Test Script** | `bench/m002_result_memory.sql` |

#### M-003: Undo Stack Memory Usage
| Field | Value |
|-------|-------|
| **Name** | Undo Stack Memory Usage |
| **Status** | `PENDING` |
| **Reason for Change** | Validate A-004 (undo stack optimization) with contiguous allocation |
| **Description** | Delete rows in edit mode and measure undo stack memory consumption. Verify memory limit enforcement and eviction behavior. |
| **Metrics** | - Memory per undo entry (bytes)<br>- Maximum undo entries before eviction<br>- Eviction trigger accuracy<br>- Memory recovery on clear |
| **Methodology** | 1. Open table with 20-column rows<br>2. Delete rows one at a time<br>3. Measure g_undoBytes after each<br>4. Continue until eviction occurs<br>5. Verify limit (UNDO_MAX_BYTES) |
| **Expected Baseline** | < 2KB per typical row, eviction at 64KB |
| **Test Script** | `bench/m003_undo_memory.sql` |

#### M-004: VDBE Memory Pre-allocation Efficiency
| Field | Value |
|-------|-------|
| **Name** | VDBE Memory Pre-allocation Efficiency |
| **Status** | `PENDING` |
| **Reason for Change** | Validate A-005 (VDBE aMem pre-allocation) reduces runtime allocations |
| **Description** | Execute queries of varying complexity and count memory allocations during VDBE execution. Compare with on-demand allocation. |
| **Metrics** | - Allocations during query execution<br>- aMem resize count<br>- Peak aMem slots used<br>- Pre-allocation accuracy |
| **Methodology** | 1. Instrument sqliteVdbeExec() allocation paths<br>2. Run simple, medium, complex queries<br>3. Count realloc calls for aMem<br>4. Compare nMem estimate vs actual |
| **Expected Baseline** | Zero aMem reallocs for typical queries |
| **Test Script** | `bench/m004_vdbe_memory.sql` |

#### M-005: Memory Pool Allocation Performance
| Field | Value |
|-------|-------|
| **Name** | Memory Pool Allocation Performance |
| **Status** | `PENDING` |
| **Reason for Change** | Validate A-006 (memory pool) reduces heap fragmentation and speeds small allocations |
| **Description** | Perform thousands of small allocations (<= 64 bytes) and measure allocation time and fragmentation. Compare pool vs direct LocalAlloc. |
| **Metrics** | - Allocations per second<br>- Average allocation time (us)<br>- Heap fragmentation (%)<br>- Pool chunk utilization (%) |
| **Methodology** | 1. Allocate 10,000 small objects (8-64 bytes)<br>2. Free 50% randomly<br>3. Allocate 5,000 more<br>4. Measure time and heap state |
| **Expected Baseline** | > 50% faster than LocalAlloc, < 10% fragmentation |
| **Test Script** | `bench/m005_mempool.c` |

#### M-006: Page Cache Memory Efficiency
| Field | Value |
|-------|-------|
| **Name** | Page Cache Memory Efficiency |
| **Status** | `PENDING` |
| **Reason for Change** | Measure B-tree page cache memory usage and hit rates |
| **Description** | Execute workloads that stress the page cache and measure memory usage, hit rates, and eviction patterns. |
| **Metrics** | - Cache memory usage (KB)<br>- Cache hit rate (%)<br>- Pages read from disk<br>- Eviction count |
| **Methodology** | 1. Set cache size to 64 pages<br>2. Run sequential scan<br>3. Run random access pattern<br>4. Run mixed workload<br>5. Record PRAGMA cache_stats |
| **Expected Baseline** | > 80% hit rate for sequential, > 50% for random |
| **Test Script** | `bench/m006_page_cache.sql` |

#### M-007: Statement Cache Memory Usage
| Field | Value |
|-------|-------|
| **Name** | Statement Cache Memory Usage |
| **Status** | `PENDING` |
| **Reason for Change** | Validate B-002 (statement cache) memory overhead is acceptable |
| **Description** | Cache prepared statements and measure memory overhead per cached statement. Verify LRU eviction works correctly. |
| **Metrics** | - Memory per cached statement (bytes)<br>- Cache hit rate (%)<br>- LRU eviction count<br>- Schema invalidation count |
| **Methodology** | 1. Execute 50 unique queries<br>2. Repeat same 50 queries<br>3. Execute 20 new queries (trigger eviction)<br>4. Measure cache stats |
| **Expected Baseline** | < 500 bytes/statement, > 90% hit rate on repeat |
| **Test Script** | `bench/m007_stmt_cache.sql` |

---

### Category Q: Query Performance Benchmarks

#### Q-001: Simple SELECT Performance
| Field | Value |
|-------|-------|
| **Name** | Simple SELECT Performance |
| **Status** | `PENDING` |
| **Reason for Change** | Establish baseline for simple query execution time |
| **Description** | Execute simple SELECT queries (single table, no joins, indexed lookup) and measure execution time. |
| **Metrics** | - Queries per second<br>- Average query time (ms)<br>- Min/Max query time (ms)<br>- Standard deviation |
| **Methodology** | 1. Create table with 10K rows, indexed<br>2. Execute 1000 SELECT WHERE id=? queries<br>3. Execute 1000 SELECT WHERE indexed_col=? queries<br>4. Execute 1000 SELECT * LIMIT 100 queries |
| **Expected Baseline** | > 500 indexed lookups/sec |
| **Test Script** | `bench/q001_simple_select.sql` |

#### Q-002: Complex JOIN Performance
| Field | Value |
|-------|-------|
| **Name** | Complex JOIN Performance |
| **Status** | `PENDING` |
| **Reason for Change** | Measure multi-table join query performance |
| **Description** | Execute queries with 2-way, 3-way, and 4-way joins and measure execution time and memory usage. |
| **Metrics** | - Query time per join complexity (ms)<br>- Rows processed per second<br>- Temporary table usage<br>- Memory peak during query |
| **Methodology** | 1. Create 4 related tables (1K-10K rows each)<br>2. Execute 2-way join (100 iterations)<br>3. Execute 3-way join (100 iterations)<br>4. Execute 4-way join (100 iterations) |
| **Expected Baseline** | 2-way < 50ms, 3-way < 200ms, 4-way < 500ms |
| **Test Script** | `bench/q002_join_perf.sql` |

#### Q-003: Aggregate Function Performance
| Field | Value |
|-------|-------|
| **Name** | Aggregate Function Performance |
| **Status** | `PENDING` |
| **Reason for Change** | Measure COUNT, SUM, AVG, MIN, MAX performance on large datasets |
| **Description** | Execute aggregate queries on tables of varying sizes and measure execution time. |
| **Metrics** | - Time per aggregate type (ms)<br>- Rows processed per second<br>- Memory usage during aggregation |
| **Methodology** | 1. Create table with 100K numeric rows<br>2. Execute COUNT(*) 100 times<br>3. Execute SUM(col) 100 times<br>4. Execute AVG, MIN, MAX 100 times each<br>5. Execute GROUP BY with aggregates |
| **Expected Baseline** | > 100K rows/sec for simple aggregates |
| **Test Script** | `bench/q003_aggregates.sql` |

#### Q-004: INSERT Performance
| Field | Value |
|-------|-------|
| **Name** | INSERT Performance |
| **Status** | `PENDING` |
| **Reason for Change** | Measure single-row and batch insert throughput |
| **Description** | Insert rows individually and in transactions, measuring throughput and I/O patterns. |
| **Metrics** | - Rows inserted per second (individual)<br>- Rows inserted per second (batch)<br>- Transaction commit time (ms)<br>- Journal file size |
| **Methodology** | 1. Insert 1000 rows individually (autocommit)<br>2. Insert 1000 rows in single transaction<br>3. Insert 10000 rows in 100-row batches<br>4. Measure sync/fsync calls |
| **Expected Baseline** | > 10 rows/sec individual, > 1000 rows/sec batched |
| **Test Script** | `bench/q004_insert_perf.sql` |

#### Q-005: UPDATE Performance
| Field | Value |
|-------|-------|
| **Name** | UPDATE Performance |
| **Status** | `PENDING` |
| **Reason for Change** | Measure update throughput for indexed and non-indexed columns |
| **Description** | Update rows by primary key and by non-indexed column, measuring throughput. |
| **Metrics** | - Updates per second (by PK)<br>- Updates per second (by non-indexed)<br>- Updates per second (batch)<br>- Index maintenance overhead |
| **Methodology** | 1. Update 1000 rows by primary key<br>2. Update 1000 rows by indexed column<br>3. Update 1000 rows by non-indexed column<br>4. Batch update 10000 rows |
| **Expected Baseline** | > 100 updates/sec by PK |
| **Test Script** | `bench/q005_update_perf.sql` |

#### Q-006: DELETE Performance
| Field | Value |
|-------|-------|
| **Name** | DELETE Performance |
| **Status** | `PENDING` |
| **Reason for Change** | Measure delete throughput and space reclamation |
| **Description** | Delete rows individually and in batches, measuring throughput and database size changes. |
| **Metrics** | - Deletes per second (individual)<br>- Deletes per second (batch)<br>- VACUUM time (ms)<br>- Space reclaimed (KB) |
| **Methodology** | 1. Delete 1000 rows individually<br>2. Delete 1000 rows in single transaction<br>3. Measure database size before/after<br>4. Run VACUUM and measure time |
| **Expected Baseline** | > 100 deletes/sec individual |
| **Test Script** | `bench/q006_delete_perf.sql` |

#### Q-007: Subquery Performance
| Field | Value |
|-------|-------|
| **Name** | Subquery Performance |
| **Status** | `PENDING` |
| **Reason for Change** | Measure correlated and non-correlated subquery performance |
| **Description** | Execute queries with IN, EXISTS, and scalar subqueries, measuring execution time. |
| **Metrics** | - Time for IN subquery (ms)<br>- Time for EXISTS subquery (ms)<br>- Time for scalar subquery (ms)<br>- Time for correlated subquery (ms) |
| **Methodology** | 1. Execute WHERE col IN (SELECT ...) 100 times<br>2. Execute WHERE EXISTS (SELECT ...) 100 times<br>3. Execute SELECT (SELECT ...) 100 times<br>4. Execute correlated subquery 100 times |
| **Expected Baseline** | Non-correlated < 100ms, correlated < 500ms |
| **Test Script** | `bench/q007_subquery_perf.sql` |

#### Q-008: LIKE/GLOB Pattern Matching
| Field | Value |
|-------|-------|
| **Name** | LIKE/GLOB Pattern Matching |
| **Status** | `PENDING` |
| **Reason for Change** | Measure pattern matching performance for text searches |
| **Description** | Execute LIKE and GLOB queries with various patterns (prefix, suffix, contains, complex). |
| **Metrics** | - Time for prefix match (ms)<br>- Time for suffix match (ms)<br>- Time for contains match (ms)<br>- Rows scanned per pattern type |
| **Methodology** | 1. Create table with 10K text rows<br>2. Execute LIKE 'prefix%' 100 times<br>3. Execute LIKE '%suffix' 100 times<br>4. Execute LIKE '%contains%' 100 times |
| **Expected Baseline** | Prefix < 10ms (index usable), contains < 100ms |
| **Test Script** | `bench/q008_pattern_match.sql` |

#### Q-009: ORDER BY and Sorting
| Field | Value |
|-------|-------|
| **Name** | ORDER BY and Sorting |
| **Status** | `PENDING` |
| **Reason for Change** | Measure sort performance for indexed and non-indexed columns |
| **Description** | Execute queries with ORDER BY on indexed and non-indexed columns, single and multiple columns. |
| **Metrics** | - Time for indexed sort (ms)<br>- Time for non-indexed sort (ms)<br>- Time for multi-column sort (ms)<br>- Temporary file usage |
| **Methodology** | 1. ORDER BY indexed column (10K rows)<br>2. ORDER BY non-indexed column (10K rows)<br>3. ORDER BY col1, col2 (10K rows)<br>4. ORDER BY with LIMIT |
| **Expected Baseline** | Indexed < 10ms, non-indexed < 100ms |
| **Test Script** | `bench/q009_sorting.sql` |

#### Q-010: Statement Cache Hit Rate
| Field | Value |
|-------|-------|
| **Name** | Statement Cache Hit Rate |
| **Status** | `PENDING` |
| **Reason for Change** | Validate B-002 (statement cache) provides expected performance improvement |
| **Description** | Compare query execution time with and without statement caching for repeated queries. |
| **Metrics** | - Cache hit rate (%)<br>- Time with cache (ms)<br>- Time without cache (ms)<br>- Speedup factor |
| **Methodology** | 1. Execute same query 1000 times (cache enabled)<br>2. Execute same query 1000 times (cache disabled)<br>3. Execute 100 different queries (measure miss rate)<br>4. Compare compile vs execute time |
| **Expected Baseline** | > 90% hit rate, > 2x speedup on repeated queries |
| **Test Script** | `bench/q010_stmt_cache_perf.sql` |

---

### Category I: I/O Performance Benchmarks

#### I-001: Sequential Read Performance
| Field | Value |
|-------|-------|
| **Name** | Sequential Read Performance |
| **Status** | `PENDING` |
| **Reason for Change** | Measure raw sequential read throughput and validate C-003 (read-ahead) |
| **Description** | Read entire database sequentially and measure throughput. Compare with/without read-ahead prefetching. |
| **Metrics** | - MB/sec read throughput<br>- Pages read per second<br>- Read-ahead hit rate (%)<br>- I/O wait time (ms) |
| **Methodology** | 1. Create 10MB database<br>2. SELECT * from all tables (sequential scan)<br>3. Measure with read-ahead enabled<br>4. Measure with read-ahead disabled |
| **Expected Baseline** | > 2MB/sec on flash storage |
| **Test Script** | `bench/i001_seq_read.sql` |

#### I-002: Random Read Performance
| Field | Value |
|-------|-------|
| **Name** | Random Read Performance |
| **Status** | `PENDING` |
| **Reason for Change** | Measure random access I/O patterns typical of indexed lookups |
| **Description** | Perform random primary key lookups and measure I/O patterns and cache behavior. |
| **Metrics** | - Random reads per second<br>- Cache hit rate (%)<br>- Average seek time (ms)<br>- Pages read from disk |
| **Methodology** | 1. Create table with 100K rows<br>2. Generate 10K random PKs<br>3. Execute SELECT WHERE id=? for each<br>4. Measure cache misses |
| **Expected Baseline** | > 100 random reads/sec with cold cache |
| **Test Script** | `bench/i002_random_read.sql` |

#### I-003: Write Performance (Sync vs Async)
| Field | Value |
|-------|-------|
| **Name** | Write Performance (Sync vs Async) |
| **Status** | `PENDING` |
| **Reason for Change** | Measure impact of synchronous pragma on write performance |
| **Description** | Compare write throughput with PRAGMA synchronous = OFF, NORMAL, FULL. |
| **Metrics** | - Writes per second (each mode)<br>- Commit latency (ms)<br>- Data durability risk<br>- Journal sync count |
| **Methodology** | 1. Set PRAGMA synchronous = OFF, insert 1000 rows<br>2. Set PRAGMA synchronous = NORMAL, insert 1000 rows<br>3. Set PRAGMA synchronous = FULL, insert 1000 rows<br>4. Compare times and durability |
| **Expected Baseline** | OFF > 10x faster than FULL |
| **Test Script** | `bench/i003_sync_modes.sql` |

#### I-004: Sorted Dirty Page Write Performance
| Field | Value |
|-------|-------|
| **Name** | Sorted Dirty Page Write Performance |
| **Status** | `PENDING` |
| **Reason for Change** | Validate C-002 (sorted dirty page writes) reduces flash wear and improves commit time |
| **Description** | Commit transactions with scattered dirty pages and measure write patterns. Compare sorted vs unsorted. |
| **Metrics** | - Commit time (ms)<br>- Write order (page numbers)<br>- Seek distance reduction (%)<br>- Flash write amplification |
| **Methodology** | 1. Modify pages 1, 100, 50, 75, 25 (scattered)<br>2. Commit with sorted writes<br>3. Instrument pager_get_all_dirty_pages()<br>4. Verify ascending page order |
| **Expected Baseline** | Write order matches page number order |
| **Test Script** | `bench/i004_sorted_writes.sql` |

#### I-005: Journal File Performance
| Field | Value |
|-------|-------|
| **Name** | Journal File Performance |
| **Status** | `PENDING` |
| **Reason for Change** | Measure rollback journal overhead for transactions |
| **Description** | Measure journal creation, growth, and deletion overhead for various transaction sizes. |
| **Metrics** | - Journal creation time (ms)<br>- Journal write throughput (KB/sec)<br>- Journal delete time (ms)<br>- Transaction overhead (%) |
| **Methodology** | 1. Small transaction (1 page modified)<br>2. Medium transaction (10 pages modified)<br>3. Large transaction (100 pages modified)<br>4. Measure journal file sizes |
| **Expected Baseline** | Journal overhead < 20% of transaction time |
| **Test Script** | `bench/i005_journal_perf.sql` |

#### I-006: Database Growth and VACUUM
| Field | Value |
|-------|-------|
| **Name** | Database Growth and VACUUM |
| **Status** | `PENDING` |
| **Reason for Change** | Measure space efficiency and VACUUM performance |
| **Description** | Insert and delete data, measuring database size growth and VACUUM reclamation. |
| **Metrics** | - Space efficiency (%)<br>- VACUUM time (ms/MB)<br>- Free page count<br>- Fragmentation level |
| **Methodology** | 1. Insert 10K rows (measure size)<br>2. Delete 50% of rows<br>3. Measure size after delete<br>4. Run VACUUM, measure time and final size |
| **Expected Baseline** | VACUUM reclaims > 90% of deleted space |
| **Test Script** | `bench/i006_vacuum_perf.sql` |

---

### Category U: UI Performance Benchmarks

#### U-001: Grid Population Time
| Field | Value |
|-------|-------|
| **Name** | Grid Population Time |
| **Status** | `PENDING` |
| **Reason for Change** | Measure time to display query results in grid view |
| **Description** | Populate grid with varying row counts and measure time from query completion to grid display. |
| **Metrics** | - Time to first paint (ms)<br>- Time to full population (ms)<br>- Rows per second displayed<br>- Scroll responsiveness (ms) |
| **Methodology** | 1. Query 100 rows, measure grid population<br>2. Query 1K rows, measure grid population<br>3. Query 10K rows, measure grid population<br>4. Scroll through results |
| **Expected Baseline** | < 100ms for 1K rows |
| **Test Script** | `bench/u001_grid_populate.sql` |

#### U-002: Grid Sorting Performance
| Field | Value |
|-------|-------|
| **Name** | Grid Sorting Performance |
| **Status** | `PENDING` |
| **Reason for Change** | Measure client-side grid sorting with type-aware comparison |
| **Description** | Sort grid columns (numeric, text, mixed) and measure sort time. |
| **Metrics** | - Sort time by column type (ms)<br>- Comparisons per second<br>- Memory during sort (KB)<br>- Redraw time after sort (ms) |
| **Methodology** | 1. Load 10K rows into grid<br>2. Click numeric column header<br>3. Click text column header<br>4. Click to reverse sort |
| **Expected Baseline** | < 500ms for 10K row sort |
| **Test Script** | `bench/u002_grid_sort.sql` |

#### U-003: Grid Search Performance
| Field | Value |
|-------|-------|
| **Name** | Grid Search Performance |
| **Status** | `PENDING` |
| **Reason for Change** | Measure Find Next performance in grid view |
| **Description** | Search for text in grid cells across large result sets. |
| **Metrics** | - Search time (ms)<br>- Cells searched per second<br>- Time to highlight match (ms) |
| **Methodology** | 1. Load 10K rows into grid<br>2. Search for text in first row<br>3. Search for text in last row<br>4. Search for non-existent text |
| **Expected Baseline** | < 100ms full grid search |
| **Test Script** | `bench/u003_grid_search.sql` |

#### U-004: Schema Tree Expansion
| Field | Value |
|-------|-------|
| **Name** | Schema Tree Expansion |
| **Status** | `PENDING` |
| **Reason for Change** | Measure lazy loading performance for schema tree nodes |
| **Description** | Expand schema tree nodes and measure time to load columns and indexes. |
| **Metrics** | - Time to expand table node (ms)<br>- Time to load all columns (ms)<br>- Time to load indexes (ms)<br>- Queries executed per expansion |
| **Methodology** | 1. Create database with 50 tables<br>2. Expand each table node<br>3. Measure time per expansion<br>4. Measure total schema load time |
| **Expected Baseline** | < 50ms per table expansion |
| **Test Script** | `bench/u004_schema_tree.sql` |

#### U-005: Editor Responsiveness
| Field | Value |
|-------|-------|
| **Name** | Editor Responsiveness |
| **Status** | `PENDING` |
| **Reason for Change** | Measure query editor input lag with large queries |
| **Description** | Type in query editor with varying buffer sizes and measure input responsiveness. |
| **Metrics** | - Input lag (ms)<br>- Line number update time (ms)<br>- Scroll responsiveness (ms)<br>- Memory with large buffer (KB) |
| **Methodology** | 1. Empty editor - type and measure lag<br>2. 100 line query - type and measure lag<br>3. 1000 line query - type and measure lag<br>4. Scroll through large query |
| **Expected Baseline** | < 50ms input lag at 1000 lines |
| **Test Script** | `bench/u005_editor_input.sql` |

#### U-006: Cell Edit Commit Time
| Field | Value |
|-------|-------|
| **Name** | Cell Edit Commit Time |
| **Status** | `PENDING` |
| **Reason for Change** | Measure time from cell edit to database commit in edit mode |
| **Description** | Edit cells in table edit mode and measure UPDATE execution and grid refresh time. |
| **Metrics** | - UPDATE execution time (ms)<br>- Grid refresh time (ms)<br>- Total edit-to-display time (ms) |
| **Methodology** | 1. Open table for editing<br>2. Edit cell, press Enter<br>3. Measure time to commit and refresh<br>4. Repeat for 100 cells |
| **Expected Baseline** | < 100ms per cell commit |
| **Test Script** | `bench/u006_cell_edit.sql` |

---

### Category S: Stress & Scalability Benchmarks

#### S-001: Maximum Table Size
| Field | Value |
|-------|-------|
| **Name** | Maximum Table Size |
| **Status** | `PENDING` |
| **Reason for Change** | Determine practical limits for table row counts |
| **Description** | Insert rows until performance degrades unacceptably or limits are reached. |
| **Metrics** | - Maximum practical rows<br>- Query time at limit (ms)<br>- Memory usage at limit (KB)<br>- Insert rate degradation |
| **Methodology** | 1. Insert rows in 10K batches<br>2. After each batch: run test queries<br>3. Stop when query time > 10 seconds<br>4. Record practical limit |
| **Expected Baseline** | > 100K rows per table |
| **Test Script** | `bench/s001_max_table.sql` |

#### S-002: Maximum Database Size
| Field | Value |
|-------|-------|
| **Name** | Maximum Database Size |
| **Status** | `PENDING` |
| **Reason for Change** | Determine practical database size limits for CE storage |
| **Description** | Grow database until storage limits or performance issues occur. |
| **Metrics** | - Maximum database size (MB)<br>- Open time at limit (ms)<br>- Query performance at limit<br>- Storage card behavior |
| **Methodology** | 1. Create database on storage card<br>2. Insert data in 1MB increments<br>3. Measure open time and query time<br>4. Stop at 100MB or performance degradation |
| **Expected Baseline** | > 50MB database size practical |
| **Test Script** | `bench/s002_max_database.sql` |

#### S-003: Concurrent Query Stress
| Field | Value |
|-------|-------|
| **Name** | Concurrent Query Stress |
| **Status** | `PENDING` |
| **Reason for Change** | Test behavior under rapid sequential query execution |
| **Description** | Execute queries as fast as possible and measure stability and resource usage. |
| **Metrics** | - Queries executed before failure<br>- Memory growth rate (KB/query)<br>- Resource leak detection<br>- Error rate (%) |
| **Methodology** | 1. Execute 10K simple queries in tight loop<br>2. Execute 1K complex queries in tight loop<br>3. Monitor memory growth<br>4. Check for handle/resource leaks |
| **Expected Baseline** | Zero failures, zero memory growth |
| **Test Script** | `bench/s003_query_stress.sql` |

#### S-004: Long-Running Query Abort
| Field | Value |
|-------|-------|
| **Name** | Long-Running Query Abort |
| **Status** | `PENDING` |
| **Reason for Change** | Test query abort mechanism for long-running operations |
| **Description** | Start long-running queries and abort via Ctrl+C, measuring abort responsiveness. |
| **Metrics** | - Time from Ctrl+C to abort (ms)<br>- Resource cleanup completeness<br>- Database state after abort<br>- Memory freed after abort |
| **Methodology** | 1. Start query that runs > 10 seconds<br>2. Press Ctrl+C after 2 seconds<br>3. Measure abort latency<br>4. Verify database integrity |
| **Expected Baseline** | Abort within 1 second of Ctrl+C |
| **Test Script** | `bench/s004_abort_query.sql` |

#### S-005: Memory Exhaustion Recovery
| Field | Value |
|-------|-------|
| **Name** | Memory Exhaustion Recovery |
| **Status** | `PENDING` |
| **Reason for Change** | Test graceful handling of low memory conditions |
| **Description** | Consume memory until allocations fail and verify graceful degradation. |
| **Metrics** | - Allocation failure handling<br>- Error message quality<br>- Database state after OOM<br>- Recovery after memory freed |
| **Methodology** | 1. Allocate memory until low<br>2. Attempt large query<br>3. Verify error handling<br>4. Free memory, verify recovery |
| **Expected Baseline** | Graceful error, no crash, no corruption |
| **Test Script** | `bench/s005_memory_exhaust.sql` |

#### S-006: Rapid Open/Close Cycles
| Field | Value |
|-------|-------|
| **Name** | Rapid Open/Close Cycles |
| **Status** | `PENDING` |
| **Reason for Change** | Test database open/close for resource leaks |
| **Description** | Open and close database rapidly and check for handle/memory leaks. |
| **Metrics** | - Open/close cycles before failure<br>- Handle leak count<br>- Memory growth (KB)<br>- File lock release time |
| **Methodology** | 1. Open database<br>2. Execute simple query<br>3. Close database<br>4. Repeat 1000 times<br>5. Check for leaks |
| **Expected Baseline** | Zero leaks after 1000 cycles |
| **Test Script** | `bench/s006_open_close.sql` |

---

### Category C: Comparison Benchmarks

#### C-001: Before/After Optimization Comparison
| Field | Value |
|-------|-------|
| **Name** | Before/After Optimization Comparison |
| **Status** | `PENDING` |
| **Reason for Change** | Quantify improvements from optimization work |
| **Description** | Run standard benchmark suite against pre-optimization and post-optimization builds. |
| **Metrics** | - Query time improvement (%)<br>- Memory usage reduction (%)<br>- I/O reduction (%)<br>- Overall speedup factor |
| **Methodology** | 1. Build pre-optimization version (tag: v0.9.0)<br>2. Build post-optimization version (current)<br>3. Run identical benchmarks on both<br>4. Calculate deltas |
| **Expected Baseline** | > 20% improvement in key metrics |
| **Test Script** | `bench/c001_before_after.sql` |

#### C-002: Cache Size Impact
| Field | Value |
|-------|-------|
| **Name** | Cache Size Impact |
| **Status** | `PENDING` |
| **Reason for Change** | Determine optimal cache size for different workloads |
| **Description** | Run benchmarks with varying page cache sizes and measure performance impact. |
| **Metrics** | - Performance at 32 pages<br>- Performance at 64 pages<br>- Performance at 128 pages<br>- Memory vs speed tradeoff |
| **Methodology** | 1. Set cache to 32 pages, run suite<br>2. Set cache to 64 pages, run suite<br>3. Set cache to 128 pages, run suite<br>4. Plot performance curve |
| **Expected Baseline** | 64 pages optimal for typical CE devices |
| **Test Script** | `bench/c002_cache_size.sql` |

#### C-003: Flash vs RAM Database
| Field | Value |
|-------|-------|
| **Name** | Flash vs RAM Database |
| **Status** | `PENDING` |
| **Reason for Change** | Compare in-memory vs persistent database performance |
| **Description** | Run identical benchmarks on :memory: database and file-based database. |
| **Metrics** | - Query speedup in RAM (%)<br>- Write speedup in RAM (%)<br>- Memory overhead of RAM DB<br>- Practical RAM DB size limit |
| **Methodology** | 1. Create identical schema in :memory: and file<br>2. Run query benchmarks on both<br>3. Run write benchmarks on both<br>4. Compare results |
| **Expected Baseline** | RAM > 10x faster for writes |
| **Test Script** | `bench/c003_flash_vs_ram.sql` |

#### C-004: Index Impact Analysis
| Field | Value |
|-------|-------|
| **Name** | Index Impact Analysis |
| **Status** | `PENDING` |
| **Reason for Change** | Quantify index benefits and costs |
| **Description** | Compare query and write performance with/without indexes on various column types. |
| **Metrics** | - Query speedup with index (%)<br>- Write slowdown with index (%)<br>- Index size overhead (KB)<br>- Break-even point (rows) |
| **Methodology** | 1. Create table without indexes<br>2. Run query benchmarks<br>3. Add indexes<br>4. Run query and write benchmarks<br>5. Calculate tradeoffs |
| **Expected Baseline** | > 10x query speedup, < 2x write slowdown |
| **Test Script** | `bench/c004_index_impact.sql` |

---

## Benchmark Execution Checklist

### Pre-Benchmark Preparation

- [ ] Fresh device/emulator boot (no background apps)
- [ ] Battery fully charged or connected to power
- [ ] Storage card empty or with known state
- [ ] Application freshly installed (no cached data)
- [ ] Benchmark scripts copied to device
- [ ] Results folder created and writable

### Execution Protocol

1. **Warm-up Run**: Execute benchmark once, discard results
2. **Measurement Runs**: Execute benchmark 3-5 times
3. **Record Results**: Log all metrics and timestamps
4. **Cool-down**: Wait 30 seconds between benchmarks
5. **Verification**: Spot-check results for anomalies

### Post-Benchmark Cleanup

- [ ] Export results to CSV/JSON
- [ ] Calculate statistics (mean, std dev, min, max)
- [ ] Compare against expected baselines
- [ ] Document any anomalies or issues
- [ ] Archive benchmark database files

---

## Results Tracking

### Results File Format

Results should be stored in `bench/results/` with naming convention:
```
YYYY-MM-DD_<benchmark-id>_<device-profile>.csv
```

### CSV Columns

```csv
timestamp,benchmark_id,iteration,metric_name,metric_value,unit,notes
```

### Example Entry

```csv
2026-01-25T14:30:00,Q-001,1,queries_per_second,523,qps,warm cache
2026-01-25T14:30:00,Q-001,1,avg_query_time_ms,1.91,ms,indexed lookup
```

---

## Change Log

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-01-25 | Claude | Initial benchmark plan with 40 benchmarks across 6 categories |

---

## Summary

### Benchmark Count by Category

| Category | Count | Focus |
|----------|-------|-------|
| **M** (Memory) | 7 | Memory usage, pools, caching |
| **Q** (Query) | 10 | Query execution performance |
| **I** (I/O) | 6 | Disk read/write patterns |
| **U** (UI) | 6 | User interface responsiveness |
| **S** (Stress) | 6 | Scalability and limits |
| **C** (Comparison) | 4 | Before/after and tradeoff analysis |
| **Total** | **39** | Comprehensive coverage |

### Priority Execution Order

1. **Phase 1 (Baseline)**: M-001, Q-001, I-001, U-001
2. **Phase 2 (Core)**: Q-002 through Q-006, M-002, M-005
3. **Phase 3 (Optimization Validation)**: M-003, M-004, M-007, Q-010, I-004
4. **Phase 4 (Stress)**: S-001 through S-006
5. **Phase 5 (Comparison)**: C-001 through C-004
