#pragma once

#include <Arduino.h>
#include <Stream.h>
#include <stdint.h>

// TODO: Move these inside class
const uint16_t MAX_E5_OUTPUT_LENGTH = 528;
const uint16_t MAX_SERIAL_INPUT_LENGTH = 256;
const uint16_t P2P_MESSAGE_PREAMBLE_LENGTH = 11; // Length of |+TEST: RX "|12345"

const uint8_t ASCII_NEW_LINE = 10;
const uint8_t ASCII_CARRIAGE_RETURN = 13;
const uint8_t ASCII_BACKSPACE = 8;

enum class PrintMode : uint8_t {
    E5_Output,
    E5_Input,
    None,
    All,
};

enum class LogLevel : uint8_t {
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
    PANIC,
    QUIET,
};

class LoraE5 {
public:
    using TimeType = decltype(millis());
    using P2PCallback = auto (*)(const char* message, uint16_t length) -> void;

    LoraE5() = delete;
    LoraE5(Stream& lora_interface, Stream& logging_interface);
    LoraE5(Stream& lora_interface);
    ~LoraE5() = default;

    void loop();
    void interactive_loop();

    void print_mode(PrintMode mode) { m_print_mode = mode; }
    void log_level(LogLevel level);

    // --------
    template<typename T, typename... Args>
    void send(const T& value, const Args&... args) const
    {
        write_impl(value);
        return send(args...);
    }

    template<typename T>
    void send(const T& value) const
    {
        write_impl(value);
        return send();
    }

    void send() const
    {
        write_impl("\r\n");
        m_lora_interface.flush();
    }
    // --------

    uint16_t wait_for_anything(uint32_t timeout);
    bool wait_for(const char* message, uint32_t timeout);

    void setup_p2p_mode();
    void send_p2p_message(const char* message);
    void set_p2p_message_callback(P2PCallback callback) { m_callback = callback; }

    static uint16_t convert_lora_rx_to_text_inplace(char* buffer, uint16_t length);
    static uint16_t convert_lora_rx_to_text(char* dest, const char* src, uint16_t length);
    uint8_t eeprom_read(uint16_t address) const;
    bool eeprom_write(uint16_t address, uint8_t byte) const;

    void enable_low_power_mode();
    void disable_low_power_mode();

    void enable_low_power_mode(uint32_t timeout_milliseconds);

    void enable_extreme_low_power_mode();
    void disable_extreme_low_power_mode();

private:
    void init();
    void print_e5_output() const;

    template<typename T>
    void write_impl(const T& value) const;

    // The E5 Always ends it's messages with \r\n
    const char E5_LAST_CHAR = '\n';

    Stream& m_lora_interface;   // & -> Required
    Stream* m_serial_interface; // * -> Optional
    PrintMode m_print_mode { PrintMode::All };

    char m_buffer[MAX_E5_OUTPUT_LENGTH];
    uint16_t m_buffer_index { 0 };

    char m_input_buffer[MAX_SERIAL_INPUT_LENGTH];
    uint16_t m_input_index { 0 };

    enum class LowPowerMode : uint8_t {
        Disabled,
        Moderate,
        Extreme
    };

    LowPowerMode m_low_power_mode { LowPowerMode::Disabled };

    P2PCallback m_callback;
};