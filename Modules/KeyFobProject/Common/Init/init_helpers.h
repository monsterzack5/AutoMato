#pragma once

#include <device.h>

enum class LoRaTXStatus : uint8_t {
    Enabled,
    Disabled
};

bool init_lora(const device* lora_device, LoRaTXStatus should_enable_tx);
bool init_flash(const device* flash_device, nvs_fs& nvs_structure);