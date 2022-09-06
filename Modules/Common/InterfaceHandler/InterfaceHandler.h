#pragma once

#include <AutomatoInterface.h>
#include <Log.hpp>

// Max amount of drivers to use.
const uint8_t MAX_DRIVERS_TO_STORE = 2;

// Defines the number of possible ACKs we can cache at once.
const uint8_t MAX_ACK_BUFFERS = 12;

enum class TranslationMode : uint8_t {
    Bidirectional,
    Unidirectional,
};

// TODO: This should not be easily visible to the end user (but it is right now)
struct TranslationLayer {
    AutomatoInterface* from_driver;
    AutomatoInterface* to_driver;
};

class InterfaceHandler {
public:
    InterfaceHandler();
    ~InterfaceHandler() = default;

    bool try_read_frame(CAN::Frame* frame);

    void add_interface(AutomatoInterface* interface);
    
    void translate_between_interfaces(AutomatoInterface* interface1, AutomatoInterface* interface2, TranslationMode mode);

    void send_buffer_to_every_interface(uint16_t to_id, uint8_t data[], uint8_t data_size, uint8_t priority = CAN::Priority::NORMAL);
    void send_frame_to_every_interface(const CAN::Frame& frame);
    void send_error_frame(uint16_t to_id, uint8_t which_error);

    // ACK
    void add_waiting_on_ack(uint16_t module_uid, uint8_t command_id);
    void remove_waiting_on_ack(uint16_t module_uid, uint8_t command_id);
    void send_ack(uint16_t module_uid, uint8_t command_id);
    void check_for_old_acks(uint64_t current_time);

    // UID
    void set_current_node_uid(uint16_t new_uid) { m_current_node_uid = new_uid; }
    uint16_t current_node_uid() { return m_current_node_uid; }

private:

    AutomatoInterface* m_interfaces[MAX_DRIVERS_TO_STORE];
    uint8_t m_interfaces_index { 0 };

    TranslationLayer m_translation_layers[2];
    uint8_t m_translation_layers_index { 0 };

    uint32_t m_ack_buffer[MAX_ACK_BUFFERS * 2];
    
    uint16_t m_current_node_uid { 0 };
};