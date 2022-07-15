#pragma once

#include <stdint.h>

#include <device.h>

namespace Crypto {

const size_t AES_GCM_IV_BUFFER_SIZE = 12;

void crypto_aes_gcm_init(const uint8_t* AES_KEY_128, uint8_t iv_size, const device* crypto_device);
bool crypto_aes_gcm_encrypt(uint8_t* from_buffer, uint8_t* to_buffer, uint8_t* tag_buffer, uint16_t data_size, uint8_t* iv);
bool crypto_aes_gcm_decrypt(uint8_t* from_buffer, uint8_t* to_buffer, uint8_t* tag_buffer, uint16_t data_size, uint8_t* iv);

} // namespace Crypto