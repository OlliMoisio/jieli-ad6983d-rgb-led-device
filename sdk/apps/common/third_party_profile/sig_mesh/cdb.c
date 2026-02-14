/*
 * Copyright (c) 2019 Tobias Svehagen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "adaptation.h"
#include "mesh.h"
#include "net.h"
#include "rpl.h"
#include "settings.h"
#include "keys.h"

#define LOG_TAG             "[MESH-cdb]"
#define LOG_INFO_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_WARN_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_DUMP_ENABLE
#include "mesh_log.h"

#if MESH_RAM_AND_CODE_MAP_DETAIL
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ble_mesh_prov_bss")
#pragma data_seg(".ble_mesh_prov_data")
#pragma const_seg(".ble_mesh_prov_const")
#pragma code_seg(".ble_mesh_prov_code")
#endif
#else /* MESH_RAM_AND_CODE_MAP_DETAIL */
#pragma bss_seg(".ble_mesh_bss")
#pragma data_seg(".ble_mesh_data")
#pragma const_seg(".ble_mesh_const")
#pragma code_seg(".ble_mesh_code")
#endif /* MESH_RAM_AND_CODE_MAP_DETAIL */

#if CONFIG_BT_MESH_CDB
/* Tracking of what storage changes are pending for App and Net Keys. We
 * track this in a separate array here instead of within the respective
 * bt_mesh_app_key and bt_mesh_subnet structs themselves, since once a key
 * gets deleted its struct becomes invalid and may be reused for other keys.
 */
struct key_update {
    u16_t key_idx: 12,   /* AppKey or NetKey Index */
          valid: 1,      /* 1 if this entry is valid, 0 if not */
          app_key: 1,    /* 1 if this is an AppKey, 0 if a NetKey */
          clear: 1;      /* 1 if key needs clearing, 0 if storing */
};

/* Tracking of what storage changes are pending for node settings. */
struct node_update {
    u16_t addr;
    bool clear;
};

/* One more entry for the node's address update. */
static struct node_update cdb_node_updates[CONFIG_BT_MESH_CDB_NODE_COUNT + 1];
static struct key_update cdb_key_updates[CONFIG_BT_MESH_CDB_SUBNET_COUNT +
                                            CONFIG_BT_MESH_CDB_APP_KEY_COUNT];

struct bt_mesh_cdb bt_mesh_cdb = {
    .nodes = {
        [0 ...(CONFIG_BT_MESH_CDB_NODE_COUNT - 1)] = {
            .addr = BT_MESH_ADDR_UNASSIGNED,
        }
    },
    .subnets = {
        [0 ...(CONFIG_BT_MESH_CDB_SUBNET_COUNT - 1)] = {
            .net_idx = BT_MESH_KEY_UNUSED,
        }
    },
    .app_keys = {
        [0 ...(CONFIG_BT_MESH_CDB_APP_KEY_COUNT - 1)] = {
            .app_idx = BT_MESH_KEY_UNUSED,
            .net_idx = BT_MESH_KEY_UNUSED,
        }
    },
};

/*
 * Check if an address range from addr_start for addr_start + num_elem - 1 is
 * free for use. When a conflict is found, next will be set to the next address
 * available after the conflicting range and -EAGAIN will be returned.
 */
static int addr_is_free(u16_t addr_start, u8_t num_elem, u16_t *next)
{
    u16_t addr_end = addr_start + num_elem - 1;
    u16_t other_start, other_end;
    int i;

    if (!BT_MESH_ADDR_IS_UNICAST(addr_start) ||
        !BT_MESH_ADDR_IS_UNICAST(addr_end) ||
        num_elem == 0) {
        return -EINVAL;
    }

    for (i = 0; i < ARRAY_SIZE(bt_mesh_cdb.nodes); i++) {
        struct bt_mesh_cdb_node *node = &bt_mesh_cdb.nodes[i];

        if (node->addr == BT_MESH_ADDR_UNASSIGNED) {
            continue;
        }

        other_start = node->addr;
        other_end = other_start + node->num_elem - 1;

        if (!(addr_end < other_start || addr_start > other_end)) {
            if (next) {
                *next = other_end + 1;
            }

            return -EAGAIN;
        }
    }

    return 0;
}

