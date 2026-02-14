/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "adaptation.h"
#include "kernel/iterable_sections.h"
#include "mesh.h"
#include "subnet.h"
#include "app_keys.h"
#include "net.h"
#include "cdb.h"
#include "crypto.h"
#include "rpl.h"
#include "transport.h"
#include "heartbeat.h"
#include "access.h"
#include "proxy.h"
#include "pb_gatt_srv.h"
#include "api/settings.h"
#include "settings.h"
#include "cfg.h"
#include "solicitation.h"
#include "va.h"
#include "os/os_cpu.h"
#include "system/syscfg_id.h"

#if CONFIG_BT_MESH_DFU_DIST
#include "mesh_dfu_app_cmd_protocol.h"
#endif
#if CONFIG_BT_MESH_DFU_TARGET
#include "dfu_target.h"
#endif

#define LOG_TAG             "[MESH-settings]"
// #define LOG_INFO_ENABLE
// #define LOG_DEBUG_ENABLE
// #define LOG_WARN_ENABLE
// #define LOG_ERROR_ENABLE
// #define LOG_DUMP_ENABLE
#include "mesh_log.h"

#if MESH_RAM_AND_CODE_MAP_DETAIL
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ble_mesh_settings_bss")
#pragma data_seg(".ble_mesh_settings_data")
#pragma const_seg(".ble_mesh_settings_const")
#pragma code_seg(".ble_mesh_settings_code")
#endif
#else /* MESH_RAM_AND_CODE_MAP_DETAIL */
#pragma bss_seg(".ble_mesh_bss")
#pragma data_seg(".ble_mesh_data")
#pragma const_seg(".ble_mesh_const")
#pragma code_seg(".ble_mesh_code")
#endif /* MESH_RAM_AND_CODE_MAP_DETAIL */

#if 0	/* These are used to store_pending() with k_work_schedule() */
#ifdef CONFIG_BT_MESH_RPL_STORE_TIMEOUT
#define RPL_STORE_TIMEOUT CONFIG_BT_MESH_RPL_STORE_TIMEOUT
#else
#define RPL_STORE_TIMEOUT (-1)
#endif

static struct k_work_delayable pending_store;
#endif

/* Mesh network information for persistent storage. */
struct net_val {
    u16_t primary_addr;
    struct bt_mesh_key dev_key;
} __packed;

/* IV Index & IV Update information for persistent storage. */
struct iv_val {
    u32_t iv_index;
    u8_t  iv_update: 1,
          iv_duration: 7;
} __packed;

/* Sequence number information for persistent storage. */
struct seq_val {
    u8_t val[3];
} __packed;

/* Heartbeat Publication information for persistent storage. */
struct hb_pub_val {
    u16_t dst;
    u8_t  period;
    u8_t  ttl;
    u16_t feat;
    u16_t net_idx: 12,
          indefinite: 1;
} __packed;

/* Miscellaneous configuration server model states */
struct cfg_val {
    u8_t net_transmit;
    u8_t relay;
    u8_t relay_retransmit;
    u8_t beacon;
    u8_t gatt_proxy;
    u8_t frnd;
    u8_t default_ttl;
} __packed;

/* Model publication information for persistent storage. */
struct mod_pub_val {
    struct {
        u16_t addr;
        u16_t key;
        u8_t  ttl;
        u8_t  retransmit;
        u8_t  period;
        u8_t  period_div: 4,
              cred: 1;
    } base;
    u16_t uuidx;
} __packed;

/* Virtual Address information for persistent storage. */
struct va_val {
    u16_t ref;
    u16_t addr;
    u8_t uuid[16];
} __packed;

/* We need this so we don't overwrite app-hardcoded values in case FCB
 * contains a history of changes but then has a NULL at the end.
 */
struct __rpl_val __store_rpl[CONFIG_BT_MESH_CRPL] = {0};

struct net_key_val __store_net_key[CONFIG_BT_MESH_SUBNET_COUNT] = {0};

struct app_key_val __store_app_key[CONFIG_BT_MESH_APP_KEY_COUNT] = {0};

struct __mod_bind __store_sig_mod_bind[MAX_SIG_MODEL_NUMS] = {0};
struct __mod_bind __store_vnd_mod_bind[MAX_VND_MODEL_NUMS] = {0};

struct __mod_sub __store_sig_mod_sub[MAX_SIG_MODEL_NUMS] = {0};
struct __mod_sub __store_vnd_mod_sub[MAX_VND_MODEL_NUMS] = {0};

//struct __mod_pub {
//struct mod_pub_val pub;
//};

struct __mod_sub_va {
    u16_t uuidxs[CONFIG_BT_MESH_LABEL_COUNT];
};

#if CONFIG_BT_MESH_PROVISIONER
/* Node storage information */
struct node_val {
    u16_t net_idx;
    u8_t num_elem;
    u8_t flags;
#define F_NODE_CONFIGURED 0x01
    u8_t uuid[16];
    struct bt_mesh_key dev_key;
    u16_t addr;
} __packed;

struct cdb_net_val {
    struct __packed {
        u32_t index;
        bool     update;
    } iv;
    u16_t lowest_avail_addr;
} __packed;

static void cdb_net_set(void);
static void cdb_node_set(void);
static void cdb_subnet_set(void);
static void cdb_app_key_set(void);
#endif /* CONFIG_BT_MESH_PROVISIONER */

static ATOMIC_DEFINE(pending_flags, BT_MESH_SETTINGS_FLAG_COUNT);

static void store_pending(struct k_work *work);

static u8 get_model_store_index(bool vnd, u8 elem_idx, u8 mod_idx)
{
    u8 i;
    u8 store_index = 0;

    for (i = 0; i < elem_idx; i++) {
        if (vnd) {
            store_index += bt_mesh_each_elem_vendor_model_numbers_get(i);
        } else {
            store_index += bt_mesh_each_elem_sig_model_numbers_get(i);
        }
    }
    store_index += mod_idx;

    return store_index;
}

static u8 bt_mesh_model_numbers_get(bool vnd)
{
    u8 model_count = 0;

    for (u8 i = 0; i < bt_mesh_elem_count(); i++) {
        if (vnd) {
            model_count += bt_mesh_each_elem_vendor_model_numbers_get(i);
            ASSERT(model_count <= MAX_VND_MODEL_NUMS, "real vnd model numbers:%d", model_count);
        } else {
            model_count += bt_mesh_each_elem_sig_model_numbers_get(i);
            ASSERT(model_count <= MAX_SIG_MODEL_NUMS, "real sig model numbers:%d", model_count);
        }
    }

    return model_count;
}

static void get_elem_and_mod_idx(bool vnd, u8 model_count, u8 *elem_idx, u8 *mod_idx)
{
    u8 i;
    u8 each_elem_model_numbers = 0;

    *elem_idx = 0;
    *mod_idx = 0;
    for (i = 0; i < bt_mesh_elem_count(); i++) {
        if (vnd) {
            each_elem_model_numbers = bt_mesh_each_elem_vendor_model_numbers_get(i);
        } else {
            each_elem_model_numbers = bt_mesh_each_elem_sig_model_numbers_get(i);
        }
        if (model_count >= each_elem_model_numbers) {
            model_count -= each_elem_model_numbers;
        } else {
            break;
        }
    }
    *elem_idx = i;
    *mod_idx = model_count;
}

static int net_set(void)
{
    struct net_val net;
    struct bt_mesh_key key;
    int err;

    err = node_info_load(NET_INDEX, &net, sizeof(net));
    if (err) {
        bt_mesh_comp_unprovision();
        bt_mesh_key_destroy(&bt_mesh.dev_key);
        memset(&bt_mesh.dev_key, 0, sizeof(struct bt_mesh_key));
        LOG_ERR("Failed to set \'net\'");
        return err;
    }

    /* One extra copying since net.dev_key is from packed structure
     * and might be unaligned.
     */
    memcpy(&key, &net.dev_key, sizeof(struct bt_mesh_key));

    bt_mesh_key_assign(&bt_mesh.dev_key, &key);
    bt_mesh_comp_provision(net.primary_addr);

    LOG_DBG("Provisioned with primary address 0x%04x", net.primary_addr);
    LOG_DBG("Recovered DevKey %s", bt_hex(&bt_mesh.dev_key, sizeof(struct bt_mesh_key)));

    return 0;
}

