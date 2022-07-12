#include "CanManager.h"

#include <Database.h>
#include <fmt/color.h>
#include <fmt/format.h>
#include <iostream>
#include <json.hpp>
#include <memory>
#include <print_u8_array.h>
#include <seconds_to_ms.h>
#include <sys/socket.h>

CanManager::CanManager(std::vector<AutomatoInterface*>& interfaces)
    : m_interfaces(interfaces)
{
    const auto saved_modules = Database::the().prepare("SELECT uid, is_active, type, name, description FROM can_modules");
    for (const auto& statement : saved_modules) {
        Module mod;

        mod.uid = statement.column(0);

        std::string active = statement.column(1);
        mod.active = (active == "TRUE");

        std::string type = statement.column(2);
        mod.type = (type == "WRITER") ? ModuleType::Writer : ModuleType::Reader;

        std::string name = statement.column(3);
        mod.name = name;

        std::string description = statement.column(4);
        mod.description = description;

        m_saved_modules.emplace_back(mod);
    }
}

void CanManager::handle_incoming_frame(const CAN::Frame& frame)
{

    if (!frame.is_for_me(CAN::UID::MCM)) {
        // This frame is not for me!
        return;
    }

    if (frame.data[0] == CAN::Protocol::ACKNOWLEDGEMENT) {
        // a Module we sent a command to, replied with ACK
        m_ack_handler.remove_waiting_on_ack(frame.from_id, frame.data[1]);
        return;
    }

    // Frames for everyone don't require an ACK.
    if (!frame.is_for_everyone()) {
        send_ack(frame.from_id, frame.data[frame.is_long_frame]);
    }

    // We don't need to lex standard frames
    if (!frame.is_long_frame) {
        parse_frame_data(frame.from_id, frame.data, frame.can_dlc);
        return;
    }

    m_long_frame_handler.append(frame);

    if (frame.can_dlc < 8) {
        auto frame_buffer = m_long_frame_handler.steal_frame_group(frame[0]);
        parse_frame_data(frame.from_id, frame_buffer.data(), frame_buffer.size());
    }
}

void CanManager::parse_frame_data(uint16_t from_id, const uint8_t data[], uint16_t can_dlc)
{

    // CAN::Protocol::ACKNOWLEDGEMENT is handled by loop()

    switch (data[0]) {

    case CAN::Protocol::NEW_UID: {
        uint16_t new_id = generate_module_uid();
        uint8_t buffer[3];
        buffer[0] = CAN::Protocol::REPLY_NEW_UID;
        buffer[1] = new_id >> 8;
        buffer[2] = new_id & 0xFF;

        send_buffer_to_every_interface(from_id, buffer, 3);
        break;
    }

    case CAN::Protocol::REPLY_UPDATE_INFO: {
        handle_reply_update_info(from_id, data, can_dlc);
        break;
    }

    case CAN::Protocol::CHECK_IN: {
        for (auto& mod : m_saved_modules) {
            if (mod.uid == from_id) {
                mod.did_come_online = true;
                // fmt::print("Module {} just came online!\n", from_id);
            }
        }

        // TODO: This is for debug mode.
        // Always ask modules to send their config
        fmt::print(fmt::fg(fmt::terminal_color::green), "Module {} now online!\n", from_id);
        uint8_t buffer[1];

        buffer[0] = CAN::Protocol::UPDATE_INFO;

        send_buffer_to_every_interface(from_id, buffer, 1);
        break;
    }

    case CAN::Protocol::INVALID: {
        fmt::print(fmt::fg(fmt::terminal_color::red), "Invalid frame passed to parse_frame_data\n");
        break;
    }

    case CAN::Protocol::ERROR_GENERIC: {
        fmt::print("Module {} reported a generic error: {}\n", from_id, data[1]);
        break;
    }

    case CAN::Protocol::REPLY_EVENT_SEND_STORED: {

        if (can_dlc == 1) {
            fmt::print("Module {} does not have any stored events!\n", from_id);
            break;
        }

        fmt::print("Module {}'s stored events:\n", from_id);
        print_u8_array(&data[1], can_dlc - 1);
        break;
    }

    case CAN::Protocol::REPLY_COMMAND: {

        // So why are we receiving this message?
        // First, check if its for a socket.

        fmt::print("SocketRequests Array:\n");
        for (const auto& req : m_socket_requests) {
            fmt::print("uid: {}\ncommand_uid: {}\n", req.module_uid, req.command_uid);
        };

        for (auto iter = m_socket_requests.begin(); iter != m_socket_requests.end(); ++iter) {
            if (iter->module_uid == from_id && iter->command_uid == data[1]) {
                send_socket_reply(data, can_dlc, *iter);
                m_socket_requests.erase(iter);
                return;
            }
        }

        fmt::print(fmt::fg(fmt::terminal_color::red), "Parsed REPLY_COMMAND But we're not sure why!\n");
        // fmt::print(fmt::fg(fmt::terminal_color::red), "Frame Data: \n");
        // print_u8_array(data, can_dlc, fmt::terminal_color::red);
        // fmt::print("\n");

        // array[0]: 10001010 | 138 | 0x8a | � -> REPLY_COMMAND
        // array[1]: 00000011 |   3 | 0x03 | -> 3? wtf
        // array[2]: 11010010 | 210 | 0xd2 | �  -> BOOL_1_BYTES
        // array[3]: 00000001 |   1 | 0x01 | -> bool

        break;
    }

    default: {

        fmt::print(fmt::fg(fmt::terminal_color::red),
            "Reached default in parse_frame_data! Unhandled byte: {}\nFrame at the time of parsing\n", data[0]);
        print_u8_array(data, can_dlc, fmt::terminal_color::red);
        fmt::print("\n");

        // buffer[0] = CAN::Protocol::ERROR_GENERIC;
        // buffer[1] = CAN::GENERIC_ERROR::UNKNOWN_ERROR;

        // send_buffer_to_every_interface(from_id, buffer, 2);
    }
    }
}

