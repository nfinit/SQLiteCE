-- Field Geology Sample Data
-- SQLite/CE Example
-- Idaho focus: Bogus Basin / Boise Front area

-- Rock types
INSERT INTO rock_types VALUES (1, 'igneous', 'granite');
INSERT INTO rock_types VALUES (2, 'igneous', 'granodiorite');
INSERT INTO rock_types VALUES (3, 'igneous', 'rhyolite');
INSERT INTO rock_types VALUES (4, 'igneous', 'basalt');
INSERT INTO rock_types VALUES (5, 'sedimentary', 'sandstone');
INSERT INTO rock_types VALUES (6, 'sedimentary', 'siltstone');
INSERT INTO rock_types VALUES (7, 'metamorphic', 'gneiss');
INSERT INTO rock_types VALUES (8, 'metamorphic', 'schist');

-- Idaho formations (real geology of the Boise area)
INSERT INTO formations VALUES (1, 'Idaho Batholith', 'Cretaceous', '75-100', 
    'Large granitic intrusion forming Boise Front mountains');
INSERT INTO formations VALUES (2, 'Challis Volcanics', 'Eocene', '45-50',
    'Volcanic rocks from Challis volcanic episode');
INSERT INTO formations VALUES (3, 'Idaho Group', 'Miocene-Pliocene', '2-10',
    'Lake sediments from ancient Lake Idaho');
INSERT INTO formations VALUES (4, 'Snake River Plain Basalts', 'Quaternary', '0.01-2',
    'Young basalt flows from hotspot volcanism');
INSERT INTO formations VALUES (5, 'Glenns Ferry Formation', 'Pliocene', '3-4',
    'Lacustrine and fluvial sediments');

-- Sample collection (mix of real locations near Bogus Basin)
INSERT INTO samples VALUES (1, 'BB-001', 2, 1, 'Bogus Basin Road mile 12',
    '43.7621', '-116.1025', 6200, '2026-01-15', 'Field Team',
    'Coarse-grained, pink feldspar prominent');
INSERT INTO samples VALUES (2, 'BB-002', 1, 1, 'Deer Point trailhead',
    '43.7589', '-116.0987', 6450, '2026-01-15', 'Field Team',
    'Weathered surface, fresh interior gray-white');
INSERT INTO samples VALUES (3, 'BB-003', 2, 1, 'Superior summit area',
    '43.7534', '-116.1102', 7582, '2026-01-15', 'Field Team',
    'Excellent exposure, visible quartz and biotite');
INSERT INTO samples VALUES (4, 'SH-001', 4, 4, 'Swan Falls Road',
    '43.2456', '-116.3789', 2650, '2025-12-10', 'Field Team',
    'Vesicular basalt, pahoehoe texture');
INSERT INTO samples VALUES (5, 'SH-002', 4, 4, 'Celebration Park',
    '43.2512', '-116.4023', 2580, '2025-12-10', 'Field Team',
    'Columnar jointing visible in canyon wall');
INSERT INTO samples VALUES (6, 'TB-001', 5, 3, 'Table Rock',
    '43.5978', '-116.1456', 3680, '2025-11-22', 'Field Team',
    'Fine-grained, tan, lake sediment origin');
INSERT INTO samples VALUES (7, 'TB-002', 6, 5, 'Table Rock east slope',
    '43.5965', '-116.1423', 3520, '2025-11-22', 'Field Team',
    'Laminated, fossil leaf impressions');
INSERT INTO samples VALUES (8, 'CR-001', 3, 2, 'Cottonwood Creek',
    '43.8234', '-115.9876', 5100, '2025-10-05', 'Field Team',
    'Flow-banded rhyolite, Challis age');
