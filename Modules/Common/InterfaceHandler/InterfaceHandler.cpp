#include "InterfaceHandler.h"

#include <seconds_to_ms.h>

InterfaceHandler::InterfaceHandler()
{
    // Mark every slot in the ACK buffer as available for writing
    memset(m_ack_buffer, 0, MAX_ACK_BUFFERS);
}

bool InterfaceHandler::try_read_frame(CAN::Frame* frame)
{
    static uint8_t interface_to_check = 0;

    interface_to_check = (interface_to_check == m_interfaces_index - 1) ? 0 : interface_to_check + 1;

    bool did_read_frame = m_interfaces[interface_to_check]->read_frame(frame);

    if (did_read_frame) {
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

            m_translation_layers[i].to_driver->send_frame(*frame);
        }
    }

    return did_read_frame;
}

void InterfaceHandler::translate_between_interfaces(AutomatoInterface* interface1, AutomatoInterface* interface2, TranslationMode mode)
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


void InterfaceHandler::add_interface(AutomatoInterface* interface)
{

    if (interface) {
        m_interfaces[m_interfaces_index++] = interface;
        return;
    }

    DEBUG_PRINTLN("INVALID INTERFACE POINTER PASSED TO ADD_INTERFACE!");
}

// TODO: This function chunks data into 8 byte chunks, for CAN.
//       This function has not been updated to properly use our
//       new driver system.

void InterfaceHandler::send_buffer_to_every_interface(uint16_t to_id, uint8_t data[], uint8_t data_size, uint8_t priority /* CAN::Priority::NORMAL */)
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

void InterfaceHandler::send_frame_to_every_interface(const CAN::Frame& frame)
{
    if (frame[0] != CAN::Protocol::ACKNOWLEDGEMENT && !frame.is_for_everyone()) {
        // In Long Frames, the first byte we send is the frame uid
        // and the second is the command byte.
        add_waiting_on_ack(frame.to_id, frame[frame.is_long_frame]);
    }

    for (uint8_t i = 0; i < m_interfaces_index; i += 1) {
        m_interfaces[i]->send_frame(frame);
    }
}

void InterfaceHandler::send_error_frame(uint16_t to_id, uint8_t which_error)
{
    uint8_t buffer[2];
    buffer[0] = CAN::Protocol::ERROR_GENERIC;
    buffer[1] = which_error;

    send_buffer_to_every_interface(to_id, buffer, 2, CAN::Priority::IMPORTANT);
}

void InterfaceHandler::add_waiting_on_ack(uint16_t module_uid, uint8_t command_id)
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

void InterfaceHandler::remove_waiting_on_ack(uint16_t module_uid, uint8_t command_id)
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

void InterfaceHandler::send_ack(uint16_t module_uid, uint8_t command_id)
{
    uint8_t buffer[2];
    buffer[0] = CAN::Protocol::ACKNOWLEDGEMENT;
    buffer[1] = command_id;

    send_buffer_to_every_interface(module_uid, buffer, 2);
}

void InterfaceHandler::check_for_old_acks(uint64_t current_time)
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
