/** @file
 *  @brief Configuration Server Model APIs.
 */

/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_BLUETOOTH_MESH_CFG_SRV_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_MESH_CFG_SRV_H_

/**
 * @brief Configuration Server Model
 * @defgroup bt_mesh_cfg_srv Configuration Server Model
 * @ingroup bt_mesh
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @cond INTERNAL_HIDDEN */
extern const struct bt_mesh_model_op bt_mesh_cfg_srv_op[];
extern const struct bt_mesh_model_cb bt_mesh_cfg_srv_cb;
/** @endcond */

/**
 *  @brief Generic Configuration Server model composition data entry.
 */
#define BT_MESH_MODEL_CFG_SRV	\
	BT_MESH_MODEL_CNT_CB(BT_MESH_MODEL_ID_CFG_SRV,	\
			     bt_mesh_cfg_srv_op, NULL,	\
			     NULL, 1, 0, &bt_mesh_cfg_srv_cb)

#ifdef __cplusplus
}
#endif

bool get_dev_comp_data_complete(void);
void set_dev_comp_data_complete(u8 flag);
/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_MESH_CFG_SRV_H_ */
