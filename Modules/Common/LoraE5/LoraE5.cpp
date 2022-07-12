#include "LoraE5.h"
#include <Arduino.h>

// TESTING
#include <print_u8_array.h>

LoraE5::LoraE5(Stream& lora_interface, Stream& logging_interface)
    : m_lora_interface(lora_interface)
    , m_serial_interface(&logging_interface)
{
    init();
}

LoraE5::LoraE5(Stream& lora_interface)
    : m_lora_interface(lora_interface)
{
    init();
}

void LoraE5::init()
{
    memset(m_buffer, 0, MAX_E5_OUTPUT_LENGTH);
    memset(m_input_buffer, 0, MAX_SERIAL_INPUT_LENGTH);
}

void LoraE5::loop()
{
    const auto is_p2p_message = [&](uint16_t offset) {
        return (strncmp(&m_buffer[offset], "+TEST: RX \"", P2P_MESSAGE_PREAMBLE_LENGTH) == 0 && m_callback);
    };

    const auto p2p_message_length = [&](uint16_t offset) {
        // Expected Format: +TEST: RX "1234"
        for (uint16_t i = offset + P2P_MESSAGE_PREAMBLE_LENGTH;; i += 1) {
            if (m_buffer[i] == '"') {
                return static_cast<uint16_t>(i - (offset + P2P_MESSAGE_PREAMBLE_LENGTH));
            }
        }

        __builtin_unreachable();
        return static_cast<uint16_t>(0);
    };

    auto bytes_read = wait_for_anything(100); // TODO: is 100 ms the best?

    if (bytes_read > 0) {
        // Why did we read something?
        // was it a message?
        // if so, pass it to the callback!

        for (uint16_t i = 0; i < bytes_read; i += 1) {

            if (is_p2p_message(i)) {
                auto message_length = p2p_message_length(i);

                // Insert a null byte where the ending " would be,
                // So the callback function can have an easier time parsing it.
                m_buffer[i + P2P_MESSAGE_PREAMBLE_LENGTH + message_length] = '\0';

                m_callback(&m_buffer[i + P2P_MESSAGE_PREAMBLE_LENGTH], message_length);
            }

            for (uint16_t j = i; j < bytes_read; j += 1) {
                // Maybe check here if we're on the last char.
                // tho i will get inc'ed above
                if (m_buffer[j] == E5_LAST_CHAR) {
                    i = j;
                    break;
                }
            }
        }
    }
}

template<typename T>
void LoraE5::write_impl(const T& value) const
{
    m_lora_interface.write(value);
}

template<>
inline void LoraE5::write_impl<const char*>(const char* const& value) const
{
    auto length = strlen(value);

    // bool is_start_of_command = (length >= 2 && (value[0] == 'a' || value[0] == 'A') && (value[1] == 't' || value[1] == 'T') && value[2] == '+');
    bool is_start_of_command = (length > 2 && (value[0] == 'a' || value[0] == 'A') && (value[1] == 't' || value[1] == 'T') && value[2] == '+');

    // FOR TESTING!
    if (is_start_of_command && m_low_power_mode == LowPowerMode::Extreme) {
        m_serial_interface->println("Adding Extra Bytes!");
    }

    if ((m_print_mode == PrintMode::All || m_print_mode == PrintMode::E5_Input) && m_serial_interface) {
        // Append ->E5 if the value starts with "AT+", case insensitive
        if (is_start_of_command) {
            m_serial_interface->print("\n->E5: ");
        }
        m_serial_interface->write(value, length);
    }

    if (is_start_of_command && m_low_power_mode == LowPowerMode::Extreme) {
        m_lora_interface.write("\xFF\xFF\xFF\xFF");
    }

    m_lora_interface.write(value);
}

