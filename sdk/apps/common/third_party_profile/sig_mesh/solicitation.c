/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "adaptation.h"
#include "access.h"
#include "cfg.h"
#include "crypto.h"
#include "mesh.h"
#include "net.h"
#include "proxy.h"
#include "settings.h"
#include "adv.h"

#define LOG_TAG             "[MESH-solicitation]"
#define LOG_INFO_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_WARN_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_DUMP_ENABLE
#include "mesh_log.h"

#if MESH_RAM_AND_CODE_MAP_DETAIL
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ble_mesh_solicitation_bss")
#pragma data_seg(".ble_mesh_solicitation_data")
#pragma const_seg(".ble_mesh_solicitation_const")
#pragma code_seg(".ble_mesh_solicitation_code")
#endif
#else /* MESH_RAM_AND_CODE_MAP_DETAIL */
#pragma bss_seg(".ble_mesh_bss")
#pragma data_seg(".ble_mesh_data")
#pragma const_seg(".ble_mesh_const")
#pragma code_seg(".ble_mesh_code")
#endif /* MESH_RAM_AND_CODE_MAP_DETAIL */

#if CONFIG_BT_MESH_OD_PRIV_PROXY_SRV
static struct srpl_entry {
    u32_t sseq;
    u16_t ssrc;
} sol_pdu_rpl[CONFIG_BT_MESH_PROXY_SRPL_SIZE];

static ATOMIC_DEFINE(store, CONFIG_BT_MESH_PROXY_SRPL_SIZE);
static atomic_tt clear;
#endif
#if CONFIG_BT_MESH_PROXY_SOLICITATION
static u32_t sseq_out;
#endif

#if CONFIG_BT_MESH_OD_PRIV_PROXY_SRV
{
    static struct srpl_entry *srpl_find_by_addr(u16_t ssrc)
    int i;

    for (i = 0; i < ARRAY_SIZE(sol_pdu_rpl); i++) {
        if (sol_pdu_rpl[i].ssrc == ssrc) {
            return &sol_pdu_rpl[i];
        }
    }

    return NULL;
}

static int srpl_entry_save(struct bt_mesh_subnet *sub, u32_t sseq, u16_t ssrc)
{
    struct srpl_entry *entry;

    if (!BT_MESH_ADDR_IS_UNICAST(ssrc)) {
        LOG_DBG("Addr not in unicast range");
        return -EINVAL;
    }

    entry = srpl_find_by_addr(ssrc);
    if (entry) {
        if (entry->sseq >= sseq) {
            LOG_WRN("Higher or equal SSEQ already saved for this SSRC");
            return -EALREADY;
        }

    } else {
        entry = srpl_find_by_addr(BT_MESH_ADDR_UNASSIGNED);
        if (!entry) {
            /* No space to save new PDU in RPL for this SSRC
             * and this PDU is first for this SSRC
             */
            return -ENOMEM;
        }
    }

    entry->sseq = sseq;
    entry->ssrc = ssrc;

    LOG_DBG("Added: SSRC %d SSEQ %d to SRPL", entry->ssrc, entry->sseq);

    if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
        atomic_set_bit(store, entry - &sol_pdu_rpl[0]);
        bt_mesh_settings_store_schedule(BT_MESH_SETTINGS_SRPL_PENDING);
    }

    return 0;
}
#endif

void bt_mesh_sseq_pending_store(void)
{
#if CONFIG_BT_MESH_PROXY_SOLICITATION
    if (sseq_out) {
        node_info_store(SSEQ_INDEX, &sseq_out, sizeof(sseq_out));
    } else {
        node_info_clear(SSEQ_INDEX, sizeof(sseq_out));
    }

    LOG_DBG("%s sseq value", (sseq_out == 0 ? "Deleted" : "Stored"));
#endif
}

#if CONFIG_BT_MESH_PROXY_SOLICITATION
int sseq_set(void)
{
    int err;

    err = node_info_load(SSEQ_INDEX, &sseq_out, sizeof(sseq_out));
    if (err) {
        LOG_ERR("Failed to load \'sseq\'");
        return err;
    }

    LOG_DBG("Restored SSeq value 0x%06x", sseq_out);

    return 0;
}
#endif

