#pragma once

#include <fmt/color.h>
#include <fmt/format.h>
#include <sqlite3.h>
#include <string>
#include <vector>

class SqliteValue {
public:
    SqliteValue(sqlite3_stmt* statement, unsigned int column)
        : m_statement(statement)
        , m_column_number(column)
    {
    }
    ~SqliteValue() = default;

    operator ssize_t() const;
    operator int32_t() const;
    operator size_t() const;
    operator uint32_t() const;
    operator uint16_t() const;
    operator uint8_t() const;
    operator bool() const;
    operator std::string() const;

    // Blob overload
    operator std::vector<uint8_t>() const;

private:

    sqlite3_stmt* m_statement;
    size_t m_column_number { 0 };
};