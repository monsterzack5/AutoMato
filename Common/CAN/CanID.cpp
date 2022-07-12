#include "CanFrame.h"

namespace CAN {

ID::ID(uint32_t can_id)
{
    priority = (0x3 & (can_id >> 30));
    from_id = (0x7FF & (can_id >> 14));
    to_id = (0x7FF & (can_id >> 3));
    is_long_frame = (0x1 & (can_id >> 29));
}

ID::ID(uint16_t from_id, uint16_t to_id, uint8_t priority /* CAN::Priority::NORMAL */, FrameFormat is_long_frame /* FrameFormat::Standard */)
    : from_id(from_id)
    , to_id(to_id)
    , priority(priority)
    , is_long_frame(static_cast<bool>(is_long_frame))
{
}

// Maybe store this in a temporary?
uint32_t ID::formatted_can_id() const
{
    if (!from_id || !to_id) {
        return 0;
    }

    uint32_t id = 0;

    id |= static_cast<uint32_t>(priority) << 30;
    id |= static_cast<uint32_t>(from_id) << 14;
    id |= static_cast<uint32_t>(to_id) << 3;
    id |= static_cast<uint32_t>(1); // Extended Frame Flag, Always Set

    if (is_long_frame) {
        id |= 0x20000000;
    }

    return id;
}

} // namespace CAN
