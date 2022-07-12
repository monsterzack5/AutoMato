#pragma once

#include <CanFrame.h>
#include <memory>
#include <unordered_map>
#include <vector>

class LongFrameHandler {
public:
    LongFrameHandler();
    ~LongFrameHandler() = default;

    static uint8_t get_long_frame_uid() { return rand() % 255; }

    void append(const CAN::Frame& frame);
    std::vector<uint8_t> steal_frame_group(uint8_t group_id);

private:
    std::unordered_map<uint8_t, std::vector<uint8_t>> m_buffers;
};
