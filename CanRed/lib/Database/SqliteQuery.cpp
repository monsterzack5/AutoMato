#include "SqliteQuery.h"

#include "sqlite_helpers.h"

SqliteQuery::SqliteQuery(sqlite3* instance, const std::string& query)
{
    sqlite3_prepare_v2(instance, query.c_str(), query.size(), &m_statement, nullptr);
}

SqliteQuery::~SqliteQuery()
{
    sqlite3_finalize(m_statement);
}

void SqliteQuery::bind(int32_t column, const std::string& value) const
{
    assert(column > 0);
    sqlite3_bind_text(m_statement, column, value.c_str(), value.size(), SQLITE_TRANSIENT);
}

void SqliteQuery::bind(int32_t column, ssize_t value) const
{
    assert(column > 0);
    sqlite3_bind_int64(m_statement, column, value);
}

void SqliteQuery::bind(int32_t column, int32_t value) const
{
    assert(column > 0);
    sqlite3_bind_int(m_statement, column, value);
}

void SqliteQuery::bind(int32_t column, size_t value) const
{
    assert(column > 0);
    sqlite3_bind_int64(m_statement, column, value);
}

void SqliteQuery::bind(int32_t column, uint32_t value) const
{
    assert(column > 0);
    sqlite3_bind_int(m_statement, column, value);
}

void SqliteQuery::bind(int32_t column, const std::vector<uint8_t>& blob) const
{
    assert(column > 0);
    sqlite3_bind_blob(m_statement, column, blob.data(), blob.size(), SQLITE_TRANSIENT);
}

SqliteResult SqliteQuery::get()
{
    if (!m_is_statement_finished) {
        sqlite_print_query(m_statement);

        auto rc = sqlite3_step(m_statement);

        if (rc == SQLITE_DONE) {
            m_is_statement_finished = true;
        }

        if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
            sqlite_print_error(rc, __PRETTY_FUNCTION__);
        }
    }
    return SqliteResult(m_statement);
}

void SqliteQuery::run()
{
    sqlite_print_query(m_statement);
    auto rc = sqlite3_step(m_statement);
    if (rc != SQLITE_DONE) {
        sqlite_print_error(rc, __PRETTY_FUNCTION__);
    }
    sqlite3_reset(m_statement);
    sqlite3_clear_bindings(m_statement);
    m_current_column = 0;
}

SqliteIterator SqliteQuery::begin() const
{
    return SqliteIterator(m_statement, SqliteIteratorState::Start);
}

SqliteIterator SqliteQuery::end() const
{
    return SqliteIterator(m_statement, SqliteIteratorState::End);
}