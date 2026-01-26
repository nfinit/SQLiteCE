-- M-001: Baseline Memory Footprint
-- Purpose: Establish baseline memory usage for empty database and idle state
-- Expected: < 500KB idle, < 1MB with schema
--
-- Methodology:
-- 1. Measure memory before any database action
-- 2. Create in-memory database
-- 3. Create 10-table schema
-- 4. Measure at each step
--
-- Note: Memory measurement requires instrumentation in bench tool.
-- This script creates the schema for memory measurement tests.

-- Step 1: Empty state (measure before running this)

-- Step 2: Create in-memory database
-- (Open :memory: database in SQLiteCEdit or bench tool)

-- Step 3: Create 10-table schema
CREATE TABLE bench_t1 (id INTEGER PRIMARY KEY, name TEXT, value INTEGER);
CREATE TABLE bench_t2 (id INTEGER PRIMARY KEY, name TEXT, value INTEGER);
CREATE TABLE bench_t3 (id INTEGER PRIMARY KEY, name TEXT, value INTEGER);
CREATE TABLE bench_t4 (id INTEGER PRIMARY KEY, name TEXT, value INTEGER);
CREATE TABLE bench_t5 (id INTEGER PRIMARY KEY, name TEXT, value INTEGER);
CREATE TABLE bench_t6 (id INTEGER PRIMARY KEY, name TEXT, value INTEGER);
CREATE TABLE bench_t7 (id INTEGER PRIMARY KEY, name TEXT, value INTEGER);
CREATE TABLE bench_t8 (id INTEGER PRIMARY KEY, name TEXT, value INTEGER);
CREATE TABLE bench_t9 (id INTEGER PRIMARY KEY, name TEXT, value INTEGER);
CREATE TABLE bench_t10 (id INTEGER PRIMARY KEY, name TEXT, value INTEGER);

-- Step 4: Create indexes
CREATE INDEX idx_t1_name ON bench_t1(name);
CREATE INDEX idx_t2_name ON bench_t2(name);
CREATE INDEX idx_t3_name ON bench_t3(name);
CREATE INDEX idx_t4_name ON bench_t4(name);
CREATE INDEX idx_t5_name ON bench_t5(name);

-- Verify schema created
SELECT COUNT(*) AS table_count FROM sqlite_master WHERE type='table';
SELECT COUNT(*) AS index_count FROM sqlite_master WHERE type='index';
