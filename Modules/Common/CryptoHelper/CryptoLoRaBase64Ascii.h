#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
// Slower performance, uses less memory (single buffer, replace in place)
// Returns the length of the output string.
uint16_t decode_LoRa_base64_string_inplace(char* input, uint16_t length);

// Faster performance, uses more memory (two different buffers)
// Returns the length of the output string.
// dest[] should be half the size of src, plus one. ((MAX OUTPUT LENGTH / 2) + 1
uint16_t decode_LoRa_base64_string(char* dest, const char* src, uint16_t length);

#ifdef __cplusplus
}
#endif