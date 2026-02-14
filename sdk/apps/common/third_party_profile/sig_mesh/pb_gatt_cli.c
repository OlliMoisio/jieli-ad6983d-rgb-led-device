/*
 * Copyright (c) 2021 Xiaomi Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "adaptation.h"
#include "mesh.h"
#include "net.h"
#include "rpl.h"
#include "transport.h"
#include "prov.h"
#include "pb_gatt.h"
#include "beacon.h"
#include "foundation.h"
#include "access.h"
#include "proxy.h"
#include "gatt_cli.h"
#include "proxy_msg.h"

#define LOG_TAG             "[MESH-pbgattcli]"
#define LOG_INFO_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_WARN_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_DUMP_ENABLE
#include "mesh_log.h"

#if MESH_RAM_AND_CODE_MAP_DETAIL
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ble_mesh_pbgattcli_bss")
#pragma data_seg(".ble_mesh_pbgattcli_data")
#pragma const_seg(".ble_mesh_pbgattcli_const")
#pragma code_seg(".ble_mesh_pbgattcli_code")
#endif
#else /* MESH_RAM_AND_CODE_MAP_DETAIL */
#pragma bss_seg(".ble_mesh_bss")
#pragma data_seg(".ble_mesh_data")
#pragma const_seg(".ble_mesh_const")
#pragma code_seg(".ble_mesh_code")
#endif /* MESH_RAM_AND_CODE_MAP_DETAIL */

static struct {
    const u8_t *target;
    struct bt_mesh_proxy_role *srv;
} server;

static void pb_gatt_msg_recv(struct bt_mesh_proxy_role *role)
{
    switch (role->msg_type) {
    case BT_MESH_PROXY_PROV:
        LOG_DBG("Mesh Provisioning PDU");
        bt_mesh_pb_gatt_recv(role->conn, &role->buf);
        break;
    default:
        LOG_WRN("Unhandled Message Type 0x%02x", role->msg_type);
        break;
    }
}

static void pb_gatt_connected(struct bt_conn *conn, void *user_data)
{
    server.srv = bt_mesh_proxy_role_setup(conn, bt_mesh_gatt_send,
                                          pb_gatt_msg_recv);

    server.target = NULL;

    bt_mesh_pb_gatt_cli_start(conn);
}

static void pb_gatt_link_open(struct bt_conn *conn)
{
    bt_mesh_pb_gatt_cli_open(conn);
}

static void pb_gatt_disconnected(struct bt_conn *conn)
{
    bt_mesh_pb_gatt_close(conn);

    bt_mesh_proxy_role_cleanup(server.srv);

    server.srv = NULL;
}

static const struct bt_mesh_gatt_cli pbgatt = {
    .srv_uuid		= BT_UUID_INIT_16(BT_UUID_MESH_PROV_VAL),
    .data_in_uuid		= BT_UUID_INIT_16(BT_UUID_MESH_PROV_DATA_IN_VAL),
    .data_out_uuid		= BT_UUID_INIT_16(BT_UUID_MESH_PROV_DATA_OUT_VAL),
    .data_out_cccd_uuid	= BT_UUID_INIT_16(BT_UUID_GATT_CCC_VAL),

    .connected		= pb_gatt_connected,
    .link_open		= pb_gatt_link_open,
    .disconnected		= pb_gatt_disconnected
};

int bt_mesh_pb_gatt_cli_setup(const u8_t uuid[16])
{
    if (server.srv) {
        return -EBUSY;
    }

    server.target = uuid;

    return 0;
}

void bt_mesh_pb_gatt_cli_adv_recv(const struct bt_le_scan_recv_info *info,
                                  struct net_buf_simple *buf)
{
    u8_t *uuid;
    bt_mesh_prov_oob_info_t oob_info;

    if (server.srv) {
        return;
    }

    if (buf->len != 18) {
        return;
    }

    uuid = net_buf_simple_pull_mem(buf, 16);

    if (server.target &&
        !memcmp(server.target, uuid, 16)) {
        (void)bt_mesh_gatt_cli_connect(info->addr, &pbgatt, NULL);
        return;
    }

    if (!bt_mesh_prov->unprovisioned_beacon_gatt) {
        return;
    }

    oob_info = (bt_mesh_prov_oob_info_t)net_buf_simple_pull_le16(buf);

    bt_mesh_prov->unprovisioned_beacon_gatt(uuid, oob_info);
}
