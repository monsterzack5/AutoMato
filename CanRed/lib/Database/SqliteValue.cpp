#include "SqliteValue.h"

#include "sqlite_helpers.h"
#include <assert.h>

#define CHECK_ROW_COUNT()                       \
    if (sqlite3_data_count(m_statement) == 0) { \
        return {};                              \
    }

SqliteValue::operator ssize_t() const
{
    CHECK_ROW_COUNT();

    const auto type = sqlite3_column_type(m_statement, m_column_number);
    if (type != SQLITE_INTEGER) {
        fmt::print(fmt::fg(fmt::terminal_color::red), "Sqlite Failure: Cannot Cast {} to SQLITE_INTEGER\n", sqlite_type_to_str(type));
        exit(1);
    }

    return sqlite3_column_int64(m_statement, m_column_number);
}

SqliteValue::operator int32_t() const
{
    CHECK_ROW_COUNT();

    const auto type = sqlite3_column_type(m_statement, m_column_number);
    if (type != SQLITE_INTEGER) {
        fmt::print(fmt::fg(fmt::terminal_color::red), "Sqlite Failure: Cannot Cast {} to SQLITE_INTEGER\n", sqlite_type_to_str(type));
        exit(1);
    }

    return sqlite3_column_int(m_statement, m_column_number);
}

SqliteValue::operator size_t() const
{
    CHECK_ROW_COUNT();

    const auto type = sqlite3_column_type(m_statement, m_column_number);
    if (type != SQLITE_INTEGER) {
        fmt::print(fmt::fg(fmt::terminal_color::red), "Sqlite Failure: Cannot Cast {} to SQLITE_INTEGER\n", sqlite_type_to_str(type));
        exit(1);
    }

    return sqlite3_column_int64(m_statement, m_column_number);
}

SqliteValue::operator uint32_t() const
{
    CHECK_ROW_COUNT();

    const auto type = sqlite3_column_type(m_statement, m_column_number);
    if (type != SQLITE_INTEGER) {
        fmt::print(fmt::fg(fmt::terminal_color::red), "Sqlite Failure: Cannot Cast {} to SQLITE_INTEGER\n", sqlite_type_to_str(type));
        exit(1);
    }

    return sqlite3_column_int(m_statement, m_column_number);
}

SqliteValue::operator uint16_t() const
{
    CHECK_ROW_COUNT();

    const auto type = sqlite3_column_type(m_statement, m_column_number);
    if (type != SQLITE_INTEGER) {
        fmt::print(fmt::fg(fmt::terminal_color::red), "Sqlite Failure: Cannot Cast {} to SQLITE_INTEGER\n", sqlite_type_to_str(type));
        exit(1);
    }

    return sqlite3_column_int(m_statement, m_column_number);
}

SqliteValue::operator uint8_t() const
{
    CHECK_ROW_COUNT();

    const auto type = sqlite3_column_type(m_statement, m_column_number);
    if (type != SQLITE_INTEGER) {
        fmt::print(fmt::fg(fmt::terminal_color::red), "Sqlite Failure: Cannot Cast {} to SQLITE_INTEGER\n", sqlite_type_to_str(type));
        exit(1);
    }

    return sqlite3_column_int(m_statement, m_column_number);
}

SqliteValue::operator bool() const
{
    CHECK_ROW_COUNT();

    const auto type = sqlite3_column_type(m_statement, m_column_number);
    if (type != SQLITE_INTEGER) {
        fmt::print(fmt::fg(fmt::terminal_color::red), "Sqlite Failure: Cannot Cast {} to SQLITE_INTEGER\n", sqlite_type_to_str(type));
        exit(1);
    }

    return !!sqlite3_column_int(m_statement, m_column_number);
}

SqliteValue::operator std::string() const
{
    CHECK_ROW_COUNT();

    const auto type = sqlite3_column_type(m_statement, m_column_number);
    if (type != SQLITE3_TEXT) {
        fmt::print(fmt::fg(fmt::terminal_color::red), "Sqlite Failure: Cannot Cast {} to SQLITE3_TEXT\n", sqlite_type_to_str(type));
        exit(1);
    }

    const auto* str_ptr = sqlite3_column_text(m_statement, m_column_number);
    const auto str_size = sqlite3_column_bytes(m_statement, m_column_number);

    return std::string(reinterpret_cast<const char*>(str_ptr), str_size);
}

// Blob overload
SqliteValue::operator std::vector<uint8_t>() const
{
    CHECK_ROW_COUNT();

    const auto type = sqlite3_column_type(m_statement, m_column_number);
    if (type != SQLITE_BLOB) {
        fmt::print(fmt::fg(fmt::terminal_color::red), "Sqlite Failure: Cannot Cast {} to SQLITE_BLOB\n", sqlite_type_to_str(type));
        exit(1);
    }

    const auto* blob_ptr = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(m_statement, m_column_number));
    const auto blob_size = sqlite3_column_bytes(m_statement, m_column_number);

    return std::vector<uint8_t>(blob_ptr, &blob_ptr[blob_size]);
}
