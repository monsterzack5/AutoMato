#include "CryptoAesGCM.h"

#include "../ThirdParty/BearSSL/bearssl_aead.h" // hacky workaround

static br_aes_ct_ctr_keys bear_aes_context;
static br_gcm_context bear_gcm_context;

void crypto_aes_gcm_init(const uint8_t aes_key[CRYPTO_AES_GCM_KEY_SIZE])
{
    br_aes_ct_ctr_init(&bear_aes_context, aes_key, CRYPTO_AES_GCM_KEY_SIZE);
    br_gcm_init(&bear_gcm_context, &bear_aes_context.vtable, br_ghash_ctmul);
}

void crypto_aes_gcm_encrypt(uint8_t data[], uint16_t data_size, uint8_t iv[], uint8_t iv_size)
{
    br_gcm_reset(&bear_gcm_context, iv, iv_size);

    br_gcm_flip(&bear_gcm_context);

    br_gcm_run(&bear_gcm_context, 1, data, data_size);

    br_gcm_get_tag(&bear_gcm_context, &data[data_size]);
}

bool crypto_aes_gcm_decrypt(uint8_t data[], uint16_t data_size, uint8_t iv[], uint8_t iv_size)
{
    br_gcm_reset(&bear_gcm_context, iv, iv_size);
    br_gcm_flip(&bear_gcm_context);

    br_gcm_run(&bear_gcm_context, 0, data, data_size);

    return br_gcm_check_tag(&bear_gcm_context, &data[data_size]);
}