// Wait timeout milliseconds for the E5 to output anything
// Returns the number of characters read
uint16_t LoraE5::wait_for_anything(uint32_t timeout)
{
    TimeType current_time = millis();
    TimeType end_time = current_time + timeout;

    m_buffer_index = 0;

    while (current_time < end_time) {
        current_time = millis();

        while (m_lora_interface.available()) {
            m_buffer[m_buffer_index++] = m_lora_interface.read();

            if (m_buffer[m_buffer_index - 1] == E5_LAST_CHAR) {
                print_e5_output();
                auto bytes_read = m_buffer_index;
                m_buffer_index = 0;
                return bytes_read;
            }
            delayMicroseconds(1045); // (10 / 9600) -> BIT TIMING (todo)
        }
    }
    return 0;
}

// Waits timeout milliseconds for a specific string
// returns if the string was found.
bool LoraE5::wait_for(const char* message, uint32_t timeout)
{
    TimeType current_time = millis();
    TimeType end_time = current_time + timeout;

    m_buffer_index = 0;

    while (current_time < end_time) {
        current_time = millis();
        // TODO: This might read something 1ms before the timeout ends
        // which will then call wait_for_anything again, with the full
        // timeout again, which can cause timing issues.
        auto bytes_read = wait_for_anything(timeout);
        if (bytes_read > 0) {
            if (strncmp(m_buffer, message, strlen(message)) == 0) {
                return true;
            }

            m_buffer_index = 0;
        }
    }

    return false;
}

void LoraE5::setup_p2p_mode()
{
    // TODO: Error checking!
    // TODO: Take any config mode
    // TODO: Print what these return!
    send("AT+MODE=TEST");
    wait_for_anything(250);
    send("AT+TEST=RFCFG,915,SF10,125,8,8,14,ON,OFF,ON");
    // send("AT+TEST=RFCFG,915,SF10,125,12,15,14,ON,OFF,ON");
    wait_for_anything(250);

    // Enable receive mode (by default)
    send("AT+TEST=RXLRPKT");
    wait_for_anything(250);
}

void LoraE5::send_p2p_message(const char* message)
{
    send("AT+TEST=TXLRSTR,\"", message, "\"");

    // TODO: if this fails, return some variety of error
    wait_for("+TEST: TX DONE", 1000);

    // Switch mode back to receiving
    // Maybe this isn't the best idea?
    send("AT+TEST=RXLRPKT");
    wait_for_anything(250);
}

void LoraE5::print_e5_output() const
{
    const char MESSAGE_PREFIX[] = "<-E5: ";
    if ((m_print_mode == PrintMode::All || m_print_mode == PrintMode::E5_Output) && m_serial_interface) {
        m_serial_interface->write(MESSAGE_PREFIX);

        for (uint16_t i = 0; i < m_buffer_index; i += 1) {
            // TODO: THIS IS DEPRECATED AND GOING TO BE REMOVED
            m_serial_interface->write(m_buffer[i]);
            // Sometimes we read several lines at once into the buffer
            // So, check to see if the buffer contains a new line
            // and if so, print "<-E5: " before it.
            if (m_buffer[i] == E5_LAST_CHAR && i != (m_buffer_index - 1)) {
                m_serial_interface->write(MESSAGE_PREFIX);
            }
        }
    }
}

void LoraE5::interactive_loop()
{
    if (!m_serial_interface) {
        return;
    }

    const auto clear_current_line = [&]() {
        const char clear_line_seq[] = { 27, '[', '2', 'K', '\r' };
        m_serial_interface->write(clear_line_seq, 5);
    };

    while (m_serial_interface->available()) {
        uint8_t byte_read = m_serial_interface->read();

        // Enter Key was pressed
        if ((byte_read == ASCII_NEW_LINE || byte_read == ASCII_CARRIAGE_RETURN)) {
            if (m_input_index == 0) {
                break;
            }

            // Clear the user input
            clear_current_line();

            m_input_buffer[m_input_index] = '\0'; // TODO:
            send(m_input_buffer);
            m_input_index = 0;
            break;
        }

        // Backspace key was pressed
        if (byte_read == ASCII_BACKSPACE) {
            if (m_input_index == 0) {
                break;
            }
            m_input_index -= 1;

            clear_current_line();

            // print out everything from start of buffer
            for (uint16_t i = 0; i < m_input_index; i += 1) {
                m_serial_interface->write(m_input_buffer[i]);
            }
            break;
        }

        m_input_buffer[m_input_index++] = byte_read;

        // Loop Back (print typed characters on the serial monitor)
        m_serial_interface->write(byte_read);
    }

    while (m_lora_interface.available()) {
        uint8_t byte_read = m_lora_interface.read();
        m_buffer[m_buffer_index++] = byte_read;
    }

    if (m_buffer_index > 0 && m_buffer[m_buffer_index - 1] == E5_LAST_CHAR) {
        print_e5_output();
        m_buffer_index = 0;
    }
}

