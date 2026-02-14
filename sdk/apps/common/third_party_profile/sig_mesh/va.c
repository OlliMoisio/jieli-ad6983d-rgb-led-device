/*
 * Copyright (c) 2017 Intel Corporation
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "adaptation.h"
#include "va.h"
#include "foundation.h"
#include "msg.h"
#include "net.h"
#include "crypto.h"
#include "settings.h"

#define LOG_TAG             "[MESH-va]"
#define LOG_INFO_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_WARN_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_DUMP_ENABLE
#include "mesh_log.h"

#if MESH_RAM_AND_CODE_MAP_DETAIL
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ble_mesh_va_bss")
#pragma data_seg(".ble_mesh_va_data")
#pragma const_seg(".ble_mesh_va_const")
#pragma code_seg(".ble_mesh_va_code")
#endif
#else /* MESH_RAM_AND_CODE_MAP_DETAIL */
#pragma bss_seg(".ble_mesh_bss")
#pragma data_seg(".ble_mesh_data")
#pragma const_seg(".ble_mesh_const")
#pragma code_seg(".ble_mesh_code")
#endif /* MESH_RAM_AND_CODE_MAP_DETAIL */

static struct bt_mesh_va virtual_addrs[CONFIG_BT_MESH_LABEL_COUNT];


static void va_store(struct bt_mesh_va *store)
{
    store->changed = 1U;

    if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
        bt_mesh_settings_store_schedule(BT_MESH_SETTINGS_VA_PENDING);
    }
}

u8_t bt_mesh_va_add(const u8_t uuid[16], const struct bt_mesh_va **entry)
{
    struct bt_mesh_va *va = NULL;
    int err;

    for (int i = 0; i < ARRAY_SIZE(virtual_addrs); i++) {
        if (!virtual_addrs[i].ref) {
            if (!va) {
                va = &virtual_addrs[i];
            }

            continue;
        }

        if (!memcmp(uuid, virtual_addrs[i].uuid,
                    ARRAY_SIZE(virtual_addrs[i].uuid))) {
            if (entry) {
                *entry = &virtual_addrs[i];
            }
            virtual_addrs[i].ref++;
            va_store(&virtual_addrs[i]);
            return STATUS_SUCCESS;
        }
    }

    if (!va) {
        return STATUS_INSUFF_RESOURCES;
    }

    memcpy(va->uuid, uuid, ARRAY_SIZE(va->uuid));
    err = bt_mesh_virtual_addr(uuid, &va->addr);
    if (err) {
        va->addr = BT_MESH_ADDR_UNASSIGNED;
        return STATUS_UNSPECIFIED;
    }

    va->ref = 1;
    va_store(va);

    if (entry) {
        *entry = va;
    }

    return STATUS_SUCCESS;
}

u8_t bt_mesh_va_del(const u8_t *uuid)
{
    struct bt_mesh_va *va;

    if (CONFIG_BT_MESH_LABEL_COUNT == 0) {
        return STATUS_CANNOT_REMOVE;
    }

    va = CONTAINER_OF(uuid, struct bt_mesh_va, uuid[0]);

    if (!PART_OF_ARRAY(virtual_addrs, va) || va->ref == 0) {
        return STATUS_CANNOT_REMOVE;
    }

    va->ref--;
    va_store(va);

    return STATUS_SUCCESS;
}

const u8_t *bt_mesh_va_uuid_get(u16_t addr, const u8_t *uuid, u16_t *retaddr)
{
    int i = 0;

    if (CONFIG_BT_MESH_LABEL_COUNT == 0) {
        return NULL;
    }

    if (uuid != NULL) {
        struct bt_mesh_va *va;

        va = CONTAINER_OF(uuid, struct bt_mesh_va, uuid[0]);
        i = ARRAY_INDEX(virtual_addrs, va);
    }

    for (; i < ARRAY_SIZE(virtual_addrs); i++) {
        if (virtual_addrs[i].ref &&
            (virtual_addrs[i].addr == addr || addr == BT_MESH_ADDR_UNASSIGNED)) {
            if (!uuid) {
                LOG_DBG("Found Label UUID for 0x%04x: %s", addr,
                        bt_hex(virtual_addrs[i].uuid, 16));

                if (retaddr) {
                    *retaddr = virtual_addrs[i].addr;
                }

                return virtual_addrs[i].uuid;
            } else if (uuid == virtual_addrs[i].uuid) {
                uuid = NULL;
            }
        }
    }

    LOG_WRN("No matching Label UUID for 0x%04x", addr);

    return NULL;
}

bool bt_mesh_va_collision_check(u16_t addr)
{
    size_t count = 0;
    const u8_t *uuid = NULL;

    do {
        uuid = bt_mesh_va_uuid_get(addr, uuid, NULL);
    } while (uuid && ++count);

    return count > 1;
}

const struct bt_mesh_va *bt_mesh_va_find(const u8_t *uuid)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(virtual_addrs); i++) {
        if (virtual_addrs[i].ref && !memcmp(virtual_addrs[i].uuid, uuid, 16)) {
            return &virtual_addrs[i];
        }
    }

    return NULL;
}

struct bt_mesh_va *va_get_by_idx(u16_t index)
{
    if (index >= ARRAY_SIZE(virtual_addrs)) {
        return NULL;
    }

    return &virtual_addrs[index];
}

const u8_t *bt_mesh_va_get_uuid_by_idx(u16_t idx)
{
    struct bt_mesh_va *va;

    va = va_get_by_idx(idx);
    return (va && va->ref > 0) ? va->uuid : NULL;
}

int bt_mesh_va_get_idx_by_uuid(const u8_t *uuid, u16_t *uuidx)
{
    struct bt_mesh_va *va;

    if (CONFIG_BT_MESH_LABEL_COUNT == 0) {
        return -ENOENT;
    }

    va = CONTAINER_OF(uuid, struct bt_mesh_va, uuid[0]);

    if (!PART_OF_ARRAY(virtual_addrs, va) || va->ref == 0) {
        return -ENOENT;
    }

    *uuidx = ARRAY_INDEX(virtual_addrs, va);
    return 0;
}

void bt_mesh_va_clear(void)
{
    int i;

    if (CONFIG_BT_MESH_LABEL_COUNT == 0) {
        return;
    }

    for (i = 0; i < ARRAY_SIZE(virtual_addrs); i++) {
        if (virtual_addrs[i].ref) {
            virtual_addrs[i].ref = 0U;
            virtual_addrs[i].changed = 1U;
        }
    }

    if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
        bt_mesh_settings_store_schedule(BT_MESH_SETTINGS_VA_PENDING);
    }
}
