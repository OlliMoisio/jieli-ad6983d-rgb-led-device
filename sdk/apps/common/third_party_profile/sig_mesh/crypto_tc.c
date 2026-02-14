/*
 * Copyright (c) 2017 Intel Corporation
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include "adaptation.h"
#include <tinycrypt/constants.h>
#include <tinycrypt/utils.h>
#include <tinycrypt/aes.h>
#include <tinycrypt/cmac_mode.h>
#include <tinycrypt/ccm_mode.h>
#include "mesh.h"
#include "crypto.h"
#include "prov.h"

#define LOG_TAG             "[MESH-cryptotc]"
// #define LOG_INFO_ENABLE
// #define LOG_DEBUG_ENABLE
#define LOG_WARN_ENABLE
#define LOG_ERROR_ENABLE
// #define LOG_DUMP_ENABLE
#include "mesh_log.h"

#if MESH_RAM_AND_CODE_MAP_DETAIL
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ble_mesh_cryptotc_bss")
#pragma data_seg(".ble_mesh_cryptotc_data")
#pragma const_seg(".ble_mesh_cryptotc_const")
#pragma code_seg(".ble_mesh_cryptotc_code")
#endif
#else /* MESH_RAM_AND_CODE_MAP_DETAIL */
#pragma bss_seg(".ble_mesh_bss")
#pragma data_seg(".ble_mesh_data")
#pragma const_seg(".ble_mesh_const")
#pragma code_seg(".ble_mesh_code")
#endif /* MESH_RAM_AND_CODE_MAP_DETAIL */

static struct {
    bool is_ready;
    uint8_t private_key_be[PRIV_KEY_SIZE];
    uint8_t public_key_be[PUB_KEY_SIZE];
} dh_pair;

int bt_encrypt_be(const u8_t key[16], const u8_t plaintext[16],
                  u8_t enc_data[16])
{
    struct tc_aes_key_sched_struct s;

    /* LOG_DBG("key %s plaintext %s", bt_hex(key, 16), bt_hex(plaintext, 16)); */

    if (tc_aes128_set_encrypt_key(&s, key) == TC_CRYPTO_FAIL) {
        return -EINVAL;
    }

    if (tc_aes_encrypt(enc_data, plaintext, &s) == TC_CRYPTO_FAIL) {
        return -EINVAL;
    }

    /* LOG_DBG("enc_data %s", bt_hex(enc_data, 16)); */

    return 0;
}


int bt_mesh_encrypt(const struct bt_mesh_key *key, const uint8_t plaintext[16],
                    uint8_t enc_data[16])
{
    return bt_encrypt_be(key->key, plaintext, enc_data);
}

