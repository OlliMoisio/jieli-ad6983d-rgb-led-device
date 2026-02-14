/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "adaptation.h"
#include "foundation.h"
#include "op_agg.h"

#define LOG_TAG             "[MESH-opagg]"
#define LOG_INFO_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_WARN_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_DUMP_ENABLE
#include "mesh_log.h"

#if MESH_RAM_AND_CODE_MAP_DETAIL
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ble_mesh_opagg_bss")
#pragma data_seg(".ble_mesh_opagg_data")
#pragma const_seg(".ble_mesh_opagg_const")
#pragma code_seg(".ble_mesh_opagg_code")
#endif
#else /* MESH_RAM_AND_CODE_MAP_DETAIL */
#pragma bss_seg(".ble_mesh_bss")
#pragma data_seg(".ble_mesh_data")
#pragma const_seg(".ble_mesh_const")
#pragma code_seg(".ble_mesh_code")
#endif /* MESH_RAM_AND_CODE_MAP_DETAIL */

#define IS_LENGTH_LONG(buf) ((buf)->data[0] & 1)
#define LENGTH_SHORT_MAX BIT_MASK(7)

int bt_mesh_op_agg_encode_msg(struct net_buf_simple *msg, struct net_buf_simple *buf)
{
    if (msg->len > LENGTH_SHORT_MAX) {
        if (net_buf_simple_tailroom(buf) < (msg->len + 2)) {
            return -ENOMEM;
        }

        net_buf_simple_add_le16(buf, (msg->len << 1) | 1);
    } else {
        if (net_buf_simple_tailroom(buf) < (msg->len + 1)) {
            return -ENOMEM;
        }

        net_buf_simple_add_u8(buf, msg->len << 1);
    }
    net_buf_simple_add_mem(buf, msg->data, msg->len);

    return 0;
}

int bt_mesh_op_agg_decode_msg(struct net_buf_simple *msg,
                              struct net_buf_simple *buf)
{
    u16_t len;

    if (IS_LENGTH_LONG(buf)) {
        if (buf->len < 2) {
            return -EINVAL;
        }

        len = net_buf_simple_pull_le16(buf) >> 1;
    } else {
        if (buf->len < 1) {
            return -EINVAL;
        }

        len = net_buf_simple_pull_u8(buf) >> 1;
    }

    if (buf->len < len) {
        return -EINVAL;
    }

    net_buf_simple_init_with_data(msg, net_buf_simple_pull_mem(buf, len), len);

    return 0;
}

bool bt_mesh_op_agg_is_op_agg_msg(struct net_buf_simple *buf)
{
    if (buf->len >= 2 && (buf->data[0] >> 6) == 2) {
        u16_t opcode;
        struct net_buf_simple_state state;

        net_buf_simple_save(buf, &state);
        opcode = net_buf_simple_pull_be16(buf);
        net_buf_simple_restore(buf, &state);

        if ((opcode == OP_OPCODES_AGGREGATOR_STATUS) ||
            (opcode == OP_OPCODES_AGGREGATOR_SEQUENCE)) {
            return true;
        }
    }
    return false;
}
