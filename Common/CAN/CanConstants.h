#pragma once

#include <stdint.h>

// Reserved Bytes
// 0-127 - Reserved For Normal Characters/Text
// 128-200 - Reserved For CAN::Protocol
// 200-214 - Reserved For CAN::Primitive
// 255 - Reserved For CAN::NOT_FOUND

namespace CAN {
namespace Protocol {

const uint8_t ACKNOWLEDGEMENT = 128;
const uint8_t CHECK_IN = 129;

const uint8_t NEW_UID = 130;
const uint8_t REPLY_NEW_UID = 131;

const uint8_t UPDATE_INFO = 132;
const uint8_t REPLY_UPDATE_INFO = 133;

const uint8_t ERROR_GENERIC = 134;
const uint8_t ERROR_MODULE = 135;

const uint8_t COMMAND = 136;
const uint8_t COMMAND_INPUT = 137;
const uint8_t REPLY_COMMAND = 138;

// Event Specific
const uint8_t EVENT_ADD = 139;
const uint8_t EVENT_REMOVE = 140;
const uint8_t EVENT_REMOVE_ALL = 141;
const uint8_t EVENT_SEND_STORED = 142;
const uint8_t REPLY_EVENT_SEND_STORED = 143;
const uint8_t EVENT_RUN_NEXT_PART = 144;

// TODO: Docs
const uint8_t FORMAT_EEPROM = 145;
const uint8_t INVALID = 199;

} // namespace Protocol

namespace Primitive {

const uint8_t UNSIGNED_1_BYTES = 200;
const uint8_t UNSIGNED_2_BYTES = 201;
const uint8_t UNSIGNED_4_BYTES = 202;
const uint8_t UNSIGNED_8_BYTES = 203;

const uint8_t SIGNED_1_BYTES = 204;
const uint8_t SIGNED_2_BYTES = 205;
const uint8_t SIGNED_4_BYTES = 206;
const uint8_t SIGNED_8_BYTES = 207;

const uint8_t FLOAT_4_BYTES = 208;
const uint8_t DOUBLE_8_BYTES = 209;

const uint8_t BOOL_1_BYTES = 210;

const uint8_t VOID = 211;

} // namespace Primitive

inline uint8_t primitive_size(uint8_t primitive)
{
    // TODO: is this the best location for this function?
    switch (primitive) {

    case Primitive::UNSIGNED_1_BYTES:
    case Primitive::SIGNED_1_BYTES:
        return 1;

    case Primitive::UNSIGNED_2_BYTES:
    case Primitive::SIGNED_2_BYTES:
        return 2;

    case Primitive::UNSIGNED_4_BYTES:
    case Primitive::SIGNED_4_BYTES:
    case Primitive::FLOAT_4_BYTES:
        return 4;

    case Primitive::UNSIGNED_8_BYTES:
    case Primitive::SIGNED_8_BYTES:
    case Primitive::DOUBLE_8_BYTES:
        return 8;

    case Primitive::BOOL_1_BYTES:
        return 1;

    case Primitive::VOID:
        return 0;
    default:
        return 0;
    }
}

// Errors Generated Before Trying To Parse an Input
namespace GENERIC_ERROR {

const uint8_t NOT_READY = 1;
const uint8_t UNKNOWN_ERROR = 2;
const uint8_t COMMAND_NOT_FOUND = 3;
const uint8_t INCOMPLETE_ARGUMENTS = 4;
const uint8_t INVALID_ARGUMENTS = 5;
const uint8_t COMMAND_NOT_AVAILABLE = 6;
const uint8_t BUSY = 7;
const uint8_t FRAME_TOO_LONG = 8;
const uint8_t PROTOCOL_NOT_SUPPORTED = 9;
const uint8_t FAILED_TO_READ_MESSAGE = 10;

} // namespace GENERIC_ERROR

// Reserved CAN IDs:
namespace UID {

const uint16_t ANY = 1;
const uint16_t MCM = 2;
const uint16_t CCM = 3;

} // namespace UID

namespace Priority {

// Arduino libraries like to define LOW and HIGH as
// macro names, which is a bit annoying.

const uint8_t NORMAL = 3;    // 0011
const uint8_t MEDIUM = 2;    // 0010
const uint8_t IMPORTANT = 1; // 0001
const uint8_t CRITICAL = 0;  // 0000

} // namespace Priority

// TODO: Remove the need for this.
// Used in functions that return a uint8_t as a marker byte
const uint8_t NOT_FOUND = 255;

} // namespace CAN