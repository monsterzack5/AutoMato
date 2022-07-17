#include "init_helpers.h"
#include <KeyFobConfig.h>
#include <drivers/flash.h>
#include <fs/nvs.h>
#include <logging/log.h>
#include <storage/flash_map.h>

LOG_MODULE_REGISTER(init_helpers);

bool init_lora(const device* lora_device, LoRaTXStatus should_enable_tx)
{
    if (!device_is_ready(lora_device)) {
        LOG_ERR("%s::ERROR Lora device reports not-ready, did you forget to enable the HP rail?\nCheck Your prj.conf!", __PRETTY_FUNCTION__);
        return false;
    }

    // Shared lora config, defined in KeyFobCommon.h
    LORA_CONFIG.tx = (should_enable_tx == LoRaTXStatus::Enabled);

    auto config_rc = lora_config(lora_device, &LORA_CONFIG);
    if (config_rc < 0) {
        LOG_ERR("%s::ERROR Invalid LoRa configuration", __PRETTY_FUNCTION__);
        return false;
    }

    return true;
}

bool init_flash(const device* flash_device, nvs_fs& nvs_structure)
{
    if (!device_is_ready(flash_device)) {
        LOG_ERR("%s::ERROR Flash reports not-ready, did you forget to enable flash in your prj.conf?", __PRETTY_FUNCTION__);
        return false;
    }

    nvs_structure.offset = FLASH_AREA_OFFSET(storage);

    flash_pages_info info;
    auto flash_info_rc = flash_get_page_info_by_offs(flash_device, nvs_structure.offset, &info);

    if (flash_info_rc) {
        // TODO: What to do if this fails?
        // - We cannot just load default values, as they are almost guaranteed
        //   to be out of date.
        // - We could switch to a hard-fault mode, and ping the CAN network
        //   with error codes, hopefully to be seen by the user in a reasonable
        //   amount of time
        LOG_ERR("%s::ERROR Failed to get flash page info, you might have a corrupted partition.", __PRETTY_FUNCTION__);
        return false;
    }

    nvs_structure.sector_size = info.size;
    nvs_structure.sector_count = 3U; // Why 3 Sectors?

    auto nvs_rc = nvs_init(&nvs_structure, flash_device->name);

    if (nvs_rc) {
        LOG_ERR("%s::ERROR Filesystem failed to start", __PRETTY_FUNCTION__);
        return false;
    }

    return true;
}