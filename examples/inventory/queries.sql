-- Home Inventory Example Queries
-- SQLite/CE Example

-- All items with room and category
SELECT i.name, c.name AS category, r.name AS room, i.current_value
FROM items i
LEFT JOIN categories c ON i.category_id = c.id
LEFT JOIN rooms r ON i.room_id = r.id
ORDER BY r.name, i.name;

-- Total value by room
SELECT r.name AS room, SUM(i.current_value) AS total_value
FROM items i
JOIN rooms r ON i.room_id = r.id
GROUP BY r.id
ORDER BY total_value DESC;

-- Total value by category
SELECT c.name AS category, SUM(i.current_value) AS total_value
FROM items i
JOIN categories c ON i.category_id = c.id
GROUP BY c.id
ORDER BY total_value DESC;

-- Total inventory value
SELECT SUM(current_value) AS total_value FROM items;

-- Items with expiring warranty (next 6 months)
SELECT name, brand, model, warranty_expires
FROM items
WHERE warranty_expires IS NOT NULL
AND warranty_expires <= '2026-07-01'
AND warranty_expires >= '2026-01-01';

-- High value items (over $1000)
SELECT name, brand, current_value, room_id
FROM items
WHERE current_value >= 1000
ORDER BY current_value DESC;

-- Items with photos
SELECT i.name, COUNT(p.id) AS photo_count
FROM items i
LEFT JOIN photos p ON i.item_id = p.item_id
GROUP BY i.id
HAVING photo_count > 0;

-- Electronics inventory
SELECT i.name, i.brand, i.model, i.serial_number, i.current_value
FROM items i
JOIN categories c ON i.category_id = c.id
WHERE c.name = 'Electronics';

-- Depreciation report (purchase vs current value)
SELECT name, purchase_price, current_value,
       purchase_price - current_value AS depreciation
FROM items
WHERE purchase_price IS NOT NULL
ORDER BY depreciation DESC;


-- Using views
SELECT * FROM v_items ORDER BY room, name;

SELECT * FROM v_room_totals ORDER BY total_value DESC;

SELECT * FROM v_warranty_expiring;
