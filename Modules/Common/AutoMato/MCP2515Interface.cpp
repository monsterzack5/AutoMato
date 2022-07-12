#include "MCP2515Interface.h"

#include <Arduino.h>
#include <Log.hpp>
#include <cstring> // For memcpy()
#include <mcp2515.h>

MCP2515Interface::MCP2515Interface(uint8_t chip_select_pin, CAN_SPEED speed /* CAN_125KBPS */) noexcept
    : m_mcp2515(chip_select_pin)
{
    m_mcp2515.reset();
    m_mcp2515.setBitrate(speed);
    m_mcp2515.setNormalMode();
}

bool MCP2515Interface::read_frame(CAN::Frame* frame)
{
    can_frame raw_frame;

    if (m_mcp2515.readMessage(&raw_frame) == MCP2515::ERROR_OK) {
        // we have a frame!
        *frame = static_cast<CAN::Frame>(raw_frame);
        return true;
    }
    return false;
}

bool MCP2515Interface::send_frame(const CAN::Frame& frame)
{
    can_frame converted_frame = static_cast<can_frame>(frame);
    MCP2515::ERROR rc = m_mcp2515.sendMessage(&converted_frame);
    return (rc == MCP2515::ERROR::ERROR_OK);
}