static int iv_set(void)
{
    struct iv_val iv;
    int err;

    LOG_DBG("\n < --%s-- >", __FUNCTION__);
    err = node_info_load(IV_INDEX, &iv, sizeof(iv));
    if (err) {
        bt_mesh.iv_index = 0U;
        atomic_clear_bit(bt_mesh.flags, BT_MESH_IVU_IN_PROGRESS);
        LOG_ERR("<iv_set> load not exist");
        return err;
    }

    bt_mesh.iv_index = iv.iv_index;
    atomic_set_bit_to(bt_mesh.flags, BT_MESH_IVU_IN_PROGRESS, iv.iv_update);
    bt_mesh.ivu_duration = iv.iv_duration;

    LOG_DBG("IV Index 0x%04x (IV Update Flag %u) duration %u hours", iv.iv_index, iv.iv_update,
            iv.iv_duration);

    return 0;
}

static int seq_set(void)
{
    struct seq_val seq;
    int err;

    LOG_DBG("\n < --%s-- >", __FUNCTION__);

    err = node_info_load(SEQ_INDEX, &seq, sizeof(seq));
    if (err) {
        bt_mesh.seq = 0U;
        LOG_ERR("seq (null)");
        return err;
    }

    bt_mesh.seq = sys_get_le24(seq.val);

    if (CONFIG_BT_MESH_SEQ_STORE_RATE > 0) {
        /* Make sure we have a large enough sequence number. We
         * subtract 1 so that the first transmission causes a write
         * to the settings storage.
         */
        bt_mesh.seq += (CONFIG_BT_MESH_SEQ_STORE_RATE -
                        (bt_mesh.seq % CONFIG_BT_MESH_SEQ_STORE_RATE));
        bt_mesh.seq--;
    }

    LOG_DBG("Sequence Number 0x%06x", bt_mesh.seq);

    return 0;
}

static int rpl_set(void)
{
    struct bt_mesh_rpl *entry;
    struct __rpl_val __rpl;
    int err;
    u8 index;

    LOG_DBG("\n < --%s-- >", __FUNCTION__);

    err = node_info_load(RPL_INDEX, __store_rpl, (sizeof(struct __rpl_val) * CONFIG_BT_MESH_CRPL));
    if (err) {
        LOG_ERR("<rpl_set> memory load fail for index:0x%x", index);
        return -ENOMEM;
    }

    for (index = 0; index < CONFIG_BT_MESH_CRPL; index++) {
        if (__store_rpl[index].src == 0U) {
            continue;
        }

        entry = bt_mesh_rpl_find(__store_rpl[index].src);
        if (!entry) {
            entry = bt_mesh_rpl_alloc(__store_rpl[index].src);
            if (!entry) {
                LOG_ERR("Unable to allocate RPL entry for 0x%04x", __store_rpl[index].src);
                continue;
            }
        }

        entry->seq = __store_rpl[index].rpl.seq;
        entry->old_iv = __store_rpl[index].rpl.old_iv;

        LOG_DBG("RPL entry for 0x%04x: Seq 0x%06x old_iv %u", entry->src,
                entry->seq, entry->old_iv);
    }

    return 0;
}

static int net_key_set(void)
{
    struct bt_mesh_subnet *sub;
    struct net_key_val key;
    struct bt_mesh_key val[2];
    int i, err;
    u16_t net_idx;

    LOG_DBG("\n < --%s-- >", __FUNCTION__);

    err = node_info_load(NET_KEY_INDEX, __store_net_key, sizeof(struct net_key_val) * CONFIG_BT_MESH_SUBNET_COUNT);
    if (err) {
        LOG_ERR("<net_key_set> memory load fail for net_idx:0x%x", net_idx);
        return -1;
    }

    for (net_idx = 0; net_idx < CONFIG_BT_MESH_SUBNET_COUNT; net_idx++) {
        if (!__store_net_key[net_idx].existence) {
            LOG_ERR("No NetKeyIndex 0x%03x", net_idx);
            continue;
        }
        /* One extra copying since key.val array is from packed structure
        * and might be unaligned.
        */
        memcpy(val, __store_net_key[net_idx].val, sizeof(__store_net_key[net_idx].val));

        LOG_DBG("NetKeyIndex 0x%03x recovered from storage", net_idx);

        err = bt_mesh_subnet_set(net_idx, key.kr_phase, &val[0],
                                 (key.kr_phase != BT_MESH_KR_NORMAL) ? &val[1] : NULL);
        if (err) {
            LOG_ERR("<bt_mesh_subnet_set> fail for net_idx:0x%x", net_idx);
            continue;
        }
    }

    return 0;
}

static int app_key_set(void)
{
    struct app_key_val key;
    struct bt_mesh_key val[2];
    u16_t app_idx;
    int err;

    LOG_DBG("\n < --%s-- >", __FUNCTION__);

    err = node_info_load(APP_KEY_INDEX, __store_app_key, (sizeof(struct app_key_val) * CONFIG_BT_MESH_APP_KEY_COUNT));
    if (err) {
        LOG_ERR("<app_key_set> memory load fail for app_idx:0x%x", app_idx);
        return -1;
    }

    for (app_idx = 0; app_idx < CONFIG_BT_MESH_APP_KEY_COUNT; app_idx++) {
        if (!__store_app_key[app_idx].existence) {
            LOG_ERR("NO app_idx:0x%x", app_idx);
            continue;
        }
        /* One extra copying since key.val array is from packed structure
        * and might be unaligned.
        */
        memcpy(val, __store_app_key[app_idx].val, sizeof(key.val));

        err = bt_mesh_app_key_set(app_idx, __store_app_key[app_idx].net_idx, &val[0],
                                  key.updated ? &val[1] : NULL);
        if (err) {
            LOG_ERR("<app_key_set> fail for app_idx:0x%x'", app_idx);
            continue;
        }

        LOG_DBG("AppKeyIndex 0x%03x recovered from storage", app_idx);
    }

    return 0;
}

static int hb_pub_set(void)
{
    struct bt_mesh_hb_pub hb_pub;
    struct hb_pub_val hb_val;
    int err;

    LOG_DBG("\n < --%s-- >", __FUNCTION__);

    err = node_info_load(HB_PUB_INDEX, &hb_val, sizeof(hb_val));
    if (err) {
        LOG_ERR("Failed to load \'hb_val\'");
        return err;
    }

    hb_pub.dst = hb_val.dst;
    hb_pub.period = bt_mesh_hb_pwr2(hb_val.period);
    hb_pub.ttl = hb_val.ttl;
    hb_pub.feat = hb_val.feat;
    hb_pub.net_idx = hb_val.net_idx;

    if (hb_val.indefinite) {
        hb_pub.count = 0xffff;
    } else {
        hb_pub.count = 0U;
    }

    (void)bt_mesh_hb_pub_set(&hb_pub);

    LOG_DBG("Restored heartbeat publication");

    return 0;
}


static int cfg_set(void)
{
    struct cfg_val cfg;
    int err;

    LOG_DBG("\n < --%s-- >", __FUNCTION__);

    err = node_info_load(CFG_INDEX, &cfg, sizeof(cfg));
    if (err) {
        LOG_ERR("Failed to load \'cfg\'");
        return err;
    }

    bt_mesh_net_transmit_set(cfg.net_transmit);
    bt_mesh_relay_set(cfg.relay, cfg.relay_retransmit);
    bt_mesh_beacon_set(cfg.beacon);
    bt_mesh_gatt_proxy_set(cfg.gatt_proxy);
    bt_mesh_friend_set(cfg.frnd);
    bt_mesh_default_ttl_set(cfg.default_ttl);

    LOG_DBG("Restored configuration state");

    return 0;
}

static void mod_set_bind(bool vnd, u8 model_count)
{
    int err;
    struct bt_mesh_model *mod;
    u8_t elem_idx, mod_idx;
    struct __mod_bind _mod_bind;

    u8 load_index = vnd ? VND_MOD_BIND_INDEX : MOD_BIND_INDEX;

    LOG_DBG("\n < --%s-- >", __FUNCTION__);

    err = node_info_load(vnd ? VND_MOD_BIND_INDEX : MOD_BIND_INDEX, vnd ? __store_vnd_mod_bind : __store_sig_mod_bind,
                         (sizeof(struct __mod_bind) * (vnd ? MAX_VND_MODEL_NUMS : MAX_SIG_MODEL_NUMS)));
    if (err) {
        LOG_ERR("%s <mod bind> load not exist", vnd ? "Vendor" : "SIG");
        return;
    }

    for (u8 i = 0; i < model_count; i++) {
        if (!(vnd ? __store_vnd_mod_bind[i].existence : __store_sig_mod_bind[i].existence)) {
            continue;
        }

        get_elem_and_mod_idx(vnd, i, &elem_idx, &mod_idx);
        LOG_DBG("Decoded elem_idx:%u; mod_idx:%u", elem_idx, mod_idx);

        mod = bt_mesh_model_get(vnd, elem_idx, mod_idx);

        if (vnd) {
            memcpy((u8 *)mod->keys, (u8 *)__store_vnd_mod_bind[i].keys, mod->keys_cnt * sizeof(mod->keys[0]));
        } else {
            memcpy((u8 *)mod->keys, (u8 *)__store_sig_mod_bind[i].keys, mod->keys_cnt * sizeof(mod->keys[0]));
        }

        LOG_DBG("Decoded %u bound keys for model", sizeof(mod->keys) / sizeof(mod->keys[0]));
        LOG_DBG("Restored model bind, keys[0]:0x%x, keys[1]:0x%x ",
                mod->keys[0], mod->keys[1]);
    }
}

