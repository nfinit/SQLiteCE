-- Q-001: Simple SELECT Performance
-- Purpose: Establish baseline for simple query execution time
-- Expected: > 500 indexed lookups/sec
--
-- Methodology:
-- 1. Create table with 10K rows, indexed
-- 2. Execute 1000 SELECT WHERE id=? queries
-- 3. Execute 1000 SELECT WHERE indexed_col=? queries
-- 4. Execute 1000 SELECT * LIMIT 100 queries
--
-- Run this script to set up the test data, then use bench tool for timing.

-- Setup: Create test table
DROP TABLE IF EXISTS q001_test;
CREATE TABLE q001_test (
    id INTEGER PRIMARY KEY,
    indexed_col TEXT,
    data1 TEXT,
    data2 INTEGER,
    data3 REAL
);

-- Create index on text column
CREATE INDEX idx_q001_col ON q001_test(indexed_col);

-- Insert 10,000 rows
-- Note: This uses a recursive approach since SQLite 2.x doesn't have CTEs
-- For bench tool, this is done programmatically

-- Generate test data (run in batches to avoid timeout)
BEGIN TRANSACTION;

-- Batch 1: Rows 1-1000
INSERT INTO q001_test VALUES(1, 'key_0001', 'Sample data row 1', 100, 1.5);
INSERT INTO q001_test VALUES(2, 'key_0002', 'Sample data row 2', 200, 2.5);
INSERT INTO q001_test VALUES(3, 'key_0003', 'Sample data row 3', 300, 3.5);
INSERT INTO q001_test VALUES(4, 'key_0004', 'Sample data row 4', 400, 4.5);
INSERT INTO q001_test VALUES(5, 'key_0005', 'Sample data row 5', 500, 5.5);
-- ... (remaining rows generated programmatically in bench tool)

COMMIT;

-- Verify data
SELECT COUNT(*) AS row_count FROM q001_test;

-- Test queries (timing measured by bench tool):

-- Test 1: Primary key lookup (fastest - uses rowid)
-- SELECT * FROM q001_test WHERE id = ?;

-- Test 2: Indexed column lookup
-- SELECT * FROM q001_test WHERE indexed_col = ?;

-- Test 3: Full scan with LIMIT
-- SELECT * FROM q001_test LIMIT 100;

-- Test 4: Range query on index
-- SELECT * FROM q001_test WHERE indexed_col BETWEEN 'key_0100' AND 'key_0200';

-- Cleanup (optional)
-- DROP TABLE q001_test;
