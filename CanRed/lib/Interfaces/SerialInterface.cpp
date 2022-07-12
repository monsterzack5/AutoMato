#include "SerialInterface.h"

#include <CanSerializer.h>
#include <SerialCommon.h>
#include <fcntl.h> /* open() */
#include <fmt/color.h>
#include <fmt/format.h>
#include <string.h>  /* strerror() */
#include <termios.h> /* termios */
#include <thread>
#include <unistd.h> /* read() */

SerialInterface::SerialInterface(const char* serial_port /* /dev/ttyUSB0 */) noexcept
    : m_serial_port_filename(serial_port)
{
    m_file_descriptor = get_file_descriptor();

    if (m_file_descriptor == -1) {
        printf("SerialInterface: Could not open serial port!\n");
        exit(0);
    }

    m_bytes.reserve(CAN::Frame::MAX_SERIALIZED_SIZE);
    configure_serial_port();

    m_module_output.reserve(512); // 512 Bytes
}

// This function maintains its own state.
bool SerialInterface::read_frame(CAN::Frame* frame)
{
    // const size_t READ_BUFFER_MAX_SIZE = 1024; // 1KB TODO
    const size_t READ_BUFFER_MAX_SIZE = 1; // 1B
    static uint8_t buffer[READ_BUFFER_MAX_SIZE];
    static bool currently_reading = false;

    ssize_t bytes_read = read(m_file_descriptor, &buffer, READ_BUFFER_MAX_SIZE);

    if (bytes_read == -1) {
        fmt::print("Some error happened with read in serial interface\n");
        return false;
    }

    if (bytes_read == 0) {
        print_module_output();
        // We timed out, return control flow back to whoever called us.
        return false;
    }

    for (ssize_t i = 0; i < bytes_read; i += 1) {

        if (buffer[i] == SERIAL_MESSAGE_START_BYTE && !currently_reading) {
            // New Frame

            currently_reading = true;
            continue;
        }
        if (buffer[i] == SERIAL_MESSAGE_STOP_BYTE && currently_reading) {
            // We're finished reading a frame

            currently_reading = false;

            *frame = CAN::deserialize_frame(m_bytes.data(), m_bytes.size());

            m_bytes.clear();
            return true;
        }

        if (currently_reading) {
            // We were expecting a byte and got one.
            m_bytes.push_back(buffer[i]);
            continue;
        }

        if (!currently_reading) {
            // We parsed a byte of data, but we didn't get a start byte,
            // This might be garbage, or it might be a module we're
            // connected to via serial, sending out bytes via
            // Serial.print / .println / .write. etc
            // We accumulate these bytes, and print them out when
            // we receive a \n, so we can print them out via fmt::print
            // As an easy way of seeing module logging
            m_module_output.push_back(buffer[i]);

            if (buffer[i] == '\n') {
                print_module_output();
            }
            continue;
        }
    }

    __builtin_unreachable();
    return false;
}

bool SerialInterface::send_frame(const CAN::Frame& frame)
{
    // + 2 For start/stop bytes
    uint8_t buffer[CAN::Frame::MAX_SERIALIZED_SIZE + 2];

    buffer[0] = SERIAL_MESSAGE_START_BYTE;

    uint8_t bytes_used = CAN::serialize_frame(frame, &buffer[1]);

    buffer[1 + bytes_used] = SERIAL_MESSAGE_STOP_BYTE;

    auto write_rc = write(m_file_descriptor, buffer, bytes_used + 2);

    return write_rc != -1;
}

void SerialInterface::print_module_output()
{
    if (m_module_output.size() == 0) {
        return;
    }

    fmt::print(fmt::fg(fmt::color::yellow), "> {}", fmt::join(m_module_output, ""));

    m_module_output.clear();
}

int32_t SerialInterface::get_file_descriptor()
{
    return open(m_serial_port_filename, O_RDWR | O_NOCTTY);
}

bool SerialInterface::configure_serial_port()
{

    termios serial_config;

    // Get the currently saved configuration for the serial port
    if (tcgetattr(m_file_descriptor, &serial_config) != 0) {
        printf("Error from tcgetattr: %s\n", strerror(errno));
        return false;
    }

    // Copy Arduino Default: 8 Data Bits, No Parity, One Stop Bit.
    serial_config.c_cflag &= ~CSIZE;
    serial_config.c_cflag |= CS8;
    serial_config.c_cflag &= ~PARENB;
    serial_config.c_cflag &= ~CSTOPB;

    // Disable RTS/CTS hardware flow control
    serial_config.c_cflag &= ~CRTSCTS;

    // Let us read from the serial port
    serial_config.c_cflag |= CREAD;

    // Ignores special cases on the serial line
    serial_config.c_cflag |= CLOCAL;

    // Disable canonical mode, which lets us receive data as soon as it arrives
    serial_config.c_lflag &= ~ICANON;

    // Disables every echo setting, just in case
    serial_config.c_lflag &= ~ECHO;
    serial_config.c_lflag &= ~ECHOE;
    serial_config.c_lflag &= ~ECHONL;

    // Disables special handling of INTR, QUIT, and SUSP
    serial_config.c_lflag &= ~ISIG;

    // Disable software flow control, for some reason?
    serial_config.c_iflag &= ~(IXON | IXOFF | IXANY);

    // Disable any extra special handling?
    serial_config.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    // Don't interpret any special characters
    serial_config.c_oflag &= ~OPOST;
    // Don't convert \n to \r\n
    serial_config.c_oflag &= ~ONLCR;

    // https://web.archive.org/web/20211130184440/https://blog.mbedded.ninja/programming/operating-systems/linux/linux-serial-ports-using-c-cpp/
    // Section 9, "VMIN and VTIME (c_cc)"
    // Wait indefinitely for atleast 1 byte
    serial_config.c_cc[VMIN] = 1;
    serial_config.c_cc[VTIME] = 0;

    // Set input, and output baud
    cfsetispeed(&serial_config, B115200);
    cfsetospeed(&serial_config, B115200);

    // Write the config to the serial port
    if (tcsetattr(m_file_descriptor, TCSANOW, &serial_config) != 0) {
        printf("Error from tcsetattr: %s\n", strerror(errno));
        return false;
    }
    return true;
}
