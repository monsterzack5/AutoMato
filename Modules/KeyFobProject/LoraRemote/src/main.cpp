#include <AES_GCM_KEY.h>
#include <KeyFobConfig.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/lora.h>
#include <logging/log.h>
#include <pm/pm.h>
#include <print_u8_array.h>
#include <sys/util.h>
#include <zephyr.h>

#include <drivers/flash.h>
#include <fs/nvs.h>
#include <storage/flash_map.h>

#include <ZephyrAesGCM.h>
#include <crypto/cipher.h>

// LoRa setup
BUILD_ASSERT(DT_NODE_HAS_STATUS(DT_ALIAS(lora0), okay),
    "No default LoRa radio specified in DT");

LOG_MODULE_REGISTER(lora_send);
static const device* lora_dev = DEVICE_DT_GET(DT_ALIAS(lora0));
// ---

// Button Setup
static K_SEM_DEFINE(button_sem, 0, 1);

static const gpio_dt_spec lock_button = GPIO_DT_SPEC_GET(DT_ALIAS(lock), gpios);
static const gpio_dt_spec unlock_button = GPIO_DT_SPEC_GET(DT_ALIAS(unlock), gpios);
static const gpio_dt_spec start_button = GPIO_DT_SPEC_GET(DT_ALIAS(start), gpios);
static gpio_callback lock_interrupt_callback;
static gpio_callback unlock_interrupt_callback;
static gpio_callback start_interrupt_callback;
// ---

// NVS Setup
static nvs_fs filesystem;

#define STORAGE_NODE DT_NODE_BY_FIXED_PARTITION_LABEL(storage)
#define FLASH_NODE DT_MTD_FROM_FIXED_PARTITION(STORAGE_NODE)

static const device* flash_device = DEVICE_DT_GET(FLASH_NODE);

static const uint8_t IV_NVS_ID = 0;
static const uint8_t IV_NVS_SIZE = sizeof(IV_t);

static const uint8_t ROLLING_CODE_NVS_ID = 1;
static const uint8_t ROLLING_CODE_NVS_SIZE = sizeof(RollingCode_t);
// ---

// Crypto Setup
#define CRYPTO_DRV_NAME CONFIG_CRYPTO_MBEDTLS_SHIM_DRV_NAME
const device* crypto_device = nullptr;
// ---

// Power state configuration, configured to enter "stop2"
// ID's from lora_e5_dev_board.dts
// https://github.com/zephyrproject-rtos/zephyr/blob/b161554ee862e9508f699e93403f7b2e667c9377/boards/arm/lora_e5_dev_board/lora_e5_dev_board.dts
pm_state_info power_state {
    .state = PM_STATE_SUSPEND_TO_IDLE,
    .substate_id = 3,
    .min_residency_us = 900,
    .exit_latency_us = 0, // Unknown
};
// ---

// Button Callback
static Command command_to_send = Command::NONE;
void button_pressed(const device* dev, gpio_callback* cb, uint32_t pins)
{
    (void)dev;
    (void)cb;

    const uint32_t lock_doors_bitmask = BIT(lock_button.pin);
    const uint32_t unlock_doors_bitmask = BIT(unlock_button.pin);
    const uint32_t start_button_bitmask = BIT(start_button.pin);

    if (pins == lock_doors_bitmask) {
        command_to_send = Command::LOCK_DOORS;
    } else if (pins == unlock_doors_bitmask) {
        command_to_send = Command::UNLOCK_DOORS;
    } else if (pins == start_button_bitmask) {
        command_to_send = Command::START_CAR;
    }

    k_sem_give(&button_sem);
}

void send_encryption_buffer_via_lora(uint8_t* buffer)
{
    int rc = lora_send(lora_dev, buffer, ENCRYPTION_BUFFER_SIZE);

    if (rc != 0) {
        printk("Message failed to send!\n");
        return;
    }
}

void update_encryption_buffer(uint8_t* buffer, const MessageParameters& params, Command command_code)
{
    // Clear the buffer, just to be safe.
    memset(buffer, 0, ENCRYPTION_BUFFER_SIZE);

    // Set marker byte
    buffer[MARKER_BYTE_INDEX] = MARKER_BYTE;

    // Set IV
    memcpy(&buffer[IV_INDEX], &params.iv, sizeof(IV_t));

    // Set rolling code
    memcpy(&buffer[ROLLING_CODE_INDEX], &params.rolling_code, sizeof(RollingCode_t));

    // Set command byte
    buffer[COMMAND_CODE_INDEX] = static_cast<uint8_t>(command_code);
}

MessageParameters get_initial_message_parameters()
{

    // in lieu of having a constexpr max() function,
    // to use the size of whichever is larger (IV_t or RollingCode_t),
    // setting this to 8 bytes is safe enough.
    uint8_t flash_read_buffer[8];
    memset(flash_read_buffer, 0, sizeof(flash_read_buffer));

    MessageParameters params;

    // Read IV from disk
    auto flash_read_rc = nvs_read(&filesystem, IV_NVS_ID, &flash_read_buffer, sizeof(IV_t));

    if (flash_read_rc > 0) {
        // IV is stored
        memcpy(&params.iv, flash_read_buffer, sizeof(IV_t));
    } else {
        // IV is not stored
        params.iv = IV_DEFAULT_VALUE;
    }

    memset(flash_read_buffer, 0, sizeof(flash_read_buffer));

    flash_read_rc = nvs_read(&filesystem, ROLLING_CODE_NVS_ID, &flash_read_buffer, sizeof(RollingCode_t));

    if (flash_read_rc > 0) {
        // Rolling Code is stored
        memcpy(&params.rolling_code, flash_read_buffer, sizeof(RollingCode_t));
    } else {
        // Rolling code is not stored
        params.rolling_code = ROLLING_CODE_DEFAULT_VALUE;
    }

    return params;
}

