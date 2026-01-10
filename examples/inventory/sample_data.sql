-- Home Inventory Sample Data
-- SQLite/CE Example

INSERT INTO rooms (id, name, floor) VALUES (1, 'Living Room', 1);
INSERT INTO rooms (id, name, floor) VALUES (2, 'Kitchen', 1);
INSERT INTO rooms (id, name, floor) VALUES (3, 'Master Bedroom', 2);
INSERT INTO rooms (id, name, floor) VALUES (4, 'Office', 2);
INSERT INTO rooms (id, name, floor) VALUES (5, 'Garage', 1);

INSERT INTO categories (id, name) VALUES (1, 'Electronics');
INSERT INTO categories (id, name) VALUES (2, 'Furniture');
INSERT INTO categories (id, name) VALUES (3, 'Appliances');
INSERT INTO categories (id, name) VALUES (4, 'Tools');
INSERT INTO categories (id, name) VALUES (5, 'Jewelry');

INSERT INTO items (id, name, category_id, room_id, brand, model, serial_number, purchase_date, purchase_price, current_value, warranty_expires, notes) VALUES (1, 'Television', 1, 1, 'Sony', 'KD-55X80K', 'SN12345678', '2024-03-15', 799.99, 600.00, '2027-03-15', '55 inch 4K');
INSERT INTO items (id, name, category_id, room_id, brand, model, serial_number, purchase_date, purchase_price, current_value, warranty_expires, notes) VALUES (2, 'Sofa', 2, 1, 'Ashley', 'Darcy', NULL, '2023-06-01', 549.00, 400.00, NULL, 'Gray fabric');
INSERT INTO items (id, name, category_id, room_id, brand, model, serial_number, purchase_date, purchase_price, current_value, warranty_expires, notes) VALUES (3, 'Refrigerator', 3, 2, 'LG', 'LRMVS3006S', 'RF98765432', '2022-11-20', 2499.00, 1800.00, '2025-11-20', 'French door');
INSERT INTO items (id, name, category_id, room_id, brand, model, serial_number, purchase_date, purchase_price, current_value, warranty_expires, notes) VALUES (4, 'Microwave', 3, 2, 'Panasonic', 'NN-SN686S', NULL, '2024-01-10', 149.99, 120.00, '2025-01-10', '1200W');
INSERT INTO items (id, name, category_id, room_id, brand, model, serial_number, purchase_date, purchase_price, current_value, warranty_expires, notes) VALUES (5, 'Bed Frame', 2, 3, 'Zinus', 'SmartBase', NULL, '2023-08-15', 189.00, 150.00, NULL, 'Queen size');
INSERT INTO items (id, name, category_id, room_id, brand, model, serial_number, purchase_date, purchase_price, current_value, warranty_expires, notes) VALUES (6, 'Laptop', 1, 4, 'Dell', 'XPS 15', 'DL55667788', '2024-06-01', 1599.00, 1300.00, '2027-06-01', '32GB RAM');
INSERT INTO items (id, name, category_id, room_id, brand, model, serial_number, purchase_date, purchase_price, current_value, warranty_expires, notes) VALUES (7, 'Monitor', 1, 4, 'Dell', 'U2722D', 'MN11223344', '2024-06-01', 449.99, 380.00, '2027-06-01', '27 inch');
INSERT INTO items (id, name, category_id, room_id, brand, model, serial_number, purchase_date, purchase_price, current_value, warranty_expires, notes) VALUES (8, 'Drill', 4, 5, 'DeWalt', 'DCD771C2', NULL, '2023-04-20', 99.00, 70.00, NULL, 'Cordless');
INSERT INTO items (id, name, category_id, room_id, brand, model, serial_number, purchase_date, purchase_price, current_value, warranty_expires, notes) VALUES (9, 'Table Saw', 4, 5, 'DeWalt', 'DWE7485', 'TS99887766', '2022-09-10', 349.00, 250.00, '2025-09-10', '10 inch');
INSERT INTO items (id, name, category_id, room_id, brand, model, serial_number, purchase_date, purchase_price, current_value, warranty_expires, notes) VALUES (10, 'Wedding Ring', 5, 3, 'Tiffany', NULL, NULL, '2015-06-20', 3500.00, 4000.00, NULL, 'Platinum band');

INSERT INTO photos (id, item_id, filename, description) VALUES (1, 1, 'tv_front.jpg', 'Front view');
INSERT INTO photos (id, item_id, filename, description) VALUES (2, 1, 'tv_serial.jpg', 'Serial number label');
INSERT INTO photos (id, item_id, filename, description) VALUES (3, 6, 'laptop.jpg', 'Laptop with accessories');
INSERT INTO photos (id, item_id, filename, description) VALUES (4, 10, 'ring.jpg', 'Wedding ring close-up');
