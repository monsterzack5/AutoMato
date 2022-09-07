#pragma once

#include <stdint.h>
#include <algorithm>

// LongFrame: a frame that specifies that it's part of a group
//            of frames. Denoted by having a specific bit set
//            inside of the frames CAN::ID
// LongFrame Group ID/UID: The first byte in a LongFrame, which
//                         denotes a unique ID for the group.
// LongFrame Index: A special indexing number, we return after
//                  a call to append()
// Note: LongFrames are "over" when the last frame received has
//       less than 8 data bytes. 

// Note: 
// Example Long Frame:
// First Frame:
//              data[0]: LongFrame UID
//              data[1 - 7]: Actual frame data
// Second and Last Frame:
//               data[0]: LongFrame UID
//               data[1-5]: Actual frame data


// Max number of long frame groups we can handle at a time
const uint8_t MAX_LONG_FRAME_GROUPS = 4;

// Max number of bytes we can handle per long frame group
const uint8_t MAX_BYTES_PER_LF_GROUP = 64;

// Note: We try to escape using the groups UID as it's identifier
//       as soon as possible, as we need to convert that to
//       the groups index number with a for loop, and that's 
//       costly
// type-based hint that we take in a index, instead of a uid
using group_index_t = uint8_t;

class LongFrameHandler {
public:
    LongFrameHandler();
    ~LongFrameHandler() = default;


    // Given the frame.data buffer, append that to our internal
    // storage
    group_index_t append(const uint8_t frame_buffer[8], uint8_t data_size);
    
    // Returns the ptr for the given index
    const uint8_t* get_data_ptr(group_index_t group_index) const;
    
    // Clears the stored data for a given index (Don't forget to call!)
    void clear_data(group_index_t group_index);

    // Returns the length of a given index
    uint8_t length(group_index_t group_index) const;
    
    static uint8_t new_longframe_uid() { return rand() % 255 + 1; };

private:
    group_index_t find_index_from_uid(uint8_t group_uid) const;
    group_index_t find_free_buffer() const;

    uint8_t m_long_frame_buffer[MAX_LONG_FRAME_GROUPS][MAX_BYTES_PER_LF_GROUP];

    // This is a lookup table, which given a group_index, returns the length
    // of that frame group.
    uint8_t m_long_frame_buffer_index[MAX_LONG_FRAME_GROUPS];
};