int bt_mesh_ccm_encrypt(const u8_t key[16], u8_t nonce[13],
                        const u8_t *msg, size_t msg_len,
                        const u8_t *aad, size_t aad_len,
                        u8_t *out_msg, size_t mic_size)
{
    u8_t pmsg[16], cmic[16], cmsg[16], mic[16], Xn[16];
    u16_t blk_cnt, last_blk;
    size_t i, j;
    int err;

    LOG_DBG("key %s", bt_hex(key, 16));
    LOG_DBG("nonce %s", bt_hex(nonce, 13));
    LOG_DBG("msg (len %u) %s", msg_len, bt_hex(msg, msg_len));
    LOG_DBG("aad_len %u mic_size %u", aad_len, mic_size);

    /* Unsupported AAD size */
    if (aad_len >= 0xff00) {
        return -EINVAL;
    }

    /* C_mic = e(AppKey, 0x01 || nonce || 0x0000) */
    pmsg[0] = 0x01;
    memcpy(pmsg + 1, nonce, 13);
    sys_put_be16(0x0000, pmsg + 14);

    err = bt_encrypt_be(key, pmsg, cmic);
    if (err) {
        return err;
    }

    /* X_0 = e(AppKey, 0x09 || nonce || length) */
    if (mic_size == sizeof(u64_t)) {
        pmsg[0] = 0x19 | (aad_len ? 0x40 : 0x00);
    } else {
        pmsg[0] = 0x09 | (aad_len ? 0x40 : 0x00);
    }

    memcpy(pmsg + 1, nonce, 13);
    sys_put_be16(msg_len, pmsg + 14);

    err = bt_encrypt_be(key, pmsg, Xn);
    if (err) {
        return err;
    }

    /* If AAD is being used to authenticate, include it here */
    if (aad_len) {
        sys_put_be16(aad_len, pmsg);

        for (i = 0; i < sizeof(u16_t); i++) {
            pmsg[i] = Xn[i] ^ pmsg[i];
        }

        j = 0;
        aad_len += sizeof(u16_t);
        while (aad_len > 16) {
            do {
                pmsg[i] = Xn[i] ^ aad[j];
                i++, j++;
            } while (i < 16);

            aad_len -= 16;
            i = 0;

            err = bt_encrypt_be(key, pmsg, Xn);
            if (err) {
                return err;
            }
        }

        for (i = 0; i < aad_len; i++, j++) {
            pmsg[i] = Xn[i] ^ aad[j];
        }

        for (i = aad_len; i < 16; i++) {
            pmsg[i] = Xn[i];
        }

        err = bt_encrypt_be(key, pmsg, Xn);
        if (err) {
            return err;
        }
    }

    last_blk = msg_len % 16;
    blk_cnt = (msg_len + 15) / 16;
    if (!last_blk) {
        last_blk = 16;
    }

    for (j = 0; j < blk_cnt; j++) {
        if (j + 1 == blk_cnt) {
            /* X_1 = e(AppKey, X_0 ^ Payload[0-15]) */
            for (i = 0; i < last_blk; i++) {
                pmsg[i] = Xn[i] ^ msg[(j * 16) + i];
            }
            for (i = last_blk; i < 16; i++) {
                pmsg[i] = Xn[i] ^ 0x00;
            }

            err = bt_encrypt_be(key, pmsg, Xn);
            if (err) {
                return err;
            }

            /* MIC = C_mic ^ X_1 */
            for (i = 0; i < sizeof(mic); i++) {
                mic[i] = cmic[i] ^ Xn[i];
            }

            /* C_1 = e(AppKey, 0x01 || nonce || 0x0001) */
            pmsg[0] = 0x01;
            memcpy(pmsg + 1, nonce, 13);
            sys_put_be16(j + 1, pmsg + 14);

            err = bt_encrypt_be(key, pmsg, cmsg);
            if (err) {
                return err;
            }

            /* Encrypted = Payload[0-15] ^ C_1 */
            for (i = 0; i < last_blk; i++) {
                out_msg[(j * 16) + i] =
                    msg[(j * 16) + i] ^ cmsg[i];
            }
        } else {
            /* X_1 = e(AppKey, X_0 ^ Payload[0-15]) */
            for (i = 0; i < 16; i++) {
                pmsg[i] = Xn[i] ^ msg[(j * 16) + i];
            }

            err = bt_encrypt_be(key, pmsg, Xn);
            if (err) {
                return err;
            }

            /* C_1 = e(AppKey, 0x01 || nonce || 0x0001) */
            pmsg[0] = 0x01;
            memcpy(pmsg + 1, nonce, 13);
            sys_put_be16(j + 1, pmsg + 14);

            err = bt_encrypt_be(key, pmsg, cmsg);
            if (err) {
                return err;
            }

            /* Encrypted = Payload[0-15] ^ C_N */
            for (i = 0; i < 16; i++) {
                out_msg[(j * 16) + i] =
                    msg[(j * 16) + i] ^ cmsg[i];
            }

        }
    }

    memcpy(out_msg + msg_len, mic, mic_size);

    return 0;
}

