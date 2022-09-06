#pragma once

#include "AutomatoInterface.h"

#include <HardwareSerial.h>

class ArduinoSerialInterface : public AutomatoInterface {
public:
    explicit ArduinoSerialInterface(HardwareSerial* serial_port) noexcept;
    ~ArduinoSerialInterface() = default;

    bool read_frame(CAN::Frame* frame) override;
    bool send_frame(const CAN::Frame& frame) override;

private:
    HardwareSerial* m_serial;
};
