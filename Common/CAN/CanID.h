#pragma once

#include <stdint.h>

namespace CAN {

// TODO: Integrate this better into Frames.
enum class FrameFormat : uint8_t {
    Standard = 0,
    Long = 1
};

class ID {
public:
    explicit ID() = default;
    explicit ID(uint32_t can_id);
    explicit ID(uint16_t from_id, uint16_t to_id, uint8_t priority = CAN::Priority::NORMAL, FrameFormat is_long_frame = FrameFormat::Standard);
    ~ID() = default;

    uint32_t formatted_can_id() const;

    uint16_t from_id { 0 };
    uint16_t to_id { 0 };
    uint8_t priority { CAN::Priority::NORMAL };
    bool is_long_frame { false };
};

} // namespace CAN
