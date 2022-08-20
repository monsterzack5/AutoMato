#pragma once

#include <Platform.h>
#include <stddef.h>
#include <stdint.h>

// TODO:
//       I am not really the biggest fan of how this type is implemented.
//       I feel like having a tagged union type is necessary
//       for a few reasons:
//        - User functions should be able to return any type of data
//        - When serializing data, having a generic type is incredibly useful
//        - We need generic and yet specific handling for many functions.
//          and avoiding having to write 9 different overloads
//          is something I appreciate.
//       But, in it's current form, this type just leads to bad
//       and incredibly un-performant branching.
//       For now, this type and system work, so I haven't put
//       much effort into revamping it, but it should be slated for
//       a re-imaging in the near future.

#ifdef PLATFORM_DESKTOP
#    include <fmt/format.h>
#    include <string>
#else
#    include <Log.hpp>
#endif

static_assert(sizeof(int8_t) == 1, "sizeof(int8_t) is not what expect it to be.");
static_assert(sizeof(int16_t) == 2, "sizeof(int16_t) is not what expect it to be.");
static_assert(sizeof(int32_t) == 4, "sizeof(int32_t) is not what expect it to be.");
static_assert(sizeof(int64_t) == 8, "sizeof(int64_t) is not what expect it to be.");
static_assert(sizeof(float) == 4, "sizeof(float) is not what expect it to be.");
static_assert(sizeof(double) == 8, "sizeof(double) is not what expect it to be.");
static_assert(sizeof(bool) == 1, "sizeof(bool) is not what expect it to be.");

enum class TypeUsed {
    i8 = 0,       // 0b000
    i16 = 1,      // 0b001
    i32 = 2,      // 0b010
    i64 = 3,      // 0b011
    f_float = 4,  // 0b100
    d_double = 5, // 0b101
    b_bool = 6,   // 0b110
    NOT_SET = 7,  // 0b111
};

enum class Conditional : uint8_t {
    LESS_THAN = 0,             // 0b000
    GREATER_THAN = 1,          // 0b001
    EQUAL = 2,                 // 0b010
    NOT_EQUAL = 3,             // 0b011
    LESS_THAN_OR_EQUAL = 4,    // 0b100
    GREATER_THAN_OR_EQUAL = 5, // 0b101
    NOT_SET = 6,
};

class AnyType {
public:
    AnyType() { set_value(); };

    template<typename T>
    AnyType(T value) { set_value(value); }

    ~AnyType() = default;

    // TODO: We should have a flag or enabling/disabling 8 byte types.
    static constexpr size_t MAX_SERIALIZED_SIZE = 8;

    uint8_t to_can_primitive() const;

    size_t serialize(uint8_t buffer[]) const;

    size_t size() const;

    void print() const;

    TypeUsed get_type() const { return m_type; }
    void set_type(const TypeUsed type) { m_type = type; }

    bool is_bool() const;
    bool is_int() const;
    bool is_floating() const;
    bool is_valid() const { return m_type != TypeUsed::NOT_SET; }

    // All these overloads are to prevent us having to use RTTI.
    void set_value() { m_type = TypeUsed::NOT_SET; };
    void set_value(int8_t value);
    void set_value(int16_t value);
    void set_value(int32_t value);
    void set_value(int64_t value);
    void set_value(float value);
    void set_value(double value);
    void set_value(bool value);

#ifdef CANRED
    static AnyType from_string(const std::string& str);
#endif

    static AnyType from_buffer(TypeUsed type, uint8_t buffer[]);

    operator int8_t() const { return i8; }
    operator int16_t() const { return i16; }
    operator int32_t() const { return i32; }
    operator int64_t() const { return i64; }
    operator float() const { return f_float; }
    operator double() const { return d_double; }
    operator bool() const { return b_bool; }

private:
    TypeUsed m_type { TypeUsed::NOT_SET };
    union {
        int8_t i8;
        int16_t i16;
        int32_t i32;
        int64_t i64;
        float f_float;
        double d_double;
        bool b_bool;
    };
};

template<typename lhs_t, typename rhs_t>
inline bool compare_conditionals(Conditional cond, rhs_t lhs, lhs_t rhs)
{
    switch (cond) {
    case Conditional::LESS_THAN:
        return lhs < rhs;
    case Conditional::GREATER_THAN:
        return lhs > rhs;
    case Conditional::EQUAL:
        return lhs == rhs;
    case Conditional::NOT_EQUAL:
        return lhs != rhs;
    case Conditional::LESS_THAN_OR_EQUAL:
        return lhs <= rhs;
    case Conditional::GREATER_THAN_OR_EQUAL:
        return lhs >= rhs;
    default:
        __builtin_unreachable();
        return false;
    }
}

