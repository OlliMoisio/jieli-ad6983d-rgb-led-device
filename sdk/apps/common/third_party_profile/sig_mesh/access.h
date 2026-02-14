/*  Bluetooth Mesh */

/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Tree walk return codes */
enum bt_mesh_walk {
    BT_MESH_WALK_STOP,
    BT_MESH_WALK_CONTINUE,
};

/* bt_mesh_model.flags */
enum {
    BT_MESH_MOD_BIND_PENDING = BIT(0),
    BT_MESH_MOD_SUB_PENDING = BIT(1),
    BT_MESH_MOD_PUB_PENDING = BIT(2),
    BT_MESH_MOD_EXTENDED = BIT(3),
    BT_MESH_MOD_DEVKEY_ONLY = BIT(4),
    BT_MESH_MOD_DATA_PENDING = BIT(5),
};

void bt_mesh_elem_register(const struct bt_mesh_elem *elem, u8_t count);

u8_t bt_mesh_elem_count(void);
size_t bt_mesh_comp_page_size(u8_t page);
int bt_mesh_comp_data_get_page_0(struct net_buf_simple *buf, size_t offset);
size_t bt_mesh_metadata_page_0_size(void);
int bt_mesh_metadata_get_page_0(struct net_buf_simple *buf, size_t offset);

/* Find local element based on unicast address */
const struct bt_mesh_elem *bt_mesh_elem_find(u16_t addr);

bool bt_mesh_has_addr(u16_t addr);
bool bt_mesh_model_has_key(const struct bt_mesh_model *mod, u16_t key);

void bt_mesh_model_extensions_walk(const struct bt_mesh_model *root,
                                   enum bt_mesh_walk(*cb)(const struct bt_mesh_model *mod,
                                           void *user_data),
                                   void *user_data);

u16_t *bt_mesh_model_find_group(const struct bt_mesh_model **mod, u16_t addr);
const u8_t **bt_mesh_model_find_uuid(const struct bt_mesh_model **mod, const u8_t *uuid);

void bt_mesh_model_foreach(void (*func)(const struct bt_mesh_model *mod,
                                        const struct bt_mesh_elem *elem,
                                        bool vnd, bool primary,
                                        void *user_data),
                           void *user_data);

s32_t bt_mesh_model_pub_period_get(const struct bt_mesh_model *mod);

void bt_mesh_comp_provision(u16_t addr);
void bt_mesh_comp_unprovision(void);

u16_t bt_mesh_primary_addr(void);

const struct bt_mesh_comp *bt_mesh_comp_get(void);

const struct bt_mesh_model *bt_mesh_model_get(bool vnd, u8_t elem_idx, u8_t mod_idx);

int bt_mesh_access_recv(struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf);
int bt_mesh_model_recv(struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf);

int bt_mesh_comp_register(const struct bt_mesh_comp *comp);
int bt_mesh_comp_store(void);
int bt_mesh_comp_read(struct net_buf_simple *buf, u8_t page);
u8_t bt_mesh_comp_parse_page(struct net_buf_simple *buf);

int bt_mesh_models_metadata_store(void);
int bt_mesh_models_metadata_read(struct net_buf_simple *buf, size_t offset);

void bt_mesh_comp_data_pending_clear(void);
void bt_mesh_comp_data_clear(void);
int bt_mesh_comp_data_get_page(struct net_buf_simple *buf, size_t page, size_t offset);

void bt_mesh_model_pending_store(void);
void bt_mesh_model_bind_store(const struct bt_mesh_model *mod);
void bt_mesh_model_sub_store(const struct bt_mesh_model *mod);
void bt_mesh_model_pub_store(const struct bt_mesh_model *mod);
void bt_mesh_model_settings_commit(void);

/** @brief Register a callback function hook for mesh model messages.
 *
 * Register a callback function to act as a hook for receiving mesh model layer messages
 * directly to the application without having instantiated the relevant models.
 *
 * @param cb A pointer to the callback function.
 */
void bt_mesh_msg_cb_set(void (*cb)(u32_t opcode, struct bt_mesh_msg_ctx *ctx,
                                   struct net_buf_simple *buf));

/** @brief Send a mesh model message.
 *
 * Send a mesh model layer message out into the mesh network without having instantiated
 * the relevant mesh models.
 *
 * @param ctx The Bluetooth Mesh message context.
 * @param buf The message payload.
 * @param src_addr The source address of model
 * @param cb Callback function.
 * @param cb_data Callback data.
 *
 * @return 0 on success or negative error code on failure.
 */
int bt_mesh_access_send(struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf, u16_t src_addr,
                        const struct bt_mesh_send_cb *cb, void *cb_data);

/** @brief Initialize the Access layer.
 *
 * Initialize the delayable message mechanism if it has been enabled.
 */
void bt_mesh_access_init(void);

/** @brief Suspend the Access layer.
 */
void bt_mesh_access_suspend(void);

/** @brief Reset the Access layer.
 */
void bt_mesh_access_reset(void);
