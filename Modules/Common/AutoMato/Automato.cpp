#include "Automato.h"
#include <CanFrame.h>
#include <CanSerializer.h>
#include <Log.hpp>
#include <seconds_to_ms.h>
#include <string.h> // For memcpy only!
#include <yield_if_needed.h>

#if defined ESP32 || defined ESP8266
#    include <ESP_LittleFS.h>
#endif

Automato::Automato(const char* config_file, uint16_t hardcoded_uid /* 0 */, FilesystemInterface* fs_interface /* nullptr */) noexcept
    : m_config_file(config_file)
    , m_filesystem(fs_interface)
{
    // TODO: If not given a filesystem interface, try and pick the best one.

    if (hardcoded_uid) {
        interfacer.set_current_node_uid(hardcoded_uid);
    }

    if (!fs_interface) {
#if defined ESP32 || defined ESP8266
        static ESP_LittleFS esp_fs;
        m_filesystem = &esp_fs;
#else
        DEBUG_PRINTLN("Could not find the correct filesystem implementation!");
#endif
    }
}

void Automato::setup()
{

    reload_event_buffers();

    YIELD_IF_NEEDED();

    if (interfacer.current_node_uid() > 0) {
        // We have a hardcoded UID
        return;
    }

    auto uid_from_fs = m_filesystem->load_uid();

    if (uid_from_fs > 0) {
        interfacer.set_current_node_uid(uid_from_fs);
        return;
    }

    // We don't have a hardcoded uid, and
    // we don't have one stored in the FS
    // Let's ask the MCM for one

    // We still need a uid to ask the MCM for one
    auto random_uid = CAN::Frame::get_temporary_can_id();

    interfacer.set_current_node_uid(random_uid);

    send_check_in_frame();
    ask_for_new_id();
}

void Automato::loop()
{
    YIELD_IF_NEEDED();

    static CAN::Frame frame(CAN::ID(0), nullptr, 0);

    check_main_events();

    if (!interfacer.try_read_frame(&frame)) {
        // We didn't read a frame, return so we can
        // give control flow back to the main loop() body.
        return;
    }

    handle_frame(frame);
}

void Automato::handle_frame(const CAN::Frame& frame)
{
    YIELD_IF_NEEDED();
    // We don't need to do any special parsing for
    // standard size frames
    if (!frame.is_long_frame) {
        parse_frame(frame.from_id, frame.data, frame.can_dlc);
        return;
    }

    uint8_t long_frame_index = long_frame_handler.append(frame.data, frame.can_dlc);

    if (frame.can_dlc == 8) {
        // There's more frames in this group
        return;
    }

    const uint8_t* long_frame_data_ptr = long_frame_handler.get_data_ptr(long_frame_index);
    uint8_t long_frame_length = long_frame_handler.length(long_frame_index);

    parse_frame(frame.from_id, long_frame_data_ptr, long_frame_length);
    long_frame_handler.clear_data(long_frame_index);
}