/*
 * Find the lowest possible starting address that can fit num_elem elements. If
 * a free address range cannot be found, BT_MESH_ADDR_UNASSIGNED will be
 * returned. Otherwise the first address in the range is returned.
 *
 * NOTE: This is quite an ineffective algorithm as it might need to look
 *       through the array of nodes N+2 times. A more effective algorithm
 *       could be used if the nodes were stored in a sorted list.
 */
static u16_t find_lowest_free_addr(u8_t num_elem)
{
    u16_t addr = bt_mesh_cdb.lowest_avail_addr;
    u16_t next;
    int err, i;

    /*
     * It takes a maximum of node count + 2 to find a free address if there
     * is any. +1 for our own address and +1 for making sure that the
     * address range is valid.
     */
    for (i = 0; i < ARRAY_SIZE(bt_mesh_cdb.nodes) + 2; ++i) {
        err = addr_is_free(addr, num_elem, &next);
        if (err == 0) {
            break;
        } else if (err != -EAGAIN) {
            addr = BT_MESH_ADDR_UNASSIGNED;
            break;
        }

        addr = next;
    }

    return addr;
}


static void schedule_cdb_store(int flag)
{
    atomic_set_bit(bt_mesh_cdb.flags, flag);
    bt_mesh_settings_store_schedule(BT_MESH_SETTINGS_CDB_PENDING);
}

static void update_cdb_net_settings(void)
{
    schedule_cdb_store(BT_MESH_CDB_SUBNET_PENDING);
}

static struct node_update *cdb_node_update_find(u16_t addr,
        struct node_update **free_slot)
{
    struct node_update *match;
    int i;

    match = NULL;
    *free_slot = NULL;

    for (i = 0; i < ARRAY_SIZE(cdb_node_updates); i++) {
        struct node_update *update = &cdb_node_updates[i];

        if (update->addr == BT_MESH_ADDR_UNASSIGNED) {
            *free_slot = update;
            continue;
        }

        if (update->addr == addr) {
            match = update;
        }
    }

    return match;
}

static void update_cdb_node_settings(const struct bt_mesh_cdb_node *node,
                                     bool store)
{
    struct node_update *update, *free_slot;

    LOG_DBG("Node 0x%04x", node->addr);

    update = cdb_node_update_find(node->addr, &free_slot);
    if (update) {
        update->clear = !store;
        schedule_cdb_store(BT_MESH_CDB_NODES_PENDING);
        return;
    }

    if (!free_slot) {
        if (store) {
            store_cdb_node(node);
        } else {
            clear_cdb_node(node->addr);
        }
        return;
    }

    free_slot->addr = node->addr;
    free_slot->clear = !store;

    schedule_cdb_store(BT_MESH_CDB_NODES_PENDING);
}

static struct key_update *cdb_key_update_find(bool app_key, uint16_t key_idx,
        struct key_update **free_slot)
{
    struct key_update *match;
    int i;

    match = NULL;
    *free_slot = NULL;

    for (i = 0; i < ARRAY_SIZE(cdb_key_updates); i++) {
        struct key_update *update = &cdb_key_updates[i];

        if (!update->valid) {
            *free_slot = update;
            continue;
        }

        if (update->app_key != app_key) {
            continue;
        }

        if (update->key_idx == key_idx) {
            match = update;
        }
    }

    return match;
}

static void update_cdb_subnet_settings(const struct bt_mesh_cdb_subnet *sub,
                                       bool store)
{
    struct key_update *update, *free_slot;
    u8_t clear = store ? 0U : 1U;

    LOG_DBG("NetKeyIndex 0x%03x", sub->net_idx);

    update = cdb_key_update_find(false, sub->net_idx, &free_slot);
    if (update) {
        update->clear = clear;
        schedule_cdb_store(BT_MESH_CDB_KEYS_PENDING);
        return;
    }

    if (!free_slot) {
        if (store) {
            store_cdb_subnet(sub);
        } else {
            clear_cdb_subnet(sub->net_idx);
        }
        return;
    }

    free_slot->valid = 1U;
    free_slot->key_idx = sub->net_idx;
    free_slot->app_key = 0U;
    free_slot->clear = clear;

    schedule_cdb_store(BT_MESH_CDB_KEYS_PENDING);
}

