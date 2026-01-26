-- Test Data Generator for SQLite/CE Benchmarks
-- Purpose: Create databases of various sizes for benchmark testing
--
-- Profiles:
--   Tiny:   ~50KB   (100 rows per table)
--   Small:  ~500KB  (1,000 rows per table)
--   Medium: ~5MB    (10,000 rows per table)
--   Large:  ~50MB   (100,000 rows per table)
--
-- Usage: Run desired section based on profile needed

-- =============================================================================
-- SCHEMA: Standard benchmark tables
-- =============================================================================

-- Main data table (used for most benchmarks)
CREATE TABLE IF NOT EXISTS bench_data (
    id INTEGER PRIMARY KEY,
    category TEXT,
    name TEXT,
    value INTEGER,
    amount REAL,
    created TEXT,
    notes TEXT
);
CREATE INDEX IF NOT EXISTS idx_bench_category ON bench_data(category);
CREATE INDEX IF NOT EXISTS idx_bench_value ON bench_data(value);

-- Lookup table for JOINs
CREATE TABLE IF NOT EXISTS bench_categories (
    id INTEGER PRIMARY KEY,
    code TEXT UNIQUE,
    description TEXT,
    active INTEGER
);

-- Order-style table for transaction tests
CREATE TABLE IF NOT EXISTS bench_orders (
    id INTEGER PRIMARY KEY,
    data_id INTEGER,
    quantity INTEGER,
    price REAL,
    order_date TEXT,
    status TEXT
);
CREATE INDEX IF NOT EXISTS idx_orders_data ON bench_orders(data_id);
CREATE INDEX IF NOT EXISTS idx_orders_date ON bench_orders(order_date);

-- Text search table
CREATE TABLE IF NOT EXISTS bench_text (
    id INTEGER PRIMARY KEY,
    title TEXT,
    body TEXT,
    keywords TEXT
);

-- =============================================================================
-- PROFILE: Tiny (~50KB, 100 rows)
-- =============================================================================

-- Categories (10 rows)
INSERT INTO bench_categories VALUES(1, 'CAT_A', 'Category Alpha', 1);
INSERT INTO bench_categories VALUES(2, 'CAT_B', 'Category Beta', 1);
INSERT INTO bench_categories VALUES(3, 'CAT_C', 'Category Gamma', 1);
INSERT INTO bench_categories VALUES(4, 'CAT_D', 'Category Delta', 1);
INSERT INTO bench_categories VALUES(5, 'CAT_E', 'Category Epsilon', 1);
INSERT INTO bench_categories VALUES(6, 'CAT_F', 'Category Zeta', 0);
INSERT INTO bench_categories VALUES(7, 'CAT_G', 'Category Eta', 1);
INSERT INTO bench_categories VALUES(8, 'CAT_H', 'Category Theta', 1);
INSERT INTO bench_categories VALUES(9, 'CAT_I', 'Category Iota', 0);
INSERT INTO bench_categories VALUES(10, 'CAT_J', 'Category Kappa', 1);

-- Sample data rows (use bench tool to generate more)
INSERT INTO bench_data VALUES(1, 'CAT_A', 'Item 0001', 100, 10.50, '1997-01-01', 'Test notes for item 1');
INSERT INTO bench_data VALUES(2, 'CAT_B', 'Item 0002', 200, 20.75, '1997-01-02', 'Test notes for item 2');
INSERT INTO bench_data VALUES(3, 'CAT_C', 'Item 0003', 300, 30.25, '1997-01-03', 'Test notes for item 3');
INSERT INTO bench_data VALUES(4, 'CAT_A', 'Item 0004', 400, 40.00, '1997-01-04', 'Test notes for item 4');
INSERT INTO bench_data VALUES(5, 'CAT_B', 'Item 0005', 500, 50.50, '1997-01-05', 'Test notes for item 5');

-- =============================================================================
-- VERIFICATION QUERIES
-- =============================================================================

SELECT 'bench_data' AS table_name, COUNT(*) AS row_count FROM bench_data
UNION ALL
SELECT 'bench_categories', COUNT(*) FROM bench_categories
UNION ALL
SELECT 'bench_orders', COUNT(*) FROM bench_orders
UNION ALL
SELECT 'bench_text', COUNT(*) FROM bench_text;

-- =============================================================================
-- BENCHMARK QUERIES (for reference)
-- =============================================================================

-- Q-001: Simple SELECT
-- SELECT * FROM bench_data WHERE id = ?;
-- SELECT * FROM bench_data WHERE category = ?;

-- Q-002: JOIN
-- SELECT d.*, c.description
-- FROM bench_data d JOIN bench_categories c ON d.category = c.code;

-- Q-003: Aggregates
-- SELECT category, COUNT(*), SUM(value), AVG(amount) FROM bench_data GROUP BY category;

-- Q-004: INSERT (transactional)
-- BEGIN; INSERT INTO bench_data VALUES(...); COMMIT;

-- Q-008: Pattern matching
-- SELECT * FROM bench_data WHERE name LIKE 'Item 00%';
-- SELECT * FROM bench_text WHERE body LIKE '%keyword%';
