#include <AES_GCM_KEY.h>
#include <KeyFobConfig.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/lora.h>
#include <logging/log.h>
#include <pm/pm.h>
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
#define CRYPTO_DRV_NAME CONFIG_CRYPTO_MBEDTLS_SHIM_DRV_NAME
const device* crypto_device;
// ---

// GPIO Setup
// D5/PB5
const device* lock_doors_device;
const uint8_t lock_doors_pin = 5;
// D9/PA9
const device* unlock_doors_device;
const uint8_t unlock_doors_pin = 9;

// D10/PB10
const device* start_car_device;
const uint8_t start_car_pin = 10;
// ---

static RollingCode_t rolling_code = 0;
static IV_t iv = 0;

bool aes_gcm_decrypt(uint8_t* buffer, IV_t found_iv)
{
    cipher_ctx init;

    init.keylen = 16;
    // TODO: Why would this not take a const ptr?
    init.key.bit_stream = const_cast<uint8_t*>(AES_128_KEY);
    init.mode_params.gcm_info.tag_len = 16;
    init.mode_params.gcm_info.nonce_len = 4;
    init.flags = CAP_RAW_KEY | CAP_SYNC_OPS | CAP_SEPARATE_IO_BUFS;

    cipher_pkt decryption_config;
    decryption_config.in_buf = &buffer[ENCRYPTION_START_INDEX];
    decryption_config.in_len = ENCRYPTED_DATA_SIZE;
    decryption_config.out_buf_max = ENCRYPTED_DATA_SIZE;
    decryption_config.out_buf = &buffer[ENCRYPTION_START_INDEX]; // Encrypt in place?

    cipher_aead_pkt gcm_options;
    gcm_options.ad = nullptr;
    gcm_options.ad_len = 0;
    gcm_options.pkt = &decryption_config;
    gcm_options.tag = &buffer[26 - 16];

    if (cipher_begin_session(crypto_device, &init, CRYPTO_CIPHER_ALGO_AES, CRYPTO_CIPHER_MODE_GCM, CRYPTO_CIPHER_OP_DECRYPT)) {
        LOG_ERR("%s::ERROR - Cannot begin session", __PRETTY_FUNCTION__);
        cipher_free_session(crypto_device, &init);
        return false;
    }

    if (cipher_gcm_op(&init, &gcm_options, reinterpret_cast<uint8_t*>(found_iv))) {
        LOG_ERR("%s::ERROR: - Configuration/Input Error inside of decryption subroutine");
        cipher_free_session(crypto_device, &init);
        return false;
    }

    cipher_free_session(crypto_device, &init);
    return true;
}
void write_iv_and_rolling_code_to_flash()
{

    uint8_t flash_write_buffer[4];

    // Write IV to disk
    memset(flash_write_buffer, 0, sizeof(flash_write_buffer));
    memcpy(flash_write_buffer, &iv, sizeof(iv));

    auto flash_write_rc = nvs_write(&filesystem, IV_NVS_ID, flash_write_buffer, IV_NVS_SIZE);
    if (flash_write_rc < 0) {
        LOG_ERR("%s::ERROR: Disk write failure", __PRETTY_FUNCTION__);
    }

    // Write Rolling Code to disk
    memset(flash_write_buffer, 0, sizeof(flash_write_buffer));
    memcpy(flash_write_buffer, &rolling_code, sizeof(rolling_code));

    flash_write_rc = nvs_write(&filesystem, ROLLING_CODE_NVS_ID, flash_write_buffer, ROLLING_CODE_NVS_SIZE);
    if (flash_write_rc < 0) {
        LOG_ERR("%s::ERROR: Disk write failure", __PRETTY_FUNCTION__);
    }
}

bool init_lora()
{
    if (!device_is_ready(lora_dev)) {
        LOG_ERR("%s::ERROR Lora device reports not-ready, did you forget to enable the HP rail?\nCheck Your prj.conf!", __PRETTY_FUNCTION__);
        return false;
    }

    // Shared lora config, defined in KeyFobCommon.h
    LORA_CONFIG.tx = false;

    auto config_rc = lora_config(lora_dev, &LORA_CONFIG);
    if (config_rc < 0) {
        LOG_ERR("%s::ERROR Invalid LoRa configuration", __PRETTY_FUNCTION__);
        return false;
    }

    return true;
}

bool init_flash()
{
    if (!device_is_ready(flash_device)) {
        LOG_ERR("%s::ERROR Flash reports not-ready, did you forget to enable flash in your prj.conf?", __PRETTY_FUNCTION__);
        return false;
    }

    filesystem.offset = FLASH_AREA_OFFSET(storage);

    flash_pages_info info;
    auto flash_info_rc = flash_get_page_info_by_offs(flash_device, filesystem.offset, &info);

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

    filesystem.sector_size = info.size;
    filesystem.sector_count = 3U; // Why 3 Sectors?

    auto nvs_rc = nvs_init(&filesystem, flash_device->name);

    if (nvs_rc) {
        LOG_ERR("%s::ERROR Filesystem failed to start", __PRETTY_FUNCTION__);
        return false;
    }

    return true;
}

