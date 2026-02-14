#ifndef __MESH_CONFIG_H__
#define __MESH_CONFIG_H__

#include "model_api.h"
/*******************************************************************/
/*
 *-------------------   SIG Mesh config
 */

/* Log debug config */
#define MESH_CODE_LOG_DEBUG_EN                  1
#define CONFIG_BT_DEBUG                         1
#define MESH_ADAPTATION_OPTIMIZE                1

/* Buf Replace Config */
#define CONFIG_BUF_REPLACE_EN					0

/* Compile config */
#define ADAPTATION_COMPILE_DEBUG                0
#define MESH_RAM_AND_CODE_MAP_DETAIL            0
// #define CONFIG_ATOMIC_OPERATIONS_BUILTIN        1
#define CMD_DIRECT_TO_BTCTRLER_TASK_EN          1

/* project config */
#if (CONFIG_MESH_MODEL == SIG_MESH_GENERIC_ONOFF_SERVER) || (CONFIG_MESH_MODEL == SIG_MESH_GENERIC_ONOFF_CLIENT)
#define CONFIG_BT_MESH_LOW_POWER                0
#define CONFIG_BT_MESH_FRIEND                   0
#define PACKET_LOSS_RATE_TEST                   0
#endif
#if (CONFIG_MESH_MODEL == SIG_MESH_VENDOR_SERVER) || (CONFIG_MESH_MODEL == SIG_MESH_VENDOR_CLIENT)
#define CONFIG_BT_MESH_LOW_POWER                0
#define CONFIG_BT_MESH_FRIEND                   0
#endif
#if (CONFIG_MESH_MODEL == SIG_MESH_ALIGENIE_SOCKET) || (CONFIG_MESH_MODEL == SIG_MESH_ALIGENIE_LIGHT) || (CONFIG_MESH_MODEL == SIG_MESH_ALIGENIE_FAN)
#define CONFIG_BT_MESH_LOW_POWER                1
#define CONFIG_BT_MESH_FRIEND                   1
#endif
#if (CONFIG_MESH_MODEL == SIG_MESH_LIGHT_LIGHTNESS_SERVER)
#define CONFIG_BT_MESH_LOW_POWER                1
#define CONFIG_BT_MESH_FRIEND                   1
#define CONFIG_BT_MESH_GEN_ONOFF_SRV            1
#define CONFIG_BT_MESH_GEN_LEVEL_SRV            1
#define CONFIG_BT_MESH_GEN_ONOFF_CLI            1
#define CONFIG_BT_MESH_GEN_POWER_ONOFF_SRV      1
#define CONFIG_BT_MESH_GEN_LIGHTNESS_SRV        1
#endif
#if (CONFIG_MESH_MODEL == SIG_MESH_TUYA_LIGHT)
#define CONFIG_BT_MESH_LOW_POWER                1
#define CONFIG_BT_MESH_FRIEND                   1
#endif
#if (CONFIG_MESH_MODEL == SIG_MESH_TENCENT_MESH)
#define CONFIG_BT_MESH_LOW_POWER                1
#define CONFIG_BT_MESH_FRIEND                   1
#endif
#if (CONFIG_MESH_MODEL == SIG_MESH_PROVISIONEE)
#define CONFIG_BT_MESH_LOW_POWER                0
#define CONFIG_BT_MESH_FRIEND                   0
#endif
#if (CONFIG_MESH_MODEL == SIG_MESH_PROVISIONER)
#define CONFIG_BT_MESH_LOW_POWER                0
#define CONFIG_BT_MESH_FRIEND                   0
#define CONFIG_BT_MESH_PROVISIONER              1
#define CONFIG_BT_MESH_CDB                      1
#define CONFIG_BT_MESH_CDB_NODE_COUNT           3
#define CONFIG_BT_MESH_CDB_SUBNET_COUNT         3
#define CONFIG_BT_MESH_CDB_APP_KEY_COUNT        3
#endif
#if (CONFIG_MESH_MODEL == SIG_MESH_OCCUPACY_SENSOR_NLC)
#define CONFIG_BT_MESH_NLC_PERF_CONF            1
#define CONFIG_BT_MESH_COMP_PAGE_2              1
#define CONFIG_BT_MESH_SENSOR_SRV               1
#define CONFIG_BT_MESH_LOW_POWER                0
#define CONFIG_BT_MESH_FRIEND                   0
#endif
#if (CONFIG_MESH_MODEL == SIG_MESH_BASIC_LIGHTNESS_CTRL_NLC)
#define CONFIG_BT_MESH_NLC_PERF_CONF            1
#define CONFIG_BT_MESH_COMP_PAGE_2              1
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV           1
#define CONFIG_BT_MESH_SENSOR_SRV               1
#define CONFIG_BT_MESH_DTT_SRV                  1
#define CONFIG_BT_MESH_PONOFF_SRV               1
#define CONFIG_BT_MESH_SCENE_SRV                1
#define CONFIG_BT_MESH_GEN_ONOFF_SRV            1
#define CONFIG_BT_MESH_GEN_LEVEL_SRV            1
#define CONFIG_BT_MESH_GEN_POWER_ONOFF_SRV      1
#define CONFIG_BT_MESH_GEN_LIGHTNESS_SRV        1
#define CONFIG_BT_MESH_LOW_POWER                0
#define CONFIG_BT_MESH_FRIEND                   0
#endif
#if (CONFIG_MESH_MODEL == SIG_MESH_ENERGY_MONITOR_NLC)
#define CONFIG_BT_MESH_NLC_PERF_CONF            1
#define CONFIG_BT_MESH_COMP_PAGE_2              1
#define CONFIG_BT_MESH_SENSOR_SRV               1
#define CONFIG_BT_MESH_LOW_POWER                0
#define CONFIG_BT_MESH_FRIEND                   0
#endif
#if (CONFIG_MESH_MODEL == SIG_MESH_BASIC_SCENE_SELECTOR_NLC)
#define CONFIG_BT_MESH_NLC_PERF_CONF            1
#define CONFIG_BT_MESH_COMP_PAGE_2              1
#define CONFIG_BT_MESH_SCENE_CLI                1
#define CONFIG_BT_MESH_LOW_POWER                0
#define CONFIG_BT_MESH_FRIEND                   0
#endif
#if (CONFIG_MESH_MODEL == SIG_MESH_DIMMING_CONTROL_NLC)
#define CONFIG_BT_MESH_NLC_PERF_CONF            1
#define CONFIG_BT_MESH_COMP_PAGE_2              1
#define CONFIG_BT_MESH_GEN_LVL_CLI              1
#define CONFIG_BT_MESH_LOW_POWER                0
#define CONFIG_BT_MESH_FRIEND                   0
#endif
#if (CONFIG_MESH_MODEL == SIG_MESH_SENSOR_OBSEVER_NLC)
#define CONFIG_BT_MESH_NLC_PERF_CONF            1
#define CONFIG_BT_MESH_SENSOR_CLI               1
#define CONFIG_BT_MESH_LOW_POWER                0
#define CONFIG_BT_MESH_FRIEND                   0
#endif
#if (CONFIG_MESH_MODEL == SIG_MESH_DFU_DISTRIBUTOR_DEMO)
#define CONFIG_BT_MESH_DFU_DIST                 1
#define CONFIG_BT_MESH_BLOB_CLI                 1
#define CONFIG_BT_MESH_BLOB_CLI_BLOCK_RETRIES   15
#define CONFIG_BT_MESH_DFD_SRV                  1
#define CONFIG_BT_MESH_DFU_SLOTS                1
#define CONFIG_BT_MESH_DFU_SLOT_CNT             1
#define CONFIG_BT_MESH_DFD_SRV_TARGETS_MAX      11
#define CONFIG_BT_MESH_DFD_SRV_SLOT_MAX_SIZE    524288
#define CONFIG_BT_MESH_DFD_SRV_SLOT_SPACE       524288
#define CONFIG_BT_MESH_LOW_POWER                0
#define CONFIG_BT_MESH_FRIEND                   0
#endif
#if (CONFIG_MESH_MODEL == SIG_MESH_DFU_TARGET_DEMO)
#define CONFIG_BT_MESH_DFU_TARGET               1
#define CONFIG_BT_MESH_BLOB_SRV                 1
#define CONFIG_BT_MESH_LOW_POWER                0
#define CONFIG_BT_MESH_FRIEND                   0
#endif