static void mod_set_sub(bool vnd, u8 model_count)
{
    int err;
    struct bt_mesh_model *mod;
    u8_t elem_idx, mod_idx;
    struct __mod_sub _mod_sub;

    LOG_DBG("\n < --%s-- >", __FUNCTION__);

    err = node_info_load(vnd ? VND_MOD_SUB_INDEX : MOD_SUB_INDEX, vnd ? __store_vnd_mod_sub : __store_sig_mod_sub,
                         (sizeof(struct __mod_sub) * (vnd ? MAX_VND_MODEL_NUMS : MAX_SIG_MODEL_NUMS)));
    if (err) {
        LOG_ERR("%s <mod sub> load not exist", vnd ? "Vendor" : "SIG");
        return;
    }

    for (u8 i = 0; i < model_count; i++) {
        if (!(vnd ? __store_vnd_mod_sub[i].existence : __store_sig_mod_sub[i].existence)) {
            continue;
        }

        get_elem_and_mod_idx(vnd, i, &elem_idx, &mod_idx);
        LOG_DBG("Decoded elem_idx:%u; mod_idx:%u",
                elem_idx, mod_idx);

        mod = bt_mesh_model_get(vnd, elem_idx, mod_idx);

        if (vnd) {
            memcpy((u8 *)mod->groups, (u8 *)__store_vnd_mod_sub[i].groups, mod->groups_cnt * sizeof(mod->groups[0]));
        } else {
            memcpy((u8 *)mod->groups, (u8 *)__store_sig_mod_sub[i].groups, mod->groups_cnt * sizeof(mod->groups[0]));
        }

        LOG_DBG("Decoded %u subscribed group addresses for model",
                sizeof(mod->groups) / sizeof(mod->groups[0]));
        LOG_DBG("Restored model subscribed, groups[0]:0x%x, groups[1]:0x%x ",
                mod->groups[0], mod->groups[1]);

#if !IS_ENABLED(CONFIG_BT_MESH_LABEL_NO_RECOVER) && (CONFIG_BT_MESH_LABEL_COUNT > 0)
        /* If uuids[0] is NULL, then either the model is not subscribed to virtual addresses or
         * uuids are not yet recovered.
         */
        if (mod->uuids[0] == NULL) {
            int i, j = 0;

            for (i = 0; i < mod->groups_cnt && j < CONFIG_BT_MESH_LABEL_COUNT; i++) {
                if (BT_MESH_ADDR_IS_VIRTUAL(mod->groups[i])) {
                    /* Recover from implementation where uuid was not stored for
                     * virtual address. It is safe to pick first matched label because
                     * previously the stack wasn't able to store virtual addresses with
                     * collisions.
                     */
                    mod->uuids[j] = bt_mesh_va_uuid_get(mod->groups[i], NULL, NULL);
                    j++;
                }
            }
        }
#endif
    }
}

static void mod_set_pub(bool vnd, u8 model_count)
{
    int len, err;
    struct bt_mesh_model *mod;
    u8_t elem_idx, mod_idx;
    struct mod_pub_val _mod_pub;

    u8 load_index = vnd ? VND_MOD_PUB_INDEX : MOD_PUB_INDEX;

    LOG_DBG("\n < --%s-- >", __FUNCTION__);

    for (u8 i = 0; i < model_count; i++) {
        err = node_info_load(load_index + i, &_mod_pub, sizeof(struct mod_pub_val));
        if (err) {
            LOG_ERR("%s <mod pub> load not exist", vnd ? "Vendor" : "SIG");
            continue;
        }

        get_elem_and_mod_idx(vnd, i, &elem_idx, &mod_idx);
        /*LOG_DBG*/printf("Decoded elem_idx:%u; mod_idx:%u",
                          elem_idx, mod_idx);

        mod = bt_mesh_model_get(vnd, elem_idx, mod_idx);

        mod->pub->addr          = _mod_pub.base.addr;
        mod->pub->key           = _mod_pub.base.key;
        mod->pub->cred          = _mod_pub.base.cred;
        mod->pub->ttl           = _mod_pub.base.ttl;
        mod->pub->period        = _mod_pub.base.period;
        mod->pub->retransmit    = _mod_pub.base.retransmit;
        mod->pub->count         = 0;

        if (BT_MESH_ADDR_IS_VIRTUAL(_mod_pub.base.addr)) {
            mod->pub->uuid = bt_mesh_va_get_uuid_by_idx(_mod_pub.uuidx);
        }

        LOG_DBG("Restored model publication, dst 0x%04x app_idx 0x%03x",
                _mod_pub.base.addr, _mod_pub.base.key);
    }
}

static int mod_set_sub_va(bool vnd, u8 model_count)
{
#if CONFIG_BT_MESH_LABEL_COUNT > 0
    u16_t uuidxs[CONFIG_BT_MESH_LABEL_COUNT];
    struct bt_mesh_model *mod;
    struct __mod_sub_va _mod_sub_va;
    u8_t elem_idx, mod_idx;
    int err;
    int count = 0;

    u8 load_index = vnd ? VND_MOD_SUB_VA_INDEX : MOD_SUB_VA_INDEX;

    LOG_DBG("\n < --%s-- >", __FUNCTION__);

    for (u8 i = 0; i < model_count; i++) {
        err = node_info_load(load_index + i, &_mod_sub_va, sizeof(_mod_sub_va));
        if (err) {
            LOG_ERR("%s <mod pub> load not exist", vnd ? "Vendor" : "SIG");
            continue;
        }

        get_elem_and_mod_idx(vnd, i, &elem_idx, &mod_idx);

        LOG_DBG("Decoded elem_idx:%u; mod_idx:%u",
                elem_idx, mod_idx);

        mod = bt_mesh_model_get(vnd, elem_idx, mod_idx);

        mod->uuids[count] = bt_mesh_va_get_uuid_by_idx(_mod_sub_va.uuidxs[i]);

        if (mod->uuids[count] != NULL) {
            count++;
        }
    }

    LOG_DBG("Decoded %zu subscribed virtual addresses for model", count);
#endif /* CONFIG_BT_MESH_LABEL_COUNT > 0 */
    return 0;
}

static int mod_data_set(bool vnd, u8 model_count)
{
    struct bt_mesh_model *mod;
    int err;
    u8_t elem_idx, mod_idx;

    LOG_DBG("\n < --%s-- >", __FUNCTION__);

    for (u8 i = 0; i < model_count; i++) {

        get_elem_and_mod_idx(vnd, i, &elem_idx, &mod_idx);

        LOG_DBG("Decoded elem_idx:%u; mod_idx:%u",
                elem_idx, mod_idx);

        mod = bt_mesh_model_get(vnd, elem_idx, mod_idx);

        if (mod->cb && mod->cb->settings_set) {
            mod->cb->settings_set(mod, NULL, 0, NULL, NULL);
        }
    }

    return 0;
}

static void mod_set(bool vnd, u8 model_count)
{
    mod_set_bind(vnd, model_count);

    mod_set_sub(vnd, model_count);

    mod_set_pub(vnd, model_count);

    mod_set_sub_va(vnd, model_count);

    mod_data_set(vnd, model_count);
}

static int sig_mod_set(void)
{
    LOG_DBG("\n < --%s-- >", __FUNCTION__);

    u8 model_count = bt_mesh_model_numbers_get(false);

    if (model_count) {
        mod_set(false, model_count);
        return 0;
    } else {
        LOG_ERR("no sig model exist");
        return -EINVAL;
    }
}

