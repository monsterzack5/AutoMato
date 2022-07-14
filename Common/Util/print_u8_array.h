#pragma once

#ifdef CANRED

#    include <fmt/color.h>
#    include <fmt/format.h>

// TODO: Add an argument to set the name of the array.
inline void print_u8_array(const uint8_t array[], const uint8_t array_size, fmt::terminal_color color = fmt::terminal_color::white)
{
    for (uint8_t i = 0; i < array_size; i += 1) {
        const uint8_t array_width = (array_size > 9) ? (array_size > 99) ? 3 : 2 : 1;
        auto copy = array[i];

        if (copy < 14) {
            copy = ' ';
        }

        fmt::print(fmt::fg(color), "array[{0:{1}}]: {2:08b} | {2:3d} | {2:#04x} | {3:c}\n", i, array_width, array[i], copy);
    }
}

#elif defined ARDUINO
#    include "print_bin.h"

inline void print_u8_array(const uint8_t array[], const size_t array_size)
{
    // TODO:
    // const uint8_t array_width = (array_size > 9) ? (array_size > 99) ? 3 : 2 : 1;

    for (size_t i = 0; i < array_size; i += 1) {
        Serial.print("data[");
        if (i < 10) {
            Serial.print(" ");
        }

        Serial.print(i);
        Serial.print("]: ");
        print_bin(array[i]);
        Serial.print(" | ");

        if (array[i] < 10) {
            Serial.print(" ");
        }

        Serial.print(array[i], DEC);
        Serial.print(" | 0x");
        if (array[i] < 16) {
            Serial.print("0");
        }
        Serial.print(array[i], HEX);

        // print as char!

        Serial.print(" | ");

        if (array[i] < 14) {
            Serial.print(' ');
        } else {
            Serial.print(static_cast<char>(array[i]));
        }

        Serial.print("\n");
    }
    Serial.print("\n");
}

#elif defined ZEPHYR
#    include "print_bin.h"

inline void print_u8_array(const uint8_t array[], const size_t array_size)
{
    for (size_t i = 0; i < array_size; i += 1) {

        printk("data[%2u]: ", i);
        print_bin(array[i]);
        printk(" | %3u | 0x%02X | ", array[i], array[i]);

        if (array[i] > 31) {
            printk("%c", (char)array[i]);
        }

        printk("\n");
    }
    printk("\n");
}

#endif