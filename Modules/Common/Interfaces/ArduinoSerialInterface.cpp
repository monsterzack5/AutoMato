#include "ArduinoSerialInterface.h"

#include <CanSerializer.h>
#include <HardwareSerial.h>
#include <Log.hpp>
#include <SerialCommon.h>

ArduinoSerialInterface::ArduinoSerialInterface(HardwareSerial* serial_port) noexcept
    : m_serial(serial_port)
{
    if (!serial_port) {
        DEBUG_PRINTLN("INVALID HardwareSerial POINTER GIVEN TO ArduinoSerialInterface!");
    }
}

// This function maintains its own state.
bool ArduinoSerialInterface::read_frame(CAN::Frame* frame)
{
    static bool currently_reading = false;

    static uint8_t buffer[CAN::Frame::MAX_SERIALIZED_SIZE];

    static uint8_t buffer_index = 0;

    uint8_t buffer_byte = 0;

    if (!m_serial->available()) {
        return false;
    }

    buffer_byte = m_serial->read();

    if (buffer_byte == SERIAL_MESSAGE_START_BYTE && !currently_reading) {
        // New Frame
        currently_reading = true;
        return false;
    }

    if (buffer_byte == SERIAL_MESSAGE_STOP_BYTE && currently_reading) {
        // We're finished reading a frame
        currently_reading = false;
        *frame = CAN::deserialize_frame(buffer, buffer_index);
        buffer_index = 0;
        return true;
    }

    if (currently_reading) {
        buffer[buffer_index++] = buffer_byte;
        return false;
    }

    if (!currently_reading) {
        // We parsed a byte of data, but we didn't get a start byte,
        m_serial->println("We just read garbage from the serial monitor!");
        return false;
    }

    __builtin_unreachable();
    return false;
}

bool ArduinoSerialInterface::send_frame(const CAN::Frame& frame)
{
    uint8_t buffer[CAN::Frame::MAX_SERIALIZED_SIZE];

    uint8_t bytes_used = CAN::serialize_frame(frame, buffer);

    m_serial->write(SERIAL_MESSAGE_START_BYTE);
    for (uint8_t i = 0; i < bytes_used; i += 1) {
        m_serial->write(buffer[i]);
    }
    m_serial->write(SERIAL_MESSAGE_STOP_BYTE);

    return true;
}