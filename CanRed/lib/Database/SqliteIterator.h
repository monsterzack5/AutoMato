#pragma once

#include <sqlite3.h>

#include "SqliteResult.h"

enum class SqliteIteratorState {
    Start,
    End,
};

class SqliteIterator {
public:
    SqliteIterator(sqlite3_stmt* statement, SqliteIteratorState state);
    ~SqliteIterator() = default;

    SqliteIterator& operator++();
    SqliteResult operator*();
    bool operator!=(const SqliteIterator& other);

    SqliteIterator& operator++(int) = delete;
    SqliteIterator& operator--(int) = delete;
    SqliteIterator& operator--() = delete;
    SqliteIterator& operator[](size_t) = delete;
    SqliteIterator* operator->() = delete;
    bool operator==(const SqliteIterator&) = delete;

private:
    sqlite3_stmt* m_statement { nullptr };
    SqliteIteratorState m_state { SqliteIteratorState::Start };
};