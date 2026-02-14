/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

static inline u16_t bt_mesh_hb_pwr2(u8_t val)
{
    if (!val) {
        return 0x0000;
    } else if (val == 0xff) {
        return 0xffff;
    } else {
        return (1 << (val - 1));
    }
}

static inline u8_t bt_mesh_hb_log(u32_t val)
{
    if (!val) {
        return 0x00;
    } else {
        return 32 - __builtin_clz(val);
    }
}

void bt_mesh_hb_init(void);
void bt_mesh_hb_start(void);
void bt_mesh_hb_suspend(void);
void bt_mesh_hb_resume(void);

int bt_mesh_hb_recv(struct bt_mesh_net_rx *rx, struct net_buf_simple *buf);
void bt_mesh_hb_feature_changed(u16_t features);

u8_t bt_mesh_hb_pub_set(struct bt_mesh_hb_pub *hb_pub);
u8_t bt_mesh_hb_sub_set(u16_t src, u16_t dst, u32_t period);
void bt_mesh_hb_sub_reset_count(void);
void bt_mesh_hb_pub_pending_store(void);
