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
#include <stdint.h>
#include <FilesystemInterface.h>

// TODO: Not sure if sharing this libary is the best idea
#include <Event.h>

// Max number of long frame groups we can handle at a time
const uint8_t MAX_LONG_FRAME_GROUPS = 4;

// Max number of bytes we can handle per long frame group
const uint8_t MAX_BYTES_PER_LF_GROUP = 64;

const uint8_t AMOUNT_OF_COMMANDS = 4; // TODO: Not This

// Defines the number of possible ACKs we can cache at once.
const uint8_t MAX_ACK_BUFFERS = 12;

// Defines the amount of extra uids we can handle.
const uint8_t AMOUNT_OF_EXTRA_UIDS_TO_HANDLE = 2;

// Defines the amount of main_events we can store
const uint8_t AMOUNT_OF_MAIN_EVENTS = 3;

// Defines the amount of child events we can store
const uint8_t AMOUNT_OF_CHILD_EVENTS = 8;

// Max amount of drivers to use.
const uint8_t MAX_DRIVERS_TO_STORE = 2;

using CommandFunction = AnyType (*)(const void*);

enum class TranslationMode : uint8_t {
    Bidirectional,
    Unidirectional,
};

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

    // TODO: Functions for specific drivers.
    void send_buffer_to_every_interface(uint16_t to_id, uint8_t data[], uint8_t data_size, uint8_t priority = CAN::Priority::NORMAL);
    void send_frame_to_every_interface(const CAN::Frame& frame);

    void send_error_frame(uint16_t to_id, uint8_t which_error);
    // TODO: We should remove this specialization, 
    //       and abstract away any specific libraries 
    void send_raw_canframe(can_frame frame);

    // ACK
    void add_waiting_on_ack(uint16_t module_uid, uint8_t command_id);
    void remove_waiting_on_ack(uint16_t module_uid, uint8_t command_id);
    void send_ack(uint16_t module_uid, uint8_t command_id);
    void check_for_old_acks(uint64_t current_time);

    uint32_t m_ack_buffer[MAX_ACK_BUFFERS * 2];

    // CAN Helpers
    void ask_for_new_id();
    void send_check_in_frame();
    void send_stored_configuration();
    void send_stored_events();

    // Config File
    const char* m_config_file;

    uint16_t m_current_node_uid { 0 };

    // Filesystem interface
    FilesystemInterface* m_filesystem { nullptr };

    struct Command {
        uint8_t id;
        CommandFunction func;
    };

    // Long frames
    void clear_long_frame_buffer(uint8_t buffer[]);
    // TODO: This should be improved.
    uint8_t get_long_frame_uid() const { return rand() % 255; };
    uint8_t find_uid_index(uint8_t uid) const;
    uint8_t find_free_long_frame_group() const;

    uint8_t m_long_frame_buffer[MAX_LONG_FRAME_GROUPS][MAX_BYTES_PER_LF_GROUP];
    uint8_t m_LF_group_index[MAX_LONG_FRAME_GROUPS];

    // Command Functions
    bool external_call_command_function(uint16_t from_id, uint8_t command_id, const void* input_data);
    AnyType internal_call_command_function(uint8_t command_id) const;

    Command m_command_buffer[AMOUNT_OF_COMMANDS]; // TODO: Automagically Compute This
    uint8_t m_command_buffer_index { 0 };

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

    // Interfaces
    AutomatoInterface* m_interfaces[MAX_DRIVERS_TO_STORE];
    uint8_t m_interfaces_index { 0 };

    struct TranslationLayer {
        AutomatoInterface* from_driver;
        AutomatoInterface* to_driver;
    };

    TranslationLayer m_translation_layers[2];
    uint8_t m_translation_layers_index { 0 };

    // Not Yet Implemented
    bool m_CAN_is_locked { false };
    bool m_use_builtin_led { true };
};