#if (CONFIG_MESH_MODEL == SIG_MESH_AMBIENT_LIGHT_SENSOR_NLC)
#define CONFIG_BT_MESH_NLC_PERF_CONF            1
#define CONFIG_BT_MESH_COMP_PAGE_2              1
#define CONFIG_BT_MESH_SENSOR_SRV               1
#define CONFIG_BT_MESH_LOW_POWER                0
#define CONFIG_BT_MESH_FRIEND                   0
#endif

/* Node features config */
#define CONFIG_BT_MESH_RELAY                    1
#define CONFIG_BT_MESH_PROXY                    1
#define CONFIG_BT_MESH_GATT_PROXY               1
#define CONFIG_BT_MESH_NODE_ID_TIMEOUT          10

/* LPN config */
#if (CONFIG_BT_MESH_LOW_POWER)
#define CONFIG_BT_MESH_LPN_ESTABLISHMENT        1
#define CONFIG_BT_MESH_LPN_AUTO                 1
#define CONFIG_BT_MESH_LPN_GROUPS               8
#define CONFIG_BT_MESH_LPN_AUTO_TIMEOUT         config_bt_mesh_lpn_auto_timeout // 15
#define CONFIG_BT_MESH_LPN_RETRY_TIMEOUT        config_bt_mesh_lpn_retry_timeout // 8
#define CONFIG_BT_MESH_LPN_SCAN_LATENCY         config_bt_mesh_lpn_scan_latency // 10
#define CONFIG_BT_MESH_LPN_MIN_QUEUE_SIZE       config_bt_mesh_lpn_min_queue_size // 1
#define CONFIG_BT_MESH_LPN_POLL_TIMEOUT         config_bt_mesh_lpn_poll_timeout // 300
#define CONFIG_BT_MESH_LPN_RECV_DELAY           config_bt_mesh_lpn_recv_delay // 100
#define CONFIG_BT_MESH_LPN_INIT_POLL_TIMEOUT    config_bt_mesh_lpn_init_poll_timeout // 300
#define CONFIG_BT_MESH_LPN_RSSI_FACTOR          config_bt_mesh_lpn_rssi_factor // 0
#define CONFIG_BT_MESH_LPN_RECV_WIN_FACTOR      config_bt_mesh_lpn_recv_win_factor // 0
#endif /* CONFIG_BT_MESH_LOW_POWER */

