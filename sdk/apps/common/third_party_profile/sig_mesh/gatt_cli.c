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
#include "beacon.h"
#include "foundation.h"
#include "access.h"
#include "proxy.h"
#include "proxy_msg.h"
#include "proxy_cli.h"
#include "gatt_cli.h"
#include "pb_gatt_cli.h"

#define LOG_TAG             "[MESH-gattcli]"
/* #define LOG_INFO_ENABLE */
/* #define LOG_DEBUG_ENABLE */
#define LOG_WARN_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_DUMP_ENABLE
#include "mesh_log.h"

#if MESH_RAM_AND_CODE_MAP_DETAIL
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ble_mesh_gattcli_bss")
#pragma data_seg(".ble_mesh_gattcli_data")
#pragma const_seg(".ble_mesh_gattcli_const")
#pragma code_seg(".ble_mesh_gattcli_code")
#endif
#else /* MESH_RAM_AND_CODE_MAP_DETAIL */
#pragma bss_seg(".ble_mesh_bss")
#pragma data_seg(".ble_mesh_data")
#pragma const_seg(".ble_mesh_const")
#pragma code_seg(".ble_mesh_code")
#endif /* MESH_RAM_AND_CODE_MAP_DETAIL */

static struct bt_mesh_gatt_server {
    struct bt_conn *conn;
    u16_t svc_start_handle;
    u16_t data_in_handle;
    const struct bt_mesh_gatt_cli *gatt;

    union {
        void *user_data;
        // struct bt_gatt_discover_params discover;	//for compiler, not used now.
        // struct bt_gatt_subscribe_params subscribe;
    };
} servers[CONFIG_BT_MAX_CONN];

static struct bt_mesh_gatt_server *get_server(struct bt_conn *conn)
{
    return &servers[bt_conn_index(conn)];
}
#if 0 //for compiler, not used now.
static u8_t notify_func(struct bt_conn *conn,
                        struct bt_gatt_subscribe_params *params,
                        const void *data, u16_t length)
{
    const u8_t *val = data;

    if (!data) {
        LOG_WRN("[UNSUBSCRIBED]");
        params->value_handle = 0U;
        return BT_GATT_ITER_STOP;
    }

    if (length < 1) {
        LOG_WRN("Too small Proxy PDU");
        return BT_GATT_ITER_STOP;
    }

    (void)bt_mesh_proxy_msg_recv(conn, val, length);

    return BT_GATT_ITER_CONTINUE;
}

static void notify_enabled(struct bt_conn *conn, u8_t err,
                           struct bt_gatt_subscribe_params *params)
{
    struct bt_mesh_gatt_server *server = get_server(conn);

    if (err != 0) {
        LOG_WRN("Enable notify failed(err:%d)", err);
        return;
    }

    LOG_DBG("[SUBSCRIBED]");

    server->gatt->link_open(conn);
}

static u8_t discover_func(struct bt_conn *conn,
                          const struct bt_gatt_attr *attr,
                          struct bt_gatt_discover_params *params)
{
    int err;
    struct bt_mesh_gatt_server *server = get_server(conn);

    if (!attr) {
        LOG_DBG("GATT Services Discover complete");
        (void)memset(params, 0, sizeof(*params));
        return BT_GATT_ITER_STOP;
    }

    LOG_DBG("[ATTRIBUTE UUID 0x%04x] handle %u", BT_UUID_16(server->discover.uuid)->val,
            attr->handle);

    if (!bt_uuid_cmp(server->discover.uuid, &server->gatt->srv_uuid.uuid)) {
        server->svc_start_handle = attr->handle;

        server->discover.uuid = &server->gatt->data_in_uuid.uuid;
        server->discover.start_handle = attr->handle + 1;
        server->discover.type = BT_GATT_DISCOVER_CHARACTERISTIC;

        err = bt_gatt_discover(conn, &server->discover);
        if (err) {
            LOG_DBG("Discover GATT data in char failed (err %d)", err);
        }
    } else if (!bt_uuid_cmp(server->discover.uuid,
                            &server->gatt->data_in_uuid.uuid)) {
        server->data_in_handle = attr->handle + 1;

        server->discover.uuid = &server->gatt->data_out_uuid.uuid;
        server->discover.start_handle = server->svc_start_handle + 1;
        server->discover.type = BT_GATT_DISCOVER_CHARACTERISTIC;

        err = bt_gatt_discover(conn, &server->discover);
        if (err) {
            LOG_DBG("Discover GATT data out char failed (err %d)", err);
        }
    } else if (!bt_uuid_cmp(server->discover.uuid,
                            &server->gatt->data_out_uuid.uuid)) {
        server->discover.uuid = &server->gatt->data_out_cccd_uuid.uuid;
        server->discover.start_handle = attr->handle + 2;
        server->discover.type = BT_GATT_DISCOVER_DESCRIPTOR;

        err = bt_gatt_discover(conn, &server->discover);
        if (err) {
            LOG_DBG("Discover GATT CCCD failed (err %d)", err);
        }
    } else {
        (void)memset(&server->subscribe, 0, sizeof(server->subscribe));

        server->subscribe.notify = notify_func;
        server->subscribe.subscribe = notify_enabled;
        server->subscribe.value = BT_GATT_CCC_NOTIFY;
        server->subscribe.ccc_handle = attr->handle;
        server->subscribe.value_handle = attr->handle - 1;

        err = bt_gatt_subscribe(conn, &server->subscribe);
        if (err && err != -EALREADY) {
            LOG_DBG("Subscribe failed (err %d)", err);
        }
    }

