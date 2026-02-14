/**
 * @file
 * @defgroup bt_mesh_lvl_cli Generic Level Client model
 * @{
 * @brief API for the Generic Level Client model.
 */
#ifndef BT_MESH_GEN_LVL_CLI_H__
#define BT_MESH_GEN_LVL_CLI_H__
#include "system/includes.h"
#include "api/sig_mesh_api.h"
#include "gen_lvl.h"
#include "model_api.h"


#define BT_MESH_LVL_MSG_LEN_GET 			0
#define BT_MESH_LVL_MSG_MINLEN_SET 			3
#define BT_MESH_LVL_MSG_MAXLEN_SET 			5
#define BT_MESH_LVL_MSG_MINLEN_STATUS 		2
#define BT_MESH_LVL_MSG_MAXLEN_STATUS 		5
#define BT_MESH_LVL_MSG_MINLEN_DELTA_SET 	5
#define BT_MESH_LVL_MSG_MAXLEN_DELTA_SET 	7
#define BT_MESH_LVL_MSG_MINLEN_MOVE_SET 	3
#define BT_MESH_LVL_MSG_MAXLEN_MOVE_SET 	5
/** @endcond */

/**
 * Generic Level Client instance.
 *
 * Should be initialized with @ref BT_MESH_LVL_CLI_INIT.
 */
struct bt_mesh_lvl_cli {
    /** Model entry. */
    struct bt_mesh_model *model;
    /** Publish parameters. */
    struct bt_mesh_model_pub pub;
    /* Publication buffer */
    struct net_buf_simple pub_buf;
    /* Publication data */
    uint8_t pub_data[BT_MESH_MODEL_BUF_LEN(
                         BT_MESH_LVL_OP_DELTA_SET, BT_MESH_LVL_MSG_MAXLEN_DELTA_SET)];
    /** Acknowledged message tracking. */
    struct bt_mesh_msg_ack_ctx ack_ctx;
    /** Current transaction ID. */
    uint8_t tid;

    /** @brief Level status message handler.
     *
     * @param[in] cli Client that received the status message.
     * @param[in] ctx Context of the message.
     * @param[in] status Generic level status contained in the message.
     */
    void (*const status_handler)(struct bt_mesh_lvl_cli *cli,
                                 struct bt_mesh_msg_ctx *ctx,
                                 const struct bt_mesh_lvl_status *status);
};
extern const struct bt_mesh_model_op _bt_mesh_lvl_cli_op[];
#endif /* BT_MESH_GEN_LVL_CLI_H__ */