int bt_mesh_ccm_decrypt(const u8_t key[16], u8_t nonce[13],
                        const u8_t *enc_msg, size_t msg_len,
                        const u8_t *aad, size_t aad_len,
                        u8_t *out_msg, size_t mic_size)
{
    u8_t msg[16], pmsg[16], cmic[16], cmsg[16], Xn[16], mic[16];
    u16_t last_blk, blk_cnt;
    size_t i, j;
    int err;

    if (msg_len < 1 || aad_len >= 0xff00) {
        return -EINVAL;
    }

    /* C_mic = e(AppKey, 0x01 || nonce || 0x0000) */
    pmsg[0] = 0x01;
    memcpy(pmsg + 1, nonce, 13);
    sys_put_be16(0x0000, pmsg + 14);

    err = bt_encrypt_be(key, pmsg, cmic);
    if (err) {
        return err;
    }

    /* X_0 = e(AppKey, 0x09 || nonce || length) */
    if (mic_size == sizeof(u64_t)) {
        pmsg[0] = 0x19 | (aad_len ? 0x40 : 0x00);
    } else {
        pmsg[0] = 0x09 | (aad_len ? 0x40 : 0x00);
    }

    memcpy(pmsg + 1, nonce, 13);
    sys_put_be16(msg_len, pmsg + 14);

    err = bt_encrypt_be(key, pmsg, Xn);
    if (err) {
        return err;
    }

    /* If AAD is being used to authenticate, include it here */
    if (aad_len) {
        sys_put_be16(aad_len, pmsg);

        for (i = 0; i < sizeof(u16_t); i++) {
            pmsg[i] = Xn[i] ^ pmsg[i];
        }

        j = 0;
        aad_len += sizeof(u16_t);
        while (aad_len > 16) {
            do {
                pmsg[i] = Xn[i] ^ aad[j];
                i++, j++;
            } while (i < 16);

            aad_len -= 16;
            i = 0;

            err = bt_encrypt_be(key, pmsg, Xn);
            if (err) {
                return err;
            }
        }

        for (i = 0; i < aad_len; i++, j++) {
            pmsg[i] = Xn[i] ^ aad[j];
        }

        for (i = aad_len; i < 16; i++) {
            pmsg[i] = Xn[i];
        }

        err = bt_encrypt_be(key, pmsg, Xn);
        if (err) {
            return err;
        }
    }

    last_blk = msg_len % 16;
    blk_cnt = (msg_len + 15) / 16;
    if (!last_blk) {
        last_blk = 16;
    }

    for (j = 0; j < blk_cnt; j++) {
        if (j + 1 == blk_cnt) {
            /* C_1 = e(AppKey, 0x01 || nonce || 0x0001) */
            pmsg[0] = 0x01;
            memcpy(pmsg + 1, nonce, 13);
            sys_put_be16(j + 1, pmsg + 14);

            err = bt_encrypt_be(key, pmsg, cmsg);
            if (err) {
                return err;
            }

            /* Encrypted = Payload[0-15] ^ C_1 */
            for (i = 0; i < last_blk; i++) {
                msg[i] = enc_msg[(j * 16) + i] ^ cmsg[i];
            }

            memcpy(out_msg + (j * 16), msg, last_blk);

            /* X_1 = e(AppKey, X_0 ^ Payload[0-15]) */
            for (i = 0; i < last_blk; i++) {
                pmsg[i] = Xn[i] ^ msg[i];
            }

            for (i = last_blk; i < 16; i++) {
                pmsg[i] = Xn[i] ^ 0x00;
            }

            err = bt_encrypt_be(key, pmsg, Xn);
            if (err) {
                return err;
            }

            /* MIC = C_mic ^ X_1 */
            for (i = 0; i < sizeof(mic); i++) {
                mic[i] = cmic[i] ^ Xn[i];
            }
        } else {
            /* C_1 = e(AppKey, 0x01 || nonce || 0x0001) */
            pmsg[0] = 0x01;
            memcpy(pmsg + 1, nonce, 13);
            sys_put_be16(j + 1, pmsg + 14);

            err = bt_encrypt_be(key, pmsg, cmsg);
            if (err) {
                return err;
            }

            /* Encrypted = Payload[0-15] ^ C_1 */
            for (i = 0; i < 16; i++) {
                msg[i] = enc_msg[(j * 16) + i] ^ cmsg[i];
            }

            memcpy(out_msg + (j * 16), msg, 16);

            /* X_1 = e(AppKey, X_0 ^ Payload[0-15]) */
            for (i = 0; i < 16; i++) {
                pmsg[i] = Xn[i] ^ msg[i];
            }

            err = bt_encrypt_be(key, pmsg, Xn);
            if (err) {
                return err;
            }
        }
    }

    if (memcmp(mic, enc_msg + msg_len, mic_size)) {
        return -EBADMSG;
    }

    return 0;
}