void read_iv_and_rolling_code_from_flash()
{

    // nvs_read rc:
    // >0: Bytes read successfully into the given buffer.

    uint8_t flash_read_buffer[4];

    // Read IV
    memset(flash_read_buffer, 0, sizeof(flash_read_buffer));

    auto flash_read_rc = nvs_read(&filesystem, IV_NVS_ID, &flash_read_buffer, IV_NVS_SIZE);

    if (flash_read_rc > 0) {
        memcpy(&iv, flash_read_buffer, sizeof(iv));
    } else {
        iv = IV_DEFAULT_VALUE;
    }

    // Read rolling code
    memset(flash_read_buffer, 0, sizeof(flash_read_buffer));

    flash_read_rc = nvs_read(&filesystem, ROLLING_CODE_NVS_ID, &flash_read_buffer, ROLLING_CODE_NVS_SIZE);

    if (flash_read_rc > 0) {
        memcpy(&rolling_code, flash_read_buffer, sizeof(rolling_code));
    } else {
        rolling_code = ROLLING_CODE_DEFAULT_VALUE;
    }
}

void handle_command(Command command)
{
    const auto blink_pin = [](const device* device, uint8_t pin) {
        gpio_pin_set(device, pin, 1);
        k_sleep(K_MSEC(20));
        gpio_pin_set(device, pin, 0);
    };

    switch (command) {
    case Command::LOCK_DOORS:
        blink_pin(lock_doors_device, lock_doors_pin);
        break;
    case Command::UNLOCK_DOORS:
        blink_pin(unlock_doors_device, unlock_doors_pin);
        break;
    case Command::START_CAR:
        blink_pin(start_car_device, start_car_pin);
        break;
    }
}

void handle_lora_message(uint8_t* message_buffer, uint16_t message_size)
{
    if (message_buffer[0] != MARKER_BYTE || message_size != ENCRYPTION_BUFFER_SIZE) {
        return;
    }

    IV_t given_iv = 0;
    memcpy(&given_iv, &message_buffer[IV_INDEX], sizeof(IV_t));

    if (!aes_gcm_decrypt(message_buffer, given_iv)) {
        LOG_INF("%s::INFO: Received an invalid, but valid looking, request", __PRETTY_FUNCTION__);
        return;
    }

    RollingCode_t given_rolling_code = 0;
    memcpy(&given_rolling_code, &message_buffer[ROLLING_CODE_INDEX], sizeof(RollingCode_t));

    // Now, this should ideally be aware of integer overflow, and account for that.
    // Since overflowing is more than possible here.
    // But, for now, converting it into a u64 works just as well.

    uint64_t given_rolling_code_u64 = given_rolling_code;

    if (given_rolling_code_u64 > rolling_code && given_rolling_code_u64 < (given_rolling_code_u64 + ROLLING_CODE_MAX_RANGE)) {
        // The rolling code is valid!
        iv = given_iv;
        rolling_code = given_rolling_code;
        write_iv_and_rolling_code_to_flash();

        auto command_to_run = static_cast<Command>(message_buffer[COMMAND_CODE_INDEX]);

        handle_command(command_to_run);
    }
}

bool init_gpio()
{

    bool ret = true;

    lock_doors_device = device_get_binding("GPIOB");
    ret &= gpio_pin_configure(lock_doors_device, 5, GPIO_OUTPUT);

    unlock_doors_device = device_get_binding("GPIOA");
    ret &= gpio_pin_configure(lock_doors_device, 9, GPIO_OUTPUT);

    start_car_device = device_get_binding("GPIOB");
    ret &= gpio_pin_configure(lock_doors_device, 10, GPIO_OUTPUT);

    return true;
}

void main()
{
    init_lora();
    init_gpio();
    init_flash();
    read_iv_and_rolling_code_from_flash();

    crypto_device = device_get_binding(CRYPTO_DRV_NAME);

    if (!crypto_device) {
        LOG_ERR("%s::ERROR Failed to get the bindings for the crypto device, did you forget to enable it in your prj.conf?", __PRETTY_FUNCTION__);
        return;
    }

    uint8_t lora_receive_buffer[64];
    memset(lora_receive_buffer, 0, sizeof(lora_receive_buffer));

    int16_t rssi = 0;
    int8_t snr = 0;

    while (1) {
        auto receive_rc = lora_recv(lora_dev, lora_receive_buffer, 64, K_SECONDS(10), &rssi, &snr);
        if (receive_rc > 0) {
            // We got a message!
            handle_lora_message(lora_receive_buffer, receive_rc);
        }
    }
}