MessageParameters get_next_message_params(MessageParameters& current_params)
{
    // Integer overflow is expected behavior here.
    current_params.iv += 1;
    current_params.rolling_code += 1;

    uint8_t flash_write_buffer[8];

    memset(flash_write_buffer, 0, sizeof(flash_write_buffer));
    memcpy(flash_write_buffer, &current_params.iv, sizeof(IV_t));

    auto flash_write_rc = nvs_write(&filesystem, IV_NVS_ID, flash_write_buffer, sizeof(IV_t));

    if (flash_write_rc < 0) {
        LOG_ERR("Failed to write IV to disk!");
    }

    memset(flash_write_buffer, 0, sizeof(flash_write_buffer));
    memcpy(flash_write_buffer, &current_params.rolling_code, sizeof(RollingCode_t));

    flash_write_rc = nvs_write(&filesystem, ROLLING_CODE_NVS_ID, flash_write_buffer, sizeof(RollingCode_t));

    if (flash_write_rc < 0) {
        LOG_ERR("Failed to write rolling code to disk!");
    }

    return current_params;
}

bool encrypt_buffer(uint8_t* buffer, const MessageParameters& params)
{
    uint8_t iv_buffer[Crypto::AES_GCM_IV_BUFFER_SIZE];

    memset(iv_buffer, 0, sizeof(iv_buffer));
    memcpy(iv_buffer, &params.iv, sizeof(IV_t));

    return Crypto::crypto_aes_gcm_encrypt(
        &buffer[ENCRYPTION_START_INDEX],
        &buffer[ENCRYPTION_START_INDEX],
        &buffer[ENCRYPTION_TAG_START_INDEX],
        ENCRYPTED_DATA_SIZE,
        iv_buffer);
}

bool init_lora()
{
    if (!device_is_ready(lora_dev)) {
        LOG_ERR("%s Device not ready", lora_dev->name);
        return false;
    }

    // Shared lora config, defined in KeyFobCommon.h
    LORA_CONFIG.tx = true;

    auto config_rc = lora_config(lora_dev, &LORA_CONFIG);
    if (config_rc < 0) {
        printk("LoRa config failed!\n");
        return false;
    }

    return true;
}

bool init_button()
{
    const auto check_and_setup_button = [](const gpio_dt_spec& button, gpio_callback& button_callback) {
        if (!device_is_ready(button.port)) {
            LOG_ERR("%s reports as unready to use", button.port->name);
            return false;
        }

        auto button_configure_rc = gpio_pin_configure_dt(&button, GPIO_INPUT | GPIO_INT_DEBOUNCE);

        if (button_configure_rc != 0) {
            LOG_ERR("Error! Failed to configure button settings. rc = %d\n", button_configure_rc);
            return false;
        }

        auto button_config_interrupt_rc = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
        if (button_config_interrupt_rc != 0) {
            LOG_ERR("Error! Failed to configure the button! rc = %d\n", button_config_interrupt_rc);
            return false;
        }

        gpio_init_callback(&button_callback, button_pressed, BIT(button.pin));
        gpio_add_callback(button.port, &button_callback);

        return true;
    };

    bool configure_buttons_ret = true;

    configure_buttons_ret &= check_and_setup_button(lock_button, lock_interrupt_callback);
    configure_buttons_ret &= check_and_setup_button(unlock_button, unlock_interrupt_callback);
    configure_buttons_ret &= check_and_setup_button(start_button, start_interrupt_callback);

    return configure_buttons_ret;
}

bool init_flash()
{
    if (!device_is_ready(flash_device)) {
        LOG_ERR("Flash device %s is not ready\n", flash_device->name);
        return false;
    }

    filesystem.offset = FLASH_AREA_OFFSET(storage);

    flash_pages_info info;
    auto flash_info_rc = flash_get_page_info_by_offs(flash_device, filesystem.offset, &info);

    if (flash_info_rc) {
        printk("Failed to get flash device page info!\n");
        return false;
    }

    filesystem.sector_size = info.size;
    filesystem.sector_count = 3U;

    auto nvs_rc = nvs_init(&filesystem, flash_device->name);

    if (nvs_rc) {
        printk("Failed to init the NVS subsystem!\n");
        return false;
    }

    printk("Started the filesystem!\n");
    return true;
}

void main()
{
    init_lora();
    init_button();
    init_flash();

    crypto_device = device_get_binding(CRYPTO_DRV_NAME);

    if (!crypto_device) {
        printk("Failed to get the crypto device!\n");
        return;
    }

    Crypto::crypto_aes_gcm_init(AES_128_KEY, 4, crypto_device);

    uint8_t encryption_buffer[ENCRYPTION_BUFFER_SIZE];
    memset(encryption_buffer, 0, sizeof(encryption_buffer));

    auto params = get_initial_message_parameters();
    while (1) {
        // Go to sleep
        pm_power_state_set(power_state);

        // Wait forever for the semaphore
        k_sem_take(&button_sem, K_FOREVER);
        if (command_to_send == Command::NONE) {
            continue;
        }

        // Wake up fully, so the CPU is actually fast enough
        // to encrypt the lora message.
        pm_power_state_exit_post_ops(power_state);

        // Update rolling code and IV
        params = get_next_message_params(params);

        update_encryption_buffer(encryption_buffer, params, command_to_send);

        bool did_encrypt = encrypt_buffer(encryption_buffer, params);

        // Send the message
        if (did_encrypt) {
            send_encryption_buffer_via_lora(encryption_buffer);
        }
        command_to_send = Command::NONE;
    }
}