void Automato::parse_frame(uint16_t from_id, const uint8_t data[], uint16_t can_dlc)
{
    YIELD_IF_NEEDED();

    DEBUG_PRINT("New Frame! data[0]: ");
    DEBUG_PRINTLN(data[0]);

    // CAN::Protocol::ACKNOWLEDGEMENT is handled by handle_frame()

    switch (data[0]) {
    case CAN::Protocol::REPLY_NEW_UID: {
        uint16_t new_uid = 0;

        memcpy(&new_uid, data, 2);
        m_filesystem->store_uid(new_uid);
        interfacer.set_current_node_uid(new_uid);
        break;
    }

    case CAN::Protocol::COMMAND: {
        // We're told to run a command!

        external_call_command_function(from_id, data[1], nullptr);
        break;
    }

    case CAN::Protocol::COMMAND_INPUT: {

        // Converting this to void* does not seem like the best,
        // but I am unsure of the proper way to pass random bytes
        // to an unknown user function as the correct type.

        const void* data_ptr = &data[2];
        external_call_command_function(from_id, data[1], data_ptr);
        break;
    }

    case CAN::Protocol::UPDATE_INFO: {
        send_stored_configuration();
        break;
    }

    case CAN::Protocol::EVENT_ADD: {
        // the MCM has told us to add a new event!
        DEBUG_PRINTLN("MCM Said to add an event!");
        m_filesystem->store_event(&data[1]);
        reload_event_buffers();
        break;
    }

    case CAN::Protocol::EVENT_REMOVE: {
        m_filesystem->remove_event(&data[1]);
        DEBUG_PRINTLN("MCM Said to remove an event!");
        reload_event_buffers();
        break;
    }

    case CAN::Protocol::EVENT_REMOVE_ALL: {
        // Remove all events!
        DEBUG_PRINTLN("MCM Said to remove all events!");
        m_filesystem->remove_all_events();
        reload_event_buffers();
        break;
    }

    case CAN::Protocol::EVENT_RUN_NEXT_PART: {
        // Someone, somewhere told us to run the next
        // part of an event flow.
        DEBUG_PRINTLN("Running the next part of an event!");
        event_run_next_part(data[1], data[2]);
        break;
    }

    case CAN::Protocol::EVENT_SEND_STORED: {
        // Send our stored events.
        DEBUG_PRINTLN("MCM asked us to send our stored events!");
        send_stored_events();
        break;
    }

    case CAN::Protocol::ERROR_GENERIC: {
        DEBUG_PRINT("We have received a generic error from: ");
        DEBUG_PRINT(from_id);
        DEBUG_PRINT(" with the error: ");
        DEBUG_PRINTLN(data[0]);

        break;
    }

    case CAN::Protocol::FORMAT_EEPROM: {
        DEBUG_PRINTLN("MCM Asked us to format our EEPROM!");
        m_filesystem->format();
        reload_event_buffers();
        break;
    }

    default: {
        DEBUG_PRINTLN("Reached default case in handle_frame!");
        uint8_t buffer[2];

        buffer[0] = CAN::Protocol::ERROR_GENERIC;
        buffer[1] = CAN::GENERIC_ERROR::PROTOCOL_NOT_SUPPORTED;

        interfacer.send_buffer_to_every_interface(from_id, buffer, 2);
        break;
    }
    }
}

void Automato::send_stored_configuration()
{
    // TODO: This function does not wait for ACKs
    //       it should, and only send a frame after
    //       we get an ACK for the last one.
    //       Even so, after multiple attemps
    //       This function has proved extremely
    //       reliable.

    YIELD_IF_NEEDED();

    DEBUG_PRINTLN("Sending stored config!");
    m_CAN_is_locked = true;

    uint8_t long_frame_uid = CAN::Frame::get_long_frame_uid();

    CAN::ID id(interfacer.current_node_uid(), CAN::UID::MCM, CAN::Priority::MEDIUM, CAN::FrameFormat::Long);

    uint8_t buffer[8];

    CAN::Frame frame(id, buffer, 8);

    frame.data[0] = long_frame_uid;                   // Constant
    frame.data[1] = CAN::Protocol::REPLY_UPDATE_INFO; // First run only

    auto config_length = strlen_P(m_config_file);
    uint16_t bytes_sent = 0;

    // send the first frame here, since it is only 6 bytes
    for (uint8_t i = 0; i < 6; i += 1) {
        frame.data[i + 2] = pgm_read_byte(&m_config_file[i]);
        bytes_sent += 1;
    }

    interfacer.send_frame_to_every_interface(frame);

    for (;;) {
        YIELD_IF_NEEDED();
        if (bytes_sent <= (config_length + 7)) {
            for (uint8_t i = 0; i < 7; i += 1) {
                frame.data[i + 1] = pgm_read_byte(&m_config_file[bytes_sent]);
                bytes_sent += 1;
            }
            interfacer.send_frame_to_every_interface(frame);
            continue;
        }

        // if we can't send 7, but still need to send some
        uint8_t index = 1;
        for (; bytes_sent < config_length; bytes_sent += 1) {
            frame.data[index] = pgm_read_byte(&m_config_file[bytes_sent]);
            index += 1;
        }

        frame.can_dlc = index;
        interfacer.send_frame_to_every_interface(frame);
        break;
    }

    // Check to see if we need to send another frame
    if (frame.can_dlc == 8) {
        // send empty frame
        interfacer.send_buffer_to_every_interface(CAN::UID::MCM, nullptr, 0);
    }

    DEBUG_PRINTLN("Done sending!");

    m_CAN_is_locked = false;
}

