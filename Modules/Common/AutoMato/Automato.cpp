#include "Automato.h"
#include <CanFrame.h>
#include <CanSerializer.h>
#include <Log.hpp>
#include <algorithm>
#include <seconds_to_ms.h>
#include <string.h> // For memcpy only!
#include <yield_if_needed.h>

#ifdef ESP32
// TODO: We need to abstract away any module specific libraries.
// #    include <FreeRTOS.h>
#endif

#if defined ESP32 || defined ESP8266
#    include <ESP_LittleFS.h>
#endif

Automato::Automato(const char* config_file, uint16_t hardcoded_id /* 0 */, FilesystemInterface* fs_interface /* nullptr */) noexcept
    : m_config_file(config_file)
    , m_current_node_uid(hardcoded_id)
    , m_filesystem(fs_interface)
{
    // TODO: If not given a filesystem interface, try and pick the best one.

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
    // Mark every long frame group as available for writing
    for (uint8_t i = 0; i < MAX_LONG_FRAME_GROUPS; i += 1) {
        clear_long_frame_buffer(m_long_frame_buffer[i]);
    }

    reload_event_buffers();

    // Mark every slot in the ACK buffer as available for writing
    memset(m_ack_buffer, 0, MAX_ACK_BUFFERS);

    YIELD_IF_NEEDED();

    if (m_current_node_uid == 0) {
        // We were not given a hardcoded uid.
        m_current_node_uid = m_filesystem->load_uid();
        if (m_current_node_uid == 0) {
            // TODO: Maybe flash the LED
            m_current_node_uid = CAN::Frame::get_temporary_can_id();
            // We don't have an ID yet, but we still need to wait for
            // the MCM to come online.
            send_check_in_frame();
            ask_for_new_id();
            return;
        }
    }

    send_check_in_frame();
}

void Automato::loop()
{
    YIELD_IF_NEEDED();
    static uint8_t interface_to_check = 0;
    static CAN::Frame frame(CAN::ID(0), nullptr, 0);

    // 1. Handle any Main Events
    // 2. Update driver_to_check
    // 3. Continue if we read a frame
    // 4. Handle any Translations
    // 5. Continue if the frame we read is for us
    // 5. if ACK, remove it from our buffer
    // 6. Pass the frame to handle_frame()

    check_main_events();

    interface_to_check = (interface_to_check == m_interfaces_index - 1) ? 0 : interface_to_check + 1;

    if (!m_interfaces[interface_to_check]->read_frame(&frame)) {
        // We didn't read a frame, return so we can
        // give control flow back to the main loop() body.
        return;
    }

    YIELD_IF_NEEDED();

    // In our current implementation of translate_between_layers
    // We always redirect frames, even if they're specifically
    // for the current module. This is to better facilitate
    // debugging.

    // TODO: We currently translate everything, including ACKs
    //       and UPDATE_INFO frames (even if they're for us)
    for (uint8_t i = 0; i < m_translation_layers_index; i += 1) {
        if (m_translation_layers[i].from_driver != m_interfaces[interface_to_check]) {
            continue;
        }

        m_translation_layers[i].to_driver->send_frame(frame);
    }

    if (!frame.is_for_me(m_current_node_uid)) {
        // This frame is not for me.
        return;
    }

    if (frame[0] == CAN::Protocol::ACKNOWLEDGEMENT) {
        // A Module we sent a command to replied with an ACK.
        remove_waiting_on_ack(frame.from_id, frame[1]);
        return;
    }

    // Frames for everyone don't require an ACK.
    if (!frame.is_for_everyone()) {
        send_ack(frame.from_id, frame.data[frame.is_long_frame]);
    }

    handle_frame(frame);
}

uint8_t Automato::find_uid_index(uint8_t uid) const
{
    for (uint8_t i = 0; i < MAX_LONG_FRAME_GROUPS; i += 1) {
        if (m_long_frame_buffer[i][0] == uid) {
            return i;
        }
    }

    return CAN::NOT_FOUND;
}

