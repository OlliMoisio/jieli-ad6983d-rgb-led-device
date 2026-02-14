/*  Bluetooth Mesh */

/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "adaptation.h"
#include "net/buf.h"
#include "adv.h"
#include "timer.h"
#include "ble/hci_ll.h"
#include "os/os_cpu.h"
#include "btstack/bluetooth.h"
#include "model_api.h"

#define LOG_TAG             "[MESH-adv_core]"
// #define LOG_INFO_ENABLE
// #define LOG_DEBUG_ENABLE
#define LOG_WARN_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_DUMP_ENABLE
#include "mesh_log.h"

#if ADAPTATION_COMPILE_DEBUG

void bt_mesh_adv_gatt_update(void) {}

void bt_mesh_adv_send(struct bt_mesh_adv *adv, const struct bt_mesh_send_cb *cb, void *cb_data) {}

void bt_mesh_adv_init(void) {}

#else /* ADAPTATION_COMPILE_DEBUG */

#define MESH_ADV_SEND_USE_HI_TIMER          1

#if MESH_ADV_SEND_USE_HI_TIMER

#define sys_timer_add               sys_hi_timer_add
#define sys_timer_change_period     sys_hi_timer_modify
#define sys_timer_remove            sys_hi_timer_del

#endif /* MESH_ADV_SEND_USE_HI_TIMER */

#define CUR_DEBUG_IO_0(i,x)         //{JL_PORT##i->DIR &= ~BIT(x), JL_PORT##i->OUT &= ~BIT(x);}
#define CUR_DEBUG_IO_1(i,x)         //{JL_PORT##i->DIR &= ~BIT(x), JL_PORT##i->OUT |= BIT(x);}

#define ADV_HW_START()              CUR_DEBUG_IO_1(A, 2)
#define ADV_HW_END()                CUR_DEBUG_IO_0(A, 2)

#define ADV_INT_FAST_MS         10
#define ADV_INT_DEFAULT_MS      100

#if (CONFIG_MESH_MODEL == SIG_MESH_PROVISIONER)
#define USER_ADV_SEND_INTERVAL config_bt_mesh_node_msg_adv_interval
#define USER_ADV_SEND_DURATION config_bt_mesh_node_msg_adv_duration
#else
#define USER_ADV_SEND_INTERVAL \
    (bt_mesh_is_provisioned()? config_bt_mesh_node_msg_adv_interval : config_bt_mesh_pb_adv_interval)
#define USER_ADV_SEND_DURATION \
    (bt_mesh_is_provisioned()? config_bt_mesh_node_msg_adv_duration : config_bt_mesh_pb_adv_duration)
#endif

static const u8_t adv_type[] = {
    [BT_MESH_ADV_PROV]   = BT_DATA_MESH_PROV,
    [BT_MESH_ADV_DATA]   = BT_DATA_MESH_MESSAGE,
    [BT_MESH_ADV_BEACON] = BT_DATA_MESH_BEACON,
    [BT_MESH_ADV_URI]    = BT_DATA_URI,
};

static sys_timer mesh_AdvSend_timer;

static sys_slist_t adv_list = {
    .head = NULL,
    .tail = NULL,
};

static void ble_adv_enable(bool en);
static bool adv_send(struct bt_mesh_adv *adv);
static void fresh_adv_info(struct bt_mesh_adv *adv);
void resume_mesh_gatt_proxy_adv_thread(void);
extern void unprovision_connectable_adv(void);
extern void proxy_connectable_adv(void);
extern void proxy_fast_connectable_adv(void);
extern int bt_mesh_get_if_app_key_add(void);
extern bool get_if_connecting(void);
extern void bt_mesh_adv_buf_alloc(void);
void ble_set_scan_enable(bool en);

static u16 mesh_adv_send_start(void *param)
{
    struct bt_mesh_adv *buf = param;

    // if (BT_MESH_ADV(buf)->delay) {   //Don't run these because BT_MESH_ADV(buf)->delay can't be 1.
    //     BT_MESH_ADV(buf)->delay = 0;
    // fresh_adv_info(buf);

    // return USER_ADV_SEND_DURATION;
    // }

    return 0;
}

static void mesh_adv_send_end(void *param)
{
    struct bt_mesh_adv *adv = param;
    void *cb_data;

    ble_adv_enable(0);

    LOG_DBG("%s adv %p cb %p end %p ", __func__, adv, adv->ctx.cb, adv->ctx.cb->end);

    if (adv) {
        if (adv->ctx.cb && adv->ctx.cb->end) {
            cb_data = adv->ctx.cb_data;
            adv->ctx.cb->end(0, cb_data);
        }

        adv->ctx.busy = 0;

        bt_mesh_adv_unref(adv);
    }


    struct bt_mesh_adv *prev_adv;
    prev_adv = net_buf_slist_simple_get(&adv_list);
    LOG_DBG("%s prev_adv %p data %p busy %d ", __func__, prev_adv, prev_adv->ctx.busy);

    if (prev_adv && prev_adv->ctx.busy) {
        bool send_busy = adv_send(prev_adv);
        LOG_DBG("mesh_adv_send_end %s", send_busy ? "busy" : "succ");
    } else {
        resume_mesh_gatt_proxy_adv_thread();
    }
}

