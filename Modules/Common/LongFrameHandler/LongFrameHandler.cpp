#include "LongFrameHandler.h"

#include <CanConstants.h>
#include <Log.hpp>

LongFrameHandler::LongFrameHandler()
{
    // Note: Written out as a for loop for better readability
    for (uint8_t i = 0; i < MAX_LONG_FRAME_GROUPS; i += 1) {
        memset(m_long_frame_buffer[i], 0, MAX_BYTES_PER_LF_GROUP);
    }

    memset(m_long_frame_buffer_index, 0, sizeof(m_long_frame_buffer_index));
}

group_index_t LongFrameHandler::append(const uint8_t frame_buffer[8], uint8_t data_size)
{
    // Check to see if we're storing this group
    uint8_t group_index = find_index_from_uid(frame_buffer[0]);

    if (group_index == CAN::NOT_FOUND) {
        // We're not storing this frame group.

        group_index = find_free_buffer();

        if (group_index == CAN::NOT_FOUND) {
            // We don't have any more free space, so panic and do nothing
            DEBUG_PRINTLN("Error! No empty buffers in LongFrameHandler!");
            return CAN::NOT_FOUND;
        }

        auto* free_block_ptr = m_long_frame_buffer[group_index];

        // We use the first byte in each array to hold the LongFrame's
        // UID, so we can find that same array again the future.
        memcpy(free_block_ptr, frame_buffer, data_size);
        m_long_frame_buffer_index[group_index] += 8;
        return group_index;
    }

    // We're currently storing this group.

    if (m_long_frame_buffer_index[group_index] + data_size > MAX_BYTES_PER_LF_GROUP) {
        DEBUG_PRINTLN("Error! Attempting to store more bytes that we currently allow in a longframe group!");
        return group_index;
    }

    // Get the ptr for the group_index, and offset that by the number
    // of byte's we're already storing
    auto* free_block_ptr = m_long_frame_buffer[group_index] + m_long_frame_buffer_index[group_index];

    // We don't need to store the groups UID twice, so we discard
    // the first byte.
    memcpy(free_block_ptr, &frame_buffer[1], data_size);
    m_long_frame_buffer_index[group_index] += data_size - 1;

    return group_index;
}

const uint8_t* LongFrameHandler::get_data_ptr(group_index_t group_index) const
{
    // +1 to remove the first byte, which is the frame groups uid
    return m_long_frame_buffer[group_index] + 1;
}

uint8_t LongFrameHandler::length(group_index_t group_index) const
{
    return m_long_frame_buffer_index[group_index];
}

void LongFrameHandler::clear_data(group_index_t group_index)
{
    memset(m_long_frame_buffer[group_index], 0, MAX_BYTES_PER_LF_GROUP);
    m_long_frame_buffer_index[group_index] = 0;
}

group_index_t LongFrameHandler::find_index_from_uid(uint8_t group_uid) const
{
    for (uint8_t i = 0; i < MAX_LONG_FRAME_GROUPS; i += 1) {
        if (m_long_frame_buffer[i][0] == group_uid) {
            return i;
        }
    }

    return CAN::NOT_FOUND;
}
group_index_t LongFrameHandler::find_free_buffer() const
{
    for (uint8_t i = 0; i < MAX_LONG_FRAME_GROUPS; i += 1) {
        if (m_long_frame_buffer[i][0] == 0) {
            return i;
        }
    }
    return CAN::NOT_FOUND;
}