uint8_t Automato::find_free_long_frame_group() const
{
    // find first group where arr[i][0] == \0

    for (uint8_t i = 0; i < MAX_LONG_FRAME_GROUPS; i += 1) {
        if (m_long_frame_buffer[i][0] == '\0') {
            return i;
        }
    }
    return CAN::NOT_FOUND;
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

    uint8_t group_index = find_uid_index(frame[0]);

    if (group_index == CAN::NOT_FOUND) {
        group_index = find_free_long_frame_group();

        if (group_index == CAN::NOT_FOUND) {
            // We are out of buffers, reply with an error!
            send_error_frame(frame.from_id, CAN::GENERIC_ERROR::BUSY);
            return;
        }

        // This is the first time we have seen this frame group
        m_long_frame_buffer[group_index][0] = frame[0];
        m_LF_group_index[group_index] = 8; // what is this doing?
        for (uint8_t i = 1; i < frame.can_dlc; i += 1) {
            m_long_frame_buffer[group_index][i] = frame[i];
        }
        return;
    }

    // This is a long frame group we are already storing.
    for (uint8_t i = 1; i < frame.can_dlc; i += 1) {
        m_long_frame_buffer[group_index][i + m_LF_group_index[group_index]] = frame[i];

        if (m_LF_group_index[group_index] > 64) {
            // Frame is too large for us!
            clear_long_frame_buffer(m_long_frame_buffer[group_index]);
            send_error_frame(frame.from_id, CAN::GENERIC_ERROR::FRAME_TOO_LONG);
            return;
        }
    }

    m_LF_group_index[group_index] += frame.can_dlc;

    if (frame.can_dlc < 8) {
        parse_frame(frame.from_id, m_long_frame_buffer[group_index], m_LF_group_index[group_index]);
        clear_long_frame_buffer(m_long_frame_buffer[group_index]);
    }
}

void Automato::parse_frame(uint16_t from_id, const uint8_t data[], uint16_t can_dlc)
{
    YIELD_IF_NEEDED();

    DEBUG_PRINT("New Frame! data[0]: ");
    DEBUG_PRINTLN(data[0]);

    // CAN::Protocol::ACKNOWLEDGEMENT is handled by handle_frame()

    switch (data[0]) {
    case CAN::Protocol::REPLY_NEW_UID: {

        memcpy(&m_current_node_uid, data, 2);
        m_filesystem->store_uid(m_current_node_uid);
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

        send_buffer_to_every_interface(from_id, buffer, 2);
        break;
    }
    }
}

// TODO: This function chunks data into 8 byte chunks, for CAN.
//       This function has not been updated to properly use our
//       new driver system.

void Automato::send_buffer_to_every_interface(uint16_t to_id, uint8_t data[], uint8_t data_size, uint8_t priority /* CAN::Priority::NORMAL */)
{
    CAN::FrameFormat is_long_frame = (data_size > 8) ? CAN::FrameFormat::Long : CAN::FrameFormat::Standard;
    CAN::ID can_id = CAN::ID(m_current_node_uid, to_id, priority, is_long_frame);

    if (data_size == 0) {
        // Empty frame

        send_frame_to_every_interface(CAN::Frame { can_id, nullptr, 0 });
        return;
    }

    if (is_long_frame == CAN::FrameFormat::Standard) {
        // data_size <= 8

        send_frame_to_every_interface(CAN::Frame { can_id, data, data_size });
        return;
    }

    // TODO: These comments are CAN Specific.

    // we can only send 7 at a time, as we need
    // the first byte to store the long frame uid
    uint32_t bytes_to_send = data_size;

    // a LongFrame group is marked as finished when
    // the last frame is less than 7 bytes
    bool need_extra_frame = (data_size % 7) == 0;
    uint8_t long_frame_uid = CAN::Frame::get_long_frame_uid();

    CAN::Frame frame(can_id, nullptr, 0);
    frame.data[0] = long_frame_uid;

    while (bytes_to_send) {
        YIELD_IF_NEEDED();

        //                min(7u, bytes_to_send);
        uint8_t can_dlc = (7u > bytes_to_send) ? bytes_to_send : 7u;

        memcpy(&frame[1], &data[data_size - bytes_to_send], can_dlc);

        bytes_to_send -= can_dlc;
        send_frame_to_every_interface(frame);
    }

    if (need_extra_frame) {
        // Send an empty frame.
        send_frame_to_every_interface(CAN::Frame { can_id, nullptr, 0 });
    }
}

