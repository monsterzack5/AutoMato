#pragma once

// TODO: Remove explicit cast to mcp2515::can_frame
#if defined ESP8266 || defined ESP32
#    include <mcp2515.h>
#endif

#include "CanConstants.h"
#include "CanID.h"
#include <random>
#include <stdint.h>

namespace CAN {

class Frame final : public ID {

public:
#if defined ESP8266 || defined ESP32
    explicit Frame(can_frame frame);
    explicit operator can_frame() const;
#endif
    Frame() = default;
    Frame(const ID can_id, const uint8_t data[8], const uint8_t can_dlc);
    Frame& operator=(const Frame& other);
    ~Frame() = default;

    // TODO: We don't need all of these.
    // TODO: is_for_me -> is_for
    // TODO: is_for_everyone -> is_for_anyone
    static bool is_for_me(uint32_t id_to_check, uint16_t current_node_uid); // Slated For Removal
    static bool is_for_me(ID id_to_check, uint16_t current_node_uid);
    // static bool is_for(uint16_t lhs, uint16_t rhs) { i }
    bool is_for_me(uint16_t current_node_id) const;
    bool is_only_for_me(uint16_t current_node_id) const;
    bool is_for_everyone() const;

    static uint16_t get_temporary_can_id() { return (rand() % 41) + 2000; }
    static uint8_t get_long_frame_uid() { return rand() % 255; };

    uint8_t operator[](const uint8_t index) const { return data[index]; }
    uint8_t& operator[](const uint8_t index) { return data[index]; }

    void dump() const;

    uint8_t data[8] { CAN::Protocol::INVALID };
    uint8_t can_dlc { 0 };

    // 4 byte can_id + 1 byte can_dlc + up to 8 bytes of data.
    static constexpr uint8_t MAX_SERIALIZED_SIZE = 4 + 1 + 8;
};

} // namespace CAN