    return BT_GATT_ITER_STOP;
}
#endif
int bt_mesh_gatt_send(struct bt_conn *conn,
                      const void *data, u16_t len,
                      bt_gatt_complete_func_t end, void *user_data)
{
    struct bt_mesh_gatt_server *server = get_server(conn);

    LOG_DBG("%u bytes: %s", len, bt_hex(data, len));

    return bt_gatt_write_without_response_cb(conn, server->data_in_handle,
            data, len, false, end, user_data);
}

static void gatt_connected(struct bt_conn *conn, u8_t conn_err)
{
    struct bt_mesh_gatt_server *server = get_server(conn);
    // struct bt_conn_info info;
    int err;

    // err = bt_conn_get_info(conn, &info); //for compiler, not used now.
    // if (err || info.role != BT_CONN_ROLE_CENTRAL ||
    //     !server->gatt) {
    // 	return;
    // }

    if (conn_err) {
        LOG_ERR("Failed to connect GATT Services(%u)", conn_err);

        bt_conn_unref(server->conn);
        server->conn = NULL;

        (void)bt_mesh_scan_enable();

        return;
    }

    LOG_DBG("conn %p err 0x%02x", (void *)conn, conn_err);

    server->gatt->connected(conn, server->user_data);

    (void)bt_mesh_scan_enable();
#if 0 //for compiler, not used now.
    server->discover.uuid = &server->gatt->srv_uuid.uuid;
    server->discover.func = discover_func;
    server->discover.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    server->discover.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    server->discover.type = BT_GATT_DISCOVER_PRIMARY;
    err = bt_gatt_discover(conn, &server->discover);
#endif
    if (err) {
        LOG_ERR("Unable discover GATT Services (err %d)", err);
    }
}

static void gatt_disconnected(struct bt_conn *conn, u8_t reason)
{
    // struct bt_conn_info info;
    struct bt_mesh_gatt_server *server = get_server(conn);
    int err;

    // err = bt_conn_get_info(conn, &info); //for compiler, not used now.
    // if (err || info.role != BT_CONN_ROLE_CENTRAL ||
    //     !server->gatt) {
    // 	return;
    // }

    server->gatt->disconnected(conn);

    bt_conn_unref(server->conn);

    (void)memset(server, 0, sizeof(struct bt_mesh_gatt_server));
}

int bt_mesh_gatt_cli_connect(const bt_addr_le_t *addr,
                             const struct bt_mesh_gatt_cli *gatt,
                             void *user_data)
{
    int err;
    struct bt_conn *conn;
    struct bt_mesh_gatt_server *server;

    /* Avoid interconnection between proxy client and server */
    conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, addr);
    if (conn) {
        bt_conn_unref(conn);
        return -EALREADY;
    }

    err = bt_mesh_scan_disable();
    if (err) {
        return err;
    }

    LOG_DBG("Try to connect services");

//	err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, //for compiler, not used now.
//				BT_LE_CONN_PARAM_DEFAULT, &conn);
    if (err) {
        LOG_ERR("Connection failed (err:%d)", err);

        (void)bt_mesh_scan_enable();

        return err;
    }

    server = get_server(conn);
    server->conn = conn;
    server->gatt = gatt;
    server->user_data = user_data;

    return 0;
}

static void gatt_advertising_recv(const struct bt_le_scan_recv_info *info,
                                  struct net_buf_simple *buf)
{
    u16_t uuid;

    if (buf->len < 3) {
        return;
    }

    uuid = net_buf_simple_pull_le16(buf);
    switch (uuid) {
#if defined(CONFIG_BT_MESH_PROXY_CLIENT)
    case BT_UUID_MESH_PROXY_VAL:
        bt_mesh_proxy_cli_adv_recv(info, buf);
        break;
#endif
#if defined(CONFIG_BT_MESH_PB_GATT_CLIENT)
    case BT_UUID_MESH_PROV_VAL:
        bt_mesh_pb_gatt_cli_adv_recv(info, buf);
        break;
#endif

    default:
        break;
    }
}

static void scan_recv(const struct bt_le_scan_recv_info *info,
                      struct net_buf_simple *buf)
{
//	if (info->adv_type != BT_GAP_ADV_TYPE_ADV_IND) {    //for compiler, not used now.
//		return;
//	}

    if (!bt_mesh_proxy_has_avail_conn()) {
        return;
    }

    while (buf->len > 1) {
        struct net_buf_simple_state state;
        u8_t len, type;

        len = net_buf_simple_pull_u8(buf);
        /* Check for early termination */
        if (len == 0U) {
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
        case BT_DATA_SVC_DATA16:
            gatt_advertising_recv(info, buf);
            break;
        default:
            break;
        }

        net_buf_simple_restore(buf, &state);
        net_buf_simple_pull(buf, len);
    }
}

// static struct bt_le_scan_cb scan_cb = {	//for compiler, not used now.
// 	.recv = scan_recv,
// };

void bt_mesh_gatt_client_init(void)
{
    //bt_le_scan_cb_register(&scan_cb);//for compiler, not used now.
}

void bt_mesh_gatt_client_deinit(void)
{
    //bt_le_scan_cb_unregister(&scan_cb);//for compiler, not used now.
}

// BT_CONN_CB_DEFINE(conn_callbacks) = {	//for compiler, not used now.
// 	.connected = gatt_connected,
// 	.disconnected = gatt_disconnected,
// };