#if CONFIG_BT_MESH_OD_PRIV_PROXY_SRV
static bool sol_pdu_decrypt(struct bt_mesh_subnet *sub, void *data)
{
    struct net_buf_simple *in = data;
    struct net_buf_simple *out = NET_BUF_SIMPLE(17);
    int err, i;
    u32_t sseq;
    u16_t ssrc;

    for (i = 0; i < ARRAY_SIZE(sub->keys); i++) {
        if (!sub->keys[i].valid) {
            LOG_ERR("invalid keys %d", i);
            continue;
        }

        net_buf_simple_init(out, 0);
        net_buf_simple_add_mem(out, in->data, in->len);

        err = bt_mesh_net_obfuscate(out->data, 0, &sub->keys[i].msg.privacy);
        if (err) {
            LOG_DBG("obfuscation err %d", err);
            continue;
        }
        err = bt_mesh_net_decrypt(&sub->keys[i].msg.enc, out,
                                  0, BT_MESH_NONCE_SOLICITATION);
        if (!err) {
            LOG_DBG("Decrypted PDU %s", bt_hex(out->data, out->len));
            memcpy(&sseq, &out->data[2], 3);
            memcpy(&ssrc, &out->data[5], 2);
            err = srpl_entry_save(sub,
                                  sys_be24_to_cpu(sseq),
                                  sys_be16_to_cpu(ssrc));
            return err ? false : true;
        }
        LOG_DBG("decrypt err %d", err);
    }

    return false;
}
#endif

void bt_mesh_sol_recv(struct net_buf_simple *buf, u8_t uuid_list_len)
{
#if CONFIG_BT_MESH_OD_PRIV_PROXY_SRV
    u8_t type;
    struct bt_mesh_subnet *sub;
    u16_t uuid;
    u8_t reported_len;
    u8_t svc_data_type;
    bool sol_uuid_found = false;
    bool svc_data_found = false;

    if (bt_mesh_gatt_proxy_get() == BT_MESH_GATT_PROXY_ENABLED ||
        bt_mesh_priv_gatt_proxy_get() == BT_MESH_GATT_PROXY_ENABLED ||
        bt_mesh_od_priv_proxy_get() == 0) {
        LOG_DBG("Not soliciting");
        return;
    }

    /* Get rid of ad_type that was checked in bt_mesh_scan_cb */
    type = net_buf_simple_pull_u8(buf);
    if (type != BT_DATA_UUID16_SOME && type != BT_DATA_UUID16_ALL) {
        LOG_DBG("Invalid type 0x%x, expected 0x%x or 0x%x",
                type, BT_DATA_UUID16_SOME, BT_DATA_UUID16_ALL);
        return;
    }

    if (buf->len < 24) {
        LOG_DBG("Invalid length (%u) Solicitation PDU", buf->len);
        return;
    }

    while (uuid_list_len >= 2) {
        uuid = net_buf_simple_pull_le16(buf);
        if (uuid == BT_UUID_MESH_PROXY_SOLICITATION_VAL) {
            sol_uuid_found = true;
        }
        uuid_list_len -= 2;
    }

    if (!sol_uuid_found) {
        LOG_DBG("No solicitation UUID found");
        return;
    }

    while (buf->len >= 22) {
        reported_len = net_buf_simple_pull_u8(buf);
        svc_data_type = net_buf_simple_pull_u8(buf);
        uuid = net_buf_simple_pull_le16(buf);

        if (reported_len == 21 && svc_data_type == BT_DATA_SVC_DATA16 &&
            uuid == BT_UUID_MESH_PROXY_SOLICITATION_VAL) {
            svc_data_found = true;
            break;
        }

        if (buf->len <= reported_len - 3) {
            LOG_DBG("Invalid length (%u) Solicitation PDU", buf->len);
            return;
        }

        net_buf_simple_pull_mem(buf, reported_len - 3);
    }

    if (!svc_data_found) {
        LOG_DBG("No solicitation service data found");
        return;
    }

    type = net_buf_simple_pull_u8(buf);
    if (type != 0) {
        LOG_DBG("Invalid type %d, expected 0x00", type);
        return;
    }

    sub = bt_mesh_subnet_find(sol_pdu_decrypt, (void *)buf);
    if (!sub) {
        LOG_DBG("Unable to find subnetwork for received solicitation PDU");
        return;
    }

    LOG_DBG("Decrypted solicitation PDU for existing subnet");

    sub->solicited = true;
    bt_mesh_adv_gatt_update();
#endif
}


