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

#include <crypto/cipher.h>

// LoRa setup
BUILD_ASSERT(DT_NODE_HAS_STATUS(DT_ALIAS(lora0), okay),
    "No default LoRa radio specified in DT");

LOG_MODULE_REGISTER(lora_send);
static const device* lora_dev = DEVICE_DT_GET(DT_ALIAS(lora0));
// ---

// Button Setup
#define SW1_NODE DT_ALIAS(sw1)
static K_SEM_DEFINE(button_sem, 0, 1);
#if !DT_NODE_HAS_STATUS(SW1_NODE, okay)
#    error "Unsupported board: sw0 devicetree alias is not defined"
#endif

static const gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW1_NODE, gpios, { 0 });
static gpio_callback button_cb_data;
// ---

// NVS Setup
static nvs_fs filesystem;

#define STORAGE_NODE DT_NODE_BY_FIXED_PARTITION_LABEL(storage)
#define FLASH_NODE DT_MTD_FROM_FIXED_PARTITION(STORAGE_NODE)

static const device* flash_device = DEVICE_DT_GET(FLASH_NODE);

static const uint8_t IV_NVS_ID = 0;
static const uint8_t IV_NVS_SIZE = 4;

static const uint8_t ROLLING_CODE_NVS_ID = 1;
static const uint8_t ROLLING_CODE_NVS_SIZE = 4;
// ---

// Crypto Setup
// #define CRYPTO_DRV_NAME CONFIG_CRYPTO_MBEDTLS_SHIM_DRV_NAME
// const device* crypto_device = device_get_binding(CRYPTO_DRV_NAME);

#define CRYPTO_DRV_NAME CONFIG_CRYPTO_MBEDTLS_SHIM_DRV_NAME
const device* crypto_device;

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

static RollingCode_t rolling_code = 0;
static IV_t iv = 0;
static uint8_t output_buffer[ENCRYPTION_BUFFER_SIZE];

// Button Callback
void button_pressed(const device* dev, gpio_callback* cb,
    uint32_t pins)
{
    (void)dev;
    (void)cb;
    (void)pins;
    k_sem_give(&button_sem);
}

void send_lora_message()
{
    int rc = lora_send(lora_dev, output_buffer, ENCRYPTION_BUFFER_SIZE);

    printk("Sending buffer via lora, buffer contents:\n");
    print_u8_array(output_buffer, sizeof(output_buffer));
    printk("\n");

    if (rc != 0) {
        printk("Message failed to send!\n");
        return;
    }
}

void aes_gcm_encrypt()
{
    cipher_ctx init;

    init.keylen = 16;

    // TODO: Why would this not take a const ptr?
    init.key.bit_stream = const_cast<uint8_t*>(AES_128_KEY);

    init.mode_params.gcm_info.tag_len = 16;
    init.mode_params.gcm_info.nonce_len = 4;
    init.flags = CAP_RAW_KEY | CAP_SYNC_OPS | CAP_SEPARATE_IO_BUFS;

    cipher_pkt encrypt_config;
    encrypt_config.in_buf = &output_buffer[ENCRYPTION_START_INDEX];
    encrypt_config.in_len = ENCRYPTED_DATA_SIZE;
    encrypt_config.out_buf_max = ENCRYPTED_DATA_SIZE;
    encrypt_config.out_buf = &output_buffer[ENCRYPTION_START_INDEX]; // Encrypt in place?

    cipher_aead_pkt gcm_op;
    gcm_op.ad = nullptr;
    gcm_op.ad_len = 0;
    gcm_op.pkt = &encrypt_config;
    gcm_op.tag = &output_buffer[26 - 16]; // TODO: Not hardcode this

    if (cipher_begin_session(crypto_device, &init, CRYPTO_CIPHER_ALGO_AES, CRYPTO_CIPHER_MODE_GCM, CRYPTO_CIPHER_OP_ENCRYPT)) {
        printk("failed in if cipher begin session\n");
        return;
    }

    gcm_op.pkt = &encrypt_config;

    if (cipher_gcm_op(&init, &gcm_op, reinterpret_cast<uint8_t*>(&iv))) {
        printk("GCM mode ENCRYPT - Failed\n");
        cipher_free_session(crypto_device, &init);
    }

    cipher_free_session(crypto_device, &init);
}

