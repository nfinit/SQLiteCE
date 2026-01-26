# SQLite/CE Benchmark Suite

This directory contains benchmark scripts, test data generators, and result tracking for SQLite/CE performance testing.

## Directory Structure

```
bench/
├── README.md           # This file
├── scripts/            # SQL and C benchmark scripts
│   ├── m001_baseline_memory.sql
│   ├── q001_simple_select.sql
│   └── ...
├── data/               # Generated test data files
│   └── .gitkeep
└── results/            # Benchmark results (CSV format)
    └── .gitkeep
```

## Running Benchmarks

### Using SQLiteCEBench Tool

The primary benchmark tool is `SQLiteCEBench.exe` (built from `src/sqlite-ce-bench/`):

1. Build SQLiteCE and SQLiteCEBench projects
2. Deploy to CE device or emulator
3. Run SQLiteCEBench.exe
4. Select iterations (Options → Iterations: 1x, 10x, 100x)
5. Tap "Run" to execute
6. Save results to sync folder

### Using SQL Scripts

SQL scripts can be run manually in SQLiteCEdit:

1. Open SQLiteCEdit
2. Load test database or create with data generator
3. File → Open Query → select script
4. Execute and note timing in status bar

## Benchmark Categories

| ID | Category | Description |
|----|----------|-------------|
| M | Memory | Memory usage, pools, caching efficiency |
| Q | Query | Query execution performance |
| I | I/O | Disk read/write patterns |
| U | UI | User interface responsiveness |
| S | Stress | Scalability and limits |
| C | Comparison | Before/after and tradeoffs |

## Results Format

Results are stored as CSV files with naming convention:
```
YYYY-MM-DD_<benchmark-id>_<device-profile>.csv
```

CSV columns:
```csv
timestamp,benchmark_id,iteration,metric_name,metric_value,unit,notes
```

## Adding New Benchmarks

1. Create SQL script in `scripts/` following naming convention
2. Add test function to `src/sqlite-ce-bench/bench_main.c`
3. Update BENCHMARK_PLAN.md status
4. Document expected baseline in script header

## See Also

- [BENCHMARK_PLAN.md](../BENCHMARK_PLAN.md) - Full benchmark specifications
- [src/sqlite-ce-bench/README.md](../src/sqlite-ce-bench/README.md) - Benchmark tool setup
