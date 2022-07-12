#pragma once

template<typename T>
constexpr auto seconds_to_ms(T seconds) -> decltype(seconds)
{
    return seconds * 1000;
}