/* Net buffer config */
#define NET_BUF_TEST_EN                         0
#define NET_BUF_FREE_EN                         1
#define NET_BUF_USE_MALLOC                      0
#define CONFIG_NET_BUF_USER_DATA_SIZE 		    4
#if (NET_BUF_USE_MALLOC == 0)
#define CONFIG_BT_MESH_ADV_BUF_COUNT 		    4
#endif /* NET_BUF_USE_MALLOC */

/* Friend config */
#if (CONFIG_BT_MESH_FRIEND)
#if NET_BUF_USE_MALLOC
#define CONFIG_BT_MESH_FRIEND_QUEUE_SIZE        config_bt_mesh_friend_queue_size
#define CONFIG_BT_MESH_FRIEND_SUB_LIST_SIZE     config_bt_mesh_friend_sub_list_size
#define CONFIG_BT_MESH_FRIEND_LPN_COUNT         config_bt_mesh_friend_lpn_count
#else
#define CONFIG_BT_MESH_FRIEND_QUEUE_SIZE        16
#define CONFIG_BT_MESH_FRIEND_SUB_LIST_SIZE     3
#define CONFIG_BT_MESH_FRIEND_LPN_COUNT         2
#endif /* NET_BUF_USE_MALLOC */
#define CONFIG_BT_MESH_FRIEND_SEG_RX            1
#define CONFIG_BT_MESH_FRIEND_RECV_WIN          config_bt_mesh_friend_recv_win // 255
#define CONFIG_BT_MESH_FRIEND_ADV_LATENCY       2
#endif