static int vnd_mod_set(void)
{
    LOG_DBG("\n < --%s-- >", __FUNCTION__);

    u8 model_count = bt_mesh_model_numbers_get(true);
    if (model_count) {
        mod_set(true, model_count);
        return 0;
    } else {
        LOG_ERR("no vendor model exist");
        return -EINVAL;
    }
}

#if (CONFIG_BT_MESH_RPR_SRV)
int dev_key_cand_set(void)
{
    int err;

    LOG_DBG("\n < --%s-- >", __FUNCTION__);

    err = node_info_load(DEVKEY_CAND_INDEX, &bt_mesh.dev_key_cand,
                         sizeof(struct bt_mesh_key));
    if (!err) {
        LOG_DBG("DevKey candidate recovered from storage");
        atomic_set_bit(bt_mesh.flags, BT_MESH_DEVKEY_CAND);
    }

    return err;
}
#endif

#if CONFIG_BT_MESH_LABEL_COUNT > 0
int va_set(void)
{
    struct va_val va;
    struct bt_mesh_va *lab;
    u16_t i;
    int err;

    LOG_DBG("\n < --%s-- >", __FUNCTION__);

    for (i = 0; i < ARRAY_SIZE(virtual_addrs); i++) {
        err = node_info_load(VIRTUAL_ADDR_INDEX + i, &va, sizeof(va));
        if (err) {
            LOG_ERR("Failed to load \'virtual address\'");
            continue;
        }

        if (va.ref == 0) {
            LOG_WRN("Ignore Mesh Virtual Address ref = 0");
            continue;
        }

        lab = va_get_by_idx(i);
        if (lab == NULL) {
            LOG_WRN("Out of labels buffers");
            return -ENOBUFS;
        }

        memcpy(lab->uuid, va.uuid, 16);
        lab->addr = va.addr;
        lab->ref = va.ref;

        LOG_DBG("Restored Virtual Address, addr 0x%04x ref 0x%04x", lab->addr, lab->ref);
    }

    return 0;
}
#endif

#if (CONFIG_BT_MESH_DFU_DIST)
extern struct file_parameter_t file_parameter;
void dist_cfg_set(void)
{
    int err;
    struct file_parameter_t cfg;

    LOG_INF("\n < --%s-- >", __FUNCTION__);

    err = node_info_load(DFU_CFG_INDEX, &file_parameter,
                         sizeof(struct file_parameter_t));
    if (err) {
        LOG_ERR("Distributor config recovered from storage");
        return;
    }

    LOG_DBG(">>> load file size 0x%x file crc 0x%x", file_parameter.file_size, file_parameter.crc);
    LOG_DBG(">>> file version %s ", bt_hex(file_parameter.file_version, 8));

    return;
}

void store_pending_dist_cfg(void)
{
    struct file_parameter_t cfg;

    LOG_INF("--func=%s", __FUNCTION__);


    memcpy(cfg.file_version, file_parameter.file_version, 8);
    cfg.file_size = file_parameter.file_size;
    cfg.crc = file_parameter.crc;

    LOG_DBG(">>> store file size 0x%x file crc 0x%x", cfg.file_size, cfg.crc);
    LOG_DBG(">>> file version %s ", bt_hex(cfg.file_version, 8));

    node_info_store(DFU_CFG_INDEX, &cfg, sizeof(cfg));
}

void clear_dist_cfg(void)
{
    LOG_INF("--func=%s", __FUNCTION__);

    struct file_parameter_t cfg;

    memset(file_parameter.file_version, 0x0, 8);
    file_parameter.file_size = 0x0;
    file_parameter.crc = 0x0;

    memset(&cfg, 0x0, sizeof(cfg));

    node_info_store(DFU_CFG_INDEX, &cfg, sizeof(cfg));
}
#endif

#if (CONFIG_BT_MESH_DFU_TARGET)
extern struct target_img_t target_img;
void target_cfg_set(void)
{
    int err;

    LOG_DBG("\n < --%s-- >", __FUNCTION__);

    err = node_info_load(DFU_CFG_INDEX, &target_img,
                         sizeof(struct target_img_t));
    if (err) {
        LOG_ERR("Target config recovered from storage");
        return;
    }

    LOG_DBG(">>> file version %s ", bt_hex(target_img.fwid, 8));

    return;
}

void store_pending_target_cfg(void)
{
    struct target_img_t cfg;

    LOG_INF("--func=%s", __FUNCTION__);


    memcpy(cfg.fwid, target_img.fwid, 8);

    LOG_DBG(">>> file version %s ", bt_hex(cfg.fwid, 8));

    node_info_store(DFU_CFG_INDEX, &cfg, sizeof(cfg));
}

void clear_target_cfg(void)
{
    LOG_INF("--func=%s", __FUNCTION__);

    struct target_img_t cfg;

    memset(target_img.fwid, 0x0, 8);

    memset(&cfg, 0x0, sizeof(cfg));

    node_info_store(DFU_CFG_INDEX, &cfg, sizeof(cfg));
}
#endif

#if (CONFIG_BT_MESH_CDB)
int cdb_set(void)
{
    cdb_net_set();

    cdb_node_set();

    cdb_subnet_set();

    cdb_app_key_set();
}
#endif

const struct mesh_setting {
    int (*func)(void);
} settings[] = {
    net_set,
    iv_set,
    seq_set,
    rpl_set,
    net_key_set,
    app_key_set,
    hb_pub_set,
    cfg_set,
    sig_mod_set,
    vnd_mod_set,
#if (CONFIG_BT_MESH_RPR_SRV)
    dev_key_cand_set,
#endif
#if CONFIG_BT_MESH_LABEL_COUNT > 0
    va_set,
#endif
#if (CONFIG_BT_MESH_CDB)
    cdb_set,
#endif
#if (CONFIG_BT_MESH_PROXY_SOLICITATION)
    sseq_set,
#endif
#if (CONFIG_BT_MESH_OD_PRIV_PROXY_SRV)
    srpl_set,
#endif
#if (CONFIG_BT_MESH_DFU_DIST)
    dist_cfg_set,
#endif
};

static void mesh_set(void)
{
    LOG_DBG("--func=%s", __FUNCTION__);

    for (int i = 0; i < ARRAY_SIZE(settings); i++) {
        settings[i].func();
    }
}

static int mesh_commit(void)
{
    if (!atomic_test_bit(bt_mesh.flags, BT_MESH_INIT)) {
        LOG_ERR(">>> %s bt mesh is not init!", __func__);
        return 0;
    }

    if (!bt_mesh_subnet_next(NULL)) {
        /* Nothing to do since we're not yet provisioned */
        LOG_ERR(">>> %s No subnet!", __func__);
        return 0;
    }

    if (IS_ENABLED(CONFIG_BT_MESH_PB_GATT)) {
        (void)bt_mesh_pb_gatt_srv_disable();
    }

    bt_mesh_net_settings_commit();
    bt_mesh_model_settings_commit();

    atomic_set_bit(bt_mesh.flags, BT_MESH_VALID);

    bt_mesh_start();

    return 0;
}

#if 0	/* These are used to store_pending() with k_work_schedule() */
/* Pending flags that use K_NO_WAIT as the storage timeout */
#define NO_WAIT_PENDING_BITS (BIT(BT_MESH_SETTINGS_NET_PENDING) |           \
			      BIT(BT_MESH_SETTINGS_IV_PENDING)  |           \
			      BIT(BT_MESH_SETTINGS_SEQ_PENDING) |           \
			      BIT(BT_MESH_SETTINGS_CDB_PENDING))

/* Pending flags that use CONFIG_BT_MESH_STORE_TIMEOUT */
#define GENERIC_PENDING_BITS (BIT(BT_MESH_SETTINGS_NET_KEYS_PENDING) |      \
			      BIT(BT_MESH_SETTINGS_APP_KEYS_PENDING) |      \
			      BIT(BT_MESH_SETTINGS_HB_PUB_PENDING)   |      \
			      BIT(BT_MESH_SETTINGS_CFG_PENDING)      |      \
			      BIT(BT_MESH_SETTINGS_MOD_PENDING)      |      \
			      BIT(BT_MESH_SETTINGS_VA_PENDING)       |      \
			      BIT(BT_MESH_SETTINGS_SSEQ_PENDING)     |      \
			      BIT(BT_MESH_SETTINGS_COMP_PENDING)     |      \
			      BIT(BT_MESH_SETTINGS_DEV_KEY_CAND_PENDING))
#endif