void LoraE5::log_level(LogLevel level)
{
    switch (level) {
    case LogLevel::DEBUG:
        send("AT+LOG=DEBUG");
        break;
    case LogLevel::INFO:
        send("AT+LOG=INFO");
        break;
    case LogLevel::WARN:
        send("AT+LOG=WARN");
        break;
    case LogLevel::ERROR:
        send("AT+LOG=ERROR");
        break;
    case LogLevel::FATAL:
        send("AT+LOG=FATAL");
        break;
    case LogLevel::PANIC:
        send("AT+LOG=PANIC");
        break;
    case LogLevel::QUIET:
        send("AT+LOG=QUIET");
        break;

    default:
        __builtin_unreachable();
    }

    wait_for_anything(100);
}

// Slower performance, uses less memory (single buffer, replace in place)
// Returns the length of the output string.
uint16_t LoraE5::convert_lora_rx_to_text_inplace(char* buffer, uint16_t length)
{
    static_assert(length / 2 == 0);
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

    const auto hex_str_to_dec = [](const char* hex, int8_t length) {
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

    for (int i = 0; i < length; i += 2) {
        // 1.
        auto value = hex_str_to_dec(&buffer[i], 2);
        // 2.
        buffer[i] = value;
    }

    // 3.
    int move_index = 1;
    for (int i = 0; i != length; i += 2) {

        buffer[i + 2 - move_index] = buffer[i + 2];
        move_index += 1;
        if (move_index == (length / 2)) {
            break;
        }
    }

    // 4.
    buffer[(length / 2)] = '\0';

    return length / 2;
}

// Faster performance, uses more memory (two different buffers)
// Returns the length of the output string.
// dest[] should be half the size of src, plus one. ((MAX_E5_OUTPUT_LENGTH / 2) + 1)
uint16_t LoraE5::convert_lora_rx_to_text(char* dest, const char* src, uint16_t length)
{
    const auto hex_str_to_dec = [](const char* hex, int8_t length) {
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

    for (int i = 0; i < length; i += 2) {
        // 1.
        auto value = hex_str_to_dec(&src[i], 2);
        dest[i >> 1] = value;
    }

    dest[(length / 2)] = '\0';

    return length / 2;
}

void LoraE5::enable_low_power_mode()
{
    m_low_power_mode = LowPowerMode::Moderate;

    send("AT+LOWPOWER");
}

void LoraE5::enable_low_power_mode(uint32_t timeout_milliseconds)
{
    m_low_power_mode = LowPowerMode::Moderate;

    send("AT+LOWPOWER=", timeout_milliseconds);
}

void LoraE5::disable_low_power_mode()
{
    if (m_low_power_mode == LowPowerMode::Disabled) {
        return;
    }

    m_low_power_mode = LowPowerMode::Disabled;

    send("AT");
    wait_for_anything(25);
}

void LoraE5::enable_extreme_low_power_mode()
{
    if (m_low_power_mode == LowPowerMode::Extreme) {
        return;
    }

    m_low_power_mode = LowPowerMode::Extreme;
    send("AT+LOWPOWER=AUTOON");
    wait_for_anything(250);
}
void LoraE5::disable_extreme_low_power_mode()
{
    if (m_low_power_mode != LowPowerMode::Extreme) {
        return;
    }

    m_low_power_mode = LowPowerMode::Disabled;
    send("AT+LOWPOWER=AUTOOFF");
    wait_for_anything(250);
}