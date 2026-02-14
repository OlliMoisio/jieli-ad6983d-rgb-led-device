/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "adaptation.h"
#include "msg.h"

#define LOG_TAG             "[MESH-msg]"
#define LOG_INFO_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_WARN_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_DUMP_ENABLE
#include "mesh_log.h"

#if MESH_RAM_AND_CODE_MAP_DETAIL
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ble_mesh_msg_bss")
#pragma data_seg(".ble_mesh_msg_data")
#pragma const_seg(".ble_mesh_msg_const")
#pragma code_seg(".ble_mesh_msg_code")
#endif
#else /* MESH_RAM_AND_CODE_MAP_DETAIL */
#pragma bss_seg(".ble_mesh_bss")
#pragma data_seg(".ble_mesh_data")
#pragma const_seg(".ble_mesh_const")
#pragma code_seg(".ble_mesh_code")
#endif /* MESH_RAM_AND_CODE_MAP_DETAIL */

void bt_mesh_model_msg_init(struct net_buf_simple *msg, u32_t opcode)
{
    net_buf_simple_init(msg, 0);

    switch (BT_MESH_MODEL_OP_LEN(opcode)) {
    case 1:
        net_buf_simple_add_u8(msg, opcode);
        break;
    case 2:
        net_buf_simple_add_be16(msg, opcode);
        break;
    case 3:
        net_buf_simple_add_u8(msg, ((opcode >> 16) & 0xff));
        /* Using LE for the CID since the model layer is defined as
         * little-endian in the mesh spec and using BT_MESH_MODEL_OP_3
         * will declare the opcode in this way.
         */
        net_buf_simple_add_le16(msg, opcode & 0xffff);
        break;
    default:
        LOG_WRN("Unknown opcode format");
        break;
    }
}

void bt_mesh_msg_ack_ctx_clear(struct bt_mesh_msg_ack_ctx *ack)
{
    ack->op = 0U;
    ack->user_data = NULL;
    ack->dst = BT_MESH_ADDR_UNASSIGNED;
}

int bt_mesh_msg_ack_ctx_prepare(struct bt_mesh_msg_ack_ctx *ack,
                                u32_t op, u16_t dst, void *user_data)
{
    if (ack->op) {
        LOG_WRN("Another synchronous operation pending");
        return -EBUSY;
    }

    ack->op = op;
    ack->user_data = user_data;
    ack->dst = dst;

    return 0;
}

int bt_mesh_msg_ack_ctx_wait(struct bt_mesh_msg_ack_ctx *ack, k_timeout_t timeout)
{
    int err;

    err = os_sem_pend(&ack->sem, timeout);
    bt_mesh_msg_ack_ctx_clear(ack);

    if (err == -EAGAIN) {
        return -ETIMEDOUT;
    }

    return err;
}

bool bt_mesh_msg_ack_ctx_match(const struct bt_mesh_msg_ack_ctx *ack,
                               u32_t op, u16_t addr, void **user_data)
{
    if (ack->op != op || (BT_MESH_ADDR_IS_UNICAST(ack->dst) && ack->dst != addr)) {
        return false;
    }

    if (user_data != NULL) {
        *user_data = ack->user_data;
    }

    return true;
}

int bt_mesh_msg_send(const struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
                     struct net_buf_simple *buf)
{
    if (!ctx && !model->pub) {
        return -ENOTSUP;
    }

    if (ctx) {
        return bt_mesh_model_send(model, ctx, buf, NULL, NULL);
    }

    net_buf_simple_reset(model->pub->msg);
    net_buf_simple_add_mem(model->pub->msg, buf->data, buf->len);

    return bt_mesh_model_publish(model);
}

int bt_mesh_msg_ackd_send(const struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
                          struct net_buf_simple *buf, const struct bt_mesh_msg_rsp_ctx *rsp)
{
    int err;

    if (rsp) {
        err = bt_mesh_msg_ack_ctx_prepare(rsp->ack, rsp->op,
                                          ctx ? ctx->addr : model->pub->addr,
                                          rsp->user_data);
        if (err) {
            return err;
        }
    }

    err = bt_mesh_msg_send(model, ctx, buf);

    if (!rsp) {
        return err;
    }

    if (!err) {
        return bt_mesh_msg_ack_ctx_wait(rsp->ack, K_MSEC(rsp->timeout));
    }

    bt_mesh_msg_ack_ctx_clear(rsp->ack);

    return err;
}