static void update_cdb_app_key_settings(const struct bt_mesh_cdb_app_key *key,
                                        bool store)
{
    struct key_update *update, *free_slot;
    u8_t clear = store ? 0U : 1U;

    LOG_DBG("AppKeyIndex 0x%03x", key->app_idx);

    update = cdb_key_update_find(true, key->app_idx, &free_slot);
    if (update) {
        update->clear = clear;
        schedule_cdb_store(BT_MESH_CDB_KEYS_PENDING);
        return;
    }

    if (!free_slot) {
        if (store) {
            store_cdb_app_key(key);
        } else {
            clear_cdb_app_key(key->app_idx);
        }

        return;
    }

    free_slot->valid = 1U;
    free_slot->key_idx = key->app_idx;
    free_slot->app_key = 1U;
    free_slot->clear = clear;

    schedule_cdb_store(BT_MESH_CDB_KEYS_PENDING);
}

static u16_t addr_assign(u16_t addr, u8_t num_elem)
{
    if (addr == BT_MESH_ADDR_UNASSIGNED) {
        addr = find_lowest_free_addr(num_elem);
    } else if (addr < bt_mesh_cdb.lowest_avail_addr) {
        return BT_MESH_ADDR_UNASSIGNED;
    } else if (addr_is_free(addr, num_elem, NULL) < 0) {
        LOG_DBG("Address range 0x%04x-0x%04x is not free", addr,
                addr + num_elem - 1);
        return BT_MESH_ADDR_UNASSIGNED;
    }

    return addr;
}

int bt_mesh_cdb_create(const u8_t key[16])
{
    struct bt_mesh_cdb_subnet *sub;
    int err;

    if (atomic_test_and_set_bit(bt_mesh_cdb.flags,
                                BT_MESH_CDB_VALID)) {
        return -EALREADY;
    }

    sub = bt_mesh_cdb_subnet_alloc(BT_MESH_KEY_PRIMARY);
    if (sub == NULL) {
        return -ENOMEM;
    }

    err = bt_mesh_key_import(BT_MESH_KEY_TYPE_NET, key, &sub->keys[0].net_key);
    if (err) {
        return err;
    }

    bt_mesh_cdb.iv_index = 0;
    bt_mesh_cdb.lowest_avail_addr = 1;

    if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
        update_cdb_net_settings();
        update_cdb_subnet_settings(sub, true);
    }

    return 0;
}

void bt_mesh_cdb_clear(void)
{
    int i;

    atomic_clear_bit(bt_mesh_cdb.flags, BT_MESH_CDB_VALID);

    for (i = 0; i < ARRAY_SIZE(bt_mesh_cdb.nodes); ++i) {
        if (bt_mesh_cdb.nodes[i].addr != BT_MESH_ADDR_UNASSIGNED) {
            bt_mesh_cdb_node_del(&bt_mesh_cdb.nodes[i], true);
        }
    }

    for (i = 0; i < ARRAY_SIZE(bt_mesh_cdb.subnets); ++i) {
        if (bt_mesh_cdb.subnets[i].net_idx != BT_MESH_KEY_UNUSED) {
            bt_mesh_cdb_subnet_del(&bt_mesh_cdb.subnets[i], true);
        }
    }

    for (i = 0; i < ARRAY_SIZE(bt_mesh_cdb.app_keys); ++i) {
        if (bt_mesh_cdb.app_keys[i].net_idx != BT_MESH_KEY_UNUSED) {
            bt_mesh_cdb_app_key_del(&bt_mesh_cdb.app_keys[i], true);
        }
    }

    if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
        update_cdb_net_settings();
        bt_mesh_settings_store_pending();
    }
}

