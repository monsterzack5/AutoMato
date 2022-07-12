#pragma once

#include <AnyType.h>
#include <cstring>
#include <iostream>
#include <stdint.h>

#if defined CANRED
#    include <fmt/format.h>
#else
#    include <Log.hpp>
#endif

enum class IntervalUnit : uint8_t {
    MILLISECONDS = 0, // 0b00
    SECONDS = 1,      // 0b01
    MINUTES = 2,      // 0b10
    HOURS = 3,        // 0b11
    NOT_SET = 4,
};

enum class EventType : uint8_t {
    Main = 0,    // 0b00
    Command = 1, // 0b01
    If = 2,      // 0b10
    NOT_SET = 3, // 0b11
};

#ifndef CANRED
inline decltype(millis()) interval_to_milliseconds(const IntervalUnit interval_unit, uint8_t interval)
{

    if (interval_unit == IntervalUnit::NOT_SET) {
        DEBUG_PRINTLN("Error! IntervalUnit::NOT_SET Passed to interval_to_milliseconds!");
        // 1 minute as a sane default.
        return 60000;
    }

    if (interval_unit == IntervalUnit::HOURS) {
        return interval * 60 * 60 * 1000;
    }

    if (interval_unit == IntervalUnit::MINUTES) {
        return interval * 60 * 1000;
    }

    if (interval_unit == IntervalUnit::SECONDS) {
        return interval * 1000;
    }

    return interval;
}
#endif

#ifdef CANRED

inline Conditional to_conditional(const std::string& input)
{

    if (input == "<") {
        return Conditional::LESS_THAN;
    }
    if (input == "<") {
        return Conditional::LESS_THAN;
    }
    if (input == ">") {
        return Conditional::GREATER_THAN;
    }
    if (input == "==") {
        return Conditional::EQUAL;
    }
    if (input == "!=") {
        return Conditional::NOT_EQUAL;
    }
    if (input == "<=") {
        return Conditional::LESS_THAN_OR_EQUAL;
    }
    if (input == ">=") {
        return Conditional::GREATER_THAN_OR_EQUAL;
    }

    return Conditional::NOT_SET;
}

inline IntervalUnit to_interval_unit(const std::string& input)
{
    if (input == "milliseconds") {
        return IntervalUnit::MILLISECONDS;
    }
    if (input == "seconds") {
        return IntervalUnit::SECONDS;
    }
    if (input == "minutes") {
        return IntervalUnit::MINUTES;
    }
    if (input == "hours") {
        return IntervalUnit::HOURS;
    }

    return IntervalUnit::NOT_SET;
}

inline void print_conditional(const Conditional cond)
{
    switch (cond) {
    case Conditional::LESS_THAN:
        fmt::print("<");
        break;
    case Conditional::GREATER_THAN:
        fmt::print(">");
        break;
    case Conditional::EQUAL:
        fmt::print("==");
        break;
    case Conditional::NOT_EQUAL:
        fmt::print("!=");
        break;
    case Conditional::GREATER_THAN_OR_EQUAL:
        fmt::print(">=");
        break;
    case Conditional::LESS_THAN_OR_EQUAL:
        fmt::print("<=");
        break;
    case Conditional::NOT_SET:
        fmt::print("!?");
        break;
    }
}

inline void print_interval_unit(const IntervalUnit unit)
{
    switch (unit) {
    case IntervalUnit::MILLISECONDS:
        fmt::print("milliseconds");
        break;
    case IntervalUnit::SECONDS:
        fmt::print("seconds");
        break;
    case IntervalUnit::MINUTES:
        fmt::print("minutes");
        break;
    case IntervalUnit::HOURS:
        fmt::print("hours");
        break;
    case IntervalUnit::NOT_SET:
        fmt::print("NOT_SET");
        break;
    }
}

#endif