#include "AnyType.h"

#include <CanConstants.h>
#include <cstring> // for memcpy
#include <stdint.h>

#if defined CANRED
#    include <fmt/format.h>
#else
#    include <Log.hpp>
#endif

uint8_t AnyType::to_can_primitive() const
{
    switch (m_type) {
    case TypeUsed::i8:
        return CAN::Primitive::SIGNED_1_BYTES;
    case TypeUsed::i16:
        return CAN::Primitive::SIGNED_2_BYTES;
    case TypeUsed::i32:
        return CAN::Primitive::SIGNED_4_BYTES;
    case TypeUsed::i64:
        return CAN::Primitive::SIGNED_8_BYTES;
    case TypeUsed::f_float:
        return CAN::Primitive::FLOAT_4_BYTES;
    case TypeUsed::d_double:
        return CAN::Primitive::DOUBLE_8_BYTES;
    case TypeUsed::b_bool:
        return CAN::Primitive::BOOL_1_BYTES;
    default:
        return CAN::Primitive::VOID;
    }
}

bool AnyType::is_int() const
{
    switch (m_type) {
    case TypeUsed::i8:
    case TypeUsed::i16:
    case TypeUsed::i32:
    case TypeUsed::i64:
        return true;
    default:
        return false;
    }
}

bool AnyType::is_bool() const { return m_type == TypeUsed::b_bool; }

bool AnyType::is_floating() const
{
    return (m_type == TypeUsed::f_float || m_type == TypeUsed::d_double);
}

size_t AnyType::size() const
{
    return type_used_size(m_type);
}

size_t AnyType::serialize(uint8_t buffer[]) const
{
    switch (m_type) {
    case TypeUsed::i8:
        memcpy(buffer, &i8, size());
        break;
    case TypeUsed::i16:
        memcpy(buffer, &i16, size());
        break;
    case TypeUsed::i32:
        memcpy(buffer, &i32, size());
        break;
    case TypeUsed::i64:
        memcpy(buffer, &i64, size());
        break;
    case TypeUsed::f_float:
        memcpy(buffer, &f_float, size());
        break;
    case TypeUsed::d_double:
        memcpy(buffer, &d_double, size());
        break;
    case TypeUsed::b_bool:
        memcpy(buffer, &b_bool, size());
        break;
    case TypeUsed::NOT_SET:
        break;
    }
    return size();
}

AnyType AnyType::from_buffer(TypeUsed type, uint8_t buffer[])
{
    AnyType any;

    any.set_type(type);

    size_t size = type_used_size(type);

    switch (type) {
    case TypeUsed::i8:
        memcpy(&any.i8, buffer, size);
        break;
    case TypeUsed::i16:
        memcpy(&any.i16, buffer, size);
        break;
    case TypeUsed::i32:
        memcpy(&any.i32, buffer, size);
        break;
    case TypeUsed::i64:
        memcpy(&any.i64, buffer, size);
        break;
    case TypeUsed::f_float:
        memcpy(&any.f_float, buffer, size);
        break;
    case TypeUsed::d_double:
        memcpy(&any.d_double, buffer, size);
        break;
    case TypeUsed::b_bool:
        memcpy(&any.b_bool, buffer, size);
        break;
    case TypeUsed::NOT_SET:
        break;
    }

    return any;
}

#ifdef CANRED

AnyType AnyType::from_string(const std::string& str)
{
    AnyType any;

    auto type = get_smallest_type(str);

    any.set_type(type);

    switch (type) {
    case TypeUsed::i8:
        any.i8 = std::stoi(str);
        break;
    case TypeUsed::i16:
        any.i16 = std::stoi(str);
        break;
    case TypeUsed::i32:
        any.i32 = std::stoi(str);
        break;
    case TypeUsed::i64:
        any.i64 = std::stoll(str);
        break;
    case TypeUsed::f_float:
        any.f_float = std::stof(str);
        break;
    case TypeUsed::d_double:
        any.d_double = std::stod(str);
        break;
    case TypeUsed::b_bool:
        // if we were passed "true" or "false"
        if (str.at(0) == 't' || str.at(0) == 'f') {
            any.b_bool = str.at(0) == 't';
            break;
        }

        any.b_bool = static_cast<bool>(std::stoi(str));
        break;
    case TypeUsed::NOT_SET:
        break;
    }

    return any;
}

#endif

// Format: <Value>:<TypeUsed>\n
void AnyType::print() const
{
#if defined CANRED
    switch (m_type) {
    case TypeUsed::i8:
        fmt::print("{}:", i8);
        break;
    case TypeUsed::i16:
        fmt::print("{}:", i16);
        break;
    case TypeUsed::i32:
        fmt::print("{}:", i32);
        break;
    case TypeUsed::i64:
        fmt::print("{}:", i64);
        break;
    case TypeUsed::f_float:
        fmt::print("{}:", f_float);
        break;
    case TypeUsed::d_double:
        fmt::print("{}:", d_double);
        break;
    case TypeUsed::b_bool:
        fmt::print("{}:", b_bool);
        break;
    case TypeUsed::NOT_SET:
        fmt::print("{}:", "NOT_SET");
        break;
    }

    print_type_used(m_type);

    fmt::print("\n");
#else
    switch (m_type) {
    case TypeUsed::i8:
        DEBUG_PRINT(i8);
        break;
    case TypeUsed::i16:
        DEBUG_PRINT(i16);
        break;
    case TypeUsed::i32:
        DEBUG_PRINT(i32);
        break;
    case TypeUsed::i64:
        DEBUG_PRINT(i64);
        break;
    case TypeUsed::f_float:
        DEBUG_PRINT(f_float);
        break;
    case TypeUsed::d_double:
        DEBUG_PRINT(d_double);
        break;
    case TypeUsed::b_bool:
        DEBUG_PRINT(b_bool);
        break;
    case TypeUsed::NOT_SET:
        DEBUG_PRINT("NOT_SET");
        break;
    }

    DEBUG_PRINT(":");
    print_type_used(m_type);
    DEBUG_PRINTLN("\n");
#endif
}

void AnyType::set_value(int8_t value)
{
    m_type = TypeUsed::i8;
    i8 = value;
}
void AnyType::set_value(int16_t value)
{
    m_type = TypeUsed::i16;
    i16 = value;
}
void AnyType::set_value(int32_t value)
{
    m_type = TypeUsed::i32;
    i32 = value;
}
void AnyType::set_value(int64_t value)
{
    m_type = TypeUsed::i64;
    i64 = value;
}
void AnyType::set_value(float value)
{
    m_type = TypeUsed::f_float;
    f_float = value;
}
void AnyType::set_value(double value)
{
    m_type = TypeUsed::d_double;
    d_double = value;
}
void AnyType::set_value(bool value)
{
    m_type = TypeUsed::b_bool;
    b_bool = value;
}