void CanManager::send_frame_to_every_interface(const CAN::Frame& frame)
{
    // Don't send ACKs for ACKs
    // Frames meant for anyone do not require ACKs.
    if (frame[0] != CAN::Protocol::ACKNOWLEDGEMENT && !frame.is_for_everyone()) {
        m_ack_handler.add_waiting_on_ack(frame.to_id, frame[frame.is_long_frame]);
    }

    for (const auto& interface : m_interfaces) {
        interface->send_frame(frame);
    }
}

// This function automatically creates LongFrames, if needed
void CanManager::send_buffer_to_every_interface(uint16_t to_id, uint8_t data[], uint8_t data_size, uint8_t priority /* CAN::Priority::NORMAL */)
{
    CAN::FrameFormat frame_format = data_size > 8 ? CAN::FrameFormat::Long : CAN::FrameFormat::Standard;
    CAN::ID id(CAN::UID::MCM, to_id, priority, frame_format);

    if (data_size == 0) {
        // Empty frame
        CAN::Frame frame(id, nullptr, 0);
        send_frame_to_every_interface(frame);
        return;
    }

    if (frame_format == CAN::FrameFormat::Standard) {
        send_frame_to_every_interface(CAN::Frame { id, data, data_size });
        return;
    }

    // Since we use 1 Byte for a group UID, we can only
    // send 7 bytes at a time

    uint32_t bytes_to_send = data_size;
    const bool need_extra_frame = (data_size % 7) == 0;
    const uint8_t long_frame_uid = get_long_frame_uid();

    while (bytes_to_send) {
        uint8_t can_dlc = std::min(7u, bytes_to_send);

        CAN::Frame frame(id, &data[(data_size - bytes_to_send) + 1], can_dlc);
        frame.data[0] = long_frame_uid;

        bytes_to_send -= can_dlc;
        send_frame_to_every_interface(frame);
    }

    if (need_extra_frame) {
        // send an empty frame
        send_frame_to_every_interface(CAN::Frame { id, nullptr, 0 });
    }
}