void bt_mesh_cdb_iv_update(u32_t iv_index, bool iv_update)
{
    LOG_DBG("Updating IV index to %d\n", iv_index);

    /* Reset the last deleted addr when IV Index is updated or recovered. */
    if (!iv_update || iv_index > bt_mesh_cdb.iv_index + 1) {
        bt_mesh_cdb.lowest_avail_addr = 1;
    }

    bt_mesh_cdb.iv_index = iv_index;

    atomic_set_bit_to(bt_mesh_cdb.flags, BT_MESH_CDB_IVU_IN_PROGRESS,
                      iv_update);

    if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
        update_cdb_net_settings();
    }
}

struct bt_mesh_cdb_subnet *bt_mesh_cdb_subnet_alloc(u16_t net_idx)
{
    struct bt_mesh_cdb_subnet *sub;
    int i;

    if (bt_mesh_cdb_subnet_get(net_idx) != NULL) {
        return NULL;
    }

    for (i = 0; i < ARRAY_SIZE(bt_mesh_cdb.subnets); ++i) {
        sub = &bt_mesh_cdb.subnets[i];

        if (sub->net_idx != BT_MESH_KEY_UNUSED) {
            continue;
        }

        sub->net_idx = net_idx;

        return sub;
    }

    return NULL;
}

void bt_mesh_cdb_subnet_del(struct bt_mesh_cdb_subnet *sub, bool store)
{
    LOG_DBG("NetIdx 0x%03x store %u", sub->net_idx, store);

    if (IS_ENABLED(CONFIG_BT_SETTINGS) && store) {
        bt_mesh_clear_cdb_subnet(sub);
    }

    sub->net_idx = BT_MESH_KEY_UNUSED;
    memset(sub->keys, 0, sizeof(sub->keys));
}

struct bt_mesh_cdb_subnet *bt_mesh_cdb_subnet_get(u16_t net_idx)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(bt_mesh_cdb.subnets); ++i) {
        if (bt_mesh_cdb.subnets[i].net_idx == net_idx) {
            return &bt_mesh_cdb.subnets[i];
        }
    }

    return NULL;
}

void bt_mesh_cdb_subnet_store(const struct bt_mesh_cdb_subnet *sub)
{
    if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
        update_cdb_subnet_settings(sub, true);
    }
}

u8_t bt_mesh_cdb_subnet_flags(const struct bt_mesh_cdb_subnet *sub)
{
    u8_t flags = 0x00;

    if (sub && SUBNET_KEY_TX_IDX(sub)) {
        flags |= BT_MESH_NET_FLAG_KR;
    }

    if (atomic_test_bit(bt_mesh_cdb.flags, BT_MESH_CDB_IVU_IN_PROGRESS)) {
        flags |= BT_MESH_NET_FLAG_IVU;
    }

    return flags;
}

int bt_mesh_cdb_subnet_key_import(struct bt_mesh_cdb_subnet *sub, int key_idx,
                                  const u8_t in[16])
{
    if (!bt_mesh_key_compare(in, &sub->keys[key_idx].net_key)) {
        return 0;
    }

    bt_mesh_key_destroy(&sub->keys[key_idx].net_key);

    return bt_mesh_key_import(BT_MESH_KEY_TYPE_NET, in, &sub->keys[key_idx].net_key);
}

int bt_mesh_cdb_subnet_key_export(const struct bt_mesh_cdb_subnet *sub, int key_idx,
                                  u8_t out[16])
{
    return bt_mesh_key_export(out, &sub->keys[key_idx].net_key);
}

struct bt_mesh_cdb_node *bt_mesh_cdb_node_alloc(const u8_t uuid[16], u16_t addr,
        u8_t num_elem, u16_t net_idx)
{
    int i;

    addr = addr_assign(addr, num_elem);
    if (addr == BT_MESH_ADDR_UNASSIGNED) {
        return NULL;
    }

    for (i = 0; i < ARRAY_SIZE(bt_mesh_cdb.nodes); i++) {
        struct bt_mesh_cdb_node *node = &bt_mesh_cdb.nodes[i];

        if (node->addr == BT_MESH_ADDR_UNASSIGNED) {
            memcpy(node->uuid, uuid, 16);
            node->addr = addr;
            node->num_elem = num_elem;
            node->net_idx = net_idx;
            atomic_sett(node->flags, 0);
            return node;
        }
    }

    return NULL;
}

