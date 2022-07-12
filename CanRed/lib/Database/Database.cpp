#include "Database.h"

#include <get_env_var.h>

Database::~Database()
{
    sqlite3_close_v2(m_sqlite_instance);
}

Database& Database::the()
{
    static Database m_the;
    return m_the;
}

SqliteQuery Database::prepare(const std::string& query) const
{
    return SqliteQuery(m_sqlite_instance, query);
}

Database::Database()
{
    m_database_file = get_env_var(ENV::DB_FILE);
    
    // TODO: Not hardcode this, and compile it into the program.
    m_schema_file = "./schema.sql";

    sqlite3_open(m_database_file.c_str(), &m_sqlite_instance);

    std::fstream file(m_schema_file, std::fstream::in);
    assert(file.is_open());

    std::stringstream buffer;
    buffer << file.rdbuf();

    sqlite3_exec(m_sqlite_instance, buffer.str().c_str(), nullptr, 0, nullptr);
}