void CanManager::update_events(std::vector<EventUpdate>& updates_needed)
{
    // We create this array and immediately copy the event_blob
    // vector into it, Since we need to reformat the array
    // to include the CAN Protocol Specifier.
    // Formatting CAN Frames should only be done inside of CANManager.

    // + 1 For the CAN::Protocol Specifier
    uint8_t buffer[Event::MAX_SIZE + 1] = { 0 };

    for (const auto& update : updates_needed) {

        buffer[0] = (update.update_type == EventUpdateType::add) ? CAN::Protocol::EVENT_ADD : CAN::Protocol::EVENT_REMOVE;

        for (uint8_t i = 0; i < update.event_blob.size(); i += 1) {
            buffer[i + 1] = update.event_blob.at(i);
        }
        send_buffer_to_every_interface(update.module_uid, buffer, update.event_blob.size() + 1);
    }
    updates_needed.clear();
}

StoredModuleStatus CanManager::insert_or_update_module(uint16_t uid, const std::string& type, const std::string& name, const std::string& description) const
{
    const auto is_module_uid_stored = [](uint8_t uid) {
        return !!Database::the().prepare("SELECT EXISTS (SELECT 1 FROM can_modules WHERE uid = ? LIMIT 1)").get(uid).column(0);
    };

    if (is_module_uid_stored(uid)) {
        // We're storing this module, check if anything has changed.

        // Get the module from the DB and compare it
        Module stored_module;

        const auto statement = Database::the().prepare("SELECT uid, is_active, type, name, description FROM can_modules WHERE uid = ?").get();
        stored_module.uid = statement.column(0);
        stored_module.active = statement.column(1);
        std::string stored_module_type = statement.column(2);
        stored_module.type = (stored_module_type == "WRITER") ? ModuleType::Writer : ModuleType::Reader;
        // TODO: The DB returns strings with extraneous quotes, we should
        // be trimming them away

        // TODO:
        // stored_module.name = statement.column(3);
        // stored_module.description = statement.column(4);

        Module given_module;
        given_module.uid = uid;
        given_module.type = type == "WRITER" ? ModuleType::Writer : ModuleType::Reader;
        given_module.name = name;               // TODO: These should be trimmed
        given_module.description = description; // TODO: These should be trimmed

        if (stored_module == given_module) {
            return StoredModuleStatus::NOT_MODIFIED;
        }
        return StoredModuleStatus::MODIFIED;
    }

    // We're not storing the module, store it!
    Database::the().prepare("INSERT INTO can_modules (uid, type, name, description) VALUES (?, ?, ?, ?)").run(uid, type, name, description);
    return StoredModuleStatus::NOT_STORED;
}

StoredModuleStatus CanManager::insert_or_update_module_command(uint16_t module_uid, uint16_t command_uid, const std::string& name, const std::string& return_format) const
{
    const auto is_module_command_id_stored = [](uint8_t module_uid, uint16_t command_uid) {
        return !!Database::the().prepare("SELECT EXISTS (SELECT 1 FROM can_module_commands WHERE module_uid = ? AND command_uid = ? LIMIT 1)").get(module_uid, command_uid).column(0);
    };

    if (is_module_command_id_stored(module_uid, command_uid)) {
        // We're storing this command, has it been modified?
        ModuleCommand stored_command;

        const auto statement = Database::the().prepare("SELECT name FROM can_module_commands WHERE module_uid = ? AND command_uid = ?").get(module_uid, command_uid);
        // TODO: stored_command.name = statement.column(0);
        stored_command.module_uid = module_uid;
        stored_command.command_uid = command_uid;

        ModuleCommand given_command;
        given_command.name = name;
        given_command.module_uid = module_uid;
        given_command.command_uid = command_uid;

        if (stored_command == given_command) {
            return StoredModuleStatus::NOT_MODIFIED;
        }
        return StoredModuleStatus::MODIFIED;
    }

    // We're not storing the command, store it!
    Database::the().prepare("INSERT INTO can_module_commands (module_uid, command_uid, name, return_format) VALUES (?, ?, ?, ?)").run(module_uid, command_uid, name, return_format);
    return StoredModuleStatus::NOT_STORED;
}

