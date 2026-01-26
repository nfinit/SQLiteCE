-- I-001: Sequential Read Performance
-- Purpose: Measure raw sequential read throughput
-- Expected: > 2MB/sec on flash storage
--
-- Methodology:
-- 1. Create 10MB database (many rows with large text)
-- 2. SELECT * from all tables (sequential scan)
-- 3. Measure with read-ahead enabled
-- 4. Measure with read-ahead disabled
--
-- Note: Requires file-based database (not :memory:)

-- Setup: Create table with large text data
DROP TABLE IF EXISTS i001_data;
CREATE TABLE i001_data (
    id INTEGER PRIMARY KEY,
    chunk TEXT
);

-- Each row is approximately 1KB of text data
-- 10,000 rows = ~10MB database

BEGIN TRANSACTION;

-- Sample insert (repeated 10,000 times in bench tool)
-- Each 'chunk' is 1000 characters of repeated text
INSERT INTO i001_data VALUES(1,
    'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' ||
    'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' ||
    'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' ||
    'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' ||
    'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' ||
    'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' ||
    'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' ||
    'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' ||
    'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' ||
    'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' ||
    'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' ||
    'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' ||
    'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' ||
    'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' ||
    'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' ||
    'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' ||
    'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' ||
    'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' ||
    'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' ||
    'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ' || 'ABCDEFGHIJ'
);

COMMIT;

-- Verify database size
SELECT COUNT(*) AS row_count FROM i001_data;
SELECT SUM(LENGTH(chunk)) AS total_bytes FROM i001_data;

-- Sequential read test (timed by bench tool):
-- SELECT * FROM i001_data;

-- Measure:
-- - Total bytes read
-- - Time elapsed
-- - Calculate MB/sec
-- - Cache hit rate (via PRAGMA cache_stats if available)

-- Cleanup
-- DROP TABLE i001_data;