int bt_mesh_proxy_solicit(u16_t net_idx)
{
#if CONFIG_BT_MESH_PROXY_SOLICITATION
    struct bt_mesh_subnet *sub;

    sub = bt_mesh_subnet_get(net_idx);
    if (!sub) {
        LOG_ERR("No subnet with net_idx %d", net_idx);
        return -EINVAL;
    }

    if (sub->sol_tx == true) {
        LOG_ERR("Solicitation already scheduled for this subnet");
        return -EALREADY;
    }

    /* SSeq reached its maximum value */
    if (sseq_out > 0xFFFFFF) {
        LOG_ERR("SSeq out of range");
        return -EOVERFLOW;
    }

    sub->sol_tx = true;

    bt_mesh_adv_gatt_update();
    return 0;
#else
    return -ENOTSUP;
#endif
}

#if CONFIG_BT_MESH_PROXY_SOLICITATION
static int sol_pdu_create(struct bt_mesh_subnet *sub, struct net_buf_simple *pdu)
{
    int err;

    net_buf_simple_add_u8(pdu, sub->keys[SUBNET_KEY_TX_IDX(sub)].msg.nid);
    /* CTL = 1, TTL = 0 */
    net_buf_simple_add_u8(pdu, 0x80);
    net_buf_simple_add_le24(pdu, sys_cpu_to_be24(sseq_out));
    net_buf_simple_add_le16(pdu, sys_cpu_to_be16(bt_mesh_primary_addr()));
    /* DST = 0x0000 */
    net_buf_simple_add_le16(pdu, 0x0000);

    err = bt_mesh_net_encrypt(&sub->keys[SUBNET_KEY_TX_IDX(sub)].msg.enc,
                              pdu, 0, BT_MESH_NONCE_SOLICITATION);

    if (err) {
        LOG_ERR("Encryption failed, err=%d", err);
        return err;
    }

    err = bt_mesh_net_obfuscate(pdu->data, 0,
                                &sub->keys[SUBNET_KEY_TX_IDX(sub)].msg.privacy);
    if (err) {
        LOG_ERR("Obfuscation failed, err=%d", err);
        return err;
    }

    net_buf_simple_push_u8(pdu, 0);
    net_buf_simple_push_le16(pdu, BT_UUID_MESH_PROXY_SOLICITATION_VAL);

    return 0;
}
#endif

#if CONFIG_BT_MESH_OD_PRIV_PROXY_SRV
int srpl_set(void)
{
    struct srpl_entry *entry;
    int err;
    u16_t ssrc;
    u32_t sseq;

    for (int i = 0; i < ARRAY_SIZE(sol_pdu_rpl); i++) {
        entry = &sol_pdu_rpl[i];
        if (!entry) {
            LOG_ERR("Unable to allocate SRPL entry for 0x%04x", ssrc);
            continue;
        }

        err = node_info_load(SRPL_INDEX + i, &sseq, sizeof(sseq));
        if (err) {
            LOG_ERR("Failed to load \'sseq\'");
            return err;
        }

        entry->sseq = sseq;

        LOG_DBG("SRPL entry for 0x%04x: Seq 0x%06x", entry->ssrc,
                entry->sseq);
    }

    return 0;
}

#endif

#if CONFIG_BT_MESH_OD_PRIV_PROXY_SRV
static void srpl_entry_clear(int i)
{
    u16_t addr = sol_pdu_rpl[i].ssrc;

    LOG_DBG("Removing entry SSRC: %d, SSEQ: %d from RPL",
            sol_pdu_rpl[i].ssrc,
            sol_pdu_rpl[i].sseq);
    sol_pdu_rpl[i].ssrc = 0;
    sol_pdu_rpl[i].sseq = 0;

    atomic_clear_bit(store, i);

    if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
        node_info_clear(SRPL_INDEX + i, sizeof(u32_t));
    }
}

