#include "ZephyrAesGCM.h"
#include <crypto/cipher.h>
#include <zephyr.h>

namespace {

static cipher_ctx cipher_context;
static const device* crypto_device = nullptr;

} // namespace

namespace Crypto {

// params: AES_128_KEY: 16 byte long uint8 array
//         iv_size: size in bytes of iv
void crypto_aes_gcm_init(const uint8_t* AES_KEY_128, uint8_t iv_size, const device* crypt_device)
{

    crypto_device = crypt_device;

    cipher_context.keylen = 16;
    // As far as I can see, internally the AES Key is never modified,
    // but the API still takes in a non-const ptr.
    // So, although const_cast-ing is horrible, it should do no harm here.
    cipher_context.key.bit_stream = const_cast<uint8_t*>(AES_KEY_128);
    cipher_context.mode_params.gcm_info.nonce_len = iv_size;
    cipher_context.mode_params.gcm_info.tag_len = 16;
    cipher_context.flags = cipher_query_hwcaps(crypto_device);
}

// Note: the IV buffer MUST be atleast 12 bytes long, even if you using
//       a smaller IV!
// Note: This can encrypt in place, given the same from_ and to_ pointers.
bool crypto_aes_gcm_encrypt(uint8_t* from_buffer, uint8_t* to_buffer, uint8_t* tag_buffer, uint16_t data_size, uint8_t* iv)
{
    cipher_aead_pkt gcm_aead_config;
    gcm_aead_config.ad = nullptr;
    gcm_aead_config.ad_len = 0;
    gcm_aead_config.tag = tag_buffer;

    cipher_pkt encrypt_config;
    encrypt_config.in_buf = from_buffer;
    encrypt_config.in_len = data_size;
    encrypt_config.out_buf_max = data_size;
    encrypt_config.out_buf = to_buffer;

    if (cipher_begin_session(crypto_device, &cipher_context, CRYPTO_CIPHER_ALGO_AES, CRYPTO_CIPHER_MODE_GCM, CRYPTO_CIPHER_OP_ENCRYPT)) {
        printk("%s::ERROR: Failed to begin cipher session!\n", __PRETTY_FUNCTION__);
        return false;
    }

    gcm_aead_config.pkt = &encrypt_config;
    if (cipher_gcm_op(&cipher_context, &gcm_aead_config, iv)) {
        cipher_free_session(crypto_device, &cipher_context);
        return false;
    }

    cipher_free_session(crypto_device, &cipher_context);
    return true;
}

bool crypto_aes_gcm_decrypt(uint8_t* from_buffer, uint8_t* to_buffer, uint8_t* tag_buffer, uint16_t data_size, uint8_t* iv)
{
    cipher_aead_pkt gcm_aead_config;
    gcm_aead_config.ad = nullptr;
    gcm_aead_config.ad_len = 0;
    gcm_aead_config.tag = tag_buffer;

    cipher_pkt decrypt_config;
    decrypt_config.in_buf = from_buffer;
    decrypt_config.in_len = data_size;
    decrypt_config.out_buf_max = data_size;
    decrypt_config.out_buf = to_buffer;

    if (cipher_begin_session(crypto_device, &cipher_context, CRYPTO_CIPHER_ALGO_AES, CRYPTO_CIPHER_MODE_GCM, CRYPTO_CIPHER_OP_DECRYPT)) {
        printk("%s::ERROR: Failed to begin cipher session!\n", __PRETTY_FUNCTION__);
        return false;
    }

    gcm_aead_config.pkt = &decrypt_config;
    if (cipher_gcm_op(&cipher_context, &gcm_aead_config, iv)) {
        printk("%s::ERROR: Failed to decrypt buffer!\n", __PRETTY_FUNCTION__);
        cipher_free_session(crypto_device, &cipher_context);
        return false;
    }

    cipher_free_session(crypto_device, &cipher_context);
    return true;
}

} // namespace Crypto