static void mesh_adv_timer_handler(void *param)
{
    u16 duration;

    //LOG_DBG("TO - adv_timer_cb 0x%x", param);

    if (NULL == param) {
        LOG_ERR("param is NULL");
        return;
    }

    duration = mesh_adv_send_start(param);

    if (duration) {
        sys_timer_change_period(mesh_AdvSend_timer, duration);
        LOG_INF("start duration= %dms", duration);
    } else {
        sys_timer_remove(mesh_AdvSend_timer);
        mesh_AdvSend_timer = 0;
        mesh_adv_send_end(param);
    }

    //LOG_DBG("adv_t_cb end");
}

static void mesh_adv_timeout_start(u16 delay, u16 duration, void *param)
{
    if (0 == mesh_AdvSend_timer) {
        /* mesh_AdvSend_timer = sys_timer_register(delay? delay : duration, mesh_adv_timer_handler); */
        /* sys_timer_set_context(mesh_AdvSend_timer, param); */
        mesh_AdvSend_timer = sys_timer_add(param, mesh_adv_timer_handler, delay ? delay : duration);
        /*LOG_INF("mesh_AdvSend_timer id = %d, %s= %dms",
                mesh_AdvSend_timer,
                delay ? "delay first" : "only duration",
                delay ? delay : duration);
        LOG_INF("param addr=0x%x", param);*/
    }

    ASSERT(mesh_adv_timer_handler);
}

bool mesh_adv_send_timer_busy(void)
{
    return !!mesh_AdvSend_timer;
}

void ble_set_adv_param(u16 interval_min, u16 interval_max, u8 type, u8 direct_addr_type, u8 *direct_addr,
                       u8 channel_map, u8 filter_policy)
{
#if CMD_DIRECT_TO_BTCTRLER_TASK_EN
    ll_hci_adv_set_params(interval_min, interval_max, type, direct_addr_type, direct_addr,
                          channel_map, filter_policy);
#else
    ble_user_cmd_prepare(BLE_CMD_ADV_PARAM, 3, interval_min, type, channel_map);
#endif /* CMD_DIRECT_TO_BTCTRLER_TASK_EN */
}

void ble_set_adv_data(u8 data_length, u8 *data)
{
#if CMD_DIRECT_TO_BTCTRLER_TASK_EN
    ll_hci_adv_set_data(data_length, data);
#else
    ble_user_cmd_prepare(BLE_CMD_ADV_DATA, 2, data_length, data);
#endif /* CMD_DIRECT_TO_BTCTRLER_TASK_EN */
}

void ble_set_scan_rsp_data(u8 data_length, u8 *data)
{
#if CMD_DIRECT_TO_BTCTRLER_TASK_EN
    ll_hci_adv_scan_response_set_data(data_length, data);
#else
    ble_user_cmd_prepare(BLE_CMD_RSP_DATA, 2, data_length, data);
#endif /* CMD_DIRECT_TO_BTCTRLER_TASK_EN */
}

static void ble_adv_enable(bool en)
{
    if (en) {
        ADV_HW_START();
    } else {
        ADV_HW_END();
    }

#if CMD_DIRECT_TO_BTCTRLER_TASK_EN
    ll_hci_adv_enable(en);
#else
    ble_user_cmd_prepare(BLE_CMD_ADV_ENABLE, 1, en);
#endif /* CMD_DIRECT_TO_BTCTRLER_TASK_EN */
}

static void fresh_adv_info(struct bt_mesh_adv *adv)
{
    const u8 direct_addr[6] = {0};
    u16 adv_interval;
    struct advertising_data_header *adv_data_head;
    u16 total_adv_data_len;

#if 0
    const s32_t adv_int_min = ADV_INT_FAST_MS;
    adv_interval = max(adv_int_min, BT_MESH_TRANSMIT_INT(BT_MESH_ADV(buf)->ctx.xmit));
    adv_interval = ADV_SCAN_UNIT(adv_interval);
#else
    adv_interval = USER_ADV_SEND_INTERVAL;
#endif

    uint8_t temp_ref;
    temp_ref = adv->__ref;

    adv_data_head = adv->b.data - BT_MESH_ADV_DATA_HEAD_SIZE;
    adv_data_head->Len = adv->b.len + 1;
    adv_data_head->Type = adv_type[adv->ctx.type];
    total_adv_data_len = adv->b.len + BT_MESH_ADV_DATA_HEAD_SIZE;

    /* ble_set_scan_enable(0); */
    ble_adv_enable(0);
    ble_set_adv_param(adv_interval, adv_interval, 0x03, 0, direct_addr, 0x07, 0x00);
    ble_set_adv_data(total_adv_data_len, adv_data_head);
    ble_adv_enable(1);

    adv->__ref = temp_ref;
}