void bt_mesh_settings_store_schedule(enum bt_mesh_settings_flag flag)
{
    u32_t timeout_ms, remaining_ms;

    atomic_set_bit(pending_flags, flag);

#if 0	/* These are used to store_pending() with k_work_schedule() */
    if (atomic_get(pending_flags) & NO_WAIT_PENDING_BITS) {
        timeout_ms = 0;
    } else if (IS_ENABLED(CONFIG_BT_MESH_RPL_STORAGE_MODE_SETTINGS) && RPL_STORE_TIMEOUT >= 0 &&
               (atomic_test_bit(pending_flags, BT_MESH_SETTINGS_RPL_PENDING) ||
                atomic_test_bit(pending_flags, BT_MESH_SETTINGS_SRPL_PENDING)) &&
               !(atomic_get(pending_flags) & GENERIC_PENDING_BITS)) {
        timeout_ms = RPL_STORE_TIMEOUT * MSEC_PER_SEC;
    } else {
        timeout_ms = CONFIG_BT_MESH_STORE_TIMEOUT * MSEC_PER_SEC;
    }

    // remaining_ms = k_ticks_to_ms_floor32(k_work_delayable_remaining_get(&pending_store));//for compiler, not used now.
    LOG_DBG("Waiting %u ms vs rem %u ms", timeout_ms, remaining_ms);

    /* If the new deadline is sooner, override any existing
     * deadline; otherwise schedule without changing any existing
     * deadline.
     */
    k_work_schedule(&pending_store, K_MSEC(timeout_ms));
#endif

    OS_ENTER_CRITICAL();
    store_pending(NULL);
    OS_EXIT_CRITICAL();
}

void bt_mesh_settings_store_cancel(enum bt_mesh_settings_flag flag)
{
    atomic_clear_bit(pending_flags, flag);
}

static void clear_net(void)
{
    LOG_INF("--func=%s", __FUNCTION__);

    node_info_clear(NET_INDEX, sizeof(struct net_val));
}

static void store_pending_net(void)
{
    struct net_val net;

    LOG_INF("--func=%s", __FUNCTION__);
    LOG_DBG("addr 0x%04x DevKey %s", bt_mesh_primary_addr(),
            bt_hex(&bt_mesh.dev_key, sizeof(struct bt_mesh_key)));

    net.primary_addr = bt_mesh_primary_addr();
    memcpy(&net.dev_key, &bt_mesh.dev_key, sizeof(struct bt_mesh_key));

    node_info_store(NET_INDEX, &net, sizeof(net));
}

void bt_mesh_net_pending_net_store(void)
{
    if (atomic_test_bit(bt_mesh.flags, BT_MESH_VALID)) {
        store_pending_net();
    } else {
        clear_net();
    }
}

static void clear_iv(void)
{
    LOG_INF("Cleared IV");

    node_info_clear(IV_INDEX, sizeof(struct iv_val));
}

static void store_pending_iv(void)
{
    struct iv_val iv;

    LOG_INF("--func=%s", __FUNCTION__);

    iv.iv_index = bt_mesh.iv_index;
    iv.iv_update = atomic_test_bit(bt_mesh.flags, BT_MESH_IVU_IN_PROGRESS);
    iv.iv_duration = bt_mesh.ivu_duration;

    node_info_store(IV_INDEX, &iv, sizeof(iv));
}

void bt_mesh_net_pending_iv_store(void)
{
    if (atomic_test_bit(bt_mesh.flags, BT_MESH_VALID)) {
        store_pending_iv();
    } else {
        clear_iv();
    }
}

void bt_mesh_net_pending_seq_store(void)
{
    struct seq_val seq;
    int err;

    if (atomic_test_bit(bt_mesh.flags, BT_MESH_VALID)) {
        LOG_DBG("Stored Seq value");

        sys_put_le24(bt_mesh.seq, seq.val);

        node_info_store(SEQ_INDEX, &seq, sizeof(seq));
    } else {
        LOG_DBG("Cleared Seq value");
        node_info_clear(SEQ_INDEX, sizeof(struct seq_val));
    }
}

void store_rpl(struct bt_mesh_rpl *entry, u8 index)
{
    int err;

    if (!entry->src) {
        return;
    }

    LOG_DBG("src 0x%04x seq 0x%06x old_iv %u", entry->src, entry->seq, entry->old_iv);

    __store_rpl[index].rpl.seq = entry->seq;
    __store_rpl[index].rpl.old_iv = entry->old_iv;
    __store_rpl[index].src = entry->src;

    node_info_store(RPL_INDEX, __store_rpl, (sizeof(struct __rpl_val) * CONFIG_BT_MESH_CRPL));
}

void clear_net_key(u16_t net_idx)
{
    LOG_DBG("NetKeyIndex 0x%03x", net_idx);

    memset(&(__store_net_key[net_idx].val[0]), 0x0, sizeof(struct bt_mesh_key));
    memset(&(__store_net_key[net_idx].val[1]), 0x0, sizeof(struct bt_mesh_key));
    __store_net_key[net_idx].kr_flag = 0U; /* Deprecated */
    __store_net_key[net_idx].kr_phase = BT_MESH_KR_NORMAL;
    __store_net_key[net_idx].existence = 0U;

    node_info_store(NET_KEY_INDEX, __store_net_key, sizeof(struct net_key_val) * CONFIG_BT_MESH_SUBNET_COUNT);

    LOG_DBG("Cleared NetKeyIndex 0x%03x", net_idx);
}

void store_subnet(u16_t net_idx)
{
    const struct bt_mesh_subnet *sub;
    struct net_key_val key;
    int err;

    sub = bt_mesh_subnet_get(net_idx);
    if (!sub) {
        LOG_WRN("NetKeyIndex 0x%03x not found", net_idx);
        return;
    }

    LOG_DBG("NetKeyIndex 0x%03x", net_idx);

    memcpy(&(__store_net_key[net_idx].val[0]), &sub->keys[0].net, sizeof(struct bt_mesh_key));
    memcpy(&(__store_net_key[net_idx].val[1]), &sub->keys[1].net, sizeof(struct bt_mesh_key));
    __store_net_key[net_idx].kr_flag = 0U; /* Deprecated */
    __store_net_key[net_idx].kr_phase = sub->kr_phase;
    __store_net_key[net_idx].existence = 1U;

    node_info_store(NET_KEY_INDEX, __store_net_key, sizeof(struct net_key_val) * CONFIG_BT_MESH_SUBNET_COUNT);

    LOG_DBG("Stored NetKey value");

}

void clear_app_key(u16_t app_idx)
{
    __store_app_key[app_idx].net_idx = 0U;
    __store_app_key[app_idx].updated = 0U;
    __store_app_key[app_idx].existence = 0U;

    memset(&__store_app_key[app_idx].val[0], 0x0, sizeof(struct bt_mesh_key));
    memset(&__store_app_key[app_idx].val[1], 0x0, sizeof(struct bt_mesh_key));

    node_info_store(APP_KEY_INDEX, __store_app_key, (sizeof(struct app_key_val) * CONFIG_BT_MESH_APP_KEY_COUNT));

    LOG_DBG("Cleared AppKeyIndex 0x%03x", app_idx);
}

void store_app_key(u16_t app_idx)
{
    const struct app_key *app;

    app = app_get(app_idx);
    if (!app) {
        LOG_WRN("ApKeyIndex 0x%03x not found", app_idx);
        return;
    }

    __store_app_key[app_idx].net_idx = app->net_idx;
    __store_app_key[app_idx].updated = app->updated;
    __store_app_key[app_idx].existence = 1U;

    memcpy(&__store_app_key[app_idx].val[0], &app->keys[0].val, sizeof(struct bt_mesh_key));
    memcpy(&__store_app_key[app_idx].val[1], &app->keys[1].val, sizeof(struct bt_mesh_key));

    node_info_store(APP_KEY_INDEX, __store_app_key, (sizeof(struct app_key_val) * CONFIG_BT_MESH_APP_KEY_COUNT));

    LOG_DBG("Stored AppKey 0x%x value", app_idx);
}

void bt_mesh_hb_pub_pending_store(void)
{
    struct bt_mesh_hb_pub hb_pub;
    struct hb_pub_val val;
    int err;

    bt_mesh_hb_pub_get(&hb_pub);

    if (hb_pub.dst == BT_MESH_ADDR_UNASSIGNED) {
        node_info_clear(HB_PUB_INDEX, sizeof(struct hb_pub_val));
    } else {
        val.indefinite = (hb_pub.count == 0xffff);
        val.dst = hb_pub.dst;
        val.period = bt_mesh_hb_log(hb_pub.period);
        val.ttl = hb_pub.ttl;
        val.feat = hb_pub.feat;
        val.net_idx = hb_pub.net_idx;

        node_info_store(HB_PUB_INDEX, &val, sizeof(val));
    }

    LOG_DBG("Stored Heartbeat Publication");
}

