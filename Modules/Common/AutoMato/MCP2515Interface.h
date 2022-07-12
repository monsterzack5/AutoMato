#pragma once

#include "AutomatoInterface.h"

class MCP2515Interface : public AutomatoInterface {
public:
    explicit MCP2515Interface(uint8_t chip_select_pin, CAN_SPEED speed = CAN_125KBPS) noexcept;
    ~MCP2515Interface() = default;

    bool read_frame(CAN::Frame* frame) override;
    bool send_frame(const CAN::Frame& frame) override;

private:
    MCP2515 m_mcp2515;
};
