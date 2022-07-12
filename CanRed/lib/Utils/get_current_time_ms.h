#pragma once

#include <chrono>
#include <stdint.h>

inline uint64_t get_current_time_ms()
{
    // In Python: time.time()
    // In Javascript: Date.now()
    // In CPP:
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        time_point_cast<milliseconds>(high_resolution_clock::now())
            .time_since_epoch())
        .count();
}