uint16_t bt_mesh_cdb_free_addr_get(u8_t num_elem)
{
    return find_lowest_free_addr(num_elem);
}

void bt_mesh_cdb_node_del(struct bt_mesh_cdb_node *node, bool store)
{
    LOG_DBG("Node addr 0x%04x store %u", node->addr, store);

    if (IS_ENABLED(CONFIG_BT_SETTINGS) && store) {
        update_cdb_node_settings(node, false);
    }

    if (store && node->addr + node->num_elem > bt_mesh_cdb.lowest_avail_addr) {
        bt_mesh_cdb.lowest_avail_addr = node->addr + node->num_elem;

        if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
            update_cdb_net_settings();
        }
    }

    node->addr = BT_MESH_ADDR_UNASSIGNED;
    bt_mesh_key_destroy(&node->dev_key);
    memset(&node->dev_key, 0, sizeof(node->dev_key));
}

void bt_mesh_cdb_node_update(struct bt_mesh_cdb_node *node, u16_t addr,
                             u8_t num_elem)
{
    /* Address is used as a key to the nodes array. Remove the current entry first, then store
     * new address.
     */
    if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
        update_cdb_node_settings(node, false);
    }

    node->addr = addr;
    node->num_elem = num_elem;

    if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
        update_cdb_node_settings(node, true);
    }
}

struct bt_mesh_cdb_node *bt_mesh_cdb_node_get(u16_t addr)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(bt_mesh_cdb.nodes); i++) {
        struct bt_mesh_cdb_node *node = &bt_mesh_cdb.nodes[i];

        if (addr >= node->addr &&
            addr <= node->addr + node->num_elem - 1) {
            return node;
        }
    }

    return NULL;
}

void bt_mesh_cdb_node_store(const struct bt_mesh_cdb_node *node)
{
    if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
        update_cdb_node_settings(node, true);
    }
}

void bt_mesh_cdb_node_foreach(bt_mesh_cdb_node_func_t func, void *user_data)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(bt_mesh_cdb.nodes); ++i) {
        if (bt_mesh_cdb.nodes[i].addr == BT_MESH_ADDR_UNASSIGNED) {
            continue;
        }

        if (func(&bt_mesh_cdb.nodes[i], user_data) ==
            BT_MESH_CDB_ITER_STOP) {
            break;
        }
    }
}

int bt_mesh_cdb_node_key_import(struct bt_mesh_cdb_node *node, const u8_t in[16])
{
    if (!bt_mesh_key_compare(in, &node->dev_key)) {
        return 0;
    }

    bt_mesh_key_destroy(&node->dev_key);

    return bt_mesh_key_import(BT_MESH_KEY_TYPE_DEV, in, &node->dev_key);
}

int bt_mesh_cdb_node_key_export(const struct bt_mesh_cdb_node *node, u8_t out[16])
{
    return bt_mesh_key_export(out, &node->dev_key);
}

struct bt_mesh_cdb_app_key *bt_mesh_cdb_app_key_alloc(u16_t net_idx, u16_t app_idx)
{
    struct bt_mesh_cdb_app_key *key;
    struct bt_mesh_cdb_app_key *vacant_key = NULL;
    int i;

    for (i = 0; i < ARRAY_SIZE(bt_mesh_cdb.app_keys); ++i) {
        key = &bt_mesh_cdb.app_keys[i];

        if (key->app_idx == app_idx) {
            return NULL;
        }

        if (key->net_idx != BT_MESH_KEY_UNUSED || vacant_key) {
            continue;
        }

        vacant_key = key;
    }

    if (vacant_key) {
        vacant_key->net_idx = net_idx;
        vacant_key->app_idx = app_idx;
    }

    return vacant_key;
}

void bt_mesh_cdb_app_key_del(struct bt_mesh_cdb_app_key *key, bool store)
{
    LOG_DBG("AppIdx 0x%03x store %u", key->app_idx, store);

    if (IS_ENABLED(CONFIG_BT_SETTINGS) && store) {
        update_cdb_app_key_settings(key, false);
    }

    key->net_idx = BT_MESH_KEY_UNUSED;
    key->app_idx = BT_MESH_KEY_UNUSED;
    bt_mesh_key_destroy(&key->keys[0].app_key);
    bt_mesh_key_destroy(&key->keys[1].app_key);
    memset(key->keys, 0, sizeof(key->keys));
}

