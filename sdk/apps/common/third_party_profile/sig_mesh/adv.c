/*  Bluetooth Mesh */

/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "adaptation.h"
#include "net/buf.h"
#include "adv.h"
#include "net.h"
#include "foundation.h"
#include "beacon.h"
#include "prov.h"
#include "proxy.h"
#include "pb_gatt_srv.h"
#include "solicitation.h"
#include "statistic.h"

#define LOG_TAG             "[MESH-adv]"
/* #define LOG_INFO_ENABLE */
#define LOG_DEBUG_ENABLE
#define LOG_WARN_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_DUMP_ENABLE
#include "mesh_log.h"

#if MESH_RAM_AND_CODE_MAP_DETAIL
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ble_mesh_adv_bss")
#pragma data_seg(".ble_mesh_adv_data")
#pragma const_seg(".ble_mesh_adv_const")
#pragma code_seg(".ble_mesh_adv_code")
#endif
#else /* MESH_RAM_AND_CODE_MAP_DETAIL */
#pragma bss_seg(".ble_mesh_bss")
#pragma data_seg(".ble_mesh_data")
#pragma const_seg(".ble_mesh_const")
#pragma code_seg(".ble_mesh_code")
#endif /* MESH_RAM_AND_CODE_MAP_DETAIL */

const uint8_t bt_mesh_adv_type[BT_MESH_ADV_TYPES] = {
    [BT_MESH_ADV_PROV]   = BT_DATA_MESH_PROV,
    [BT_MESH_ADV_DATA]   = BT_DATA_MESH_MESSAGE,
    [BT_MESH_ADV_BEACON] = BT_DATA_MESH_BEACON,
    [BT_MESH_ADV_URI]    = BT_DATA_URI,
};

static bool active_scanning;
// static K_FIFO_DEFINE(bt_mesh_adv_queue); 	//for compiler, not used now.
// static K_FIFO_DEFINE(bt_mesh_relay_queue);	//for compiler, not used now.
// static K_FIFO_DEFINE(bt_mesh_friend_queue);	//for compiler, not used now.

// K_MEM_SLAB_DEFINE_STATIC(local_adv_pool, sizeof(struct bt_mesh_adv),
// 			 CONFIG_BT_MESH_ADV_BUF_COUNT, __alignof__(struct bt_mesh_adv));	//for compiler, not used now.

#if defined(CONFIG_BT_MESH_RELAY_BUF_COUNT)
K_MEM_SLAB_DEFINE_STATIC(relay_adv_pool, sizeof(struct bt_mesh_adv),
                         CONFIG_BT_MESH_RELAY_BUF_COUNT, __alignof__(struct bt_mesh_adv));
#endif

#if (CONFIG_BT_MESH_ADV_EXT_FRIEND_SEPARATE)
K_MEM_SLAB_DEFINE_STATIC(friend_adv_pool, sizeof(struct bt_mesh_adv),
                         CONFIG_BT_MESH_FRIEND_LPN_COUNT, __alignof__(struct bt_mesh_adv));
#endif

//for compiler, not used now.
#if (CONFIG_BT_MESH_RELAY)
struct k_mem_slab *relay_adv_pool;
#endif
struct k_mem_slab *local_adv_pool;

#if NET_BUF_USE_MALLOC
NET_BUF_POOL_DEFINE(adv_buf_pool, CONFIG_BT_MESH_ADV_BUF_COUNT,
                    BT_MESH_ADV_DATA_SIZE, BT_MESH_ADV_USER_DATA_SIZE, NULL);
static struct bt_mesh_adv *adv_pool;
#else
static struct bt_mesh_adv adv_pool[CONFIG_BT_MESH_ADV_BUF_COUNT];
#endif /* NET_BUF_USE_MALLOC */
void bt_mesh_adv_send_start(u16_t duration, int err, struct bt_mesh_adv_ctx *ctx)
{
    if (!ctx->started) {
        ctx->started = 1;

        if (ctx->cb && ctx->cb->start) {
            ctx->cb->start(duration, err, ctx->cb_data);
        }

        if (err) {
            ctx->cb = NULL;
        } else if (IS_ENABLED(CONFIG_BT_MESH_STATISTIC)) {
            bt_mesh_stat_succeeded_count(ctx);
        }
    }
}

