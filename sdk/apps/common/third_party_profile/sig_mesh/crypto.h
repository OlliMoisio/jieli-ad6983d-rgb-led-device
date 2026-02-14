/*  Bluetooth Mesh */

/*
 * Copyright (c) 2017 Intel Corporation
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "keys.h"

enum bt_mesh_nonce_type {
    BT_MESH_NONCE_NETWORK,
    BT_MESH_NONCE_PROXY,
    BT_MESH_NONCE_SOLICITATION,
};

struct bt_mesh_sg {
    const void *data;
    size_t len;
};

int bt_mesh_crypto_init(void);

int bt_mesh_encrypt(const struct bt_mesh_key *key, const u8_t plaintext[16],
                    u8_t enc_data[16]);

int bt_mesh_ccm_encrypt(const u8_t key[16], u8_t nonce[13],
                        const u8_t *msg, size_t msg_len,
                        const u8_t *aad, size_t aad_len,
                        u8_t *out_msg, size_t mic_size);

int bt_mesh_ccm_decrypt(const u8_t key[16], u8_t nonce[13],
                        const u8_t *enc_msg, size_t msg_len,
                        const u8_t *aad, size_t aad_len,
                        u8_t *out_msg, size_t mic_size);

int bt_mesh_aes_cmac_mesh_key(const struct bt_mesh_key *key, struct bt_mesh_sg *sg, size_t sg_len,
                              u8_t mac[16]);

int bt_mesh_aes_cmac_raw_key(const u8_t key[16], struct bt_mesh_sg *sg, size_t sg_len,
                             u8_t mac[16]);

int bt_mesh_sha256_hmac_raw_key(const u8_t key[32], struct bt_mesh_sg *sg, size_t sg_len,
                                u8_t mac[32]);

int bt_mesh_s1(const char *m, size_t m_len, u8_t salt[16]);

static inline int bt_mesh_s1_str(const char *m, u8_t salt[16])
{
    return bt_mesh_s1(m, strlen(m), salt);
}

int bt_mesh_s2(const char *m, size_t m_len, u8_t salt[32]);

int bt_mesh_k1(const u8_t *ikm, size_t ikm_len, const u8_t salt[16], const char *info,
               u8_t okm[16]);

int bt_mesh_k2(const u8_t n[16], const u8_t *p, size_t p_len, u8_t net_id[1],
               struct bt_mesh_key *enc_key, struct bt_mesh_key *priv_key);

int bt_mesh_k3(const u8_t n[16], u8_t out[8]);

int bt_mesh_k4(const u8_t n[16], u8_t out[1]);

int bt_mesh_k5(const u8_t *n, size_t n_len, const u8_t salt[32], u8_t *p, u8_t out[32]);

int bt_mesh_id128(const u8_t n[16], const char *s, enum bt_mesh_key_type type,
                  struct bt_mesh_key *out);

static inline int bt_mesh_identity_key(const u8_t net_key[16], struct bt_mesh_key *identity_key)
{
    return bt_mesh_id128(net_key, "nkik", BT_MESH_KEY_TYPE_ECB, identity_key);
}

static inline int bt_mesh_beacon_key(const u8_t net_key[16], struct bt_mesh_key *beacon_key)
{
    return bt_mesh_id128(net_key, "nkbk", BT_MESH_KEY_TYPE_CMAC, beacon_key);
}

static inline int bt_mesh_private_beacon_key(const u8_t net_key[16],
        struct bt_mesh_key *private_beacon_key)
{
    return bt_mesh_id128(net_key, "nkpk", BT_MESH_KEY_TYPE_ECB, private_beacon_key);
}

int bt_mesh_beacon_auth(const struct bt_mesh_key *beacon_key, u8_t flags,
                        const u8_t net_id[8], u32_t iv_index, u8_t auth[8]);

static inline int bt_mesh_app_id(const u8_t app_key[16], u8_t app_id[1])
{
    return bt_mesh_k4(app_key, app_id);
}

int bt_mesh_session_key(const u8_t dhkey[32], const u8_t prov_salt[16],
                        struct bt_mesh_key *session_key);

int bt_mesh_prov_nonce(const u8_t dhkey[32], const u8_t prov_salt[16], u8_t nonce[13]);

int bt_mesh_dev_key(const u8_t dhkey[32], const u8_t prov_salt[16], u8_t dev_key[16]);

int bt_mesh_prov_salt(u8_t algorithm, const u8_t *conf_salt, const u8_t *prov_rand,
                      const u8_t *dev_rand, u8_t *prov_salt);

int bt_mesh_net_obfuscate(u8_t *pdu, u32_t iv_index, const struct bt_mesh_key *privacy_key);

int bt_mesh_net_encrypt(const struct bt_mesh_key *key, struct net_buf_simple *buf,
                        u32_t iv_index, enum bt_mesh_nonce_type type);

int bt_mesh_net_decrypt(const struct bt_mesh_key *key, struct net_buf_simple *buf,
                        u32_t iv_index, enum bt_mesh_nonce_type type);

struct bt_mesh_app_crypto_ctx {
    bool dev_key;
    u8_t aszmic;
    u16_t src;
    u16_t dst;
    u32_t seq_num;
    u32_t iv_index;
    const u8_t *ad;
};

int bt_mesh_app_encrypt(const struct bt_mesh_key *key, const struct bt_mesh_app_crypto_ctx *ctx,
                        struct net_buf_simple *buf);

int bt_mesh_app_decrypt(const struct bt_mesh_key *key, const struct bt_mesh_app_crypto_ctx *ctx,
                        struct net_buf_simple *buf, struct net_buf_simple *out);

u8_t bt_mesh_fcs_calc(const u8_t *data, u8_t data_len);

bool bt_mesh_fcs_check(struct net_buf_simple *buf, u8_t received_fcs);

int bt_mesh_virtual_addr(const u8_t virtual_label[16], u16_t *addr);

int bt_mesh_prov_conf_salt(u8_t algorithm, const u8_t conf_inputs[145], u8_t *salt);

int bt_mesh_prov_conf_key(u8_t algorithm, const u8_t *k_input, const u8_t *conf_salt,
                          u8_t *conf_key);

int bt_mesh_prov_conf(u8_t algorithm, const u8_t *conf_key, const u8_t *prov_rand,
                      const u8_t *auth, u8_t *conf);

int bt_mesh_prov_decrypt(struct bt_mesh_key *key, u8_t nonce[13], const u8_t data[25 + 8],
                         u8_t out[25]);

int bt_mesh_prov_encrypt(struct bt_mesh_key *key, u8_t nonce[13], const u8_t data[25],
                         u8_t out[25 + 8]);

int bt_mesh_pub_key_gen(void);

const u8_t *bt_mesh_pub_key_get(void);

int bt_mesh_dhkey_gen(const u8_t *pub_key, const u8_t *priv_key, u8_t *dhkey);

int bt_mesh_beacon_decrypt(const struct bt_mesh_key *pbk, const u8_t random[13],
                           const u8_t data[5], const u8_t expected_auth[8], u8_t out[5]);

int bt_mesh_beacon_encrypt(const struct bt_mesh_key *pbk, u8_t flags, u32_t iv_index,
                           const u8_t random[13], u8_t data[5], u8_t auth[8]);
