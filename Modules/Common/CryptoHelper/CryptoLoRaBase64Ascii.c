#include "CryptoLoRaBase64Ascii.h"

uint8_t hex_str_to_dec(const char* hex, int8_t length)
{
    uint8_t decimal_value = 0;
    uint8_t base = 1;

    for (int8_t i = length - 1; i >= 0; i--) {

        if (hex[i] >= '0' && hex[i] <= '9') {
            decimal_value += (hex[i] - 48) * base;
        } else if (hex[i] >= 'A' && hex[i] <= 'F') {
            decimal_value += (hex[i] - 55) * base;
        }

        base *= 16;
    }

    return decimal_value;
};

uint16_t decode_LoRa_base64_string_inplace(char* input, uint16_t length)
{
    if (length % 2 != 0) {
        // LoRa specific, LoRa output strings should always
        // be divisible by 2.
        return 0;
    }

    // The E5 Outputs messages as a hex encoded string of ascii characters.
    // For Example, the string "Hello World!" converts to 48656C6C6F20576F726C6421

    // Here's an example of parsing "Hello World!":
    // Initial input: 48656C6C6F20576F726C6421
    // Spaced Out:           48  65  6C  6C  6F 20 57  6F  72  6C  64 21 (hex values)
    // Converted to Decimal: 72 101 108 108 111 32 87 111 114 108 100 33 (decimal Values)
    // Printed out: Hello World!

    // Here's how we parse that:
    // 1. For every 2 characters, convert them to decimal
    // 2. Replace the location of the left most digit
    //    with the decimal representation.
    //
    // Example of the buffer at this point:
    //                        These Values Were Never Touched, they are the
    //                        original hex char's
    //     v      v      v     v     v    v    v     v     v     v     v    v
    //  72,8, 101,5, 108,C 108,C 101,F 32,0 87,7 111,F 114,2 108,C 100,4 33,1
    //  ^^    ^^^    ^^^   ^^^   ^^^   ^^   ^^   ^^^   ^^^   ^^^   ^^^   ^^
    //                        These Values we placed in, they are the ASCII Value of
    //                        the converted 2 digit hex strings.
    //
    // 3. "Shrink the array", Remove every odd index, and keep
    //    only the even indexes. Then move them over so they're all
    //    contigious.
    // 4. Append a null byte at the end of the converted array.

    for (int i = 0; i < length; i += 2) {
        // 1.
        uint8_t value = hex_str_to_dec(&input[i], 2);
        // 2.
        input[i] = value;
    }

    // 3.
    int move_index = 1;
    for (int i = 0; i != length; i += 2) {

        input[i + 2 - move_index] = input[i + 2];
        move_index += 1;
        if (move_index == (length / 2)) {
            break;
        }
    }

    // 4.
    input[(length / 2)] = '\0';

    return length / 2;
}

uint16_t decode_LoRa_base64_string(char* dest, const char* src, uint16_t length)
{
    for (int i = 0; i < length; i += 2) {
        // 1.
        uint8_t value = hex_str_to_dec(&src[i], 2);
        dest[i >> 1] = value;
    }

    dest[(length / 2)] = '\0';

    return length / 2;
}
