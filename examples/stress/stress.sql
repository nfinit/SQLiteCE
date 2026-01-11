-- Stress Test Schema
-- SQLite/CE Testing

-- Wide table with many columns
CREATE TABLE wide_table (
    id INTEGER PRIMARY KEY,
    col1 TEXT, col2 TEXT, col3 TEXT, col4 TEXT, col5 TEXT,
    col6 TEXT, col7 TEXT, col8 TEXT, col9 TEXT, col10 TEXT,
    col11 INTEGER, col12 INTEGER, col13 REAL, col14 REAL,
    col15 TEXT
);

-- Table for bulk data
CREATE TABLE numbers (
    id INTEGER PRIMARY KEY,
    value INTEGER,
    name TEXT,
    factor REAL
);

-- Mixed data types
CREATE TABLE mixed_types (
    id INTEGER PRIMARY KEY,
    int_val INTEGER,
    real_val REAL,
    text_val TEXT,
    null_val TEXT,
    wide_text TEXT
);

-- Multiple indexes on one table
CREATE INDEX idx_numbers_value ON numbers(value);
CREATE INDEX idx_numbers_name ON numbers(name);
CREATE INDEX idx_numbers_factor ON numbers(factor);

-- View joining tables
CREATE VIEW v_numbers AS
SELECT id, value, name, factor, value * factor AS product
FROM numbers;

-- Trigger for testing
CREATE TABLE trigger_log (msg TEXT);
CREATE TRIGGER numbers_log AFTER INSERT ON numbers FOR EACH ROW
BEGIN
    INSERT INTO trigger_log (msg) VALUES ('row inserted');
END;

