#pragma once

#include "CanFrame.h"

#include <AnyType.h>

#ifdef CANRED
#    include <fmt/color.h>
#    include <fmt/format.h>
#endif

// TODO: Shrink the amount of data we need to send.
// Don't serialize and send the can_dlc, we can guess it
// on the other end
// We don't need several of the bits in the can_id

// TODO: Make documentation about this
// TODO: Move the constants to CanConstants.h
// TODO: CAN::Serializer::?

namespace CAN {

// Format:
// Byte[0]: 1st Byte of can_id
// Byte[1]: 2nd Byte of can_id
// Byte[2]: 3rd Byte of can_id
// Byte[3]: 4th Byte of can_id
// Byte[4]: can_dlc byte
// Byte[5...can_dlc]: Can Frame Data Bytes

// Given an array, and the size of the array, return a CAN::Frame
inline Frame deserialize_frame(const uint8_t data[], const uint8_t data_size)
{

    if (data_size > Frame::MAX_SERIALIZED_SIZE) {
#ifdef CANRED
        fmt::print(fmt::fg(fmt::terminal_color::red), "ERROR! FRAME TO LARGE TO PARSE GIVEN TO CAN::DESERIALIZE_FRAME!\n");
#endif
        return {};
    }

    uint32_t can_id = 0;

    memcpy(&can_id, data, 4);

    uint8_t can_dlc = data[4];

    if (can_dlc > 8) {
        return {};
    }

    uint8_t frame_data[8] = { 0 };

    memcpy(&frame_data, &data[5], data_size - 4 - 1);

    // TODO: Should frame always take a const u8 ptr?
    return Frame(ID(can_id), frame_data, can_dlc);
}

// Given a Frame, and a Buffer, serialize the frame,
// and return the amount of bytes used.

inline uint8_t serialize_frame(const Frame& frame, uint8_t buffer[])
{

    if (frame.can_dlc > 8) {
        // Error!
        return 0;
    }

    uint32_t cached_id = frame.formatted_can_id();

    memcpy(buffer, &cached_id, 4);

    buffer[4] = frame.can_dlc;

    memcpy(&buffer[5], &frame.data, frame.can_dlc);

    // 4 bytes for can_id
    // 1 byte for can_dlc
    // + can_dlc number of bytes
    return (4 + 1 + frame.can_dlc);
}

} // namespace CAN
