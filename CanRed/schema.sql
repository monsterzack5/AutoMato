-- Table to store all current can modules
CREATE TABLE IF NOT EXISTS can_modules (
    rowid INTEGER PRIMARY KEY AUTOINCREMENT,
    uid INTEGER NOT NULL UNIQUE,
    -- uuid TEXT | Not Currently Implemented
    is_active TEXT DEFAULT "TRUE" CHECK(is_active IN ("TRUE", "FALSE")),
    type TEXT CHECK(type IN ("READER", "WRITER")),
    name TEXT NOT NULL,
    description TEXT
);

-- Table to store commands on modules
CREATE TABLE IF NOT EXISTS can_module_commands (
    rowid INTEGER PRIMARY KEY AUTOINCREMENT,
    module_uid INTEGER NOT NULL,
    command_uid INTEGER NOT NULL,
    -- action TEXT CHECK(action IN ("READ", "WRITE")) NOT NULL,
    name TEXT NOT NULL,
    return_format TEXT NOT NULL,

    FOREIGN KEY(module_uid) REFERENCES can_modules(uid)
);

-- Maybe convert this to a WITHOUT ROWID table
CREATE TABLE IF NOT EXISTS broadcast_flows (
    rowid INTEGER PRIMARY KEY AUTOINCREMENT,
    flow_name TEXT NOT NULL,
    flow_id INTEGER NOT NULL,
    is_active TEXT DEFAULT "TRUE" CHECK(is_active IN ("TRUE", "FALSE"))
);

CREATE TABLE IF NOT EXISTS broadcast_events (
    rowid INTEGER PRIMARY KEY AUTOINCREMENT,
    flow_id INTEGER NOT NULL,
    flow_name TEXT NOT NULL,
    module_uid INTEGER NOT NULL,
    event_blob BLOB NOT NULL,
    section_number INTEGER NOT NULL,
    is_on_module TEXT DEFAULT "FALSE" CHECK(is_on_module IN ("TRUE", "FALSE")),

    FOREIGN KEY(module_uid) REFERENCES can_modules(uid),
    FOREIGN KEY(flow_name) REFERENCES broadcast_flows(flow_name),
    FOREIGN KEY(flow_id) REFERENCES broadcast_flows(flow_id)
);


-- Table to store possible error messages on modules
-- CREATE TABLE IF NOT EXISTS can_module_errors (
    -- id INTEGER PRIMARY KEY AUTOINCREMENT,
    -- can_id INTEGER NOT NULL,
    -- error TEXT NOT NULL,
    
    -- FOREIGN KEY(can_id) REFERENCES can_modules(can_id)
-- );

-- -- Stores Program Wide Events
-- CREATE TABLE IF NOT EXISTS event_log (
--     id INTEGER PRIMARY KEY AUTOINCREMENT,
--     event TEXT NOT NULL,
--     epoch INTEGER NOT NULL,
--     can_id INTEGER
-- );

-- -- Stores Program Wide Errors
-- CREATE TABLE IF NOT EXISTS error_log (
--     id INTEGER PRIMARY KEY AUTOINCREMENT,
--     error TEXT NOT NULL,
--     epoch INTEGER NOT NULL,
--     can_id INTEGER
-- );

-- https://sqlite.org/wal.html
PRAGMA journal_mode=WAL;
