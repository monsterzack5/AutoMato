#pragma once

#include <AutomatoInterface.h>
#include <stdint.h>
#include <unordered_map>
#include <vector>

class ACK {
public:
    ACK(uint16_t module_uid, uint8_t command_id)
        : module_uid(module_uid)
        , command_id(command_id)
    {
    }

    uint16_t module_uid;
    uint8_t command_id;

    bool operator==(const ACK& other) const
    {
        return (module_uid == other.module_uid && command_id == other.command_id);
    }

    struct HashFunction {
        size_t operator()(const ACK& ack) const
        {
            return ack.module_uid ^ ack.command_id;
        }
    };
};

class ACKHandler {
public:
    ACKHandler() = default;
    ~ACKHandler() = default;

    void add_waiting_on_ack(uint16_t module_uid, uint8_t command_id);
    void remove_waiting_on_ack(uint16_t module_uid, uint8_t command_id);

    std::vector<ACK> get_outdated_acks();


private:
    std::unordered_multimap<ACK, uint64_t, ACK::HashFunction> m_buffer;
};