void Automato::send_frame_to_every_interface(const CAN::Frame& frame)
{
    // Don't send acks for acks
    // Frames meant for anyone do not require ACKs
    if (frame[0] != CAN::Protocol::ACKNOWLEDGEMENT && !frame.is_for_everyone()) {
        // In Long Frames, the first byte we send is the frame uid
        // and the second is the command byte.
        add_waiting_on_ack(frame.to_id, frame[frame.is_long_frame]);
    }

    for (uint8_t i = 0; i < m_interfaces_index; i += 1) {
        m_interfaces[i]->send_frame(frame);
    }
}

void Automato::add_waiting_on_ack(uint16_t module_uid, uint8_t command_id)
{
    // Find the first empty slot in the ack buffer
    for (uint8_t i = 0; i < MAX_ACK_BUFFERS * 2; i += 2) {
        if (m_ack_buffer[i] == 0) {
            // this is an empty slot!
            uint32_t packed = ((static_cast<uint32_t>(module_uid) << 16) | command_id);
            m_ack_buffer[i] = packed;
            m_ack_buffer[i + 2] = millis();
            return;
        }
    }
}

void Automato::remove_waiting_on_ack(uint16_t module_uid, uint8_t command_id)
{
    uint32_t packed = ((static_cast<uint32_t>(module_uid) << 16) | command_id);
    for (uint8_t i = 0; i < MAX_ACK_BUFFERS * 2; i += 2) {
        if (m_ack_buffer[i] == packed) {
            m_ack_buffer[i] = 0;
            m_ack_buffer[i + 1] = 0;
            break;
        }
    }
}

void Automato::send_ack(uint16_t module_uid, uint8_t command_id)
{
    uint8_t buffer[2];
    buffer[0] = CAN::Protocol::ACKNOWLEDGEMENT;
    buffer[1] = command_id;

    send_buffer_to_every_interface(module_uid, buffer, 2);
}

