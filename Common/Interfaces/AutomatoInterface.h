#pragma once

#include <CanFrame.h>

// TODO:
// Interfaces can fail, and become invalid at any point in time.
// I think implementing proper error handling for them is more
// than a good idea.
// A rough outline of how this could be implemented:
// - Return a int32_t instead of a bool, which can represent
//   error codes.
// - Implement is_working()/is_functional() function,
//   that check's if an interface is currently valid.
// - Implement a retry_setup() function that tries
//   to restart the interface and gets it back to a working
//   state
// More over, having handling for deleting threads for failed
// interfaces, and decent error logging for why an interface failed
// generally implemented. But this will ideally be implemented
// when the system for loading interfaces from a config file
// is created.

class AutomatoInterface {
public:
    // Blocks indefinitely trying to read a frame
    // And will fill empty_frame with the read frame, if possible
    // Returns true if a frame was correctly read,
    // or false if the function timed out, or read an invalid frame.
    virtual bool read_frame(CAN::Frame* empty_frame) = 0;

    // Send the given frame, return true if the frame
    // sent properly.
    virtual bool send_frame(const CAN::Frame& frame) = 0;
};
