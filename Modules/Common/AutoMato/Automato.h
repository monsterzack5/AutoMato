#pragma once

// TODO:
// This file is doing a million things, and needs to be split up,
// it has been kept in it's current massive state mainly for rapid
// development purposes.
// Here's how I believe we should split up this class:
// - Convert ACKHandler functions into a separate class
// - Convert LongFrameHandler functions into a separate class
// - Convert translation functions into namespace functions,
//   in a different file
// - Move all of the configuration options to a Config header file
// - Convert Event functions into namespace functions

// TODO: Optimize how we handle translation layers.
// TODO: Verify .translate_between_interfaces input.
//       - No Duplicate translations
//       - No duplicate from's?

#include <AnyType.h>
#include <AutomatoInterface.h>
#include <CanFrame.h>
#include <CommandHandler.h>
#include <FilesystemInterface.h>
#include <InterfaceHandler.h>
#include <LongFrameHandler.h>
#include <stdint.h>

// TODO: Not sure if sharing this libary is the best idea
#include <Event.h>

// Defines the amount of extra uids we can handle.
const uint8_t AMOUNT_OF_EXTRA_UIDS_TO_HANDLE = 2;

// Defines the amount of main_events we can store
const uint8_t AMOUNT_OF_MAIN_EVENTS = 3;

// Defines the amount of child events we can store
const uint8_t AMOUNT_OF_CHILD_EVENTS = 8;

class Automato {
public:
    explicit Automato(const char* config_file, uint16_t hardcoded_uid = 0, FilesystemInterface* fs_interface = nullptr) noexcept;

    ~Automato() = default;

    void loop();
    void setup();
    // TODO: This is not currently implemented
    void dont_use_builtin_led() { m_use_builtin_led = false; }
    void register_callback(uint8_t command_id, const CommandFunction& function);

    // Advanced functions
    void add_interface(AutomatoInterface* interface);
    void translate_between_interfaces(AutomatoInterface* interface1, AutomatoInterface* interface2, TranslationMode mode);

private:
    /* CAN */
    // Frame handling
    void handle_frame(const CAN::Frame& frame);
    void parse_frame(uint16_t from_id, const uint8_t data[], uint16_t can_dlc);

    // Interfaces with all interfaces
    InterfaceHandler interfacer;

    // CAN Helpers
    void ask_for_new_id();
    void send_check_in_frame();
    void send_stored_configuration();
    void send_stored_events();

    // Config File
    const char* m_config_file;

    // Filesystem interface
    FilesystemInterface* m_filesystem { nullptr };

    // Long frames
    LongFrameHandler long_frame_handler;

    // Command Functions
    CommandHandler command_handler;
    bool external_run_command(uint16_t from_id, uint8_t command_id, const void* input_data);

    // Broadcast Events
    // TODO: This is in a state of flux.
    void check_main_events();
    void event_run_next_part(uint8_t flow_id, uint8_t section_number);
    uint8_t run_child_event(Event event);
    bool is_child_event_for_me(uint8_t flow_id, uint8_t section_number) const;

    void reload_event_buffers();

    MainEvent m_main_events[AMOUNT_OF_MAIN_EVENTS];
    uint8_t m_main_events_index { 0 };

    Event m_child_events[AMOUNT_OF_CHILD_EVENTS];
    uint8_t m_child_events_index { 0 };

    // Not Yet Implemented
    bool m_CAN_is_locked { false };
    bool m_use_builtin_led { true };
};