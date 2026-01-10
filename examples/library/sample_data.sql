-- Home Library Sample Data
-- SQLite/CE Example

INSERT INTO authors (id, name, birth_year) VALUES (1, 'Donald Knuth', 1938);
INSERT INTO authors (id, name, birth_year) VALUES (2, 'Brian Kernighan', 1942);
INSERT INTO authors (id, name, birth_year) VALUES (3, 'Andrew Tanenbaum', 1944);
INSERT INTO authors (id, name, birth_year) VALUES (4, 'Charles Petzold', 1953);
INSERT INTO authors (id, name, birth_year) VALUES (5, 'Steve McConnell', 1962);
INSERT INTO authors (id, name, birth_year) VALUES (6, 'Douglas Boling', NULL);
INSERT INTO authors (id, name, birth_year) VALUES (7, 'John Murray', NULL);
INSERT INTO authors (id, name, birth_year) VALUES (8, 'Dominic Selly', NULL);
INSERT INTO authors (id, name, birth_year) VALUES (9, 'David Patterson', 1947);
INSERT INTO authors (id, name, birth_year) VALUES (10, 'John Hennessy', 1952);
INSERT INTO authors (id, name, birth_year) VALUES (11, 'Joseph Yiu', NULL);

INSERT INTO books (id, title, author_id, isbn, year_published, genre, format, location, rating, date_added) VALUES (1, 'The Art of Computer Programming Vol 1', 1, '0201896834', 1968, 'Computer Science', 'Hardcover', 'Shelf A1', 5, '2025-01-15');
INSERT INTO books (id, title, author_id, isbn, year_published, genre, format, location, rating, date_added) VALUES (2, 'The Art of Computer Programming Vol 2', 1, '0201896842', 1969, 'Computer Science', 'Hardcover', 'Shelf A1', 5, '2025-01-15');
INSERT INTO books (id, title, author_id, isbn, year_published, genre, format, location, rating, date_added) VALUES (3, 'The Art of Computer Programming Vol 3', 1, '0201896850', 1973, 'Computer Science', 'Hardcover', 'Shelf A1', 5, '2025-01-15');
INSERT INTO books (id, title, author_id, isbn, year_published, genre, format, location, rating, date_added) VALUES (4, 'The C Programming Language', 2, '0131103628', 1978, 'Programming', 'Paperback', 'Shelf A2', 5, '2025-02-10');
INSERT INTO books (id, title, author_id, isbn, year_published, genre, format, location, rating, date_added) VALUES (5, 'The Practice of Programming', 2, '020161586X', 1999, 'Programming', 'Paperback', 'Shelf A2', 5, '2025-02-10');
INSERT INTO books (id, title, author_id, isbn, year_published, genre, format, location, rating, date_added) VALUES (6, 'Operating Systems: Design and Implementation', 3, '0136386776', 1987, 'Operating Systems', 'Hardcover', 'Shelf B1', 5, '2025-03-01');
INSERT INTO books (id, title, author_id, isbn, year_published, genre, format, location, rating, date_added) VALUES (7, 'Programming Windows', 4, '157231995X', 1998, 'Programming', 'Hardcover', 'Shelf B2', 5, '2025-04-20');
INSERT INTO books (id, title, author_id, isbn, year_published, genre, format, location, rating, date_added) VALUES (8, 'Code: The Hidden Language', 4, '0735611319', 1999, 'Computer Science', 'Paperback', 'Shelf B2', 5, '2025-04-20');
INSERT INTO books (id, title, author_id, isbn, year_published, genre, format, location, rating, date_added) VALUES (9, 'Code Complete', 5, '0735619670', 1993, 'Software Engineering', 'Paperback', 'Shelf C1', 5, '2025-05-05');
INSERT INTO books (id, title, author_id, isbn, year_published, genre, format, location, rating, date_added) VALUES (10, 'Programming Windows CE', 6, '1572318562', 1998, 'Windows CE', 'Hardcover', 'Shelf D1', 5, '2025-06-01');
INSERT INTO books (id, title, author_id, isbn, year_published, genre, format, location, rating, date_added) VALUES (11, 'Programming Microsoft Windows CE .NET', 6, '0735618844', 2003, 'Windows CE', 'Hardcover', 'Shelf D1', 5, '2025-06-01');
INSERT INTO books (id, title, author_id, isbn, year_published, genre, format, location, rating, date_added) VALUES (12, 'Windows CE 3.0 Application Programming', 7, '0130255920', 2000, 'Windows CE', 'Paperback', 'Shelf D1', 4, '2025-06-15');
INSERT INTO books (id, title, author_id, isbn, year_published, genre, format, location, rating, date_added) VALUES (13, 'Pocket PC Developers Guide', 8, '0072132302', 2001, 'Windows CE', 'Paperback', 'Shelf D2', 4, '2025-06-15');
INSERT INTO books (id, title, author_id, isbn, year_published, genre, format, location, rating, date_added) VALUES (14, 'Computer Organization and Design', 9, '1558604286', 1994, 'Computer Architecture', 'Hardcover', 'Shelf E1', 5, '2025-07-01');
INSERT INTO books (id, title, author_id, isbn, year_published, genre, format, location, rating, date_added) VALUES (15, 'Computer Architecture: A Quantitative Approach', 10, '1558605967', 1990, 'Computer Architecture', 'Hardcover', 'Shelf E1', 5, '2025-07-01');
INSERT INTO books (id, title, author_id, isbn, year_published, genre, format, location, rating, date_added) VALUES (16, 'The Definitive Guide to ARM Cortex-M0', 11, '0123854776', 2011, 'Embedded Systems', 'Paperback', 'Shelf E2', 4, '2025-08-01');
INSERT INTO books (id, title, author_id, isbn, year_published, genre, format, location, rating, date_added) VALUES (17, 'See MIPS Run', NULL, '1558604103', 1999, 'Embedded Systems', 'Paperback', 'Shelf E2', 5, '2025-08-15');
INSERT INTO books (id, title, author_id, isbn, year_published, genre, format, location, rating, date_added) VALUES (18, 'SH3/SH4 Programming Manual', NULL, NULL, 1998, 'Embedded Systems', 'Paperback', 'Shelf E3', 4, '2025-09-01');
INSERT INTO books (id, title, author_id, isbn, year_published, genre, format, location, rating, date_added) VALUES (19, 'StrongARM System Developers Guide', NULL, '0126256209', 2000, 'Embedded Systems', 'Hardcover', 'Shelf E3', 4, '2025-09-01');

INSERT INTO loans (id, book_id, borrower, date_out, date_due, date_returned) VALUES (1, 2, 'Alice Chen', '2025-11-01', '2025-12-01', '2025-11-20');
INSERT INTO loans (id, book_id, borrower, date_out, date_due, date_returned) VALUES (2, 8, 'Bob Martinez', '2025-12-15', '2026-01-15', NULL);
INSERT INTO loans (id, book_id, borrower, date_out, date_due, date_returned) VALUES (3, 15, 'Carol Singh', '2026-01-05', '2026-02-05', NULL);