/* Proxy config */
#define CONFIG_BT_MAX_CONN                      1
#define CONFIG_BT_MESH_PROXY_FILTER_SIZE        3
#define CONFIG_BT_MESH_PROXY_SOLICITATION       0
#define CONFIG_BT_MESH_PROXY_MSG_LEN            120

/* Net config */
#define CONFIG_BT_MESH_SUBNET_COUNT             2
#define CONFIG_BT_MESH_MSG_CACHE_SIZE 		    4
#define CONFIG_BT_MESH_IVU_DIVIDER              4

/* Transport config */
#define CONFIG_BT_MESH_TX_SEG_MAX 			    10
#define CONFIG_BT_MESH_RX_SEG_MAX               10
#define CONFIG_BT_MESH_TX_SEG_MSG_COUNT 	    5
#define CONFIG_BT_MESH_RX_SEG_MSG_COUNT 	    5
#define CONFIG_BT_MESH_RX_SDU_MAX 			    72

/* Element models config */
#define CONFIG_BT_MESH_CFG_CLI                  1
// #define CONFIG_BT_MESH_HEALTH_SRV               1
#define CONFIG_BT_MESH_APP_KEY_COUNT            2
#define CONFIG_BT_MESH_MODEL_KEY_COUNT          2
#define CONFIG_BT_MESH_MODEL_GROUP_COUNT        2
#define CONFIG_BT_MESH_CRPL                     32
#define CONFIG_BT_MESH_LABEL_COUNT              0
#define CONFIG_BT_MESH_DEFAULT_TTL              7

/* Provisioning config */
#define CONFIG_BT_MESH_PROV                     1
#define CONFIG_BT_MESH_PB_ADV                   1
#define CONFIG_BT_MESH_PB_GATT                  1
#define CONFIG_BT_MESH_UNPROV_BEACON_INT        1

/* Store config */
#define CONFIG_BT_SETTINGS                      1
#define CONFIG_BT_MESH_STORE_TIMEOUT            2
#define CONFIG_BT_MESH_SEQ_STORE_RATE 		    128
#define CONFIG_BT_MESH_RPL_STORE_TIMEOUT        600

/* Remote Provisioning config */
#define CONFIG_BT_MESH_RPR_AD_TYPES_MAX             2
#define CONFIG_BT_MESH_RPR_SRV_SCANNED_ITEMS_MAX    2
#define CONFIG_BT_MESH_RPR_SRV_AD_DATA_MAX          2
#define CONFIG_BT_MESH_RPR_AD_TYPES_MAX             2

#define CONFIG_BT_MESH_SOL_PDU_RPL_CLI_TIMEOUT      100

/* Conn config*/
#define CONFIG_BT_MESH_MAX_CONN                 1

/* Health cli config*/
// #define CONFIG_BT_MESH_HEALTH_CLI               1
#define CONFIG_BT_MESH_HEALTH_CLI_TIMEOUT       100

/* network config */
#define CONFIG_BT_MESH_NETWORK_TRANSMIT_COUNT       2
#define CONFIG_BT_MESH_NETWORK_TRANSMIT_INTERVAL    20
#define CONFIG_BT_MESH_RELAY_RETRANSMIT_COUNT       2
#define CONFIG_BT_MESH_RELAY_RETRANSMIT_INTERVAL    20

#define CONFIG_BT_MESH_RELAY_ENABLED        1
#define CONFIG_BT_MESH_BEACON_ENABLED       0
#define CONFIG_BT_MESH_GATT_PROXY_ENABLED   0
#define CONFIG_BT_MESH_FRIEND_ENABLED       0

