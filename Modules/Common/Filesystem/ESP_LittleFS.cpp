#include "ESP_LittleFS.h"

#include <Log.hpp>
#include <print_u8_array.h>

#define START_LittleFS_IF_NEEDED()                   \
    if (!init_filesystem()) {                        \
        DEBUG_PRINT("FAILED TO START LittleFS IN "); \
        DEBUG_PRINTLN(__FUNCTION__);                 \
        return false;                                \
    }

bool ESP_LittleFS::init_filesystem(bool force /* false */)
{
    static bool is_littlefs_started = false;

    if (!force && is_littlefs_started) {
        return true;
    }

#if defined ESP32
    if (!LittleFS.begin(true)) {
#elif defined ESP8266
    if (!LittleFS.begin()) {
#endif
        DEBUG_PRINTLN("LittleFS failed to start!");
        return false;
    }

    if (!LittleFS.exists("/events")) {

        if (!LittleFS.mkdir("/events")) {
            DEBUG_PRINTLN("LittleFS failed to create dir!");
            return false;
        }
    }

    if (!LittleFS.exists(EVENTS_FILE)) {
        File file = LittleFS.open(EVENTS_FILE, LITTLEFS_FILE_CREATE_OR_TRUNCATE);
        file.close();
    }

    is_littlefs_started = true;
    return is_littlefs_started;
}

bool ESP_LittleFS::format()
{
    START_LittleFS_IF_NEEDED();

    bool did_format = LittleFS.format();
    init_filesystem(true);
    return did_format;
}

uint16_t ESP_LittleFS::load_uid()
{
    START_LittleFS_IF_NEEDED();

    uint8_t uid_buffer[2];

    File file = LittleFS.open(CAN_UID_FILE, LITTLEFS_FILE_READ_WRITE);
    file.read(uid_buffer, 2);
    file.close();

    uint16_t uid = 0;

    memcpy(&uid, uid_buffer, 2);

    return uid;
}

bool ESP_LittleFS::store_uid(uint16_t uid)
{
    START_LittleFS_IF_NEEDED();

    uint8_t uid_buffer[2];
    memcpy(uid_buffer, &uid, 2);

    File file = LittleFS.open(CAN_UID_FILE, LITTLEFS_FILE_CREATE_OR_TRUNCATE);
    auto bytes_written = file.write(uid_buffer, 2);
    file.close();

    return (bytes_written == 2);
}

bool ESP_LittleFS::seek_curser_to_next_event(File& file, bool return_first_event /* false */)
{
    // Given a file, seek until we find an event
    // Return true if we found another event.
    // Return false if we did not find any more events.

    uint8_t buffer[1] = { 0 };
    size_t curser = file.position();
    bool already_past_the_first_element = false;

    while (curser < file.size()) {
        file.seek(curser, SeekSet);
        file.read(buffer, 1);
        file.seek(curser, SeekSet);

        if (buffer[0] == 0) {
            curser += 1;
            continue;
        }

        if (return_first_event || already_past_the_first_element) {
            return true;
        }

        already_past_the_first_element = true;

        auto event_type = get_event_type_from_byte(*buffer);

        if (event_type == EventType::Command) {
            curser += 5;
            continue;
        }

        auto value_type = get_value_type_from_byte(*buffer);
        auto value_type_size = type_used_size(value_type);

        if (event_type == EventType::Main) {
            curser += 5 + value_type_size;
            continue;
        }

        if (event_type == EventType::If) {
            curser += 6 + value_type_size;
            continue;
        }

        return true;
    }

    // Did not find another event.
    return false;
}

bool ESP_LittleFS::store_event(const uint8_t event_blob[])
{

    START_LittleFS_IF_NEEDED();

    File file = LittleFS.open(EVENTS_FILE, LITTLEFS_FILE_APPEND);

    const size_t event_blob_size = Event::serialized_size(event_blob);

    auto bytes_written = file.write(event_blob, event_blob_size);

    if (bytes_written != event_blob_size) {
        DEBUG_PRINTLN("store event: bytes_written != event_blob_size");
        DEBUG_PRINT("Bytes_written: ");
        DEBUG_PRINTLN(bytes_written);
        DEBUG_PRINT("event_blob_size: ");
        DEBUG_PRINTLN(event_blob_size);
    }

    file.close();
    return (bytes_written == event_blob_size);
}

bool ESP_LittleFS::remove_event(const uint8_t event_blob[])
{
    // TODO:
    // This function pretty much always returns true.

    START_LittleFS_IF_NEEDED();

    File file = LittleFS.open(EVENTS_FILE, LITTLEFS_FILE_READ_WRITE);

    uint8_t event_blob_size = Event::serialized_size(event_blob);

    if (file.size() == 0 || file.size() < event_blob_size) {
        // We were told to delete a file that can't possibly be stored.
        file.close();
        return true;
    }

    uint8_t buffer_byte[1] = { 0 };

    bool return_first_event = true;

    for (;;) {

        // Seek through the file, trying to find the same byte
        // as event_blob[0]

        bool did_find_event = seek_curser_to_next_event(file, return_first_event);

        size_t curser = file.position();

        return_first_event = false;

        if (!did_find_event) {
            // We did not find another event,
            // and did not find the event we wanted
            // This means we're not storing the event, either way.
            break;
        }

        file.read(buffer_byte, 1);
        file.seek(curser, SeekSet);

        if (*buffer_byte == event_blob[0]) {
            // we might have found the right position.
            if (file.position() + event_blob_size > file.size()) {
                // There's not enough bytes left for this event
                // to be stored.
                break;
            }

            uint8_t buffer[Event::MAX_SIZE];

            file.read(buffer, event_blob_size);
            file.seek(curser, SeekSet);

            for (size_t i = 1; i < event_blob_size; i += 1) {
                if (event_blob[i] != buffer[i]) {
                    // Did not find the correct location.
                    continue;
                }
            }

            // We found this correct event position.
            // Zero out the buffer, and use that to zero
            // out the event on disk.
            memset(buffer, 0, Event::MAX_SIZE);
            file.write(buffer, event_blob_size);

            file.close();
            return true;
        }
    }

    // We have read through the entire file, and could not find the event.
    // So it's (probably) deleted.
    file.close();
    return true;
}

bool ESP_LittleFS::remove_all_events()
{
    START_LittleFS_IF_NEEDED();

    LittleFS.remove(EVENTS_FILE);
    File file = LittleFS.open(EVENTS_FILE, LITTLEFS_FILE_CREATE_OR_TRUNCATE);

    file.close();
    return true;
}

bool ESP_LittleFS::load_events(MainEvent main_event_buffer[], Event child_event_buffer[])
{
    START_LittleFS_IF_NEEDED();

    File file = LittleFS.open(EVENTS_FILE, LITTLEFS_FILE_READ_WRITE);

    uint8_t buffer_byte[1] = { 0 };
    uint8_t child_event_index = 0;
    uint8_t main_event_index = 0;
    uint8_t event_buffer[Event::MAX_SIZE];

    bool return_first_event = true;

    for (;;) {

        bool did_find_event = seek_curser_to_next_event(file, return_first_event);
        return_first_event = false;

        if (!did_find_event) {
            // We did not find an another event
            // we have loaded all the main events we can.
            break;
        }

        size_t curser = file.position();
        file.read(buffer_byte, 1);
        file.seek(curser, SeekSet);

        auto event_type = get_event_type_from_byte(*buffer_byte);

        if (event_type == EventType::Command) {
            file.read(event_buffer, 5);
            file.seek(curser, SeekSet);

            child_event_buffer[child_event_index++] = Event::from_buffer(event_buffer);
            continue;
        }

        auto value_type = get_value_type_from_byte(*buffer_byte);
        auto value_type_size = type_used_size(value_type);

        if (event_type == EventType::Main) {

            file.read(event_buffer, 5 + value_type_size);
            file.seek(curser, SeekSet);

            main_event_buffer[main_event_index++] = { 0, Event::from_buffer(event_buffer) };
            continue;
        }

        if (event_type == EventType::If) {

            file.read(event_buffer, 6 + value_type_size);
            file.seek(curser, SeekSet);
            child_event_buffer[child_event_index++] = Event::from_buffer(event_buffer);
            continue;
        }
    }

    file.close();

    DEBUG_PRINT("We have loaded ");
    DEBUG_PRINT(main_event_index);
    DEBUG_PRINT(" main events, and ");
    DEBUG_PRINT(child_event_index);
    DEBUG_PRINTLN(" child events!");

    // TODO: Use this instead of the indexes.
    m_main_event_count = main_event_index;
    m_child_event_count = child_event_index;

    return (main_event_index > 0 || child_event_index > 0);
}

uint16_t ESP_LittleFS::read_event_file(uint8_t buffer[], uint8_t size, uint16_t offset /* 0 */)
{
    File file = LittleFS.open(EVENTS_FILE, LITTLEFS_FILE_READ_WRITE);

    file.seek(offset, SeekSet);

    auto bytes_read = file.read(buffer, size);

    file.close();

    return bytes_read;
}