int bt_mesh_aes_cmac_raw_key(const uint8_t key[16], struct bt_mesh_sg *sg, size_t sg_len,
                             uint8_t mac[16])
{
    struct tc_aes_key_sched_struct sched;
    struct tc_cmac_struct state;

    if (tc_cmac_setup(&state, key, &sched) == TC_CRYPTO_FAIL) {
        return -EIO;
    }

    for (; sg_len; sg_len--, sg++) {
        if (tc_cmac_update(&state, sg->data, sg->len) == TC_CRYPTO_FAIL) {
            return -EIO;
        }
    }

    if (tc_cmac_final(mac, &state) == TC_CRYPTO_FAIL) {
        return -EIO;
    }

    return 0;
}

int bt_mesh_aes_cmac_mesh_key(const struct bt_mesh_key *key, struct bt_mesh_sg *sg,
                              size_t sg_len, uint8_t mac[16])
{
    return bt_mesh_aes_cmac_raw_key(key->key, sg, sg_len, mac);
}

// int bt_mesh_sha256_hmac_raw_key(const uint8_t key[32], struct bt_mesh_sg *sg, size_t sg_len,
// 				uint8_t mac[32])
// {
// 	struct tc_hmac_state_struct h;

// 	if (tc_hmac_set_key(&h, key, 32) == TC_CRYPTO_FAIL) {
// 		return -EIO;
// 	}

// 	if (tc_hmac_init(&h) == TC_CRYPTO_FAIL) {
// 		return -EIO;
// 	}

// 	for (; sg_len; sg_len--, sg++) {
// 		if (tc_hmac_update(&h, sg->data, sg->len) == TC_CRYPTO_FAIL) {
// 			return -EIO;
// 		}
// 	}

// 	if (tc_hmac_final(mac, 32, &h) == TC_CRYPTO_FAIL) {
// 		return -EIO;
// 	}

// 	return 0;
// }

int bt_mesh_pub_key_gen(void)
{
    // int rc = uECC_make_key(dh_pair.public_key_be,
    // 		       dh_pair.private_key_be,
    // 		       &curve_secp256r1);

    // if (rc == TC_CRYPTO_FAIL) {
    // 	dh_pair.is_ready = false;
    // 	LOG_ERR("Failed to create public/private pair");
    // 	return -EIO;
    // }

    // dh_pair.is_ready = true;

    bt_pub_key_gen();
    return 0;
}

const uint8_t *bt_mesh_pub_key_get(void)
{
    return bt_pub_key_get();//dh_pair.is_ready ? dh_pair.public_key_be : NULL;
}

// int bt_mesh_dhkey_gen(const uint8_t *pub_key, const uint8_t *priv_key, uint8_t *dhkey)
// {
// 	if (uECC_valid_public_key(pub_key, &curve_secp256r1)) {
// 		LOG_ERR("Public key is not valid");
// 		return -EIO;
// 	} else if (uECC_shared_secret(pub_key, priv_key ? priv_key :
// 							  dh_pair.private_key_be,
// 				      dhkey, &curve_secp256r1) != TC_CRYPTO_SUCCESS) {
// 		LOG_ERR("DHKey generation failed");
// 		return -EIO;
// 	}

// 	return 0;
// }

// __weak int default_CSPRNG(uint8_t *dst, unsigned int len)
// {
// 	return !bt_rand(dst, len);
// }

int bt_mesh_crypto_init(void)
{
    return 0;
}
