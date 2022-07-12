#pragma once

#include <sqlite3.h>

#include "SqliteValue.h"
#include "sqlite_helpers.h"

class SqliteResult {
public:
    SqliteResult(sqlite3_stmt* statement)
        : m_statement(statement)
    {
    }
    ~SqliteResult() = default;

    SqliteValue column(int column) const
    {
        return SqliteValue(m_statement, column);
    }

    void print_debug() const
    {
        auto columns = sqlite3_data_count(m_statement);
        char* expanded_sql_query = sqlite3_expanded_sql(m_statement);

        fmt::print("For Statement {}\nSqlite3 returned {} rows.\n", expanded_sql_query, columns);

        sqlite3_free(expanded_sql_query);

        for (int i = 0; i < columns; i += 1) {
            const auto type = sqlite3_column_type(m_statement, i);
            fmt::print("Column {} Type: {}\n", i, sqlite_type_to_str(type));
        }

        fmt::print("\n");
    }

private:
    sqlite3_stmt* m_statement;
};