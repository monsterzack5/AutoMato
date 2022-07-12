#pragma once

// This is meant to serve as an easy way to detect the 
// platform, and the framework you're currently using.
// Although, currently it just works for Embedded vs Desktop

#if defined(ESP32) || defined(ESP8266) || defined(ARDUINO) || defined(ZEPHYR) || defined(STCUBE)
#define PLATFORM_EMBEDDED
#else
#define PLATFORM_DESKTOP
#endif