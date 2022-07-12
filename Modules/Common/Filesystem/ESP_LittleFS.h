#pragma once

#include <FilesystemInterface.h>
#include <stdint.h>

#if defined ESP8266
#    include <LittleFS.h>
#elif defined ESP32
#    include <LittleFS.h>
#else
#    error "ESP_LittleFS.h -> ESP32 || ESP8266 Not Defined!"
#endif

class ESP_LittleFS : public FilesystemInterface {

public:
    ESP_LittleFS() = default;
    ~ESP_LittleFS() = default;

    bool store_uid(uint16_t module_uid) override;
    uint16_t load_uid() override;

    bool store_event(const uint8_t event_blob[]) override;
    bool remove_event(const uint8_t event_blob[]) override;
    bool remove_all_events() override;

    bool load_events(MainEvent main_event_buffer[], Event child_event_buffer[]) override;
    uint16_t read_event_file(uint8_t buffer[], uint8_t size, uint16_t offset) override;

    bool format() override;

    uint8_t main_event_count() override { return m_main_event_count; }
    uint8_t child_event_count() override { return m_child_event_count; }

private:
    bool init_filesystem(bool force = false);
    bool seek_curser_to_next_event(File& file, bool return_first_event = false);

    uint8_t m_main_event_count = { 0 };
    uint8_t m_child_event_count = { 0 };

    const char* CAN_UID_FILE = "/can_uid";
    const char* EVENTS_FILE = "/events/all_events";

    const char* LITTLEFS_FILE_READ_WRITE = "r+";
    const char* LITTLEFS_FILE_APPEND = "a";
    const char* LITTLEFS_FILE_CREATE_OR_TRUNCATE = "w";
};
