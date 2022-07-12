#pragma once

// This should be somehow merged with ArduinoSerialInterface
// or, we should define some common start and stop bytes.

#include "AutomatoInterface.h"

class SerialInterface : public AutomatoInterface {
public:
    explicit SerialInterface(const char* serial_port = "/dev/ttyUSB0") noexcept;
    ~SerialInterface() = default;

    bool read_frame(CAN::Frame* frame) override;
    bool send_frame(const CAN::Frame& frame) override;

private:
    const char* m_serial_port_filename;

    int32_t get_file_descriptor();
    bool configure_serial_port();

    void print_module_output();

    std::vector<uint8_t> m_bytes;
    std::vector<char> m_module_output;
    int32_t m_file_descriptor;
};
