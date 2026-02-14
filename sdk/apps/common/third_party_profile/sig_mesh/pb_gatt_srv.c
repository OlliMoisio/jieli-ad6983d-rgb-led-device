/*
 * Copyright (c) 2017 Intel Corporation
 * Copyright (c) 2021 Lingao Meng
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
#include "proxy_msg.h"
#include "pb_gatt_srv.h"

#define LOG_TAG             "[MESH-pbgattsrv]"
#define LOG_INFO_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_WARN_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_DUMP_ENABLE
#include "mesh_log.h"

#if MESH_RAM_AND_CODE_MAP_DETAIL
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ble_mesh_pbgattsrv_bss")
#pragma data_seg(".ble_mesh_pbgattsrv_data")
#pragma const_seg(".ble_mesh_pbgattsrv_const")
#pragma code_seg(".ble_mesh_pbgattsrv_code")
#endif
#else /* MESH_RAM_AND_CODE_MAP_DETAIL */
#pragma bss_seg(".ble_mesh_bss")
#pragma data_seg(".ble_mesh_data")
#pragma const_seg(".ble_mesh_const")
#pragma code_seg(".ble_mesh_code")
#endif /* MESH_RAM_AND_CODE_MAP_DETAIL */

#define ADV_OPT_PROV                                                           //\
// (BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_SCANNABLE |                 \
//  BT_LE_ADV_OPT_ONE_TIME | ADV_OPT_USE_IDENTITY)

#define FAST_ADV_TIME (60LL * MSEC_PER_SEC)

static s64_t fast_adv_timestamp;

static int gatt_send(struct bt_conn *conn,
                     const void *data, u16_t len,
                     bt_gatt_complete_func_t end, void *user_data);

static struct bt_mesh_proxy_role *cli;
static bool service_registered;

static void proxy_msg_recv(struct bt_mesh_proxy_role *role)
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

static ssize_t gatt_recv(struct bt_conn *conn,
                         const struct bt_gatt_attr *attr, const void *buf,
                         u16_t len, u16_t offset, u8_t flags)
{
    const u8_t *data = buf;

    if (cli->conn != conn) {
        LOG_ERR("No PB-GATT Client found");
        return -ENOTCONN;
    }

    if (len < 1) {
        LOG_WRN("Too small Proxy PDU");
        return -EINVAL;
    }

    if (PDU_TYPE(data) != BT_MESH_PROXY_PROV) {
        LOG_WRN("Proxy PDU type doesn't match GATT service");
        return -EINVAL;
    }

    return bt_mesh_proxy_msg_recv(conn, buf, len);
}

