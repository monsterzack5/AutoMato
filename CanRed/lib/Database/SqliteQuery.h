#pragma once

#include <assert.h>
#include <fmt/color.h>
#include <fmt/format.h>
#include <sqlite3.h>
#include <string>

#include "SqliteIterator.h"
#include "SqliteResult.h"
#include "SqliteValue.h"

class SqliteQuery {
public:
    SqliteQuery(sqlite3* instance, const std::string& query);
    ~SqliteQuery();

    // get() Variadic Template
    template<typename T, typename... Args>
    SqliteResult get(const T& value, const Args&... args)
    {
        bind(value);
        return get(args...);
    }

    template<typename T>
    SqliteResult get(const T& value)
    {
        bind(value);
        return get();
    }

    SqliteResult get();
    // ---

    // run() Variadic Template
    template<typename T, typename... Args>
    void run(const T& value, const Args&... args)
    {
        bind(value);
        run(args...);
    }

    template<typename T>
    void run(const T& value)
    {
        bind(value);
        run();
    }

    void run();
    // ---

    SqliteIterator begin() const;
    SqliteIterator end() const;

private:
    template<typename T>
    void bind(const T& value)
    {
        m_current_column += 1;
        bind(m_current_column, value);
    }

    void bind(int32_t column, const std::string& value) const;
    void bind(int32_t column, ssize_t value) const;
    void bind(int32_t column, int32_t value) const;
    void bind(int32_t column, size_t value) const;
    void bind(int32_t column, uint32_t value) const;
    void bind(int32_t column, const std::vector<uint8_t>& blob) const;

    sqlite3_stmt* m_statement;
    bool m_is_statement_finished { false };

    int32_t m_current_column { 0 };
};
