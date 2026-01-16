-- Field Geology Example Queries
-- SQLite/CE Example

-- All samples with full details
SELECT s.name AS sample, r.name AS rock, f.name AS formation, 
       s.location, s.collected_date
FROM samples s
LEFT JOIN rock_types r ON s.rock_type_id = r.id
LEFT JOIN formations f ON s.formation_id = f.id
ORDER BY s.collected_date DESC;

-- Samples by rock category
SELECT r.category, COUNT(*) AS count
FROM samples s
JOIN rock_types r ON s.rock_type_id = r.id
GROUP BY r.category
ORDER BY count DESC;

-- Samples from Idaho Batholith
SELECT s.name, r.name AS rock_type, s.location, s.elevation_ft
FROM samples s
JOIN rock_types r ON s.rock_type_id = r.id
JOIN formations f ON s.formation_id = f.id
WHERE f.name = 'Idaho Batholith'
ORDER BY s.elevation_ft DESC;

-- High elevation samples (above 5000 ft)
SELECT name, location, elevation_ft
FROM samples
WHERE elevation_ft > 5000
ORDER BY elevation_ft DESC;

-- Samples collected today (for field use)
SELECT s.name, r.name AS rock_type, s.location, s.notes
FROM samples s
LEFT JOIN rock_types r ON s.rock_type_id = r.id
WHERE s.collected_date = '2026-01-15';

-- Formation age summary
SELECT name, age_period, age_mya || ' Ma' AS age
FROM formations
ORDER BY CAST(age_mya AS INTEGER) DESC;

-- Igneous rocks only
SELECT s.name, r.name AS rock_type, s.location
FROM samples s
JOIN rock_types r ON s.rock_type_id = r.id
WHERE r.category = 'igneous';

-- Using views
SELECT * FROM v_samples ORDER BY collected_date DESC;

SELECT * FROM v_collection_summary;