void write_iv_and_rolling_code_to_flash()
{

    uint8_t flash_write_buffer[4];

    // Write IV to disk
    memset(flash_write_buffer, 0, sizeof(flash_write_buffer));
    memcpy(flash_write_buffer, &iv, sizeof(iv));

    auto flash_write_rc = nvs_write(&filesystem, IV_NVS_ID, flash_write_buffer, IV_NVS_SIZE);
    if (flash_write_rc < 0) {
        LOG_ERR("Failed to write IV to disk!");
    }

    // Write Rolling Code to disk
    memset(flash_write_buffer, 0, sizeof(flash_write_buffer));
    memcpy(flash_write_buffer, &rolling_code, sizeof(rolling_code));

    flash_write_rc = nvs_write(&filesystem, ROLLING_CODE_NVS_ID, flash_write_buffer, ROLLING_CODE_NVS_SIZE);
    if (flash_write_rc < 0) {
        LOG_ERR("Failed to write Rolling Code to disk!");
    }
}

void update_output_buffer(Command command)
{

    // Marker byte
    output_buffer[MARKER_BYTE_INDEX] = MARKER_BYTE;

    // IV
    iv += 1;
    memcpy(&output_buffer[IV_INDEX], &iv, sizeof(iv));

    // Rolling Code
    rolling_code += 1;
    memcpy(&output_buffer[ROLLING_CODE_INDEX], &rolling_code, sizeof(rolling_code));

    // Command Byte
    output_buffer[COMMAND_CODE_INDEX] = static_cast<CommandCode_t>(command);

    aes_gcm_encrypt();

    write_iv_and_rolling_code_to_flash();
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
    if (!device_is_ready(button.port)) {
        LOG_ERR("%s Device not ready", button.port->name);
        return false;
    }

    auto button_config_rc = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (button_config_rc != 0) {
        printk("Error! Failed to configure the button! rc = %d\n", button_config_rc);
        return false;
    }

    auto button_config_int_rc = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    if (button_config_int_rc != 0) {
        printk("Error! Failed to configure the button! rc = %d\n", button_config_int_rc);
        return false;
    }

    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);

    return true;
}

bool init_flash()
{
    if (!device_is_ready(flash_device)) {
        printk("Flash device %s is not ready\n", flash_device->name);
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
    filesystem.sector_count = 3U; // Why 3 Sectors?

    auto nvs_rc = nvs_init(&filesystem, flash_device->name);

    if (nvs_rc) {
        printk("Failed to init the NVS subsystem!\n");
        return false;
    }

    printk("Started the filesystem!\n");
    return true;
}

void read_iv_and_rolling_code_from_flash()
{
    // 16 Bytes is what is given in the sample NVS implementation
    // I am not entirely sure if the size changes per device, and/or
    // if there is a macro to get the proper size, instead of hard
    // coding it.
    uint8_t flash_read_buffer[4];

    // Read IV

    memset(flash_read_buffer, 0, sizeof(flash_read_buffer));

    auto flash_read_rc = nvs_read(&filesystem, IV_NVS_ID, &flash_read_buffer, IV_NVS_SIZE);

    if (flash_read_rc > 0) {
        // IV is stored
        memcpy(&iv, flash_read_buffer, sizeof(iv));
    } else {
        // IV is not stored.
        iv = IV_DEFAULT_VALUE;
    }

    // Read rolling code
    memset(flash_read_buffer, 0, sizeof(flash_read_buffer));

    flash_read_rc = nvs_read(&filesystem, ROLLING_CODE_NVS_ID, &flash_read_buffer, ROLLING_CODE_NVS_SIZE);

    if (flash_read_rc > 0) {
        // Rolling Code is stored
        memcpy(&rolling_code, flash_read_buffer, sizeof(rolling_code));
    } else {
        // Rolling Code is not stored.
        rolling_code = ROLLING_CODE_DEFAULT_VALUE;
    }
}

void main()
{
    init_lora();
    init_button();
    init_flash();
    read_iv_and_rolling_code_from_flash();

    // TODO: Convert to a macro
    crypto_device = device_get_binding(CRYPTO_DRV_NAME);

    if (!crypto_device) {
        printk("Failed to get the crypto device!\n");
        return;
    }

    memset(output_buffer, 0, sizeof(output_buffer));

    while (1) {
        // Go to sleep
        // pm_power_state_set(power_state);

        // Wait forever for the sem.
        k_sem_take(&button_sem, K_FOREVER);

        // Wake up fully, so the CPU is actually fast enough
        // to encrypt the lora message.
        // pm_power_state_exit_post_ops(power_state);

        // Update rolling code and IV
        update_output_buffer(Command::LOCK_DOORS);

        // Send the message
        send_lora_message();
    }
}