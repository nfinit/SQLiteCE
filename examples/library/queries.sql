-- Home Library Example Queries
-- SQLite/CE Example

-- List all books with author names
SELECT b.title, a.name AS author, b.year_published, b.genre
FROM books b
LEFT JOIN authors a ON b.author_id = a.id
ORDER BY a.name, b.title;

-- Books by genre
SELECT genre, COUNT(*) AS count
FROM books
GROUP BY genre
ORDER BY count DESC;

-- Currently loaned books
SELECT b.title, l.borrower, l.date_out, l.date_due
FROM loans l
JOIN books b ON l.book_id = b.id
WHERE l.date_returned IS NULL;

-- Overdue books (assuming today is 2026-01-10)
SELECT b.title, l.borrower, l.date_due
FROM loans l
JOIN books b ON l.book_id = b.id
WHERE l.date_returned IS NULL
AND l.date_due < '2026-01-10';

-- Top rated books
SELECT title, rating
FROM books
WHERE rating = 5;

-- Books added this year
SELECT title, date_added
FROM books
WHERE date_added >= '2025-01-01'
ORDER BY date_added;


-- Using views
SELECT * FROM v_books ORDER BY author, title;

SELECT * FROM v_loans_active;
