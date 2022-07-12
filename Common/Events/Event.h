#pragma once

#include <AnyType.h>
#include <EventConstants.h>
#include <stdint.h>

#ifdef CANRED
#    include <Database.h>
#    include <json.hpp>
#    include <string>
#else
#    include <Log.hpp>
#endif

class Event {
public:
    Event(uint32_t offset) = delete;

    Event() = default;
    ~Event() = default;

#ifdef CANRED
    void set_condition(const std::string& value);
    void set_interval_unit(const std::string& value);
    static Event from_json(const nlohmann::json& json);
    void print() const;

    uint16_t module_uid { 0 };
    std::string flow_name { "" };

    std::vector<uint8_t> serialize() const;
#endif
    size_t serialize(uint8_t buffer[]) const;
    static Event from_buffer(uint8_t event_blob[]);

    size_t serialized_size() const;
    static size_t serialized_size(const uint8_t event_blob[]);

    union {
        uint8_t next_section; // Command Only
        uint8_t if_false;     // If Only
        uint8_t interval;     // Event Only
    };

    union {
        IntervalUnit interval_unit; // Command Only
        uint8_t if_true;            // IF Only
    };

    uint8_t section_number { 0 };
    uint8_t flow_id { 0 };
    uint8_t this_function_id { 0 };

    Conditional conditional { Conditional::NOT_SET };

    AnyType value_to_check;

    EventType event_type { EventType::NOT_SET };

    static constexpr uint8_t MAIN_MAX_SIZE = 5 + 8;
    static constexpr uint8_t IF_MAX_SIZE = 6 + 8;
    static constexpr uint8_t COMMAND_MAX_SIZE = 5;
    // TODO: Rename this
    static constexpr uint8_t MAX_SIZE = 6 + 8;

    // returns the max size based on event_type
    uint8_t max_size() const;
};

// Only used inside this file.
constexpr uint8_t EVENT_TYPE_BITMAP = 192;    // 0b11000000
constexpr uint8_t CONDITIONAL_BITMAP = 56;    // 0b00111000
constexpr uint8_t VALUE_TYPE_BITMAP = 7;      // 0b00000111
constexpr uint8_t INTERVAL_UNIT_BITMAP = 192; // 0b11000000

constexpr uint8_t EVENT_MIN_SIZE_MAIN = 5;
constexpr uint8_t EVENT_MIN_SIZE_IF = 6;
constexpr uint8_t EVENT_MIN_SIZE_COMMAND = 5;

/* Main Event Format: */
// Minimum Size: 5 Bytes + 1 Byte
// Maximum Size: 5 Bytes + 8 Bytes
// Format:
//  Event Type | Conditional | ValueToCheckType
// byte[0]: 00 | 000 | 000
// byte[1]: <Flow ID>
// byte[2]: <Function ID>
// Interval Unit | Unused Bits
// byte[3]: 00 | ------
// byte[4]: <Interval>
// byte[5..n]: <Value to check>

/* IF Event Format: */
// Minimum Size: 6 Bytes + 1 Byte
// Maximum Size: 6 Bytes + 8 Bytes
// Format:
//  Event Type | Conditional | ValueToCheckType
// byte[0]: 00 | 000 | 000
// byte[1]: <Flow ID>
// byte[2]: <Function ID>
// byte[3]: <Section Number>
// byte[4]: <True Section Number>
// byte[5]: <False Section Number>
// byte[6..n]: <Value to check>

/* Command Event Format: */
// Size: 5 Bytes
// Format:
//  Event Type | Unused Bits
// byte[0]: 00 | ------
// byte[1]: <Flow ID>
// byte[2]: <Function ID>
// byte[3]: <Section number>
// byte[4]: <Next Section Number>
