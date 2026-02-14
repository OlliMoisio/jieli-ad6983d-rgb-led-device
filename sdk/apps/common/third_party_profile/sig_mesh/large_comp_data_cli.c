/*  Bluetooth Mesh */

/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "adaptation.h"
#include "msg.h"
#include "net.h"
#include "access.h"
#include "foundation.h"

#define LOG_TAG             "[MESH-largecompdatacli]"
/* #define LOG_INFO_ENABLE */
/* #define LOG_DEBUG_ENABLE */
#define LOG_WARN_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_DUMP_ENABLE
#include "mesh_log.h"

#if MESH_RAM_AND_CODE_MAP_DETAIL
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ble_mesh_largecompdatacli_bss")
#pragma data_seg(".ble_mesh_largecompdatacli_data")
#pragma const_seg(".ble_mesh_largecompdatacli_const")
#pragma code_seg(".ble_mesh_largecompdatacli_code")
#endif
#else /* MESH_RAM_AND_CODE_MAP_DETAIL */
#pragma bss_seg(".ble_mesh_bss")
#pragma data_seg(".ble_mesh_data")
#pragma const_seg(".ble_mesh_const")
#pragma code_seg(".ble_mesh_code")
#endif /* MESH_RAM_AND_CODE_MAP_DETAIL */

static struct bt_mesh_large_comp_data_cli *cli;
static s32_t msg_timeout;

static int data_status(const struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
                       struct net_buf_simple *buf, u32_t op,
                       void (*cb)(struct bt_mesh_large_comp_data_cli *cli, u16_t addr,
                                  struct bt_mesh_large_comp_data_rsp *rsp))
{
    struct bt_mesh_large_comp_data_rsp *rsp;
    u8_t page;
    u16_t offset;
    u16_t total_size;

    LOG_DBG("net_idx 0x%04x app_idx 0x%04x src 0x%04x len %u: %s",
            ctx->net_idx, ctx->app_idx, ctx->addr, buf->len,
            bt_hex(buf->data, buf->len));

    page = net_buf_simple_pull_u8(buf);
    offset = net_buf_simple_pull_le16(buf);
    total_size = net_buf_simple_pull_le16(buf);

    if (bt_mesh_msg_ack_ctx_match(&cli->ack_ctx, op, ctx->addr, (void **)&rsp)) {
        rsp->page = page;
        rsp->offset = offset;
        rsp->total_size = total_size;

        if (rsp->data) {
            net_buf_simple_add_mem(rsp->data, buf->data,
                                   MIN(net_buf_simple_tailroom(rsp->data), buf->len));
        }

        bt_mesh_msg_ack_ctx_rx(&cli->ack_ctx);
    }

    if (cb) {
        struct bt_mesh_large_comp_data_rsp status_rsp = {
            .page = page,
            .offset = offset,
            .total_size = total_size,
            .data = buf,
        };

        cb(cli, ctx->addr, &status_rsp);
    }

    return 0;
}

static int large_comp_data_status(const struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
                                  struct net_buf_simple *buf)
{
    return data_status(model, ctx, buf, OP_LARGE_COMP_DATA_STATUS,
                       (cli->cb && cli->cb->large_comp_data_status ?
                        cli->cb->large_comp_data_status : NULL));
}

static int models_metadata_status(const struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
                                  struct net_buf_simple *buf)
{
    return data_status(model, ctx, buf, OP_MODELS_METADATA_STATUS,
                       (cli->cb && cli->cb->models_metadata_status ?
                        cli->cb->models_metadata_status : NULL));
}

const struct bt_mesh_model_op _bt_mesh_large_comp_data_cli_op[] = {
    { OP_LARGE_COMP_DATA_STATUS,   BT_MESH_LEN_MIN(5),  large_comp_data_status },
    { OP_MODELS_METADATA_STATUS,   BT_MESH_LEN_MIN(5),  models_metadata_status },
    BT_MESH_MODEL_OP_END,
};

static int large_comp_data_cli_init(const struct bt_mesh_model *model)
{
    if (!bt_mesh_model_in_primary(model)) {
        LOG_ERR("Large Comp Data Client only allowed in primary element");
        return -EINVAL;
    }

    model->keys[0] = BT_MESH_KEY_DEV_ANY;
    model->rt->flags |= BT_MESH_MOD_DEVKEY_ONLY;

    cli = model->rt->user_data;
    cli->model = model;

    msg_timeout = 5000;
    bt_mesh_msg_ack_ctx_init(&cli->ack_ctx);

    return 0;
}

const struct bt_mesh_model_cb _bt_mesh_large_comp_data_cli_cb = {
    .init = large_comp_data_cli_init,
};

static int data_get(u16_t net_idx, u16_t addr, u32_t op, u32_t status_op, u8_t page,
                    size_t offset, struct bt_mesh_large_comp_data_rsp *rsp)
{
    BT_MESH_MODEL_BUF_DEFINE(msg, op, 3);
    struct bt_mesh_msg_ctx ctx = BT_MESH_MSG_CTX_INIT_DEV(net_idx, addr);
    const struct bt_mesh_msg_rsp_ctx rsp_ctx = {
        .ack = &cli->ack_ctx,
        .op = status_op,
        .user_data = rsp,
        .timeout = msg_timeout,
    };

    bt_mesh_model_msg_init(&msg, op);
    net_buf_simple_add_u8(&msg, page);
    net_buf_simple_add_le16(&msg, offset);

    return bt_mesh_msg_ackd_send(cli->model, &ctx, &msg, rsp ? &rsp_ctx : NULL);
}

int bt_mesh_large_comp_data_get(u16_t net_idx, u16_t addr, u8_t page,
                                size_t offset, struct bt_mesh_large_comp_data_rsp *rsp)
{
    return data_get(net_idx, addr, OP_LARGE_COMP_DATA_GET, OP_LARGE_COMP_DATA_STATUS,
                    page, offset, rsp);
}

int bt_mesh_models_metadata_get(u16_t net_idx, u16_t addr, u8_t page,
                                size_t offset, struct bt_mesh_large_comp_data_rsp *rsp)
{
    return data_get(net_idx, addr, OP_MODELS_METADATA_GET, OP_MODELS_METADATA_STATUS,
                    page, offset, rsp);
}
