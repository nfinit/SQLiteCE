-- Field Geology Sample Catalog Schema
-- SQLite/CE Example
-- Idaho focus: Bogus Basin / Boise Front area

CREATE TABLE rock_types (
    id INTEGER PRIMARY KEY,
    category TEXT NOT NULL,  -- igneous, sedimentary, metamorphic
    name TEXT NOT NULL
);

CREATE TABLE formations (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    age_period TEXT,
    age_mya TEXT,  -- millions of years ago
    description TEXT
);

CREATE TABLE samples (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    rock_type_id INTEGER,
    formation_id INTEGER,
    location TEXT,
    latitude TEXT,
    longitude TEXT,
    elevation_ft INTEGER,
    collected_date TEXT,
    collector TEXT,
    notes TEXT
);

CREATE INDEX idx_samples_rock ON samples(rock_type_id);
CREATE INDEX idx_samples_formation ON samples(formation_id);
CREATE INDEX idx_samples_date ON samples(collected_date);

-- Views
CREATE VIEW v_samples AS
SELECT s.id, s.name, r.category, r.name AS rock_type,
       f.name AS formation, s.location, s.collected_date
FROM samples s
LEFT JOIN rock_types r ON s.rock_type_id = r.id
LEFT JOIN formations f ON s.formation_id = f.id;

CREATE VIEW v_collection_summary AS
SELECT r.category, COUNT(*) AS count
FROM samples s
JOIN rock_types r ON s.rock_type_id = r.id
GROUP BY r.category;