void Automato::check_for_old_acks(uint64_t current_time)
{
    for (uint8_t i = 0; i < MAX_ACK_BUFFERS * 2; i += 2) {
        // I don't think this is an off by one error, but I am not sure
        if (m_ack_buffer[i + 1] > current_time + seconds_to_ms(3)) {
            // We are not storing enough information currently
            // in order to actually resend a reply, so lets just
            // send a placeholder for now.
            uint16_t module_uid = m_ack_buffer[i] >> 16;

            uint8_t buffer[2];
            buffer[0] = CAN::Protocol::ERROR_GENERIC;
            buffer[1] = CAN::GENERIC_ERROR::FAILED_TO_READ_MESSAGE;

            send_buffer_to_every_interface(module_uid, buffer, 2);
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

    CAN::ID id(m_current_node_uid, CAN::UID::MCM, CAN::Priority::MEDIUM, CAN::FrameFormat::Long);

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

    send_frame_to_every_interface(frame);

    for (;;) {
        YIELD_IF_NEEDED();
        if (bytes_sent <= (config_length + 7)) {
            for (uint8_t i = 0; i < 7; i += 1) {
                frame.data[i + 1] = pgm_read_byte(&m_config_file[bytes_sent]);
                bytes_sent += 1;
            }
            send_frame_to_every_interface(frame);
            continue;
        }

        // if we can't send 7, but still need to send some
        uint8_t index = 1;
        for (; bytes_sent < config_length; bytes_sent += 1) {
            frame.data[index] = pgm_read_byte(&m_config_file[bytes_sent]);
            index += 1;
        }

        frame.can_dlc = index;
        send_frame_to_every_interface(frame);
        break;
    }

    // Check to see if we need to send another frame
    if (frame.can_dlc == 8) {
        // send empty frame
        send_buffer_to_every_interface(CAN::UID::MCM, nullptr, 0);
    }

    DEBUG_PRINTLN("Done sending!");

    m_CAN_is_locked = false;
}

// Only Pass CAN::GENERIC_ERROR::*
void Automato::send_error_frame(uint16_t to_id, uint8_t which_error)
{
    uint8_t buffer[2];
    buffer[0] = CAN::Protocol::ERROR_GENERIC;
    buffer[1] = which_error;

    send_buffer_to_every_interface(to_id, buffer, 2, CAN::Priority::IMPORTANT);
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
            send_buffer_to_every_interface(CAN::UID::MCM, buffer, 1, CAN::Priority::MEDIUM);
            DEBUG_PRINTLN("Screaming into the void");
        }

        // Check every interface for a response.
        for (uint8_t i = 0; i < m_interfaces_index; i += 1) {
            if (m_interfaces[i]->read_frame(&frame)) {
                if (frame.is_only_for_me(m_current_node_uid)) {
                    DEBUG_PRINTLN("MCM Said We're Good To Go!");
                    return;
                }
            }
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
                return;
            }

            uint8_t buffer[3];
            buffer[0] = CAN::Protocol::EVENT_RUN_NEXT_PART;
            buffer[1] = main.event.flow_id;
            buffer[2] = 1; // Events always call section number 1

            send_buffer_to_every_interface(CAN::UID::ANY, buffer, 3);
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

    CAN::ID id(m_current_node_uid, CAN::UID::MCM, CAN::Priority::NORMAL, CAN::FrameFormat::Standard);
    CAN::Frame frame(id, nullptr, 8);

    // first run, read 6 bytes.
    auto bytes_read = m_filesystem->read_event_file(&frame.data[2], 6, 0);

    if (bytes_read == 0) {
        // No events saved on disk!
        frame[0] = CAN::Protocol::REPLY_EVENT_SEND_STORED;

        frame.can_dlc = 1;
        send_frame_to_every_interface(frame);
        return;
    }

    // We have some events to read!
    frame[0] = CAN::Frame::get_long_frame_uid();
    frame[1] = CAN::Protocol::REPLY_EVENT_SEND_STORED;

    // TODO: make frame.is_long_frame actually use the frame_format
    // though that might cause issues with our branchess casting!
    frame.is_long_frame = true;

    frame.can_dlc = bytes_read + 2;
    send_frame_to_every_interface(frame);

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
            send_frame_to_every_interface(frame);
            return;
        }

        // we're here if we have read 7 bytes
        offset += 7;
        send_frame_to_every_interface(frame);
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
        send_buffer_to_every_interface(CAN::UID::ANY, buffer, 3);
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
        send_error_frame(from_id, CAN::GENERIC_ERROR::COMMAND_NOT_FOUND);
        return false;
    }

    AnyType function_output = m_command_buffer[command_index].func(function_input);

    if (function_output.get_type() == TypeUsed::NOT_SET) {
        // Theres nothing to return.
        uint8_t buffer[2];
        buffer[0] = CAN::Protocol::REPLY_COMMAND;
        buffer[1] = command_id;

        send_buffer_to_every_interface(from_id, buffer, 2);
        return true;
    }

    // There is something to return
    uint8_t buffer[12];

    buffer[0] = CAN::Protocol::REPLY_COMMAND;
    buffer[1] = command_id;
    buffer[2] = function_output.to_can_primitive();

    uint8_t bytes_needed = function_output.serialize(&buffer[3]);

    send_buffer_to_every_interface(from_id, buffer, bytes_needed + 3);

    return true;
}

void Automato::clear_long_frame_buffer(uint8_t buffer[])
{
    memset(buffer, '\0', MAX_BYTES_PER_LF_GROUP);
}

void Automato::ask_for_new_id()
{
    uint8_t buffer[1];
    buffer[0] = CAN::Protocol::NEW_UID;

    send_buffer_to_every_interface(CAN::UID::MCM, buffer, 1, CAN::Priority::CRITICAL);
}

void Automato::add_interface(AutomatoInterface* interface)
{
    if (interface) {
        // interface is valid
        m_interfaces[m_interfaces_index++] = interface;
        return;
    }

    DEBUG_PRINTLN("INVALID INTERFACE POINTER PASSED TO ADD_INTERFACE!");
    // We should lock up here probably.
}

void Automato::translate_between_interfaces(AutomatoInterface* interface1, AutomatoInterface* interface2, TranslationMode mode)
{
    if (interface1 == interface2) {
        // No-op
        DEBUG_PRINTLN("Translate between layers called for the same interface!");
        return;
    }

    if (!interface1 || !interface2) {
        DEBUG_PRINTLN("INVALID DRIVER POINTERS PASSED TO translate_drivers_for!");
        return;
    }

    if (mode == TranslationMode::Bidirectional) {
        m_translation_layers[m_translation_layers_index++] = TranslationLayer { interface1, interface2 };
        m_translation_layers[m_translation_layers_index++] = TranslationLayer { interface2, interface1 };
        return;
    }

    if (mode == TranslationMode::Unidirectional) {
        m_translation_layers[m_translation_layers_index++] = TranslationLayer { interface1, interface2 };
        return;
    }
}