void clear_cfg(void)
{
    node_info_clear(CFG_INDEX, sizeof(struct cfg_val));

    LOG_DBG("Cleared configuration");
}

void store_pending_cfg(void)
{
    struct cfg_val val;
    int err;

    val.net_transmit = bt_mesh_net_transmit_get();
    val.relay = bt_mesh_relay_get();
    val.relay_retransmit = bt_mesh_relay_retransmit_get();
    val.beacon = bt_mesh_beacon_enabled();
    val.gatt_proxy = bt_mesh_gatt_proxy_get();
    val.frnd = bt_mesh_friend_get();
    val.default_ttl = bt_mesh_default_ttl_get();

    node_info_store(CFG_INDEX, &val, sizeof(val));

    LOG_DBG("Stored configuration value");
    LOG_HEXDUMP_INF(&val, sizeof(val));
}

void store_pending_mod_bind(struct bt_mesh_model *mod, bool vnd)
{
    int i, count;
    u8 store_index;

    LOG_DBG("--func=%s", __FUNCTION__);

    store_index = get_model_store_index(vnd, mod->rt->elem_idx, mod->rt->mod_idx);

    for (i = 0, count = 0; i < mod->keys_cnt; i++) {
        if (mod->keys[i] != BT_MESH_KEY_UNUSED) {
            if (vnd) {
                __store_vnd_mod_bind[store_index].keys[count++] = mod->keys[i];
                __store_vnd_mod_bind[store_index].existence = 1;
            } else {
                __store_sig_mod_bind[store_index].keys[count++] = mod->keys[i];
                __store_sig_mod_bind[store_index].existence = 1;
            }
        }
    }

    if (count) {
        node_info_store(vnd ? VND_MOD_BIND_INDEX : MOD_BIND_INDEX, vnd ? __store_vnd_mod_bind : __store_sig_mod_bind,
                        (sizeof(struct __mod_bind) * (vnd ? MAX_VND_MODEL_NUMS : MAX_SIG_MODEL_NUMS)));
    } else {//clear mod_bind
        for (i = 0; i < mod->keys_cnt; i++) {
            if (vnd) {
                __store_vnd_mod_bind[store_index].keys[i] = 0;
                __store_vnd_mod_bind[store_index].existence = 0;
            } else {
                __store_sig_mod_bind[store_index].keys[i] = 0;
                __store_sig_mod_bind[store_index].existence = 0;
            }
        }
        node_info_store(vnd ? VND_MOD_BIND_INDEX : MOD_BIND_INDEX, vnd ? __store_vnd_mod_bind : __store_sig_mod_bind,
                        (sizeof(struct __mod_bind) * (vnd ? MAX_VND_MODEL_NUMS : MAX_SIG_MODEL_NUMS)));
    }
}

void store_pending_mod_sub(struct bt_mesh_model *mod, bool vnd)
{
    int i, count;
    u8 store_index;

    LOG_DBG("--func=%s", __FUNCTION__);

    store_index = get_model_store_index(vnd, mod->rt->elem_idx, mod->rt->mod_idx);

    for (i = 0, count = 0; i < mod->groups_cnt; i++) {
        if (mod->groups[i] != BT_MESH_ADDR_UNASSIGNED) {
            if (vnd) {
                __store_vnd_mod_sub[store_index].groups[count++] = mod->groups[i];
                __store_vnd_mod_sub[store_index].existence = 1;
            } else {
                __store_sig_mod_sub[store_index].groups[count++] = mod->groups[i];
                __store_sig_mod_sub[store_index].existence = 1;
            }
        }
    }

    if (count) {
        node_info_store(vnd ? VND_MOD_SUB_INDEX : MOD_SUB_INDEX, vnd ? __store_vnd_mod_sub : __store_sig_mod_sub,
                        (sizeof(struct __mod_sub) * (vnd ? MAX_VND_MODEL_NUMS : MAX_SIG_MODEL_NUMS)));
    } else {	//clear mod_sub
        for (i = 0, count = 0; i < mod->groups_cnt; i++) {
            if (vnd) {
                __store_vnd_mod_sub[store_index].groups[i] = 0;
                __store_vnd_mod_sub[store_index].existence = 0;
            } else {
                __store_sig_mod_sub[store_index].groups[i] = 0;
                __store_sig_mod_sub[store_index].existence = 0;
            }
        }
        LOG_DBG("%s L %d", __func__, __LINE__);
        node_info_store(vnd ? VND_MOD_SUB_INDEX : MOD_SUB_INDEX, vnd ? __store_vnd_mod_sub : __store_sig_mod_sub,
                        (sizeof(struct __mod_sub) * (vnd ? MAX_VND_MODEL_NUMS : MAX_SIG_MODEL_NUMS)));
    }
}

void store_pending_mod_sub_va(const struct bt_mesh_model *mod, bool vnd)
{
#if CONFIG_BT_MESH_LABEL_COUNT > 0
    struct __mod_sub_va _mod_sub_va;
    int i, count, err;
    u8 store_index;

    LOG_DBG("--func=%s", __FUNCTION__);

    for (i = 0, count = 0; i < CONFIG_BT_MESH_LABEL_COUNT; i++) {
        if (mod->uuids[i] != NULL) {
            err = bt_mesh_va_get_idx_by_uuid(mod->uuids[i], &_mod_sub_va.uuidxs[count]);
            if (!err) {
                count++;
            }
        }
    }

    store_index = get_model_store_index(vnd, mod->rt->elem_idx, mod->rt->mod_idx);
    store_index += vnd ? VND_MOD_SUB_VA_INDEX : MOD_SUB_VA_INDEX;

    if (count) {
        node_info_store(store_index, &_mod_sub_va, sizeof(struct __mod_sub_va));
    } else {
        node_info_clear(store_index, sizeof(struct __mod_sub_va));
    }
#endif /* CONFIG_BT_MESH_LABEL_COUNT > 0 */
}

void store_pending_mod_pub(struct bt_mesh_model *mod, bool vnd)
{
    struct mod_pub_val _mod_pub;
    u8 store_index;

    LOG_DBG("--func=%s", __FUNCTION__);

    store_index = get_model_store_index(vnd, mod->rt->elem_idx, mod->rt->mod_idx);
    store_index += vnd ? VND_MOD_PUB_INDEX : MOD_PUB_INDEX;

    if (!mod->pub || mod->pub->addr == BT_MESH_ADDR_UNASSIGNED) {
        node_info_clear(store_index, sizeof(struct mod_pub_val));
    } else {
        _mod_pub.base.addr = mod->pub->addr;
        _mod_pub.base.key = mod->pub->key;
        _mod_pub.base.ttl = mod->pub->ttl;
        _mod_pub.base.retransmit = mod->pub->retransmit;
        _mod_pub.base.period = mod->pub->period;
        _mod_pub.base.period_div = 0U;
        _mod_pub.base.cred = mod->pub->cred;
        _mod_pub.uuidx = 0U;

        if (BT_MESH_ADDR_IS_VIRTUAL(mod->pub->addr)) {
            (void)bt_mesh_va_get_idx_by_uuid(mod->pub->uuid, &_mod_pub.uuidx);
        }

        node_info_store(store_index, &_mod_pub, sizeof(struct mod_pub_val));
    }
}

int bt_mesh_model_data_store(const struct bt_mesh_model *mod, bool vnd,
                             const char *name, const void *data,
                             size_t data_len)
{
    u8 store_index;
    u16 clear_len;

    LOG_DBG("--func=%s %s", __FUNCTION__, name);

    if (!strncmp(name, "blob", 4)) {
        store_index = BLOB_SRV_STATE_INDEX;
        clear_len = sizeof(data);
    } else if (!strncmp(name, "dfu", 3)) {
        store_index = DFU_SRV_STATE_INDEX;
        clear_len = sizeof(data);
    } else if (!strncmp(name, "pp", 2)) {
        store_index = OD_PP_STATE_INDEX;
        clear_len = sizeof(uint8_t);
    } else if (!strncmp(name, "pb", 2)) {
        store_index = PRIV_BEACON_STATE_INDEX;
        extern priv_beacon_state;
        clear_len = sizeof(priv_beacon_state);
    } else if (!strncmp(name, "sar_rx", 6)) {
        store_index = SAR_RX_STATE_INDEX;
        clear_len = sizeof(struct bt_mesh_sar_rx);
    } else if (!strncmp(name, "sar_tx", 6)) {
        store_index = SAR_TX_STATE_INDEX;
        clear_len = sizeof(struct bt_mesh_sar_tx);
    } else {
        LOG_WRN("No Adaptation");
        return -1;
    }

    if (data_len) {
        node_info_store(store_index, data, data_len);
    } else {
        node_info_clear(store_index, clear_len);
    }

    return 0;
}

