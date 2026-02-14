/** @file
 *  @brief BLE mesh statistic APIs.
 */

/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_MESH_STATISTIC_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_MESH_STATISTIC_H_

#include <stdint.h>

/**
 * @brief Statistic
 * @defgroup bt_mesh_stat Statistic
 * @ingroup bt_mesh
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** The structure that keeps statistics of mesh frames handling. */
struct bt_mesh_statistic {
    /** All received frames passed basic validation and decryption. */
    /** Received frames over advertiser. */
    u32_t rx_adv;
    /** Received frames over loopback. */
    u32_t rx_loopback;
    /** Received frames over proxy. */
    u32_t rx_proxy;
    /** Received over unknown interface. */
    u32_t rx_uknown;
    /** Counter of frames that were initiated to relay over advertiser bearer. */
    u32_t tx_adv_relay_planned;
    /** Counter of frames that succeeded relaying over advertiser bearer. */
    u32_t tx_adv_relay_succeeded;
    /** Counter of frames that were initiated to send over advertiser bearer locally. */
    u32_t tx_local_planned;
    /** Counter of frames that succeeded to send over advertiser bearer locally. */
    u32_t tx_local_succeeded;
    /** Counter of frames that were initiated to send over friend bearer. */
    u32_t tx_friend_planned;
    /** Counter of frames that succeeded to send over friend bearer. */
    u32_t tx_friend_succeeded;
};

/** @brief Get mesh frame handling statistic.
 *
 *  @param st   BLE mesh statistic.
 */
void bt_mesh_stat_get(struct bt_mesh_statistic *st);

/** @brief Reset mesh frame handling statistic.
 */
void bt_mesh_stat_reset(void);

#ifdef __cplusplus
}
#endif
/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_MESH_STATISTIC_H_ */
