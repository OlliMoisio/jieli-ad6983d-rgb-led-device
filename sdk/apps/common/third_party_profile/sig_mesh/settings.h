/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "syscfg_id.h"

/* Pending storage actions. */
enum bt_mesh_settings_flag {
    BT_MESH_SETTINGS_RPL_PENDING,
    BT_MESH_SETTINGS_NET_KEYS_PENDING,
    BT_MESH_SETTINGS_APP_KEYS_PENDING,
    BT_MESH_SETTINGS_NET_PENDING,
    BT_MESH_SETTINGS_IV_PENDING,
    BT_MESH_SETTINGS_SEQ_PENDING,
    BT_MESH_SETTINGS_HB_PUB_PENDING,
    BT_MESH_SETTINGS_CFG_PENDING,
    BT_MESH_SETTINGS_MOD_PENDING,
    BT_MESH_SETTINGS_VA_PENDING,
    BT_MESH_SETTINGS_CDB_PENDING,
    BT_MESH_SETTINGS_SRPL_PENDING,
    BT_MESH_SETTINGS_SSEQ_PENDING,
    BT_MESH_SETTINGS_COMP_PENDING,
    BT_MESH_SETTINGS_DEV_KEY_CAND_PENDING,

    BT_MESH_SETTINGS_FLAG_COUNT,
};

#if (CONFIG_BT_MESH_CDB)
#define MAX_SIG_MODEL_NUMS      6
#define MAX_VND_MODEL_NUMS      3
#else
#define MAX_SIG_MODEL_NUMS      18
#define MAX_VND_MODEL_NUMS      3
#endif

typedef enum _NODE_INFO_SETTING_INDEX { //range= 72
    /* NODE_MAC_ADDR_INDEX = 0, */
    NET_INDEX = VM_MESH_NODE_INFO_START, //176
    IV_INDEX,
    SEQ_INDEX,
    RPL_INDEX,
    NET_KEY_INDEX,
    APP_KEY_INDEX,
    HB_PUB_INDEX,
    CFG_INDEX,

    MOD_BIND_INDEX,
    VND_MOD_BIND_INDEX,
    MOD_SUB_INDEX,
    VND_MOD_SUB_INDEX,
    MOD_PUB_INDEX,//188
    VND_MOD_PUB_INDEX = MOD_PUB_INDEX + MAX_SIG_MODEL_NUMS,

    COMP_DATA_PAGE_0_INDEX = VND_MOD_PUB_INDEX + MAX_VND_MODEL_NUMS,
    COMP_DATA_PAGE_1_INDEX,//210
    COMP_DATA_PAGE_2_INDEX,

    BLOB_SRV_STATE_INDEX,
    DFU_SRV_STATE_INDEX,
    OD_PP_STATE_INDEX,
    PRIV_BEACON_STATE_INDEX,
    SAR_RX_STATE_INDEX,
    SAR_TX_STATE_INDEX,

    METADATA_PAGE_INDEX,

    VIRTUAL_ADDR_INDEX,//219
    MOD_SUB_VA_INDEX = VIRTUAL_ADDR_INDEX  + CONFIG_BT_MESH_LABEL_COUNT,
    VND_MOD_SUB_VA_INDEX = MOD_SUB_VA_INDEX + MAX_SIG_MODEL_NUMS,

    DEVKEY_CAND_INDEX = VND_MOD_SUB_VA_INDEX + MAX_VND_MODEL_NUMS,

    SRPL_INDEX,
    SSEQ_INDEX = SRPL_INDEX + CONFIG_BT_MESH_PROXY_SRPL_SIZE,

#if (CONFIG_BT_MESH_DFU_DIST) || (CONFIG_BT_MESH_DFU_TARGET)
    DFU_CFG_INDEX,
#endif

#if (CONFIG_BT_MESH_CDB)
    CDB_INDEX,
    CDB_NODE_INDEX,
    CDB_APP_KEY_INDEX = CDB_NODE_INDEX + CONFIG_BT_MESH_CDB_NODE_COUNT,
    CDB_SUBNET_INDEX = CDB_APP_KEY_INDEX + CONFIG_BT_MESH_CDB_SUBNET_COUNT + CONFIG_BT_MESH_CDB_APP_KEY_COUNT,
    CDB_MAX_INDEX = CDB_SUBNET_INDEX + CONFIG_BT_MESH_CDB_SUBNET_COUNT + CONFIG_BT_MESH_CDB_APP_KEY_COUNT,
#endif
    //CDB_MAX_INDEX,need <254.
} NODE_INFO_SETTING_INDEX;

#ifdef TEST_COMPIER//CONFIG_BT_SETTINGS
#define BT_MESH_SETTINGS_DEFINE(_hname, _subtree, _set)                                            \
	static int pre_##_set(const char *name, size_t len_rd, settings_read_cb read_cb,           \
			      void *cb_arg)                                                        \
	{                                                                                          \
		if (!atomic_test_bit(bt_mesh.flags, BT_MESH_INIT)) {                               \
			return 0;                                                                  \
		}                                                                                  \
		return _set(name, len_rd, read_cb, cb_arg);                                        \
	}                                                                                          \
	SETTINGS_STATIC_HANDLER_DEFINE(bt_mesh_##_hname, "bt/mesh/" _subtree, NULL, pre_##_set,    \
				       NULL, NULL)
#else
/* Declaring non static settings handler helps avoid unnecessary ifdefs
 * as well as unused function warning. Since the declared handler structure is
 * unused, linker will discard it.
 */
#define BT_MESH_SETTINGS_DEFINE(_hname, _subtree, _set)\
	const struct settings_handler settings_handler_bt_mesh_ ## _hname = {\
		.h_set = _set,						     \
	}
#endif

/* Replay Protection List information for persistent storage. */
struct rpl_val {
    u32_t seq: 24,
          old_iv: 1;
};

/* NetKey storage information */
struct net_key_val {
    u8_t existence;
#if CONFIG_BT_MESH_PROVISIONER
    u16_t net_idx;
#endif
    u8_t kr_flag: 1,
         kr_phase: 7;
    struct bt_mesh_key val[2];
} __packed;

/* AppKey information for persistent storage. */
struct app_key_val {
    u8_t existence;
#if CONFIG_BT_MESH_PROVISIONER
    u16_t app_idx;
#endif
    u16_t net_idx;
    bool updated;
    struct bt_mesh_key val[2];
} __packed;

/* We need this so we don't overwrite app-hardcoded values in case FCB
 * contains a history of changes but then has a NULL at the end.
 */
struct __rpl_val {
    u16_t src;
    struct rpl_val rpl;
};

struct __mod_bind {
    u8_t existence;
    u16_t keys[CONFIG_BT_MESH_MODEL_KEY_COUNT];
};
struct __mod_sub {
    u8_t existence;
    u16_t groups[CONFIG_BT_MESH_MODEL_GROUP_COUNT];
};

void bt_mesh_settings_init(void);
void bt_mesh_settings_store_schedule(enum bt_mesh_settings_flag flag);
void bt_mesh_settings_store_cancel(enum bt_mesh_settings_flag flag);
void bt_mesh_settings_store_pending(void);
int bt_mesh_settings_set(settings_read_cb read_cb, void *cb_arg,
                         void *out, size_t read_len);
