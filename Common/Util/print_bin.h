#pragma once

#if defined CANRED
#    include <fmt/format.h>
#elif defined ARDUINO
#    include <HardwareSerial.h>
#elif defined ZEPHYR
#    include <sys/printk.h>
#endif

template<typename T>
inline void print_bin(T number)
{
    auto number_of_bits = ((sizeof(T) * 8));
#ifdef CANRED
    fmt::print("{:0{}b}", number, number_of_bits);
#elif defined ARDUINO
    for (int16_t i = number_of_bits - 1; i != -1; i -= 1) {
        Serial.print(((number >> i) & 1));
    }
#elif defined ZEPHYR
    for (int16_t i = number_of_bits - 1; i != -1; i -= 1) {
        printk("%u", ((number >> i) & 1));
    }
#endif
}
