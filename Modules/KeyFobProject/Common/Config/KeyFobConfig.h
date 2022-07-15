#pragma once

#include <drivers/lora.h>
#include <stdint.h>

const uint8_t CRYPTO_AES_GCM_KEY_SIZE = 16;
const uint8_t CRYPTO_AES_GCM_TAG_SIZE = 16;

using MarkerByte_t = uint8_t;
using IV_t = uint32_t;
using RollingCode_t = uint32_t;
using CommandCode_t = uint8_t;

// Both of these values are empirical
const IV_t IV_DEFAULT_VALUE = 50000;
const RollingCode_t ROLLING_CODE_DEFAULT_VALUE = 1000000;

// For a message to be accepted, the given rolling code must be:
// greater than the current rolling code, and less than the current
// rolling code + ROLLING_CODE_MAX_RANGE.
const uint32_t ROLLING_CODE_MAX_RANGE = 1000;

const uint8_t MARKER_BYTE_INDEX = 0;
const uint8_t IV_INDEX = sizeof(MarkerByte_t);
const uint8_t ROLLING_CODE_INDEX = sizeof(IV_t) + IV_INDEX;
const uint8_t COMMAND_CODE_INDEX = sizeof(RollingCode_t) + ROLLING_CODE_INDEX;
//                                   + 1 to convert from index to size
const uint8_t ENCRYPTION_BUFFER_SIZE = COMMAND_CODE_INDEX + 1 + CRYPTO_AES_GCM_TAG_SIZE;
const uint8_t ENCRYPTION_START_INDEX = ROLLING_CODE_INDEX;
const uint8_t ENCRYPTED_DATA_SIZE = sizeof(RollingCode_t) + sizeof(CommandCode_t);
// Always append the tag right after the encrypted data.
const uint8_t ENCRYPTION_TAG_START_INDEX = ENCRYPTION_BUFFER_SIZE - 16;
static lora_modem_config LORA_CONFIG {
    915000000u, // frequency
    BW_125_KHZ, // bandwidth,
    SF_10,      // data rate
    CR_4_5,     // coding_rate
    8,          // preamble_len
    14,         // tx_power
    false,      // tx (to be set by the specific module)
};

const MarkerByte_t MARKER_BYTE = static_cast<MarkerByte_t>(~128);

enum class Command : CommandCode_t {
    NONE = 0,
    UNLOCK_DOORS = 9,
    LOCK_DOORS = 19,
    START_CAR = 29,
};

struct MessageParameters {
    IV_t iv;
    RollingCode_t rolling_code;
};
