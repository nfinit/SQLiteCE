-- Home Inventory Schema
-- SQLite/CE Example

CREATE TABLE rooms (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    floor INTEGER
);

CREATE TABLE categories (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL
);

CREATE TABLE items (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    category_id INTEGER,
    room_id INTEGER,
    brand TEXT,
    model TEXT,
    serial_number TEXT,
    purchase_date TEXT,
    purchase_price REAL,
    current_value REAL,
    warranty_expires TEXT,
    notes TEXT
);

CREATE TABLE photos (
    id INTEGER PRIMARY KEY,
    item_id INTEGER NOT NULL,
    filename TEXT NOT NULL,
    description TEXT
);

CREATE INDEX idx_items_room ON items(room_id);
CREATE INDEX idx_items_category ON items(category_id);
CREATE INDEX idx_photos_item ON photos(item_id);

-- Views
CREATE VIEW v_items AS
SELECT i.id, i.name, c.name AS category, r.name AS room,
       i.brand, i.model, i.purchase_price, i.current_value
FROM items i
LEFT JOIN categories c ON i.category_id = c.id
LEFT JOIN rooms r ON i.room_id = r.id;

CREATE VIEW v_room_totals AS
SELECT r.name AS room, COUNT(i.id) AS item_count, 
       SUM(i.current_value) AS total_value
FROM rooms r
LEFT JOIN items i ON r.id = i.room_id
GROUP BY r.id;

CREATE VIEW v_warranty_expiring AS
SELECT name, brand, model, warranty_expires
FROM items
WHERE warranty_expires IS NOT NULL
AND warranty_expires <= date('now', '+6 months');