struct bt_mesh_cdb_app_key *bt_mesh_cdb_app_key_get(u16_t app_idx)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(bt_mesh_cdb.app_keys); i++) {
        struct bt_mesh_cdb_app_key *key = &bt_mesh_cdb.app_keys[i];

        if (key->net_idx != BT_MESH_KEY_UNUSED &&
            key->app_idx == app_idx) {
            return key;
        }
    }

    return NULL;
}

void bt_mesh_cdb_app_key_store(const struct bt_mesh_cdb_app_key *key)
{
    if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
        update_cdb_app_key_settings(key, true);
    }
}

int bt_mesh_cdb_app_key_import(struct bt_mesh_cdb_app_key *key, int key_idx, const u8_t in[16])
{
    if (!bt_mesh_key_compare(in, &key->keys[key_idx].app_key)) {
        return 0;
    }

    bt_mesh_key_destroy(&key->keys[key_idx].app_key);

    return bt_mesh_key_import(BT_MESH_KEY_TYPE_APP, in, &key->keys[key_idx].app_key);
}

int bt_mesh_cdb_app_key_export(const struct bt_mesh_cdb_app_key *key, int key_idx, u8_t out[16])
{
    return bt_mesh_key_export(out, &key->keys[key_idx].app_key);
}

static void store_cdb_pending_nodes(void)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(cdb_node_updates); ++i) {
        struct node_update *update = &cdb_node_updates[i];
        uint16_t addr;

        if (update->addr == BT_MESH_ADDR_UNASSIGNED) {
            continue;
        }

        addr = update->addr;
        update->addr = BT_MESH_ADDR_UNASSIGNED;

        LOG_DBG("addr: 0x%04x, clear: %d", addr, update->clear);

        if (update->clear) {
            clear_cdb_node(addr);
        } else {
            struct bt_mesh_cdb_node *node;

            node = bt_mesh_cdb_node_get(addr);
            if (node) {
                store_cdb_node(node);
            } else {
                LOG_WRN("Node 0x%04x not found", addr);
            }
        }
    }
}

static void store_cdb_pending_keys(void)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(cdb_key_updates); i++) {
        struct key_update *update = &cdb_key_updates[i];

        if (!update->valid) {
            continue;
        }

        update->valid = 0U;

        if (update->clear) {
            if (update->app_key) {
                clear_cdb_app_key(update->key_idx);
            } else {
                clear_cdb_subnet(update->key_idx);
            }
        } else {
            if (update->app_key) {
                struct bt_mesh_cdb_app_key *key;

                key = bt_mesh_cdb_app_key_get(update->key_idx);
                if (key) {
                    store_cdb_app_key(key);
                } else {
                    LOG_WRN("AppKeyIndex 0x%03x not found", update->key_idx);
                }
            } else {
                struct bt_mesh_cdb_subnet *sub;

                sub = bt_mesh_cdb_subnet_get(update->key_idx);
                if (sub) {
                    store_cdb_subnet(sub);
                } else {
                    LOG_WRN("NetKeyIndex 0x%03x not found", update->key_idx);
                }
            }
        }
    }
}

void bt_mesh_cdb_pending_store(void)
{
    if (atomic_test_and_clear_bit(bt_mesh_cdb.flags,
                                  BT_MESH_CDB_SUBNET_PENDING)) {
        if (atomic_test_bit(bt_mesh_cdb.flags,
                            BT_MESH_CDB_VALID)) {
            store_cdb_pending_net();
        } else {
            clear_cdb_net();
        }
    }

    if (atomic_test_and_clear_bit(bt_mesh_cdb.flags,
                                  BT_MESH_CDB_NODES_PENDING)) {
        store_cdb_pending_nodes();
    }

    if (atomic_test_and_clear_bit(bt_mesh_cdb.flags,
                                  BT_MESH_CDB_KEYS_PENDING)) {
        store_cdb_pending_keys();
    }
}
#endif
