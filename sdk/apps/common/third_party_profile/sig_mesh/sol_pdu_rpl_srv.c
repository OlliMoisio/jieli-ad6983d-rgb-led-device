/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "adaptation.h"
#include "foundation.h"
#include "subnet.h"
#include "solicitation.h"

#define LOG_TAG             "[MESH-solpdurplsrv]"
/* #define LOG_INFO_ENABLE */
/* #define LOG_DEBUG_ENABLE */
#define LOG_WARN_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_DUMP_ENABLE
#include "mesh_log.h"

#if MESH_RAM_AND_CODE_MAP_DETAIL
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ble_mesh_solpdurplsrv_bss")
#pragma data_seg(".ble_mesh_solpdurplsrv_data")
#pragma const_seg(".ble_mesh_solpdurplsrv_const")
#pragma code_seg(".ble_mesh_solpdurplsrv_code")
#endif
#else /* MESH_RAM_AND_CODE_MAP_DETAIL */
#pragma bss_seg(".ble_mesh_bss")
#pragma data_seg(".ble_mesh_data")
#pragma const_seg(".ble_mesh_const")
#pragma code_seg(".ble_mesh_code")
#endif /* MESH_RAM_AND_CODE_MAP_DETAIL */

static void sol_rpl_status_rsp(const struct bt_mesh_model *mod,
                               struct bt_mesh_msg_ctx *ctx,
                               u16_t range,
                               u8_t len)
{
    BT_MESH_MODEL_BUF_DEFINE(buf, OP_SOL_PDU_RPL_ITEM_STATUS, 2 + (len < 2 ? 0 : 1));

    bt_mesh_model_msg_init(&buf, OP_SOL_PDU_RPL_ITEM_STATUS);

    net_buf_simple_add_le16(&buf, range);

    if (len >= 2) {
        net_buf_simple_add_u8(&buf, len);
    }

    bt_mesh_model_send(mod, ctx, &buf, NULL, NULL);
}

static int item_clear(const struct bt_mesh_model *mod,
                      struct bt_mesh_msg_ctx *ctx,
                      struct net_buf_simple *buf,
                      bool acked)
{
    u16_t primary, range;
    u8_t len = 0;

    LOG_DBG("");

    if (buf->len > 3) {
        return -EMSGSIZE;
    }

    range = net_buf_simple_pull_le16(buf);
    primary = range >> 1;

    LOG_DBG("Start address: 0x%02x, %d", primary, buf->len);
    if (range & BIT(0)) {
        if (buf->len == 0) {
            return -EMSGSIZE;
        }

        len = net_buf_simple_pull_u8(buf);

        if (len < 2) {
            return -EINVAL;
        }
    }

    if ((primary + len)  > 0x8000 || primary == 0) {
        LOG_WRN("Range outside unicast address range or equal to 0");
        return -EINVAL;
    }

    for (int i = 0; i < len || i == 0; i++) {
        bt_mesh_srpl_entry_clear(primary + i);
    }

    if (acked) {
        sol_rpl_status_rsp(mod, ctx, range, len);
    }

    return 0;
}

static int handle_item_clear(const struct bt_mesh_model *mod,
                             struct bt_mesh_msg_ctx *ctx,
                             struct net_buf_simple *buf)
{
    return item_clear(mod, ctx, buf, true);
}

static int handle_item_clear_unacked(const struct bt_mesh_model *mod,
                                     struct bt_mesh_msg_ctx *ctx,
                                     struct net_buf_simple *buf)
{
    return item_clear(mod, ctx, buf, false);
}

const struct bt_mesh_model_op _bt_mesh_sol_pdu_rpl_srv_op[] = {
    { OP_SOL_PDU_RPL_ITEM_CLEAR, BT_MESH_LEN_MIN(2), handle_item_clear },
    { OP_SOL_PDU_RPL_ITEM_CLEAR_UNACKED, BT_MESH_LEN_MIN(2), handle_item_clear_unacked },

    BT_MESH_MODEL_OP_END
};

static int sol_pdu_rpl_srv_init(const struct bt_mesh_model *mod)
{
    if (!bt_mesh_model_in_primary(mod)) {
        LOG_ERR("Solicitation PDU RPL Configuration server not in primary element");
        return -EINVAL;
    }

    return 0;
}

const struct bt_mesh_model_cb _bt_mesh_sol_pdu_rpl_srv_cb = {
    .init = sol_pdu_rpl_srv_init,
};