void Automato::send_check_in_frame()
{
    // This function enters a holding pattern where
    // it will send a check in frame every second
    // until the MCM replies with ACK

    uint8_t buffer[1];
    buffer[0] = CAN::Protocol::CHECK_IN;

    auto time_last_sent = millis();
    const uint16_t ms_between_sends = 1000;

    CAN::Frame frame(CAN::ID(0), nullptr, 0);

    for (;;) {
        YIELD_IF_NEEDED();
        auto current_time = millis();
        if (current_time > time_last_sent + ms_between_sends) {
            time_last_sent = current_time;
            interfacer.send_buffer_to_every_interface(CAN::UID::MCM, buffer, 1, CAN::Priority::MEDIUM);
            DEBUG_PRINTLN("Screaming into the void");
        }

        bool did_read_frame = interfacer.try_read_frame(&frame);

        if (did_read_frame && frame.is_only_for_me(interfacer.current_node_uid())) {
            DEBUG_PRINTLN("MCM Said we're good to go!");
            return;
        }
    }
}

void Automato::register_callback(uint8_t command_id, const CommandFunction& function)
{
    m_command_buffer[m_command_buffer_index++] = { command_id, function };
}

AnyType Automato::internal_call_command_function(uint8_t command_id) const
{
    uint8_t command_index = CAN::NOT_FOUND;
    for (uint8_t i = 0; i < m_command_buffer_index; i += 1) {
        if (m_command_buffer[i].id == command_id) {
            command_index = i;
            break;
        }
    }

    if (command_index == CAN::NOT_FOUND) {
        // This happens when we're storing Events
        // that are out of date with the actual code
        // of the module.
        // TODO: We should report this and update
        // all of our saved events.
        DEBUG_PRINTLN("ERROR: internal_call_command_function: command ID not found.");
        return {};
    }

    // TODO: Don't use this on functions that take input
    AnyType data = m_command_buffer[command_index].func(nullptr);

    return data;
}

void Automato::check_main_events()
{
    auto current_time = millis();
    for (uint8_t i = 0; i < m_main_events_index; i += 1) {

        auto& main = m_main_events[i];
        auto milliseconds_to_wait = interval_to_milliseconds(main.event.interval_unit, main.event.interval);

        if (current_time - main.time_last_ran < milliseconds_to_wait) {
            continue;
        }

        main.time_last_ran = current_time;

        YIELD_IF_NEEDED();

        AnyType function_output = internal_call_command_function(main.event.this_function_id);
        AnyType value_to_compare = main.event.value_to_check;

        if (compare_better_any_type(main.event.conditional, function_output, value_to_compare)) {
            // The function matches the event criteria, run the next section number.

            if (is_child_event_for_me(main.event.flow_id, 1)) {
                // it is for me, call event run next part.
                event_run_next_part(main.event.flow_id, 1);
                continue;
            }

            uint8_t buffer[3];
            buffer[0] = CAN::Protocol::EVENT_RUN_NEXT_PART;
            buffer[1] = main.event.flow_id;
            buffer[2] = 1; // Events always call section number 1

            interfacer.send_buffer_to_every_interface(CAN::UID::ANY, buffer, 3);
        }
    }
}

void Automato::reload_event_buffers()
{
    // TODO: Error Handling!
    DEBUG_PRINTLN("Reloading event buffers!");
    m_filesystem->load_events(m_main_events, m_child_events);
    m_main_events_index = m_filesystem->main_event_count();
    m_child_events_index = m_filesystem->child_event_count();
}

bool Automato::is_child_event_for_me(uint8_t flow_id, uint8_t section_number) const
{
    for (uint8_t i = 0; i < m_child_events_index; i += 1) {
        if (m_child_events[i].flow_id == flow_id
            && m_child_events[i].section_number == section_number) {
            return true;
        }
    }
    return false;
}

void Automato::send_stored_events()
{
    // TODO: We Assume the MCM Called us
    //       but that might not be correct.

    CAN::ID id(interfacer.current_node_uid(), CAN::UID::MCM, CAN::Priority::NORMAL, CAN::FrameFormat::Standard);
    CAN::Frame frame(id, nullptr, 8);

    // first run, read 6 bytes.
    auto bytes_read = m_filesystem->read_event_file(&frame.data[2], 6, 0);

    if (bytes_read == 0) {
        // No events saved on disk!
        frame[0] = CAN::Protocol::REPLY_EVENT_SEND_STORED;

        frame.can_dlc = 1;
        interfacer.send_frame_to_every_interface(frame);
        return;
    }

    // We have some events to read!
    frame[0] = CAN::Frame::get_long_frame_uid();
    frame[1] = CAN::Protocol::REPLY_EVENT_SEND_STORED;

    // TODO: make frame.is_long_frame actually use the frame_format
    // though that might cause issues with our branchess casting!
    frame.is_long_frame = true;

    frame.can_dlc = bytes_read + 2;
    interfacer.send_frame_to_every_interface(frame);

    if (bytes_read < 6) {
        // We have sent all we need to.
        return;
    }

    uint16_t offset = 6;

    frame.can_dlc = 8;
    for (;;) {
        bytes_read = m_filesystem->read_event_file(&frame.data[1], 7, offset);

        if (bytes_read < 7) {
            // we don't need to read any more.
            // + 1 for long frame group uid
            frame.can_dlc = bytes_read + 1;
            // Ah yeah, we are sending 7 bytes
            interfacer.send_frame_to_every_interface(frame);
            return;
        }

        // we're here if we have read 7 bytes
        offset += 7;
        interfacer.send_frame_to_every_interface(frame);
    }
}