/* pb-adv config */
#define CONFIG_BT_MESH_PB_ADV_RETRANS_TIMEOUT               100
#define CONFIG_BT_MESH_PB_ADV_TRANS_PDU_RETRANSMIT_COUNT    5
#define CONFIG_BT_MESH_PB_ADV_TRANS_ACK_RETRANSMIT_COUNT    5
#define CONFIG_BT_MESH_PB_ADV_LINK_CLOSE_RETRANSMIT_COUNT   5

#define CONFIG_BT_MESH_SOL_ADV_XMIT 1

/* dfu blob config */
#define CONFIG_BT_MESH_BLOB_IO_FLASH            1
#define CONFIG_BT_MESH_BLOB_REPORT_TIMEOUT      10
#define CONFIG_BT_MESH_BLOB_SRV_PULL_REQ_COUNT  4
#define CONFIG_BT_MESH_BLOB_SIZE_MAX            524288
#define CONFIG_BT_MESH_BLOB_BLOCK_SIZE_MIN      4096
#define CONFIG_BT_MESH_BLOB_BLOCK_SIZE_MAX      4096
#define CONFIG_BT_MESH_BLOB_CHUNK_COUNT_MAX     256
#define CONFIG_BT_MESH_DFU_METADATA             1
#define CONFIG_BT_MESH_DFU_METADATA_MAXLEN      32
#define CONFIG_BT_MESH_DFU_FWID_MAXLEN          8
#define CONFIG_BT_MESH_DFU_URI_MAXLEN           32

/* access dalayable config*/
#if (CONFIG_BT_MESH_ACCESS_DELAYABLE_MSG)
#define CONFIG_BT_MESH_ACCESS_DELAYABLE_MSG_CHUNK_SIZE  10
#define CONFIG_BT_MESH_ACCESS_DELAYABLE_MSG_CHUNK_COUNT 1
#define CONFIG_BT_MESH_ACCESS_DELAYABLE_MSG_COUNT       2
#endif

#define CONFIG_BT_MESH_OD_PRIV_PROXY_CLI_TIMEOUT        100

#define CONFIG_BT_MESH_OP_AGG_CLI_TIMEOUT               100

//#define CONFIG_BT_MESH_SEG_BUFS                         6 //not used now

/* NLC config */
#if (CONFIG_BT_MESH_SENSOR_CLI) || (CONFIG_BT_MESH_SENSOR_SRV)
#define CONFIG_BT_MESH_SENSOR_SRV_SENSORS_MAX   5
#define CONFIG_BT_MESH_SENSOR_SRV_SETTINGS_MAX  2
#define CONFIG_BT_MESH_SENSOR_CHANNELS_MAX      1
#endif

#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_TIME_MANUAL 5
#define CONFIG_BT_MESH_LIGHTNESS_SRV 1
#define CONFIG_BT_MESH_LIGHTNESS_ACTUAL 1
#define CONFIG_BT_MESH_LIGHT_CTRL_REG 1
#define CONFIG_BT_MESH_LIGHT_CTRL_REG_SPEC 0
#define CONFIG_BT_MESH_LIGHT_CTRL_REG_SPEC_INTERVAL 100
#define CONFIG_BT_MESH_LIGHT_CTRL_AMB_LIGHT_LEVEL_TIMEOUT 300
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_REG 0
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_REG_KIU 250
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_REG_KID 25
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_REG_KPU 80
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_REG_KPD 80
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_REG_ACCURACY 2
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_REG_LUX_ON 500
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_REG_LUX_PROLONG 80
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_REG_LUX_STANDBY 0
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_OCCUPANCY_DELAY 0
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_TIME_FADE_ON 500
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_TIME_FADE_PROLONG 5000
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_TIME_FADE_STANDBY_AUTO 5000
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_TIME_FADE_STANDBY_MANUAL 500
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_TIME_ON 3
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_TIME_PROLONG 3
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_TIME_MANUAL 5
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_RESUME_DELAY 30
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_LVL_ON 65535
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_LVL_PROLONG 10000
#define CONFIG_BT_MESH_LIGHT_CTRL_SRV_LVL_STANDBY 0