void bt_mesh_net_pending_dev_key_cand_store(void)
{
#if (CONFIG_BT_MESH_RPR_SRV)
    int err;

    if (atomic_test_bit(bt_mesh.flags, BT_MESH_DEVKEY_CAND)) {
        node_info_store(DEVKEY_CAND_INDEX, &bt_mesh.dev_key_cand, sizeof(struct bt_mesh_key));
    } else {
        node_info_clear(DEVKEY_CAND_INDEX, sizeof(struct bt_mesh_key));
    }

#endif
}

#if CONFIG_BT_MESH_LABEL_COUNT > 0
#define IS_VA_DEL(_label)	((_label)->ref == 0)
void bt_mesh_va_pending_store(void)
{
    struct bt_mesh_va *lab;
    struct va_val va;
    u16_t i;
    int err;

    for (i = 0; (lab = va_get_by_idx(i)) != NULL; i++) {
        if (!lab->changed) {
            continue;
        }

        lab->changed = 0U;

        if (IS_VA_DEL(lab)) {
            node_info_clear(VIRTUAL_ADDR_INDEX, sizeof(struct va_val));
        } else {
            va.ref = lab->ref;
            va.addr = lab->addr;
            memcpy(va.uuid, lab->uuid, 16);

            node_info_store(VIRTUAL_ADDR_INDEX, &va, sizeof(va));
        }

        LOG_DBG("%s %s value", IS_VA_DEL(lab) ? "Deleted" : "Stored", path);
    }
}
#else
void bt_mesh_va_pending_store(void)
{
    /* Do nothing. */
}
#endif /* CONFIG_BT_MESH_LABEL_COUNT > 0 */

static void store_pending(struct k_work *work)
{
    LOG_DBG("");

    if (atomic_test_and_clear_bit(pending_flags, BT_MESH_SETTINGS_RPL_PENDING)) {
        bt_mesh_rpl_pending_store(BT_MESH_ADDR_ALL_NODES);
    }

    if (atomic_test_and_clear_bit(pending_flags,
                                  BT_MESH_SETTINGS_NET_KEYS_PENDING)) {
        bt_mesh_subnet_pending_store();
    }

    if (atomic_test_and_clear_bit(pending_flags,
                                  BT_MESH_SETTINGS_APP_KEYS_PENDING)) {
        bt_mesh_app_key_pending_store();
    }

    if (atomic_test_and_clear_bit(pending_flags,
                                  BT_MESH_SETTINGS_NET_PENDING)) {
        bt_mesh_net_pending_net_store();
    }

    if (atomic_test_and_clear_bit(pending_flags,
                                  BT_MESH_SETTINGS_IV_PENDING)) {
        bt_mesh_net_pending_iv_store();
    }

    if (atomic_test_and_clear_bit(pending_flags,
                                  BT_MESH_SETTINGS_SEQ_PENDING)) {
        bt_mesh_net_pending_seq_store();
    }

    if (atomic_test_and_clear_bit(pending_flags,
                                  BT_MESH_SETTINGS_DEV_KEY_CAND_PENDING)) {
        bt_mesh_net_pending_dev_key_cand_store();
    }

    if (atomic_test_and_clear_bit(pending_flags,
                                  BT_MESH_SETTINGS_HB_PUB_PENDING)) {
        bt_mesh_hb_pub_pending_store();
    }

    if (atomic_test_and_clear_bit(pending_flags,
                                  BT_MESH_SETTINGS_CFG_PENDING)) {
        bt_mesh_cfg_pending_store();
    }

    if (atomic_test_and_clear_bit(pending_flags,
                                  BT_MESH_SETTINGS_COMP_PENDING)) {
        bt_mesh_comp_data_pending_clear();
    }

    if (atomic_test_and_clear_bit(pending_flags,
                                  BT_MESH_SETTINGS_MOD_PENDING)) {
        bt_mesh_model_pending_store();
    }

    if (atomic_test_and_clear_bit(pending_flags,
                                  BT_MESH_SETTINGS_VA_PENDING)) {
        bt_mesh_va_pending_store();
    }

    if (IS_ENABLED(CONFIG_BT_MESH_CDB) &&
        atomic_test_and_clear_bit(pending_flags,
                                  BT_MESH_SETTINGS_CDB_PENDING)) {
        bt_mesh_cdb_pending_store();
    }

    if (IS_ENABLED(CONFIG_BT_MESH_OD_PRIV_PROXY_SRV) &&
        atomic_test_and_clear_bit(pending_flags,
                                  BT_MESH_SETTINGS_SRPL_PENDING)) {
        bt_mesh_srpl_pending_store();
    }

    if (IS_ENABLED(CONFIG_BT_MESH_PROXY_SOLICITATION) &&
        atomic_test_and_clear_bit(pending_flags,
                                  BT_MESH_SETTINGS_SSEQ_PENDING)) {
        bt_mesh_sseq_pending_store();
    }
}

void settings_load(void)
{
    mesh_set();

    mesh_commit();
}

void bt_mesh_settings_init(void)
{
#if 0	/* These are used to store_pending() with k_work_schedule() */
    k_work_init_delayable(&pending_store, store_pending);
#endif
}

void bt_mesh_settings_store_pending(void)
{
#if 0	/* These are used to store_pending() with k_work_schedule() */
    (void)k_work_cancel_delayable(&pending_store);

    store_pending(&pending_store.work);
#endif
}

int settings_name_next(const char *name, const char **next)
{
    int rc = 0;

    if (next) {
        *next = NULL;
    }

    if (!name) {
        return 0;
    }

    /* name might come from flash directly, in flash the name would end
     * with '=' or '\0' depending how storage is done. Flash reading is
     * limited to what can be read
     */
    while ((*name != '\0') && (*name != SETTINGS_NAME_END) &&
           (*name != SETTINGS_NAME_SEPARATOR)) {
        rc++;
        name++;
    }

    if (*name == SETTINGS_NAME_SEPARATOR) {
        if (next) {
            *next = name + 1;
        }
        return rc;
    }

    return rc;
}

#if CONFIG_BT_MESH_PROVISIONER
static void cdb_net_set(void)
{
    LOG_INF("\n < --%s-- >", __FUNCTION__);

    struct cdb_net_val net;
    int err;

    err = node_info_load(CDB_INDEX, &net, sizeof(net));
    if (err) {
        LOG_ERR("<cdb_net_set> load fail, index:0x%x", index);
        return;
    }

    bt_mesh_cdb.iv_index = net.iv.index;

    if (net.iv.update) {
        atomic_set_bit(bt_mesh_cdb.flags, BT_MESH_CDB_IVU_IN_PROGRESS);
    }

    bt_mesh_cdb.lowest_avail_addr = net.lowest_avail_addr;

    atomic_set_bit(bt_mesh_cdb.flags, BT_MESH_CDB_VALID);
}

void clear_cdb_net(void)
{
    LOG_INF("--func=%s", __FUNCTION__);

    node_info_clear(CDB_INDEX, sizeof(struct cdb_net_val));
}

void store_cdb_pending_net(void)
{
    struct cdb_net_val net;

    LOG_INF("--func=%s", __FUNCTION__);

    net.iv.index = bt_mesh_cdb.iv_index;
    net.iv.update = atomic_test_bit(bt_mesh_cdb.flags,
                                    BT_MESH_CDB_IVU_IN_PROGRESS);
    net.lowest_avail_addr = bt_mesh_cdb.lowest_avail_addr;

    node_info_store(CDB_INDEX, &net, sizeof(net));

    LOG_DBG("Stored Network value");
}

