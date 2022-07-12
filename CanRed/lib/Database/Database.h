#pragma once

#include <SqliteQuery.h>
#include <fstream>
#include <iostream>
#include <sqlite3.h>
#include <string>
#include <sstream>

class Database {
public:
    ~Database();

    static Database& the();

    SqliteQuery prepare(const std::string& query) const;

private:
    Database();

    sqlite3* m_sqlite_instance;

    std::string m_database_file;
    std::string m_schema_file;
};