/* composition data config */
#define CONFIG_BT_MESH_COMP_PAGE_1              0

/* model extensions config */
#define CONFIG_BT_MESH_MODEL_EXTENSIONS         0

/* Crypto config*/
#define CONFIG_BT_MESH_ECDH_P256_HMAC_SHA256_AES_CCM    0
#define CONFIG_BT_MESH_ECDH_P256_CMAC_AES128_AES_CCM    1


/* Sar tx config*/
/* Interval between sending two consecutive segments */
#define CONFIG_BT_MESH_SAR_TX_SEG_INT_STEP                          0x05 // range 0x00 ~ 0x0F  default 0x05
/* Maximum number of retransmissions to unicast address */
#define CONFIG_BT_MESH_SAR_TX_UNICAST_RETRANS_COUNT                 0x05 // range 0x00 ~ 0x0F  default 0x02
/* Maximum number of retransmissions without progress to a unicast address */
#define CONFIG_BT_MESH_SAR_TX_UNICAST_RETRANS_WITHOUT_PROG_COUNT    0x03 // range 0x00 ~ 0x0F  default 0x02
/* Retransmissions interval step of missing segments to unicast address */
#define CONFIG_BT_MESH_SAR_TX_UNICAST_RETRANS_INT_STEP              0x07 // range 0x00 ~ 0x0F  default 0x07
/* Retransmissions interval increment of missing segments to unicast address */
#define CONFIG_BT_MESH_SAR_TX_UNICAST_RETRANS_INT_INC               0x01 // range 0x00 ~ 0x0F  default 0x01
/* Total number of retransmissions to multicast address */
#define CONFIG_BT_MESH_SAR_TX_MULTICAST_RETRANS_COUNT               0x02 // range 0x00 ~ 0x0F  default 0x02
/* Interval between retransmissions to multicast address */
#define CONFIG_BT_MESH_SAR_TX_MULTICAST_RETRANS_INT                 0x09 // range 0x00 ~ 0x0F  default 0x09

/* Sar rx config*/
/* Acknowledgments retransmission threshold */
#define CONFIG_BT_MESH_SAR_RX_SEG_THRESHOLD     0x1f //range 0x00 0x1F default 0x03
/* Acknowledgment delay increment */
#define CONFIG_BT_MESH_SAR_RX_ACK_DELAY_INC     0x07 //range 0x00 0x07 default 0x01
/* Discard timeout for reception of a segmented message */
#define CONFIG_BT_MESH_SAR_RX_DISCARD_TIMEOUT   0x01 //range 0x00 0x0F default 0x01
/* Segments reception interval step */
#define CONFIG_BT_MESH_SAR_RX_SEG_INT_STEP      0x0f //range 0x00 0x0F default 0x05
/* Total number of acknowledgment message retransmission */
#define CONFIG_BT_MESH_SAR_RX_ACK_RETRANS_COUNT 0x00 //range 0x00 0x03 default 0x00

#define CONFIG_BT_MESH_COMP_PST_BUF_SIZE        600

#define CONFIG_BT_MESH_CFG_CLI_TIMEOUT          6000

/* Adv ext config */
#define CONFIG_BT_MESH_ADV_EXT_FRIEND_SEPARATE          0
#define CONFIG_BT_MESH_ADV_EXT_GATT_SEPARATE            0
#define CONFIG_BT_MESH_ADV_EXT_RELAY_USING_MAIN_ADV_SET 0

#define CONFIG_BT_EXT_ADV_MAX_ADV_SET                   2

/* Keys config */
#define CONFIG_BT_MESH_USES_TINYCRYPT           1
#define CONFIG_BT_MESH_USES_MBEDTLS_PSA         0
#define CONFIG_BT_MESH_USES_TFM_PSA             0

