#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define CRYPTO_AES_GCM_KEY_SIZE 16
#define CRYPTO_AES_GCM_TAG_SIZE 16

bool crypto_aes_gcm_init(const struct CryptoConfig* config);

// Note: Encrypts in place
// Note: This will append the gcm Tag at the end of the buffer, make sure
//       your buffer is big enough to hold it! (tag size is CRYPTO_AES_gcm_TAG_SIZE)
// Note: data_size should not include the CRYPTO_AES_GCM_TAG_SIZE bytes for the tag
void crypto_aes_gcm_encrypt(uint8_t data[], uint16_t data_size, uint8_t iv[]);

// Note: data_size should not include the CRYPTO_AES_GCM_TAG_SIZE tag bytes
bool crypto_aes_gcm_decrypt(uint8_t data[], uint16_t data_size, uint8_t iv[]);

#ifdef __cplusplus
}
#endif