void bt_mesh_adv_send_end(int err, struct bt_mesh_adv_ctx const *ctx)
{
    if (ctx->started && ctx->cb && ctx->cb->end) {
        ctx->cb->end(err, ctx->cb_data);
    }
}

static struct bt_mesh_adv *adv_create_from_pool(struct k_mem_slab *buf_pool,
        enum bt_mesh_adv_type type,
        enum bt_mesh_adv_tag tag,
        uint8_t xmit, k_timeout_t timeout)
{
    struct bt_mesh_adv_ctx *ctx;
    struct bt_mesh_adv *adv;
    int err;

    if (atomic_test_bit(bt_mesh.flags, BT_MESH_SUSPENDED)) {
        LOG_WRN("Refusing to allocate buffer while suspended");
        return NULL;
    }

    // err = k_mem_slab_alloc(buf_pool, (void **)&adv, timeout);
    adv = malloc(sizeof(struct bt_mesh_adv));

    if (!adv) {
        return NULL;
    }

    adv->__ref = 1;

    net_buf_simple_init_with_data(&adv->b, adv->__bufs, BT_MESH_ADV_DATA_SIZE);
    net_buf_simple_reset(&adv->b);

    ctx = &adv->ctx;

    (void)memset(ctx, 0, sizeof(*ctx));

    ctx->type         = type;
    ctx->tag          = tag;
    ctx->xmit         = xmit;

    return adv;
}

struct bt_mesh_adv *bt_mesh_adv_ref(struct bt_mesh_adv *adv)
{
    __ASSERT_NO_MSG(adv->__ref < UINT8_MAX);

    adv->__ref++;

    return adv;
}

void bt_mesh_adv_unref(struct bt_mesh_adv *adv)
{
    __ASSERT_NO_MSG(adv->__ref > 0);

    if (--adv->__ref > 0) {
        return;
    }

    // struct k_mem_slab *slab = &local_adv_pool;	//for compiler, not used now.

#if (CONFIG_BT_MESH_RELAY)
    if (adv->ctx.tag == BT_MESH_ADV_TAG_RELAY) {
        // slab = &relay_adv_pool;	//for compiler, not used now.
    }
#endif

#if (CONFIG_BT_MESH_ADV_EXT_FRIEND_SEPARATE)
    if (adv->ctx.tag == BT_MESH_ADV_TAG_FRIEND) {
        slab = &friend_adv_pool;
    }
#endif

    // k_mem_slab_free(slab, (void *)adv);	//for compiler, not used now.
    free((void *)adv);
}

struct bt_mesh_adv *bt_mesh_adv_create(enum bt_mesh_adv_type type,
                                       enum bt_mesh_adv_tag tag,
                                       uint8_t xmit, k_timeout_t timeout)
{
#if (CONFIG_BT_MESH_RELAY)
    if (tag == BT_MESH_ADV_TAG_RELAY) {
        return adv_create_from_pool(&relay_adv_pool,
                                    type, tag, xmit, timeout);	//for compiler, not used now.
    }
#endif

#if (CONFIG_BT_MESH_ADV_EXT_FRIEND_SEPARATE)
    if (tag == BT_MESH_ADV_TAG_FRIEND) {
        return adv_create_from_pool(&friend_adv_pool,
                                    type, tag, xmit, timeout);
    }
#endif

    return adv_create_from_pool(&local_adv_pool, type,
                                tag, xmit, timeout);	//for compiler, not used now.
}