/* Gatt config */
#define CONFIG_BT_MESH_GATT_SERVER              1

/* Settings config */
#define CONFIG_BT_MESH_SETTINGS_WORKQ           0

#define CONFIG_BT_MESH_OD_PRIV_PROXY_SRV        0
#define CONFIG_BT_MESH_PROXY_SRPL_SIZE          1

/*******************************************************************/
/*
 *-------------------   SIG Mesh features
 */

#define BT_MESH_FEAT_RELAY                  BIT(0)
#define BT_MESH_FEAT_PROXY                  BIT(1)
#define BT_MESH_FEAT_FRIEND                 BIT(2)
#define BT_MESH_FEAT_LOW_POWER              BIT(3)

#define BT_MESH_FEATURES_GET(x)             (!!(BT_MESH_FEAT_SUPPORTED & x))

#define BT_MESH_FEATURES_IS_SUPPORT(x)              (config_bt_mesh_features & (x))

#define BT_MESH_FEATURES_IS_SUPPORT_OPTIMIZE(x)     if (BT_MESH_FEATURES_IS_SUPPORT(x) == 0x0) return


/*******************************************************************/
/*
 *-------------------   APP config
 */

/**
 * @brief Config current node features(Relay/Proxy/Friend/Low Power)
 */
/*-----------------------------------------------------------*/
extern const int config_bt_mesh_features;

/**
 * @brief Config adv bearer hardware param when node send messages
 */
/*-----------------------------------------------------------*/
extern const u16 config_bt_mesh_node_msg_adv_interval;
extern const u16 config_bt_mesh_node_msg_adv_duration;

/**
 * @brief Config proxy connectable adv hardware param
 */
/*-----------------------------------------------------------*/
extern const u16 config_bt_mesh_proxy_unprovision_adv_interval;
extern const u16 config_bt_mesh_proxy_node_adv_interval;
extern const u16 config_bt_mesh_proxy_pre_node_adv_interval;

/**
 * @brief Config lpn node character
 */
/*-----------------------------------------------------------*/
extern const u8 config_bt_mesh_lpn_auto_timeout;
extern const u8 config_bt_mesh_lpn_retry_timeout;
extern const int config_bt_mesh_lpn_scan_latency;
extern const u32 config_bt_mesh_lpn_init_poll_timeout;
extern const u8 config_bt_mesh_lpn_powerup_add_sub_list;
extern const u8 config_bt_mesh_lpn_recv_delay;
extern const u32 config_bt_mesh_lpn_poll_timeout;
extern const u8 config_bt_mesh_lpn_rssi_factor;
extern const u8 config_bt_mesh_lpn_recv_win_factor;
extern const u8 config_bt_mesh_lpn_min_queue_size;

/**
 * @brief Config friend node character
 */
/*-----------------------------------------------------------*/
extern const u8 config_bt_mesh_friend_lpn_count;
extern const u8 config_bt_mesh_friend_recv_win;
extern const u8 config_bt_mesh_friend_sub_list_size;
extern const u8 config_bt_mesh_friend_queue_size;

/**
 * @brief Config cache buffer
 */
/*-----------------------------------------------------------*/
extern const u8 config_bt_mesh_adv_buf_count;

/**
 * @brief Config PB-ADV param
 */
/*-----------------------------------------------------------*/
extern const u16 config_bt_mesh_pb_adv_interval;
extern const u16 config_bt_mesh_pb_adv_duration;
extern const u32 config_bt_mesh_prov_retransmit_timeout;
extern const u8 config_bt_mesh_prov_transaction_timeout;
extern const u8 config_bt_mesh_prov_link_close_timeout;
extern const u8 config_bt_mesh_prov_protocol_timeout;

/**
 * @brief Config beacon param
 */
/*-----------------------------------------------------------*/
extern const u32 config_bt_mesh_unprov_beacon_interval;
extern const u16 config_bt_mesh_secure_beacon_interval;

#endif /* __MESH_CONFIG_H__ */
