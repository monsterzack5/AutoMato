#include "CanFrame.h"
#include <string.h> // for memcpy
#include <Platform.h>

#ifdef PLATFORM_EMBEDDED
#    include <Log.hpp>
#else
#    include <print_u8_array.h>
#endif

#include <print_bin.h>

namespace CAN {

#if defined ESP8266 || defined ESP32

Frame::Frame(can_frame frame)
    : ID(frame.can_id)
    , can_dlc(frame.can_dlc)
{
    memcpy(data, frame.data, can_dlc);
}

Frame::operator can_frame() const
{
    can_frame frame {
        .can_id = formatted_can_id(),
        .can_dlc = can_dlc,
        .data = {},
    };
    memcpy(frame.data, this->data, this->can_dlc);
    return frame;
}

#endif

Frame::Frame(const ID can_id, const uint8_t from_data[], const uint8_t can_dlc)
    : ID(can_id)
    , can_dlc(can_dlc)
{
    // Passing nullptr and zero to memcpy is undefined behavior
    if (from_data && can_dlc > 0 && can_dlc < 9) {
        memcpy(data, from_data, can_dlc);
    }
}

Frame& Frame::operator=(const Frame& other)
{
    this->from_id = other.from_id;
    this->to_id = other.to_id;
    this->is_long_frame = other.is_long_frame;
    this->priority = other.priority;
    this->can_dlc = other.can_dlc;
    memcpy(this->data, other.data, other.can_dlc);
    return *this;
}

bool Frame::is_for_me(uint32_t id_to_check, uint16_t current_node_uid)
{
    return (id_to_check == CAN::UID::ANY || ID(id_to_check).to_id == current_node_uid);
}

bool Frame::is_for_me(ID id_to_check, uint16_t current_node_uid)
{
    return (id_to_check.to_id == CAN::UID::ANY || id_to_check.to_id == current_node_uid);
}

bool Frame::is_for_me(uint16_t current_node_id) const
{
    return (current_node_id == CAN::UID::ANY || current_node_id == to_id);
}

bool Frame::is_only_for_me(uint16_t current_node_id) const
{
    return (current_node_id == to_id);
}

bool Frame::is_for_everyone() const
{
    return (to_id == CAN::UID::ANY);
}

void Frame::dump() const
{
#if defined ESP32 || defined ESP8266
    DEBUG_PRINT("Frame ID:  ");
    DEBUG_PRINTLN(formatted_can_id(), BIN);
    DEBUG_PRINT("\nFrom ID: ");
    DEBUG_PRINTLN(from_id);
    DEBUG_PRINT("To ID:   ");
    DEBUG_PRINTLN(to_id);
    DEBUG_PRINT("Frame DLC: ");
    DEBUG_PRINTLN(can_dlc);
    for (uint8_t i = 0; i < can_dlc; i += 1) {
        DEBUG_PRINT("Frame.data[");
        DEBUG_PRINT(i);
        DEBUG_PRINT("]: ");
        print_bin(data[i]);
        DEBUG_PRINT(" | ");
        DEBUG_PRINTLN(data[i]);
    }
#else
    fmt::print("Frame ID:  ");
    print_bin(formatted_can_id());
    fmt::print("\nFrom ID: {}\nTo ID:   {}", from_id, to_id);
    fmt::print("\nFrame DLC: {}\n", can_dlc);
    fmt::print("Frame data:\n");
    print_u8_array(data, can_dlc);
    fmt::print("\n");
#endif
}

} // namespace CAN
