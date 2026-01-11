-- Home Library Catalog Schema
-- SQLite/CE Example

CREATE TABLE authors (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    birth_year INTEGER
);

CREATE TABLE books (
    id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    author_id INTEGER,
    isbn TEXT,
    year_published INTEGER,
    genre TEXT,
    format TEXT,
    location TEXT,
    rating INTEGER,
    date_added TEXT,
    notes TEXT
);

CREATE TABLE loans (
    id INTEGER PRIMARY KEY,
    book_id INTEGER NOT NULL,
    borrower TEXT NOT NULL,
    date_out TEXT NOT NULL,
    date_due TEXT,
    date_returned TEXT
);

CREATE INDEX idx_books_author ON books(author_id);
CREATE INDEX idx_books_genre ON books(genre);
CREATE INDEX idx_loans_book ON loans(book_id);

-- Views
CREATE VIEW v_books AS
SELECT b.id, b.title, a.name AS author, b.year_published, 
       b.genre, b.format, b.location, b.rating
FROM books b
LEFT JOIN authors a ON b.author_id = a.id;

CREATE VIEW v_loans_active AS
SELECT l.id, b.title, l.borrower, l.date_out, l.date_due
FROM loans l
JOIN books b ON l.book_id = b.id
WHERE l.date_returned IS NULL;


-- Audit log for tracking changes
CREATE TABLE audit_log (
    id INTEGER PRIMARY KEY,
    table_name TEXT,
    action TEXT,
    record_id INTEGER
);

-- Triggers for books
CREATE TRIGGER books_insert AFTER INSERT ON books FOR EACH ROW
BEGIN
    INSERT INTO audit_log (table_name, action, record_id) VALUES ('books', 'INSERT', NEW.id);
END;

CREATE TRIGGER books_update AFTER UPDATE ON books FOR EACH ROW
BEGIN
    INSERT INTO audit_log (table_name, action, record_id) VALUES ('books', 'UPDATE', NEW.id);
END;

CREATE TRIGGER books_delete AFTER DELETE ON books FOR EACH ROW
BEGIN
    INSERT INTO audit_log (table_name, action, record_id) VALUES ('books', 'DELETE', OLD.id);
END;