static bool adv_send(struct bt_mesh_adv *adv)
{
    //LOG_INF("--func=%s", __FUNCTION__);
    //LOG_INF("entry_node addr=0x%x", &adv->node);

    OS_ENTER_CRITICAL();

    if (TRUE == mesh_adv_send_timer_busy()) {    // not used now.
        adv->node.next = 0;
        net_buf_slist_simple_put(&adv_list, &adv->node);

        LOG_DBG("send timer busy. adv %p ", adv);

        OS_EXIT_CRITICAL();
        return 1;
    }


    const struct bt_mesh_send_cb *cb = adv->ctx.cb;
    void *cb_data = adv->ctx.cb_data;
    u16 delay = 0;
    u16 duration;

    duration = USER_ADV_SEND_DURATION;
    // duration = (BT_MESH_TRANSMIT_INT(adv->ctx.xmit) +
    //            ((BT_MESH_TRANSMIT_COUNT(adv->ctx.xmit) + 1) * (BT_MESH_TRANSMIT_INT(adv->ctx.xmit) + 10)));

    LOG_DBG("%s duration %d int %d count %d \r\n", __func__, duration, BT_MESH_TRANSMIT_INT(adv->ctx.xmit),
            BT_MESH_TRANSMIT_COUNT(adv->ctx.xmit));
    LOG_DBG("%s adv %p cb %p start %p ", __func__, adv, adv->ctx.cb, adv->ctx.cb->start);

    if (cb) {
        if (cb->start) {
            cb->start(duration, 0, cb_data);
        }
    }

    if (0 == delay) {
        fresh_adv_info(adv);
    } else {
        // BT_MESH_ADV(buf)->delay = 1; //for compiler, not used now.
    }

    mesh_adv_timeout_start(delay, duration, (void *)adv);

    OS_EXIT_CRITICAL();

    return 0;
}

void resume_mesh_gatt_proxy_adv_thread(void)
{
    BT_MESH_FEATURES_IS_SUPPORT_OPTIMIZE(BT_MESH_FEAT_PROXY);

    if (!(IS_ENABLED(CONFIG_BT_MESH_PROXY) & IS_ENABLED(CONFIG_BT_MESH_PB_GATT))) {
        return;
    }

    //LOG_INF("--func=%s", __FUNCTION__);

    if (TRUE == mesh_adv_send_timer_busy()) {
        LOG_ERR(">>>>> %s mesh adv send timer busy", __func__);
        return;
    }

    ble_adv_enable(0);

    //< send period connectable adv ( unprovision adv || proxy adv )
    if (FALSE == bt_mesh_is_provisioned()) {
        unprovision_connectable_adv();
    } else if (!(is_pb_adv()) && (false == bt_mesh_get_if_app_key_add())) {
        proxy_fast_connectable_adv();
    } else if (bt_mesh_is_provisioned()) {
        proxy_connectable_adv();
    }

    if (FALSE == get_if_connecting()) {
        ble_adv_enable(1);
    }
}

void bt_mesh_adv_gatt_update(void)
{
    LOG_INF("--func=%s", __FUNCTION__);

    if (bt_mesh_is_provisioned()) {
        if (IS_ENABLED(CONFIG_BT_MESH_GATT_PROXY)) {
            bt_mesh_gatt_proxy_set(BT_MESH_FEATURE_ENABLED);
            bt_mesh_proxy_adv_start();  //for refresh proxy adv data.
        }
    }

    if (!get_if_connecting()) {
        resume_mesh_gatt_proxy_adv_thread();
    }
}

void newbuf_replace(struct net_buf_pool *pool)
{
    struct net_buf *buf;

    buf = net_buf_slist_simple_get(&adv_list);

    BT_MESH_ADV(buf)->ctx.busy = 0;

    buf->ref = 0;

    buf->flags = 0;

    pool->free_count++;
}

void bt_mesh_adv_send(struct bt_mesh_adv *adv, const struct bt_mesh_send_cb *cb,
                      void *cb_data)
{
    //LOG_DBG("--func=%s", __FUNCTION__);
    /* LOG_DBG("type 0x%02x len %u: %s", adv->ctx.type, buf->len, */
    /* bt_hex(buf->data, buf->len)); */

    adv->ctx.cb = cb;
    adv->ctx.cb_data = cb_data;
    adv->ctx.busy = 1U;

    bt_mesh_adv_ref(adv);

    bool send_busy =  adv_send(adv);

    if (send_busy) {
        LOG_WRN("mesh_adv_send busy");
    } else {
        LOG_INF("mesh_adv_send succ");
    }
}

int bt_mesh_adv_gatt_send(void)
{
    if (bt_mesh_is_provisioned()) {
        if (IS_ENABLED(CONFIG_BT_MESH_GATT_PROXY)) {
            bt_mesh_gatt_proxy_set(BT_MESH_FEATURE_ENABLED);
            bt_mesh_proxy_adv_start();  //for refresh proxy adv data.
        }
    }
    resume_mesh_gatt_proxy_adv_thread();
    return 0;
}


void bt_mesh_adv_init(void)
{
#if NET_BUF_USE_MALLOC
    bt_mesh_adv_buf_alloc();
#endif /* NET_BUF_USE_MALLOC */
}

#endif /* ADAPTATION_COMPILE_DEBUG */