void Automato::event_run_next_part(uint8_t flow_id, uint8_t section_number)
{

    uint8_t event_index = CAN::NOT_FOUND;
    uint8_t next_section_number = section_number;

    do {
        // Find the index of the event
        for (uint8_t i = 0; i < m_child_events_index; i++) {
            if (m_child_events[i].flow_id == flow_id && m_child_events[i].section_number == next_section_number) {
                event_index = i;
                break;
            }
        }

        if (event_index == CAN::NOT_FOUND) {
            // We're not the intended audience.
            return;
        }

        // Run the event
        next_section_number = run_child_event(m_child_events[event_index]);
        //  Repeat if the child event is also for me.
    } while (is_child_event_for_me(flow_id, next_section_number));

    // if there is another event, and its not for me, broadcast it.
    if (next_section_number > 0) {
        uint8_t buffer[3];
        buffer[0] = CAN::Protocol::EVENT_RUN_NEXT_PART;
        buffer[1] = flow_id;
        buffer[2] = next_section_number;
        interfacer.send_buffer_to_every_interface(CAN::UID::ANY, buffer, 3);
    }
}

// Return the next section number.
uint8_t Automato::run_child_event(Event event)
{
    if (event.event_type == EventType::Command) {
        internal_call_command_function(event.this_function_id);
        return event.next_section;
    }

    if (event.event_type == EventType::If) {
        AnyType function_output = internal_call_command_function(event.this_function_id);
        AnyType value_to_compare = event.value_to_check;

        bool comparison_output = compare_better_any_type(event.conditional, function_output, value_to_compare);

        return comparison_output ? event.if_true : event.if_false;
    }

    __builtin_unreachable();
    return 0;
}

bool Automato::external_call_command_function(uint16_t from_id, uint8_t command_id, const void* function_input)
{
    uint8_t command_index = CAN::NOT_FOUND;
    for (uint8_t i = 0; i < m_command_buffer_index; i += 1) {
        if (m_command_buffer[i].id == command_id) {
            command_index = i;
            break;
        }
    }

    if (command_index == CAN::NOT_FOUND) {
        interfacer.send_error_frame(from_id, CAN::GENERIC_ERROR::COMMAND_NOT_FOUND);
        return false;
    }

    AnyType function_output = m_command_buffer[command_index].func(function_input);

    if (function_output.get_type() == TypeUsed::NOT_SET) {
        // Theres nothing to return.
        uint8_t buffer[2];
        buffer[0] = CAN::Protocol::REPLY_COMMAND;
        buffer[1] = command_id;

        interfacer.send_buffer_to_every_interface(from_id, buffer, 2);
        return true;
    }

    // There is something to return
    uint8_t buffer[12];

    buffer[0] = CAN::Protocol::REPLY_COMMAND;
    buffer[1] = command_id;
    buffer[2] = function_output.to_can_primitive();

    uint8_t bytes_needed = function_output.serialize(&buffer[3]);

    interfacer.send_buffer_to_every_interface(from_id, buffer, bytes_needed + 3);

    return true;
}

void Automato::ask_for_new_id()
{
    uint8_t buffer[1];
    buffer[0] = CAN::Protocol::NEW_UID;

    interfacer.send_buffer_to_every_interface(CAN::UID::MCM, buffer, 1, CAN::Priority::CRITICAL);
}

void Automato::add_interface(AutomatoInterface* interface)
{
    interfacer.add_interface(interface);
}

void Automato::translate_between_interfaces(AutomatoInterface* interface1, AutomatoInterface* interface2, TranslationMode mode)
{
    interfacer.translate_between_interfaces(interface1, interface2, mode);
}
