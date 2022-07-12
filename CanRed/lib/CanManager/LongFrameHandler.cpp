#include "LongFrameHandler.h"

#include <fmt/format.h>

LongFrameHandler::LongFrameHandler()
{
    m_buffers.reserve(10);
}

void LongFrameHandler::append(const CAN::Frame& frame)
{
    // frame[0] = long frame group uid
    auto element = m_buffers.find(frame[0]);

    if (element == m_buffers.end()) {
        // we're not currently storing this group.

        std::vector<uint8_t> bytes;
        bytes.reserve(1024); // 1KB

        auto did_insert = m_buffers.insert({ frame[0], std::move(bytes) });

        if (did_insert.second) {
            // insert was successful
            element = did_insert.first;
        } else {
            fmt::print("failed to insert element into map in LongFrameHandler\n");
            return;
        }
    }

    // Store bytes 1-7
    for (uint8_t i = 1; i < frame.can_dlc; i += 1) {
        element->second.push_back(frame[i]);
    }
}

std::vector<uint8_t> LongFrameHandler::steal_frame_group(uint8_t group_id)
{
    // Return the underlying vector of bytes
    // that we have for the group.

    auto element = m_buffers.find(group_id);

    if (element == m_buffers.end()) {
        return {};
    }

    auto data_buffer = std::move(element->second);

    m_buffers.erase(element);

    return data_buffer;
}
