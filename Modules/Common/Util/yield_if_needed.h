#pragma once

#ifdef ESP32
// FreeRTOS yield.
#    define YIELD_IF_NEEDED() vPortYield()
#elif defined ESP8266
#    define YIELD_IF_NEEDED() yield()
#else
#    define YIELD_IF_NEEDED()
#endif