-- Insert 100 rows for scrolling/abort testing
INSERT INTO numbers (value, name, factor) VALUES (1, 'one', 1.0);
INSERT INTO numbers (value, name, factor) VALUES (2, 'two', 1.5);
INSERT INTO numbers (value, name, factor) VALUES (3, 'three', 2.0);
INSERT INTO numbers (value, name, factor) VALUES (4, 'four', 2.5);
INSERT INTO numbers (value, name, factor) VALUES (5, 'five', 3.0);
INSERT INTO numbers (value, name, factor) VALUES (6, 'six', 3.5);
INSERT INTO numbers (value, name, factor) VALUES (7, 'seven', 4.0);
INSERT INTO numbers (value, name, factor) VALUES (8, 'eight', 4.5);
INSERT INTO numbers (value, name, factor) VALUES (9, 'nine', 5.0);
INSERT INTO numbers (value, name, factor) VALUES (10, 'ten', 5.5);
INSERT INTO numbers (value, name, factor) VALUES (11, 'eleven', 6.0);
INSERT INTO numbers (value, name, factor) VALUES (12, 'twelve', 6.5);
INSERT INTO numbers (value, name, factor) VALUES (13, 'thirteen', 7.0);
INSERT INTO numbers (value, name, factor) VALUES (14, 'fourteen', 7.5);
INSERT INTO numbers (value, name, factor) VALUES (15, 'fifteen', 8.0);
INSERT INTO numbers (value, name, factor) VALUES (16, 'sixteen', 8.5);
INSERT INTO numbers (value, name, factor) VALUES (17, 'seventeen', 9.0);
INSERT INTO numbers (value, name, factor) VALUES (18, 'eighteen', 9.5);
INSERT INTO numbers (value, name, factor) VALUES (19, 'nineteen', 10.0);
INSERT INTO numbers (value, name, factor) VALUES (20, 'twenty', 10.5);
INSERT INTO numbers (value, name, factor) VALUES (21, 'twenty-one', 11.0);
INSERT INTO numbers (value, name, factor) VALUES (22, 'twenty-two', 11.5);
INSERT INTO numbers (value, name, factor) VALUES (23, 'twenty-three', 12.0);
INSERT INTO numbers (value, name, factor) VALUES (24, 'twenty-four', 12.5);
INSERT INTO numbers (value, name, factor) VALUES (25, 'twenty-five', 13.0);
INSERT INTO numbers (value, name, factor) VALUES (26, 'twenty-six', 13.5);
INSERT INTO numbers (value, name, factor) VALUES (27, 'twenty-seven', 14.0);
INSERT INTO numbers (value, name, factor) VALUES (28, 'twenty-eight', 14.5);
INSERT INTO numbers (value, name, factor) VALUES (29, 'twenty-nine', 15.0);
INSERT INTO numbers (value, name, factor) VALUES (30, 'thirty', 15.5);
INSERT INTO numbers (value, name, factor) VALUES (31, 'thirty-one', 16.0);
INSERT INTO numbers (value, name, factor) VALUES (32, 'thirty-two', 16.5);
INSERT INTO numbers (value, name, factor) VALUES (33, 'thirty-three', 17.0);
INSERT INTO numbers (value, name, factor) VALUES (34, 'thirty-four', 17.5);
INSERT INTO numbers (value, name, factor) VALUES (35, 'thirty-five', 18.0);
INSERT INTO numbers (value, name, factor) VALUES (36, 'thirty-six', 18.5);
INSERT INTO numbers (value, name, factor) VALUES (37, 'thirty-seven', 19.0);
INSERT INTO numbers (value, name, factor) VALUES (38, 'thirty-eight', 19.5);
INSERT INTO numbers (value, name, factor) VALUES (39, 'thirty-nine', 20.0);
INSERT INTO numbers (value, name, factor) VALUES (40, 'forty', 20.5);
INSERT INTO numbers (value, name, factor) VALUES (41, 'forty-one', 21.0);
INSERT INTO numbers (value, name, factor) VALUES (42, 'forty-two', 21.5);
INSERT INTO numbers (value, name, factor) VALUES (43, 'forty-three', 22.0);
INSERT INTO numbers (value, name, factor) VALUES (44, 'forty-four', 22.5);
INSERT INTO numbers (value, name, factor) VALUES (45, 'forty-five', 23.0);
INSERT INTO numbers (value, name, factor) VALUES (46, 'forty-six', 23.5);
INSERT INTO numbers (value, name, factor) VALUES (47, 'forty-seven', 24.0);
INSERT INTO numbers (value, name, factor) VALUES (48, 'forty-eight', 24.5);
INSERT INTO numbers (value, name, factor) VALUES (49, 'forty-nine', 25.0);
INSERT INTO numbers (value, name, factor) VALUES (50, 'fifty', 25.5);
INSERT INTO numbers (value, name, factor) VALUES (51, 'fifty-one', 26.0);
INSERT INTO numbers (value, name, factor) VALUES (52, 'fifty-two', 26.5);
INSERT INTO numbers (value, name, factor) VALUES (53, 'fifty-three', 27.0);
INSERT INTO numbers (value, name, factor) VALUES (54, 'fifty-four', 27.5);
INSERT INTO numbers (value, name, factor) VALUES (55, 'fifty-five', 28.0);
INSERT INTO numbers (value, name, factor) VALUES (56, 'fifty-six', 28.5);
INSERT INTO numbers (value, name, factor) VALUES (57, 'fifty-seven', 29.0);
INSERT INTO numbers (value, name, factor) VALUES (58, 'fifty-eight', 29.5);
INSERT INTO numbers (value, name, factor) VALUES (59, 'fifty-nine', 30.0);
INSERT INTO numbers (value, name, factor) VALUES (60, 'sixty', 30.5);
INSERT INTO numbers (value, name, factor) VALUES (61, 'sixty-one', 31.0);
INSERT INTO numbers (value, name, factor) VALUES (62, 'sixty-two', 31.5);
INSERT INTO numbers (value, name, factor) VALUES (63, 'sixty-three', 32.0);
INSERT INTO numbers (value, name, factor) VALUES (64, 'sixty-four', 32.5);
INSERT INTO numbers (value, name, factor) VALUES (65, 'sixty-five', 33.0);
INSERT INTO numbers (value, name, factor) VALUES (66, 'sixty-six', 33.5);
INSERT INTO numbers (value, name, factor) VALUES (67, 'sixty-seven', 34.0);
INSERT INTO numbers (value, name, factor) VALUES (68, 'sixty-eight', 34.5);
INSERT INTO numbers (value, name, factor) VALUES (69, 'sixty-nine', 35.0);
INSERT INTO numbers (value, name, factor) VALUES (70, 'seventy', 35.5);
INSERT INTO numbers (value, name, factor) VALUES (71, 'seventy-one', 36.0);
INSERT INTO numbers (value, name, factor) VALUES (72, 'seventy-two', 36.5);
INSERT INTO numbers (value, name, factor) VALUES (73, 'seventy-three', 37.0);
INSERT INTO numbers (value, name, factor) VALUES (74, 'seventy-four', 37.5);
INSERT INTO numbers (value, name, factor) VALUES (75, 'seventy-five', 38.0);
INSERT INTO numbers (value, name, factor) VALUES (76, 'seventy-six', 38.5);
INSERT INTO numbers (value, name, factor) VALUES (77, 'seventy-seven', 39.0);
INSERT INTO numbers (value, name, factor) VALUES (78, 'seventy-eight', 39.5);
INSERT INTO numbers (value, name, factor) VALUES (79, 'seventy-nine', 40.0);
INSERT INTO numbers (value, name, factor) VALUES (80, 'eighty', 40.5);
INSERT INTO numbers (value, name, factor) VALUES (81, 'eighty-one', 41.0);
INSERT INTO numbers (value, name, factor) VALUES (82, 'eighty-two', 41.5);
INSERT INTO numbers (value, name, factor) VALUES (83, 'eighty-three', 42.0);
INSERT INTO numbers (value, name, factor) VALUES (84, 'eighty-four', 42.5);
INSERT INTO numbers (value, name, factor) VALUES (85, 'eighty-five', 43.0);
INSERT INTO numbers (value, name, factor) VALUES (86, 'eighty-six', 43.5);
INSERT INTO numbers (value, name, factor) VALUES (87, 'eighty-seven', 44.0);
INSERT INTO numbers (value, name, factor) VALUES (88, 'eighty-eight', 44.5);
INSERT INTO numbers (value, name, factor) VALUES (89, 'eighty-nine', 45.0);
INSERT INTO numbers (value, name, factor) VALUES (90, 'ninety', 45.5);
INSERT INTO numbers (value, name, factor) VALUES (91, 'ninety-one', 46.0);
INSERT INTO numbers (value, name, factor) VALUES (92, 'ninety-two', 46.5);
INSERT INTO numbers (value, name, factor) VALUES (93, 'ninety-three', 47.0);
INSERT INTO numbers (value, name, factor) VALUES (94, 'ninety-four', 47.5);
INSERT INTO numbers (value, name, factor) VALUES (95, 'ninety-five', 48.0);
INSERT INTO numbers (value, name, factor) VALUES (96, 'ninety-six', 48.5);
INSERT INTO numbers (value, name, factor) VALUES (97, 'ninety-seven', 49.0);
INSERT INTO numbers (value, name, factor) VALUES (98, 'ninety-eight', 49.5);
INSERT INTO numbers (value, name, factor) VALUES (99, 'ninety-nine', 50.0);
INSERT INTO numbers (value, name, factor) VALUES (100, 'one hundred', 50.5);

-- Mixed type data
INSERT INTO mixed_types (int_val, real_val, text_val, null_val, wide_text) 
VALUES (42, 3.14159, 'Hello', NULL, 'This is a much longer text value for testing column width handling');
INSERT INTO mixed_types (int_val, real_val, text_val, null_val, wide_text) 
VALUES (-999, 0.001, '', NULL, 'Another wide column test');
INSERT INTO mixed_types (int_val, real_val, text_val, null_val, wide_text) 
VALUES (0, -273.15, 'Test', NULL, 'Short');

-- Wide table data
INSERT INTO wide_table VALUES (1, 'a','b','c','d','e','f','g','h','i','j', 1, 2, 1.1, 2.2, 'end');
INSERT INTO wide_table VALUES (2, 'k','l','m','n','o','p','q','r','s','t', 3, 4, 3.3, 4.4, 'end');