static void srpl_store(struct srpl_entry *entry, u8_t load_index)
{
    LOG_DBG("src 0x%04x seq 0x%06x", entry->ssrc, entry->sseq);

    node_info_store(SRPL_INDEX + load_index, &entry->sseq, sizeof(entry->sseq));

    LOG_DBG("Stored RPL value");
}
#endif

void bt_mesh_srpl_pending_store(void)
{
#if CONFIG_BT_MESH_OD_PRIV_PROXY_SRV
    bool clr;

    clr = atomic_cas(&clear, 1, 0);

    for (int i = 0; i < ARRAY_SIZE(sol_pdu_rpl); i++) {
        LOG_DBG("src 0x%04x seq 0x%06x", sol_pdu_rpl[i].ssrc, sol_pdu_rpl[i].sseq);

        if (clr) {
            srpl_entry_clear(i);
        } else if (atomic_test_and_clear_bit(store, i)) {
            srpl_store(&sol_pdu_rpl[i], i);
        }
    }
#endif
}

void bt_mesh_srpl_entry_clear(u16_t addr)
{
#if CONFIG_BT_MESH_OD_PRIV_PROXY_SRV
    struct srpl_entry *entry;

    if (!BT_MESH_ADDR_IS_UNICAST(addr)) {
        LOG_DBG("Addr not in unicast range");
        return;
    }

    entry = srpl_find_by_addr(addr);
    if (!entry) {
        return;
    }

    srpl_entry_clear(entry - &sol_pdu_rpl[0]);
#endif
}

void bt_mesh_sol_reset(void)
{
#if CONFIG_BT_MESH_PROXY_SOLICITATION
    sseq_out = 0;

    if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
        bt_mesh_settings_store_schedule(BT_MESH_SETTINGS_SSEQ_PENDING);
    }
#endif

#if CONFIG_BT_MESH_OD_PRIV_PROXY_SRV
    if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
        (void)atomic_cas(&clear, 0, 1);

        bt_mesh_settings_store_schedule(BT_MESH_SETTINGS_SRPL_PENDING);
    }
#endif
}

#if CONFIG_BT_MESH_PROXY_SOLICITATION
static bool sol_subnet_find(struct bt_mesh_subnet *sub, void *cb_data)
{
    return sub->sol_tx;
}
#endif

int bt_mesh_sol_send(void)
{
#if CONFIG_BT_MESH_PROXY_SOLICITATION
    u16_t adv_int;
    struct bt_mesh_subnet *sub;
    int err;

    NET_BUF_SIMPLE_DEFINE(pdu, 20);

    sub = bt_mesh_subnet_find(sol_subnet_find, NULL);
    if (!sub) {
        return -ENOENT;
    }

    /* SSeq reached its maximum value */
    if (sseq_out > 0xFFFFFF) {
        LOG_ERR("SSeq out of range");
        sub->sol_tx = false;
        return -EOVERFLOW;
    }

    net_buf_simple_init(&pdu, 3);

    adv_int = BT_MESH_TRANSMIT_INT(CONFIG_BT_MESH_SOL_ADV_XMIT);

    err = sol_pdu_create(sub, &pdu);
    if (err) {
        LOG_ERR("Failed to create Solicitation PDU, err=%d", err);
        return err;
    }

    struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS,
        (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
        BT_DATA_BYTES(BT_DATA_UUID16_ALL,
        BT_UUID_16_ENCODE(
            BT_UUID_MESH_PROXY_SOLICITATION_VAL)),
        BT_DATA(BT_DATA_SVC_DATA16, pdu.data, pdu.size),
    };

    err = bt_mesh_adv_bt_data_send(CONFIG_BT_MESH_SOL_ADV_XMIT,
                                   adv_int, ad, 3);
    if (err) {
        LOG_ERR("Failed to advertise Solicitation PDU, err=%d", err);

        sub->sol_tx = false;

        return err;
    }
    sub->sol_tx = false;

    sseq_out++;

    if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
        bt_mesh_settings_store_schedule(BT_MESH_SETTINGS_SSEQ_PENDING);
    }

    return 0;
#else
    return -ENOTSUP;
#endif
}