void bt_mesh_pb_gatt_disconnect(void)
{
    if (cli && cli->conn) {
        bt_conn_disconnect(cli->conn,
                           BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
}

void pb_gatt_connected(struct bt_conn *conn, u8_t conn_err)
{
    if (!service_registered || bt_mesh_is_provisioned() || cli)  {
        //LOG_ERR("pb_gatt_connected err!");
        return;
    }

    cli = bt_mesh_proxy_role_setup(conn, gatt_send, proxy_msg_recv);
}

void pb_gatt_disconnected(struct bt_conn *conn, u8_t reason)
{
    if (!service_registered || !cli || cli->conn != conn) {
        //LOG_ERR("pb_gatt_disconnected err!");
        return;
    }

    bt_mesh_proxy_role_cleanup(cli);
    cli = NULL;

    bt_mesh_pb_gatt_close(conn);

    if (bt_mesh_is_provisioned()) {
        if (IS_ENABLED(CONFIG_BT_MESH_PB_GATT)) {
            (void)bt_mesh_pb_gatt_srv_disable();
        }
        if (IS_ENABLED(CONFIG_BT_MESH_GATT_PROXY)) {
            (void)bt_mesh_proxy_gatt_enable();
        }
    }
}

static void prov_ccc_changed(const struct bt_gatt_attr *attr, u16_t value)
{
    LOG_DBG("value 0x%04x", value);
}

ssize_t prov_ccc_write(struct bt_conn *conn,
                       const void *buf, u16_t len,
                       u16_t offset, u8_t flags)
{
    if (cli->conn != conn) {
        LOG_ERR("No PB-GATT Client found");
        return -ENOTCONN;
    }

    u16_t value;

    LOG_DBG("len %u: %s", len, bt_hex(buf, len));

    if (len != sizeof(value)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    value = sys_get_le16(buf);

    LOG_DBG("value 0x%04x", value);

    if (value != BT_GATT_CCC_NOTIFY) {
        LOG_WRN("Client wrote 0x%04x instead enabling notify", value);
        return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
    }

    bt_mesh_pb_gatt_start(conn);

    return sizeof(value);
}

int bt_mesh_pb_gatt_srv_enable(void)
{
    LOG_DBG("");

    if (bt_mesh_is_provisioned()) {
        return -ENOTSUP;
    }

    if (service_registered) {
        return -EBUSY;
    }

    (void)bt_gatt_service_register(BT_UUID_MESH_PROV_VAL);
    service_registered = true;
    fast_adv_timestamp = k_uptime_get();

    return 0;
}

int bt_mesh_pb_gatt_srv_disable(void)
{
    LOG_DBG("");

    if (!service_registered) {
        return -EALREADY;
    }

    bt_gatt_service_unregister(BT_UUID_MESH_PROV_VAL);
    service_registered = false;

    bt_mesh_adv_gatt_update();

    return 0;
}

static u8_t prov_svc_data[20] = {0};//{
// 	BT_UUID_16_ENCODE(BT_UUID_MESH_PROV_VAL),//for compiler, not used now.
// };

static const struct bt_data prov_ad[] = {0};//{
// 	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
// 	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
// 		      BT_UUID_16_ENCODE(BT_UUID_MESH_PROV_VAL)),
// 	BT_DATA(BT_DATA_SVC_DATA16, prov_svc_data, sizeof(prov_svc_data)),//for compiler, not used now.
// };

static size_t gatt_prov_adv_create(struct bt_data prov_sd[2])
{
    size_t prov_sd_len = 0;

    const struct bt_mesh_prov *prov = bt_mesh_prov_get();
    size_t uri_len;

    memcpy(prov_svc_data + 2, prov->uuid, 16);
    sys_put_be16(prov->oob_info, prov_svc_data + 18);

    if (!prov->uri) {
        goto dev_name;
    }

    uri_len = strlen(prov->uri);
    if (uri_len > 29) {
        /* There's no way to shorten an URI */
        LOG_WRN("Too long URI to fit advertising packet");
        goto dev_name;
    }

    prov_sd[prov_sd_len].type = BT_DATA_URI;
    prov_sd[prov_sd_len].data_len = uri_len;
    prov_sd[prov_sd_len].data = (const u8_t *)prov->uri;

    prov_sd_len += 1;

dev_name:
#if defined(CONFIG_BT_MESH_PB_GATT_USE_DEVICE_NAME)
    prov_sd[prov_sd_len].type = BT_DATA_NAME_COMPLETE;
    prov_sd[prov_sd_len].data_len = sizeof(CONFIG_BT_DEVICE_NAME) - 1;
    prov_sd[prov_sd_len].data = CONFIG_BT_DEVICE_NAME;

    prov_sd_len += 1;
#endif

    return prov_sd_len;
}

static int gatt_send(struct bt_conn *conn,
                     const void *data, u16_t len,
                     bt_gatt_complete_func_t end, void *user_data)
{
    LOG_DBG("%u bytes: %s", len, bt_hex(data, len));

    int err;

    err = bt_gatt_notify(conn, data, len);

    if ((!err) && end) {
        end(NULL, user_data);	//Fix: used for bt_mesh_adv_unref().
    }

    return err;
}

int bt_mesh_pb_gatt_srv_adv_start(void)
{
    LOG_DBG("");

    if (!service_registered || bt_mesh_is_provisioned() ||
        !bt_mesh_proxy_has_avail_conn()) {
        return -ENOTSUP;
    }

    struct bt_le_adv_param fast_adv_param = {
        .id = BT_ID_DEFAULT,
        //.options = ADV_OPT_PROV,//for compiler, not used now.
        ADV_FAST_INT,
    };
    struct bt_data prov_sd[2];
    size_t prov_sd_len;
    s64_t timestamp = fast_adv_timestamp;
    s64_t elapsed_time = 10;//k_uptime_delta(&timestamp);

    prov_sd_len = gatt_prov_adv_create(prov_sd);

    if (elapsed_time > FAST_ADV_TIME) {
        struct bt_le_adv_param slow_adv_param = {
            .id = BT_ID_DEFAULT,
            //.options = ADV_OPT_PROV,//for compiler, not used now.
            ADV_SLOW_INT,
        };

        return bt_mesh_adv_gatt_start(&slow_adv_param, SYS_FOREVER_MS, prov_ad,
                                      ARRAY_SIZE(prov_ad), prov_sd, prov_sd_len);
    }

    LOG_DBG("remaining fast adv time (%lld ms)", (FAST_ADV_TIME - elapsed_time));
    /* Advertise 60 seconds using fast interval */
    return bt_mesh_adv_gatt_start(&fast_adv_param, (FAST_ADV_TIME - elapsed_time),
                                  prov_ad, ARRAY_SIZE(prov_ad),
                                  prov_sd, prov_sd_len);

}
