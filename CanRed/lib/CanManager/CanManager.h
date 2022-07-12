#pragma once

#include <ACKHandler.h>
#include <AutomatoInterface.h>
#include <CanFrame.h>
#include <Database.h>
#include <EventManager.h>
#include <LongFrameHandler.h>
#include <Module.h>
#include <SocketWatcher.h>

class CanManager {
public:
    CanManager() = delete;
    CanManager(std::vector<AutomatoInterface*>& interfaces);
    ~CanManager() = default;

    void handle_incoming_frame(const CAN::Frame& frame);

    void check_for_old_acks();
    void update_events(std::vector<EventUpdate>& updates_needed);

    // Injects a CAN::Frame into the CANBUS, meant only to be used for development
    void inject_frame(const CAN::Frame& frame);
    
    void parse_socket_input(std::vector<SocketRequest>& socket_requests, std::mutex& socket_requests_lock);

private:
    // Modules
    // TODO: These are some long names...
    StoredModuleStatus insert_or_update_module(uint16_t uid, const std::string& type, const std::string& name, const std::string& description) const;
    StoredModuleStatus insert_or_update_module_command(uint16_t module_uid, uint16_t command_uid, const std::string& name, const std::string& return_format) const;

    // CAN
    void send_frame_to_every_interface(const CAN::Frame& frame);
    void send_buffer_to_every_interface(uint16_t to_id, uint8_t data[], uint8_t data_size, uint8_t priority = CAN::Priority::NORMAL);

    void parse_frame_data(uint16_t from_id, const uint8_t data[], uint16_t length);

    void handle_reply_update_info(uint16_t from_id, const uint8_t data[], uint16_t can_dlc);

    std::vector<Module> m_saved_modules;

    // ACK
    void send_ack(uint16_t from_id, uint8_t command_id);

    ACKHandler m_ack_handler;

    // Long Frames
    LongFrameHandler m_long_frame_handler;

    // Interfaces
    std::vector<AutomatoInterface*>& m_interfaces;

    // Sockets
    bool send_socket_reply(const uint8_t data[], uint16_t can_dlc, const SocketRequest& socket_request);

    std::vector<SocketRequest> m_socket_requests;

    // Helpers
    uint16_t generate_module_uid() const;
    uint8_t get_long_frame_uid() const { return rand() % 255; };
};
