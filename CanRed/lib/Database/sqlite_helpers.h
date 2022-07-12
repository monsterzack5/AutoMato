#pragma once

#include <fmt/color.h>
#include <fmt/format.h>
#include <sqlite3.h>
#include <stdint.h>
#include <string>

// TODO: Move this somewhere else.
#define CANRED_DEBUG_SQLITE

inline std::string sqlite_type_to_str(const int32_t sqlite_type)
{
    switch (sqlite_type) {
    case SQLITE_INTEGER:
        return "SQLITE_INTEGER";
    case SQLITE_FLOAT:
        return "SQLITE_FLOAT";
    case SQLITE_BLOB:
        return "SQLITE_BLOB";
    case SQLITE_NULL:
        return "SQLITE_NULL";
    case SQLITE3_TEXT:
        return "SQLITE3_TEXT";
    default:
        __builtin_unreachable();
        throw std::logic_error("Incorrect value passed to sqlite_type_to_str");
    }
}

inline void sqlite_print_query(sqlite3_stmt* statement)
{
#ifdef CANRED_DEBUG_SQLITE
    char* expanded_sql_query = sqlite3_expanded_sql(statement);
    fmt::print(fmt::fg(fmt::terminal_color::cyan), "{}\n", expanded_sql_query);
    sqlite3_free(expanded_sql_query);
#endif
}

inline void sqlite_print_error(int32_t error_code, const char* function)
{
    const char* error = sqlite3_errstr(error_code);
    fmt::print(fmt::fg(fmt::terminal_color::red), "Sqlite Error in {}:\nCode: {}, Error: {}\n", function, error_code, error);
}