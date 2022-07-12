#include "SqliteIterator.h"

#include "sqlite_helpers.h"

SqliteIterator::SqliteIterator(sqlite3_stmt* statement, SqliteIteratorState state)
    : m_statement(statement)
    , m_state(state)
{
    // Make sure the statement is in a known good state
    // and then call step, for the first run.
    if (m_state == SqliteIteratorState::Start) {
        sqlite3_reset(m_statement);
        sqlite_print_query(m_statement);
        auto rc = sqlite3_step(m_statement);
        if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
            sqlite_print_error(rc, __PRETTY_FUNCTION__);
        }
    }
}

SqliteIterator& SqliteIterator::operator++()
{
    if (m_state == SqliteIteratorState::End) {
        return *this;
    }

    auto rc = sqlite3_step(m_statement);
    if (rc == SQLITE_DONE) {
        m_state = SqliteIteratorState::End;
    }

    if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
        sqlite_print_error(rc, __PRETTY_FUNCTION__);
    }

    return *this;
}

SqliteResult SqliteIterator::operator*()
{
    return SqliteResult(m_statement);
}

bool SqliteIterator::operator!=(const SqliteIterator& other)
{
    return m_state != other.m_state;
}
