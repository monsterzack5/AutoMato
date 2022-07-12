#pragma once

#include <Event.h>

struct MainEvent {
    decltype(millis()) time_last_ran;
    Event event;
};

class FilesystemInterface {
public:
    // Store the modules 11 bit uid, return true if successful
    virtual bool store_uid(uint16_t module_uid) = 0;
    // Load the modules uid, and return it. Return 0 if not stored.
    virtual uint16_t load_uid() = 0;

    // Store an event on disk from an event blob.
    virtual bool store_event(const uint8_t event_blob[]) = 0;
    // Remove a (possibly) stored event.
    virtual bool remove_event(const uint8_t event_blob[]) = 0;
    // Delete all stored events.
    virtual bool remove_all_events() = 0;

    // Load main and child events into the given buffers.
    virtual bool load_events(MainEvent main_event_buffer[], Event child_event_buffer[]) = 0;

    virtual uint8_t main_event_count() = 0;
    virtual uint8_t child_event_count() = 0;

    // raw read `size` number of bytes of raw event data into the `buffer`, at the given offset.
    // Return the number of bytes read, 0 if none are read.
    virtual uint16_t read_event_file(uint8_t buffer[], uint8_t size, uint16_t offset) = 0;

    // Format and clean entire storage medium.
    virtual bool format() = 0;
};

// TODO: These should not be in this file.
inline TypeUsed get_value_type_from_byte(uint8_t byte)
{
    return static_cast<TypeUsed>(byte & VALUE_TYPE_BITMAP);
}

inline EventType get_event_type_from_byte(uint8_t byte)
{
    return static_cast<EventType>((byte & EVENT_TYPE_BITMAP) >> 6);
}