static void cdb_node_set(void)
{
    LOG_INF("\n < --%s-- >", __FUNCTION__);

    struct bt_mesh_cdb_node *node;
    struct node_val val;
    struct bt_mesh_key tmp;
    u16_t addr;
    int err;

    for (u8 index = 0; index < CONFIG_BT_MESH_CDB_NODE_COUNT; index++) {
        err = node_info_load(CDB_NODE_INDEX + index, &val, sizeof(val));
        addr = val.addr;
        if (err) {
            /* LOG_ERR("Failed to set \'node\'"); */
            /* LOG_DBG("Deleting node 0x%04x", addr); */
            /* node = bt_mesh_cdb_node_get(addr); */
            /* if (node) { */
            /*     bt_mesh_cdb_node_del(node, false); */
            /* } */
            LOG_ERR("<cdb_node_set> load fail, index:0x%x", index);
            continue;
        }

        node = bt_mesh_cdb_node_get(addr);
        if (!node) {
            node = bt_mesh_cdb_node_alloc(val.uuid, addr, val.num_elem, val.net_idx);
        }

        if (!node) {
            LOG_ERR("No space for a new node");
            return;
        }

        if (val.flags & F_NODE_CONFIGURED) {
            atomic_set_bit(node->flags, BT_MESH_CDB_NODE_CONFIGURED);
        }

        memcpy(node->uuid, val.uuid, 16);

        /* One extra copying since val.dev_key is from packed structure
        * and might be unaligned.
        */
        memcpy(&tmp, &val.dev_key, sizeof(struct bt_mesh_key));
        bt_mesh_key_assign(&node->dev_key, &tmp);

        LOG_DBG("Node 0x%04x recovered from storage", addr);
    }
}

static void cdb_subnet_set(void)
{
    struct bt_mesh_cdb_subnet *sub;
    struct net_key_val key;
    struct bt_mesh_key tmp[2];
    u16_t net_idx;
    int err;

    for (u8 index = 0; index < (CONFIG_BT_MESH_CDB_SUBNET_COUNT + CONFIG_BT_MESH_CDB_APP_KEY_COUNT); index++) {
        err = node_info_load(CDB_SUBNET_INDEX + index, &key, sizeof(key));
        if (err) {
            /* LOG_ERR("Failed to set \'net-key\'"); */
            /* LOG_DBG("Deleting NetKeyIndex 0x%03x", net_idx); */
            /* bt_mesh_cdb_subnet_del(sub, false); */
            LOG_ERR("<cdb_subnet_set> load fail, index:0x%x", index);
            continue;
        }

        net_idx = key.net_idx;

        sub = bt_mesh_cdb_subnet_get(net_idx);

        /* One extra copying since key.val[] is from packed structure
        * and might be unaligned.
        */
        memcpy(&tmp[0], &key.val[0], sizeof(struct bt_mesh_key));
        memcpy(&tmp[1], &key.val[1], sizeof(struct bt_mesh_key));

        if (sub) {
            LOG_DBG("Updating existing NetKeyIndex 0x%03x", net_idx);

            sub->kr_phase = key.kr_phase;
            bt_mesh_key_assign(&sub->keys[0].net_key, &tmp[0]);
            bt_mesh_key_assign(&sub->keys[1].net_key, &tmp[1]);

            return;
        }

        sub = bt_mesh_cdb_subnet_alloc(net_idx);
        if (!sub) {
            LOG_ERR("No space to allocate a new subnet");
            return;
        }

        sub->kr_phase = key.kr_phase;
        bt_mesh_key_assign(&sub->keys[0].net_key, &tmp[0]);
        bt_mesh_key_assign(&sub->keys[1].net_key, &tmp[1]);

        LOG_DBG("NetKeyIndex 0x%03x recovered from storage", net_idx);
    }
}

static void cdb_app_key_set(void)
{
    struct bt_mesh_cdb_app_key *app;
    struct app_key_val key;
    struct bt_mesh_key tmp[2];
    u16_t app_idx;
    int err;

    for (u8 index = 0; index < (CONFIG_BT_MESH_CDB_SUBNET_COUNT + CONFIG_BT_MESH_CDB_APP_KEY_COUNT); index++) {
        err = node_info_load(CDB_APP_KEY_INDEX + index, &key, sizeof(key));
        if (err) {
            /* LOG_ERR("Failed to set \'app-key\'"); */
            /* LOG_DBG("val (null)"); */
            /* LOG_DBG("Deleting AppKeyIndex 0x%03x", app_idx); */
            /* app = bt_mesh_cdb_app_key_get(app_idx); */
            /* if (app) { */
            /*     bt_mesh_cdb_app_key_del(app, false); */
            /* } */
            LOG_ERR("<cdb_app_key_set> load fail, index:0x%x", index);
            continue;
        }

        app_idx = key.app_idx;

        /* One extra copying since key.val[] is from packed structure
        * and might be unaligned.
        */
        memcpy(&tmp[0], &key.val[0], sizeof(struct bt_mesh_key));
        memcpy(&tmp[1], &key.val[1], sizeof(struct bt_mesh_key));

        app = bt_mesh_cdb_app_key_get(app_idx);
        if (!app) {
            app = bt_mesh_cdb_app_key_alloc(key.net_idx, app_idx);
        }

        if (!app) {
            LOG_ERR("No space for a new app key");
            return;
        }

        bt_mesh_key_assign(&app->keys[0].app_key, &tmp[0]);
        bt_mesh_key_assign(&app->keys[1].app_key, &tmp[1]);

        LOG_DBG("AppKeyIndex 0x%03x recovered from storage", app_idx);
    }
}

void store_cdb_node(const struct bt_mesh_cdb_node *node)
{
    LOG_INF("--func=%s", __FUNCTION__);

    struct node_val val;
    u8 index = node - bt_mesh_cdb.nodes;

    LOG_DBG("store index 0x%x", index);

    val.net_idx = node->net_idx;
    val.num_elem = node->num_elem;
    val.flags = 0;
    val.addr = node->addr;

    if (atomic_test_bit(node->flags, BT_MESH_CDB_NODE_CONFIGURED)) {
        val.flags |= F_NODE_CONFIGURED;
    }

    memcpy(val.uuid, node->uuid, 16);
    memcpy(&val.dev_key, &node->dev_key, sizeof(struct bt_mesh_key));

    node_info_store(CDB_NODE_INDEX + index, &val, sizeof(val));

    LOG_DBG("Store Node 0x%04x", val.addr);
}

void clear_cdb_node(u16_t addr)
{
    LOG_INF("--func=%s", __FUNCTION__);

    u8 index = bt_mesh_cdb_node_get(addr) - bt_mesh_cdb.nodes;

    LOG_DBG("clear index 0x%x", index);

    node_info_clear(CDB_NODE_INDEX + index, sizeof(struct node_val));

    LOG_DBG("Cleared Node 0x%04x", addr);
}

void store_cdb_subnet(const struct bt_mesh_cdb_subnet *sub)
{
    LOG_INF("--func=%s", __FUNCTION__);

    struct net_key_val key;

    LOG_DBG("NetKeyIndex 0x%03x NetKey %s", sub->net_idx,
            bt_hex(&sub->keys[0].net_key, sizeof(struct bt_mesh_key)));

    memcpy(&key.val[0], &sub->keys[0].net_key, sizeof(struct bt_mesh_key));
    memcpy(&key.val[1], &sub->keys[1].net_key, sizeof(struct bt_mesh_key));
    key.kr_flag = 0U; /* Deprecated */
    key.kr_phase = sub->kr_phase;
    key.net_idx = sub->net_idx;

    node_info_store(CDB_SUBNET_INDEX + sub->net_idx, &key, sizeof(key));
}

void clear_cdb_subnet(u16_t net_idx)
{
    LOG_INF("--func=%s", __FUNCTION__);

    node_info_clear(CDB_SUBNET_INDEX + net_idx, sizeof(struct net_key_val));

    LOG_DBG("Cleared NetKeyIndex 0x%03x", net_idx);
}

void store_cdb_app_key(const struct bt_mesh_cdb_app_key *app)
{
    LOG_INF("--func=%s", __FUNCTION__);

    struct app_key_val key;

    key.net_idx = app->net_idx;
    key.updated = false;
    key.app_idx = app->app_idx;
    memcpy(&key.val[0], &app->keys[0].app_key, sizeof(struct bt_mesh_key));
    memcpy(&key.val[1], &app->keys[1].app_key, sizeof(struct bt_mesh_key));

    node_info_store(CDB_APP_KEY_INDEX + app->app_idx, &key, sizeof(key));
}

void clear_cdb_app_key(u16_t app_idx)
{
    LOG_INF("--func=%s", __FUNCTION__);

    node_info_clear(CDB_APP_KEY_INDEX + app_idx, sizeof(struct app_key_val));

    LOG_DBG("Cleared AppKeyIndex 0x%03x", app_idx);
}
#endif /* CONFIG_BT_MESH_PROVISIONER */