inline bool compare_better_any_type(Conditional cond, const AnyType& lhs, const AnyType& rhs)
{

    if (!lhs.is_valid() || !rhs.is_valid()) {
        // Error!
        return false;
    }

    if ((lhs.is_int() || lhs.is_bool()) && (rhs.is_int() || rhs.is_bool())) {
        int64_t lhs_value = lhs;
        int64_t rhs_value = rhs;
        return compare_conditionals(cond, lhs_value, rhs_value);
    }

    // At this point, it's safe to assume that either of them are
    // a floating point type.

    double lhs_value = lhs;
    double rhs_value = rhs;

    return compare_conditionals(cond, rhs_value, lhs_value);
}

inline size_t type_used_size(TypeUsed type)
{
    switch (type) {
    case TypeUsed::i8:
        return 1;
    case TypeUsed::i16:
        return 2;
    case TypeUsed::i32:
        return 4;
    case TypeUsed::i64:
        return 8;
    case TypeUsed::f_float:
        return 4;
    case TypeUsed::d_double:
        return 8;
    case TypeUsed::b_bool:
        return 1;
    case TypeUsed::NOT_SET:
        return 0;
    }
    __builtin_unreachable();
}

template<typename T>
inline AnyType automato_return(T value)
{
    return AnyType { value };
}

inline AnyType automato_return()
{
    return AnyType {};
}

#ifdef CANRED
inline TypeUsed get_smallest_type(const std::string& value)
{
    // Checks for 'T'rue or 'F'alse or 0 or 1
    if (value.at(0) == 't' || value.at(0) == 'f' || value == "0" || value == "1") {
        return TypeUsed::b_bool;
    }

    // Float or Double
    if (value.find('.')) {
        if (value == std::to_string(std::stof(value))) {
            return TypeUsed::f_float;
        }

        return TypeUsed::d_double;
    }

    // Must be an integer
    // Brute force converting from smaller, to larger
    // types.
    const uint8_t CHAR_MAX_DIGITS = 3;
    const uint8_t SHORT_MAX_DIGITS = 5;
    const uint8_t INT_MAX_DIGITS = 10;

    if (value.length() < CHAR_MAX_DIGITS) {
        return TypeUsed::i8;
    }

    if (value.length() == CHAR_MAX_DIGITS) {
        if (value == std::to_string(std::stoi(value))) {
            return TypeUsed::i8;
        }
    }

    if (value.length() <= SHORT_MAX_DIGITS) {
        if (value == std::to_string(std::stoi(value))) {
            return TypeUsed::i16;
        }
    }

    if (value.length() <= INT_MAX_DIGITS) {
        if (value == std::to_string(std::stoi(value))) {
            return TypeUsed::i32;
        }
    }

    return TypeUsed::i64;
}
#endif

inline void print_type_used(const TypeUsed type)
{
#ifdef CANRED
    switch (type) {
    case TypeUsed::i8:
        fmt::print("i8");
        break;
    case TypeUsed::i16:
        fmt::print("i16");
        break;
    case TypeUsed::i32:
        fmt::print("i32");
        break;
    case TypeUsed::i64:
        fmt::print("i64");
        break;
    case TypeUsed::b_bool:
        fmt::print("bool");
        break;
    case TypeUsed::f_float:
        fmt::print("float");
        break;
    case TypeUsed::d_double:
        fmt::print("double");
        break;
    case TypeUsed::NOT_SET:
        fmt::print("NOT_SET");
        break;
    }
#else
    switch (type) {
    case TypeUsed::i8:
        DEBUG_PRINT("i8");
        break;
    case TypeUsed::i16:
        DEBUG_PRINT("i16");
        break;
    case TypeUsed::i32:
        DEBUG_PRINT("i32");
        break;
    case TypeUsed::i64:
        DEBUG_PRINT("i64");
        break;
    case TypeUsed::b_bool:
        DEBUG_PRINT("bool");
        break;
    case TypeUsed::f_float:
        DEBUG_PRINT("float");
        break;
    case TypeUsed::d_double:
        DEBUG_PRINT("double");
        break;
    case TypeUsed::NOT_SET:
        DEBUG_PRINT("NOT_SET");
        break;
    }
#endif
}

using AutomatoReturnData = AnyType;