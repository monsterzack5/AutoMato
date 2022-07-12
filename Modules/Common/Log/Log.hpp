#pragma once

#define DEBUG_CODE

#ifdef DEBUG_CODE

#    if __has_include(<Arduino.h>)
#        include <Arduino.h>
#        include <HardwareSerial.h>
#        define ENABLE_DEBUG_LOGGING(x) \
            do {                        \
                while (!Serial)         \
                    ;                   \
                Serial.begin(x);        \
            } while (0);
#        define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
#        define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#        define DEBUG_PRINT_VALUE(x, y) \
            Serial.print(x);            \
            Serial.println(y);

#    elif defined STM32 && defined APP_LOG

#        define ENABLE_DEBUG_LOGGING(x) static_assert(false, "ENABLE_DEBUG_LOGGING Not currently implemented for STM32")
#        define DEBUG_PRINT(...) APP_LOG(TS_OFF, VLEVEL_ALWAYS, __VA_ARGS__)
#        define DEBUG_PRINTLN(...)                      \
            APP_LOG(TS_OFF, VLEVEL_ALWAYS, __VA_ARGS__) \
            APP_LOG(TS_OFF, VLEVEL_ALWAYS, "\n")
#        define DEBUG_PRINT_VALUE(x, y) static_assert(false, "DEBUG_PRINT_VALUE Not currently implemented for STM32")

#    endif

#else
#    define ENABLE_DEBUG_LOGGING(x)
#    define DEBUG_PRINT(...)
#    define DEBUG_PRINTLN(...)
#    define DEBUG_PRINT_VALUE(x, y)
#endif