void CanManager::handle_reply_update_info(uint16_t from_id, const uint8_t data[], uint16_t can_dlc)
{
    // TODO: This function is rickety, it needs much better validation
    //       for the incoming JSON, and error handing for all ::json calls.

    std::string json_string;
    json_string.reserve(1024);

    for (uint16_t i = 1; i < can_dlc; i += 1) {
        if (data[i] != CAN::Protocol::REPLY_UPDATE_INFO) {
            json_string.push_back(std::move(data[i]));
        }
    }

    auto json = nlohmann::json::parse(json_string);
    // TODO: These can fail!
    auto type = json["Type"].dump();
    auto name = json["Name"].dump();
    auto description = json["Description"].dump();

    auto changes = insert_or_update_module(from_id, type, name, description);

    switch (changes) {
    case StoredModuleStatus::NOT_STORED:
        fmt::print("Storing a new module!\n");
        break;
    case StoredModuleStatus::MODIFIED:
        fmt::print("A Module Changed Its Configuration!\n");
        // We should tell this module to reload its saved events.
        // and do some error checking, if theres incompatible changes
        break;
    case StoredModuleStatus::NOT_MODIFIED:
        fmt::print("Module configuration has not changed!\n");
        // Do Nothing
        break;
    default:
        fmt::print("insert_or_update_module returned an error!\n");
        break;
    }

    for (auto const& value : json.at("Commands")) {

        auto command_name = value.at("CommandName").dump();
        auto command_id = std::stoi(value.at("CommandID").dump());
        auto return_format = value.at("ReturnFormat").dump();

        auto changes = insert_or_update_module_command(from_id, command_id, command_name, return_format);
        if (changes != StoredModuleStatus::NOT_MODIFIED) {
            // TODO: Print out a nice summary of the changes.
            fmt::print("a Module command was changed, in some way.\n");
        }
    }
}

void CanManager::send_ack(uint16_t from_id, uint8_t command_id)
{
    uint8_t buffer[2];

    buffer[0] = CAN::Protocol::ACKNOWLEDGEMENT;
    buffer[1] = command_id;

    send_buffer_to_every_interface(from_id, buffer, 2);
}

void CanManager::check_for_old_acks()
{
    auto outdated_acks = m_ack_handler.get_outdated_acks();

    for (const auto& ack : outdated_acks) {
        // we have outdated ACKs!
        // TODO: In the future we should retain enough information
        //       about frames that we can resent dropped frames.
        fmt::print("Dropped ACK: module_uid: {} command_id: {}\n", ack.module_uid, ack.command_id);
    }
}

void CanManager::inject_frame(const CAN::Frame& frame)
{
    send_frame_to_every_interface(frame);
}

uint16_t CanManager::generate_module_uid() const
{
    // TODO: This can possibly go on forever
    // TODO: This should be much more intelligent
    for (;;) {
        auto random_id = rand() % 2000 + 5;
        if (!!Database::the().prepare("SELECT EXISTS (SELECT 1 FROM can_modules WHERE uid = ? LIMIT 1").get(random_id).column(0)) {
            return random_id;
        }
    }
}

void CanManager::parse_socket_input(std::vector<SocketRequest>& socket_requests, std::mutex& socket_requests_lock)
{
    // Cache new socket data
    {
        std::unique_lock<std::mutex> lock(socket_requests_lock);
        m_socket_requests.insert(m_socket_requests.begin(), socket_requests.begin(), socket_requests.end());
        socket_requests.clear();
    }

    // TODO: For now, our IPC socket system only works with
    //       one type of request, but it would be cool if
    //       we could handle multiple types.
    for (auto& request : m_socket_requests) {
        request.module_uid = Database::the().prepare("SELECT uid FROM can_modules where name = ?").get(request.module_name).column(0);
        request.command_uid = Database::the().prepare("SELECT command_uid FROM can_module_commands WHERE name = ? AND module_uid = ?").get(request.module_uid, request.module_function).column(0);

        fmt::print("Socket Request Info:\nmodule_uid: {}\nmodule_function: {}\n", request.module_uid, request.command_uid);
        fmt::print("module_name (str): {}\nmodule_function_name (str): {}\n", request.module_name, request.module_function);

        CAN::ID id(CAN::UID::MCM, request.module_uid);

        uint8_t buffer[2];
        buffer[0] = CAN::Protocol::COMMAND;
        buffer[1] = request.command_uid;

        CAN::Frame frame(id, buffer, 2);
        send_frame_to_every_interface(frame);
    }
}