static void bt_mesh_scan_cb(const bt_addr_le_t *addr, s8_t rssi,
                            u8_t adv_type, struct net_buf_simple *buf)
{
    if (adv_type != BT_LE_ADV_NONCONN_IND) {
        return;
    }

    // LOG_DBG("len %u: %s", buf->len, bt_hex(buf->data, buf->len));

    while (buf->len > 1) {
        struct net_buf_simple_state state;
        u8_t len, type;

        len = net_buf_simple_pull_u8(buf);
        /* Check for early termination */
        if (len == 0) {
            return;
        }

        if (len > buf->len) {
            LOG_WRN("AD malformed");
            return;
        }

        net_buf_simple_save(buf, &state);

        type = net_buf_simple_pull_u8(buf);

        buf->len = len - 1;

        switch (type) {
        case BT_DATA_MESH_MESSAGE:
            LOG_INF("\n< ADV-BT_DATA_MESH_MESSAGE >\n");
            bt_mesh_net_recv(buf, rssi, BT_MESH_NET_IF_ADV);
            break;
#if (CONFIG_BT_MESH_PB_ADV)
        case BT_DATA_MESH_PROV:
            LOG_INF("\n< ADV-BT_DATA_MESH_PROV >\n");
            bt_mesh_pb_adv_recv(buf);
            break;
#endif
        case BT_DATA_MESH_BEACON:
            LOG_INF("\n< ADV-BT_DATA_MESH_BEACON>\n");
            bt_mesh_beacon_recv(buf);
            break;
        case BT_DATA_UUID16_SOME:
        /* Fall through */
        case BT_DATA_UUID16_ALL:
            if (IS_ENABLED(CONFIG_BT_MESH_OD_PRIV_PROXY_SRV)) {
                /* Restore buffer with Solicitation PDU */
                net_buf_simple_restore(buf, &state);
                bt_mesh_sol_recv(buf, len - 1);
            }
            break;
        default:
            break;
        }

        net_buf_simple_restore(buf, &state);
        net_buf_simple_pull(buf, len);
    }
}

int bt_mesh_scan_active_set(bool active)
{
    if (active_scanning == active) {
        return 0;
    }

    active_scanning = active;
    bt_mesh_scan_disable();
    return bt_mesh_scan_enable();
}

int bt_mesh_scan_enable(void)
{
    LOG_DBG("");

    return bt_le_scan_start(bt_mesh_scan_cb);
}

int bt_mesh_scan_disable(void)
{
    LOG_DBG("");

    return bt_le_scan_stop();
}

#if NET_BUF_USE_MALLOC
#include "system/malloc.h"

void bt_mesh_adv_buf_alloc(void)
{
    LOG_DBG("--func=%s, adv buffer cnt = %d", __FUNCTION__, config_bt_mesh_adv_buf_count);

    u32 buf_size;
    u32 net_buf_p, net_buf_data_p, adv_pool_p;

    buf_size = sizeof(struct net_buf) * config_bt_mesh_adv_buf_count;
    LOG_DBG("net_buf size=0x%x", buf_size);
    net_buf_data_p = buf_size;
    buf_size += ALIGN_4BYTE(config_bt_mesh_adv_buf_count * BT_MESH_ADV_DATA_SIZE);
    LOG_DBG("net_buf_data size=0x%x", ALIGN_4BYTE(config_bt_mesh_adv_buf_count * BT_MESH_ADV_DATA_SIZE));
    adv_pool_p = buf_size;
    buf_size += (sizeof(struct bt_mesh_adv) * config_bt_mesh_adv_buf_count);
    LOG_DBG("adv_pool size=0x%x", sizeof(struct bt_mesh_adv) * config_bt_mesh_adv_buf_count);

    net_buf_p = (u32)malloc(buf_size);
    ASSERT(net_buf_p);
    memset((u8 *)net_buf_p, 0, buf_size);

    net_buf_data_p += net_buf_p;
    adv_pool_p += net_buf_p;

    NET_BUF_MALLOC(adv_buf_pool,
                   net_buf_p, net_buf_data_p, (struct bt_mesh_adv *)adv_pool_p,
                   config_bt_mesh_adv_buf_count);

    LOG_DBG("total buf_size=0x%x", buf_size);
    LOG_DBG("net_buf addr=0x%x", net_buf_p);
    LOG_DBG("net_buf_data addr=0x%x", net_buf_data_p);
    LOG_DBG("adv_pool addr=0x%x", adv_pool_p);
    // LOG_DBG("*fixed->data_pool=0x%x", *((u32 *)net_buf_fixed_adv_buf_pool.data_pool));
    // LOG_DBG("net_buf_adv_buf_pool user_data addr=0x%x\r\n", net_buf_adv_buf_pool->user_data);
}

void bt_mesh_adv_buf_free(void)
{
    free(NET_BUF_FREE(adv_buf_pool));
}
#endif /* NET_BUF_USE_MALLOC */