// Expected Output Format:
// {
//     "module_function": "Return True",
//     "module_name": "CCM",
//     "output": "<output as string>",
//     "output_type": <number defining output type>
// }

bool CanManager::send_socket_reply(const uint8_t data[], uint16_t can_dlc, const SocketRequest& socket_request)
{
    nlohmann::json json;

    json["module_function"] = socket_request.module_function;
    json["module_name"] = socket_request.module_name;

    if (can_dlc == 1) {
        // We don't have any data to send back!
        json["output"] = "";
        json["output_type"] = 0;

        std::string buffer = json.dump();

        send(socket_request.file_descriptor, buffer.data(), buffer.size(), 0);
        return true;
    }

    const auto primitive_to_socket_output = [](uint8_t primitive) {
        switch (primitive) {
        case CAN::Primitive::UNSIGNED_1_BYTES:
        case CAN::Primitive::UNSIGNED_2_BYTES:
        case CAN::Primitive::UNSIGNED_4_BYTES:
            return OutputType::unsigned_int;

        case CAN::Primitive::SIGNED_1_BYTES:
        case CAN::Primitive::SIGNED_2_BYTES:
        case CAN::Primitive::SIGNED_4_BYTES:
            return OutputType::signed_int;

        case CAN::Primitive::UNSIGNED_8_BYTES:
            return OutputType::unsigned_bigint;

        case CAN::Primitive::SIGNED_8_BYTES:
            return OutputType::signed_bigint;

        case CAN::Primitive::FLOAT_4_BYTES:
            return OutputType::f_float;

        case CAN::Primitive::DOUBLE_8_BYTES:
            return OutputType::d_double;

        case CAN::Primitive::BOOL_1_BYTES:
            return OutputType::b_bool;

        case CAN::Primitive::VOID:
            return OutputType::v_void;

        default:
            return OutputType::v_void;
        };
    };

    auto size = CAN::primitive_size(data[2]);
    auto type = primitive_to_socket_output(data[2]);
    std::string output;

    // TODO: This can definitely be improved
    switch (data[2]) {

    case CAN::Primitive::UNSIGNED_1_BYTES: {
        uint8_t value = data[3];
        output = std::to_string(value);
        break;
    }

    case CAN::Primitive::SIGNED_1_BYTES: {
        int8_t value = data[3];
        output = std::to_string(value);
        break;
    }

    case CAN::Primitive::UNSIGNED_2_BYTES: {
        uint16_t value = 0;
        memcpy(&value, &data[3], size);
        output = std::to_string(value);
        break;
    }

    case CAN::Primitive::SIGNED_2_BYTES: {
        int16_t value = 0;
        memcpy(&value, &data[3], size);
        output = std::to_string(value);
        break;
    }

    case CAN::Primitive::UNSIGNED_4_BYTES: {
        uint32_t value = 0;
        memcpy(&value, &data[3], size);
        output = std::to_string(value);
        break;
    }

    case CAN::Primitive::SIGNED_4_BYTES: {
        int32_t value = 0;
        memcpy(&value, &data[3], size);
        output = std::to_string(value);
        break;
    }

    case CAN::Primitive::FLOAT_4_BYTES: {
        float value = 0;
        memcpy(&value, &data[3], size);
        output = std::to_string(value);
        break;
    }

    case CAN::Primitive::UNSIGNED_8_BYTES: {
        uint64_t value = 0;
        memcpy(&value, &data[3], size);
        output = std::to_string(value);
        break;
    }

    case CAN::Primitive::SIGNED_8_BYTES: {
        int64_t value = 0;
        memcpy(&value, &data[3], size);
        output = std::to_string(value);
        break;
    }

    case CAN::Primitive::DOUBLE_8_BYTES: {
        double value = 0;
        memcpy(&value, &data[3], size);
        output = std::to_string(value);
        break;
    }

    case CAN::Primitive::BOOL_1_BYTES: {
        bool value = data[3];
        output = std::to_string(value);
        break;
    }

    case CAN::Primitive::VOID: {
        output = "";
        break;
    }

    default:
        output = "";
        break;
    }

    json["output"] = output;
    json["output_type"] = type;

    std::string buffer = json.dump();
    send(socket_request.file_descriptor, buffer.data(), buffer.size(), 0);

    return true;
}
