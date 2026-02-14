#ifndef __ADAPTATION_H__
#define __ADAPTATION_H__

#include <stddef.h>
#include "printf.h"
#include "api/basic_depend.h"
#include "generic/list.h"
#include "generic/jiffies.h"
#include "misc/byteorder.h"
#include "misc/slist.h"
// #include "kernel/atomic_h.h"
#include "api/sig_mesh_api.h"

/*******************************************************************/
/*
 *-------------------  common adapter
 */
//< base operation
#define min(a, b)       MIN(a, b)
#define max(a, b)       MAX(a, b)
#define CONTAINER_OF    container_of
#undef offsetof
#define offsetof        list_offsetof
#define _STRINGIFY(x)   #x
#define CHECKIF(expr)   if (expr)
#define snprintk(...)   snprintf(__VA_ARGS__)
#define POPCOUNT(x)     __builtin_popcount(x)
#define ARG_UNUSED(x) 	(void)(x)

#define ___in_section(a, b, c) \
	__attribute__((section("." _STRINGIFY(a)			\
				"." _STRINGIFY(b)			\
				"." _STRINGIFY(c))))
#define __in_section(a, b, c) ___in_section(a, b, c)
/* Unaligned access */
#define UNALIGNED_GET(p)						\
__extension__ ({							\
	struct  __attribute__((__packed__)) {				\
		__typeof__(*(p)) __v;					\
	} *__p = (__typeof__(__p)) (p);					\
	__p->__v;							\
})

#ifndef __fallthrough
#if __GNUC__ >= 7
#define __fallthrough        __attribute__((fallthrough))
#else
#define __fallthrough
#endif	/* __GNUC__ >= 7 */
#endif

// #define __must_check __attribute__((warn_unused_result))
/* concatenate the values of the arguments into one */
#define _DO_CONCAT(x, y) x ## y
#define _CONCAT(x, y) _DO_CONCAT(x, y)

#define popcount(x) __builtin_popcount(x)

#define CODE_UNREACHABLE return 0

#define ALWAYS_INLINE   inline

#define __noinit

#define __noasan /**/

/**
 * @brief Value of @p x rounded up to the next multiple of @p align.
 */
#define ROUND_UP(x, align)                                   \
	((((unsigned long)(x) + ((unsigned long)(align) - 1)) / \
	  (unsigned long)(align)) * (unsigned long)(align))

static inline int irq_lock(void)
{
    CPU_CRITICAL_ENTER();

    return 0;
}

static inline void irq_unlock(int key)
{
    CPU_CRITICAL_EXIT();
}

/**
 *
 * @brief find most significant bit set in a 32-bit word
 *
 * This routine finds the first bit set starting from the most significant bit
 * in the argument passed in and returns the index of that bit.  Bits are
 * numbered starting at 1 from the least significant bit.  A return value of
 * zero indicates that the value passed is zero.
 *
 * @return most significant bit set, 0 if @a op is 0
 */

static ALWAYS_INLINE unsigned int find_msb_set(u32_t op)
{
    if (!op) {
        return 0;
    }
    return 32 - __builtin_clz(op);
}

/**
 *
 * @brief find least significant bit set in a 32-bit word
 *
 * This routine finds the first bit set starting from the least significant bit
 * in the argument passed in and returns the index of that bit.  Bits are
 * numbered starting at 1 from the least significant bit.  A return value of
 * zero indicates that the value passed is zero.
 *
 * @return least significant bit set, 0 if @a op is 0
 */

static ALWAYS_INLINE unsigned int find_lsb_set(u32_t op)
{
#if 0
    return __builtin_ffs(op);

#else
    /*
     * Toolchain does not have __builtin_ffs().
     * Need to do this manually.
     */
    int bit;

    if (op == 0) {
        return 0;
    }

    for (bit = 0; bit < 32; bit++) {
        if ((op & (1 << bit)) != 0) {
            return (bit + 1);
        }
    }

    /*
     * This should never happen but we need to keep
     * compiler happy.
     */
    return 0;
#endif /* CONFIG_TOOLCHAIN_HAS_BUILTIN_FFS */
}

/*******************************************************************/
/*
 *-------------------   k_work_delayable
 */

typedef u32_t k_timeout_t;

#if 0
/**
 * @brief Kernel timeout type
 *
 * Timeout arguments presented to kernel APIs are stored in this
 * opaque type, which is capable of representing times in various
 * formats and units.  It should be constructed from application data
 * using one of the macros defined for this purpose (e.g. `K_MSEC()`,
 * `K_TIMEOUT_ABS_TICKS()`, etc...), or be one of the two constants
 * K_NO_WAIT or K_FOREVER.  Applications should not inspect the
 * internal data once constructed.  Timeout values may be compared for
 * equality with the `K_TIMEOUT_EQ()` macro.
 */
typedef struct {
    k_ticks_t ticks;
} k_timeout_t;
#endif

/** @brief System-wide macro to denote "forever" in milliseconds
 *
 *  Usage of this macro is limited to APIs that want to expose a timeout value
 *  that can optionally be unlimited, or "forever".
 *  This macro can not be fed into kernel functions or macros directly. Use
 *  @ref SYS_TIMEOUT_MS instead.
 */
#define SYS_FOREVER_MS (-1)

#define K_FOREVER       (-1)

/**
 * @brief Generate null timeout delay.
 *
 * This macro generates a timeout delay that that instructs a kernel API
 * not to wait if the requested operation cannot be performed immediately.
 *
 * @return Timeout delay value.
 */
#define K_NO_WAIT 0

/**
 * @brief Generate timeout delay from milliseconds.
 *
 * This macro generates a timeout delay that that instructs a kernel API
 * to wait up to @a ms milliseconds to perform the requested operation.
 *
 * @param ms Duration in milliseconds.
 *
 * @return Timeout delay value.
 */
#define K_MSEC(ms)     (ms)

/**
 * @brief Generate timeout delay from seconds.
 *
 * This macro generates a timeout delay that that instructs a kernel API
 * to wait up to @a s seconds to perform the requested operation.
 *
 * @param s Duration in seconds.
 *
 * @return Timeout delay value.
 */
#define K_SECONDS(s)   K_MSEC((s) * MSEC_PER_SEC)

/**
 * @brief Generate timeout delay from minutes.
 *
 * This macro generates a timeout delay that that instructs a kernel API
 * to wait up to @a m minutes to perform the requested operation.
 *
 * @param m Duration in minutes.
 *
 * @return Timeout delay value.
 */
#define K_MINUTES(m)   K_SECONDS((m) * 60)

/**
 * @brief Generate timeout delay from hours.
 *
 * This macro generates a timeout delay that that instructs a kernel API
 * to wait up to @a h hours to perform the requested operation.
 *
 * @param h Duration in hours.
 *
 * @return Timeout delay value.
 */
#define K_HOURS(h)     K_MINUTES((h) * 60)

u32 k_uptime_get(void);
u32 k_uptime_get_32(void);
int64_t k_uptime_delta(int64_t *reftime);
uint32_t k_ticks_to_ms_ceil32(uint32_t t);
u32 k_work_delayable_remaining_get(struct k_work_delayable *timer);
int64_t k_uptime_delta(int64_t *reftime);
void k_work_schedule(struct k_work_delayable *timer, u32 timeout);
void k_work_cancel_delayable(struct k_work_delayable *timer);
void k_work_submit(struct k_work *work);
void k_work_init_delayable(struct k_work_delayable *timer, void *callback);
void net_buf_simple_clone(const struct net_buf_simple *original, struct net_buf_simple *clone);
bool k_work_delayable_is_pending(const struct k_work_delayable *dwork);
void k_work_init(struct k_work *work, k_work_handler_t handler);
int k_work_reschedule(struct k_work_delayable *dwork, u32_t delay);

static inline struct k_work_delayable *k_work_delayable_from_work(struct k_work *work)
{
    return CONTAINER_OF(work, struct k_work_delayable, work);
}
static inline void *net_buf_pull_mem(struct net_buf *buf, size_t len)
{
    //for compiler, not used now.
}

/*******************************************************************/
/*
 *-------------------   Ble core
 */
/** @brief Bluetooth UUID types */
enum {
    BT_UUID_TYPE_16,
    BT_UUID_TYPE_32,
    BT_UUID_TYPE_128,
};

/** @brief This is a 'tentative' type and should be used as a pointer only */
struct bt_uuid {
    u8_t type;
};

struct bt_uuid_16 {
    struct bt_uuid uuid;
    u16_t val;
};

struct bt_uuid_32 {
    struct bt_uuid uuid;
    u32_t val;
};

struct bt_uuid_128 {
    struct bt_uuid uuid;
    u8_t val[16];
};

/** Bluetooth Device Address */
typedef struct {
    u8_t  val[6];
} bt_addr_t;

/** Bluetooth LE Device Address */
typedef struct {
    u8_t      type;
    bt_addr_t a;
} bt_addr_le_t;

/** @brief Initialize a 16-bit UUID.
 *
 *  @param value 16-bit UUID value in host endianness.
 */
#define BT_UUID_INIT_16(value)		\
{					\
	.uuid = { BT_UUID_TYPE_16 },	\
	.val = (value),			\
}

/** @brief Initialize a 32-bit UUID.
 *
 *  @param value 32-bit UUID value in host endianness.
 */
#define BT_UUID_INIT_32(value)		\
{					\
	.uuid = { BT_UUID_TYPE_32 },	\
	.val = (value),			\
}

/** @brief Initialize a 128-bit UUID.
 *
 *  @param value 128-bit UUID array values in little-endian format.
 *               Can be combined with @ref BT_UUID_128_ENCODE to initialize a
 *               UUID from the readable form of UUIDs.
 */
#define BT_UUID_INIT_128(value...)	\
{					\
	.uuid = { BT_UUID_TYPE_128 },	\
	.val = { value },		\
}

/*******************************************************************/
/*
 *-------------------   utils
 */
#define bt_hex      bt_hex_real

const char *bt_hex_real(const void *buf, size_t len);

const char *bt_uuid_str(const struct bt_uuid *uuid);

/*******************************************************************/
/*
 *-------------------   hci_core
 */
typedef void (*bt_dh_key_cb_t)(const u8_t key[32]);

struct bt_conn {
    u8_t            index;
    u16_t			handle;
    u16_t           mtu;
    atomic_tt		ref;
};

struct bt_conn_cb {
    /** @brief A new connection has been established.
     *
     *  This callback notifies the application of a new connection.
     *  In case the err parameter is non-zero it means that the
     *  connection establishment failed.
     *
     *  @param conn New connection object.
     *  @param err HCI error. Zero for success, non-zero otherwise.
     */
    void (*connected)(struct bt_conn *conn, u8_t err);

    /** @brief A connection has been disconnected.
     *
     *  This callback notifies the application that a connection
     *  has been disconnected.
     *
     *  @param conn Connection object.
     *  @param reason HCI reason for the disconnection.
     */
    void (*disconnected)(struct bt_conn *conn, u8_t reason);
};

struct bt_pub_key_cb {
    /** @brief Callback type for Public Key generation.
     *
     *  Used to notify of the local public key or that the local key is not
     *  available (either because of a failure to read it or because it is
     *  being regenerated).
     *
     *  @param key The local public key, or NULL in case of no key.
     */
    void (*func)(const u8_t key[64]);

    struct bt_pub_key_cb *_next;
};

const u8_t *bt_pub_key_get(void);

int bt_pub_key_gen(void);

int bt_dh_key_gen(const u8_t remote_pk[64], bt_dh_key_cb_t cb);

struct bt_conn *bt_conn_ref(struct bt_conn *conn);

void bt_conn_unref(struct bt_conn *conn);

void bt_conn_cb_register(struct bt_conn_cb *cb);

void hci_core_init(void);
const struct bt_conn_cb *bt_conn_get_callbacks(void);

/*******************************************************************/
/*
 *-------------------   adv_core
 */
/**
 * Convenience macro for specifying the default identity. This helps
 * make the code more readable, especially when only one identity is
 * supported.
 */
#define BT_ID_DEFAULT 0

/* Advertising types */
#define BT_LE_ADV_IND                           0x00
#define BT_LE_ADV_DIRECT_IND                    0x01
#define BT_LE_ADV_SCAN_IND                      0x02
#define BT_LE_ADV_NONCONN_IND                   0x03
#define BT_LE_ADV_DIRECT_IND_LOW_DUTY           0x04
/* Needed in advertising reports when getting info about */
#define BT_LE_ADV_SCAN_RSP                      0x04

#define BT_DATA_URI                             0x24 /* URI */
#define BT_DATA_MESH_PROV                       0x29 /* Mesh Provisioning PDU */
#define BT_DATA_MESH_MESSAGE                    0x2a /* Mesh Networking PDU */
#define BT_DATA_MESH_BEACON                     0x2b /* Mesh Beacon */

#define BT_HCI_ADV_NONCONN_IND                  0x03

/** Advertising PDU types */
enum {
    /** Scannable and connectable advertising. */
    BT_GAP_ADV_TYPE_ADV_IND               = 0x00,
    /** Directed connectable advertising. */
    BT_GAP_ADV_TYPE_ADV_DIRECT_IND        = 0x01,
    /** Non-connectable and scannable advertising. */
    BT_GAP_ADV_TYPE_ADV_SCAN_IND          = 0x02,
    /** Non-connectable and non-scannable advertising. */
    BT_GAP_ADV_TYPE_ADV_NONCONN_IND       = 0x03,
    /** Additional advertising data requested by an active scanner. */
    BT_GAP_ADV_TYPE_SCAN_RSP              = 0x04,
    /** Extended advertising, see advertising properties. */
    BT_GAP_ADV_TYPE_EXT_ADV               = 0x05,
};

/** LE advertisement and scan response packet information */
struct bt_le_scan_recv_info {
    /**
     * @brief Advertiser LE address and type.
     *
     * If advertiser is anonymous then this address will be
     * @ref BT_ADDR_LE_ANY.
     */
    const bt_addr_le_t *addr;

    /** Advertising Set Identifier. */
    u8_t sid;

    /** Strength of advertiser signal. */
    s8_t rssi;

    /** Transmit power of the advertiser. */
    s8_t tx_power;

    /**
     * @brief Advertising packet type.
     *
     * Uses the BT_GAP_ADV_TYPE_* value.
     *
     * May indicate that this is a scan response if the type is
     * @ref BT_GAP_ADV_TYPE_SCAN_RSP.
     */
    u8_t adv_type;

    /**
     * @brief Advertising packet properties bitfield.
     *
     * Uses the BT_GAP_ADV_PROP_* values.
     * May indicate that this is a scan response if the value contains the
     * @ref BT_GAP_ADV_PROP_SCAN_RESPONSE bit.
     *
     */
    u16_t adv_props;

    /**
     * @brief Periodic advertising interval (N * 1.25 ms).
     *
     * If 0 there is no periodic advertising.
     */
    u16_t interval;

    /** Primary advertising channel PHY. */
    u8_t primary_phy;

    /** Secondary advertising channel PHY. */
    u8_t secondary_phy;
};

void bt_mesh_adv_init(void);


/*******************************************************************/
/*
 *-------------------   scan_core
 */
typedef void bt_le_scan_cb_t(const bt_addr_le_t *addr, s8_t rssi,
                             u8_t adv_type, struct net_buf_simple *buf);

int bt_le_scan_start(bt_le_scan_cb_t cb);

int bt_le_scan_stop(void);


/*******************************************************************/
/*
 *-------------------   gatt_core
 */
#define BT_DATA_FLAGS                   0x01 /* AD flags */
#define BT_DATA_UUID16_SOME             0x02 /**< 16-bit UUID, more available */
#define BT_DATA_UUID16_ALL              0x03 /* 16-bit UUID, all listed */
#define BT_DATA_UUID32_SOME             0x04 /**< 32-bit UUID, more available */
#define BT_DATA_UUID128_SOME            0x06 /**< 128-bit UUID, more available */
#define BT_DATA_UUID128_ALL             0x07 /**< 128-bit UUID, all listed */
#define BT_DATA_NAME_SHORTENED          0x08 /**< Shortened name */
#define BT_DATA_NAME_COMPLETE           0x09 /**< Complete name */
#define BT_DATA_SVC_DATA16              0x16 /* Service data, 16-bit UUID */
#define BT_LE_AD_GENERAL                0x02 /* General Discoverable */
#define BT_LE_AD_NO_BREDR               0x04 /* BR/EDR not supported */

/**
 *  @brief Mesh Provisioning Service UUID value
 */
#define BT_UUID_MESH_PROV_VAL               0x1827
/**
 *  @brief Mesh Proxy Service UUID value
 */
#define BT_UUID_MESH_PROXY_VAL              0x1828
/**
 *  @brief Proxy Solicitation UUID value
 */
#define BT_UUID_MESH_PROXY_SOLICITATION_VAL 0x1859
/**
 *  @brief Mesh Provisioning Data In UUID value
 */
#define BT_UUID_MESH_PROV_DATA_IN_VAL       0x2adb
/**
 *  @brief Mesh Provisioning Data Out UUID value
 */
#define BT_UUID_MESH_PROV_DATA_OUT_VAL      0x2adc
/**
 *  @brief Mesh Proxy Data In UUID value
 */
#define BT_UUID_MESH_PROXY_DATA_IN_VAL      0x2add
/**
 *  @brief Mesh Proxy Data Out UUID value
 */
#define BT_UUID_MESH_PROXY_DATA_OUT_VAL     0x2ade
/**
 *  @brief GATT Client Characteristic Configuration UUID value
 */
#define BT_UUID_GATT_CCC_VAL                0x2902

#define BT_HCI_ERR_REMOTE_USER_TERM_CONN        0x13
#define BT_GATT_ERR(_att_err)                   (-(_att_err))
#define BT_ATT_ERR_INVALID_ATTRIBUTE_LEN	    0x0d
#define BT_ATT_ERR_VALUE_NOT_ALLOWED            0x13
#define BT_GATT_CCC_NOTIFY			            0x0001

/** Description of different data types that can be encoded into
  * advertising data. Used to form arrays that are passed to the
  * bt_le_adv_start() function.
  */
struct bt_data {
    u8_t type;
    u8_t data_len;
    const u8_t *data;
};


/** @brief Notification complete result callback.
 *
 *  @param conn Connection object.
 *  @param user_data Data passed in by the user.
 */
typedef void (*bt_gatt_complete_func_t)(struct bt_conn *conn, void *user_data);

u8 *get_server_data_addr(void);

void bt_gatt_service_register(u32 uuid);

void bt_gatt_service_unregister(u32 uuid);

int bt_gatt_notify(struct bt_conn *conn, const void *data, u16_t len);

u16 bt_gatt_get_mtu(struct bt_conn *conn);

int bt_conn_disconnect(struct bt_conn *conn, u8 reason);

void proxy_gatt_init(void);

extern void mesh_set_gap_name(const u8 *name);

/** LE Advertising Parameters. */
struct bt_le_adv_param {
    /**
     * @brief Local identity.
     *
     * @note When extended advertising @kconfig{CONFIG_BT_EXT_ADV} is not
     *       enabled or not supported by the controller it is not possible
     *       to scan and advertise simultaneously using two different
     *       random addresses.
     */
    uint8_t  id;

    /**
     * @brief Advertising Set Identifier, valid range 0x00 - 0x0f.
     *
     * @note Requires @ref BT_LE_ADV_OPT_EXT_ADV
     **/
    uint8_t  sid;

    /**
     * @brief Secondary channel maximum skip count.
     *
     * Maximum advertising events the advertiser can skip before it must
     * send advertising data on the secondary advertising channel.
     *
     * @note Requires @ref BT_LE_ADV_OPT_EXT_ADV
     */
    uint8_t  secondary_max_skip;

    /** Bit-field of advertising options */
    uint32_t options;

    /** Minimum Advertising Interval (N * 0.625 milliseconds)
     * Minimum Advertising Interval shall be less than or equal to the
     * Maximum Advertising Interval. The Minimum Advertising Interval and
     * Maximum Advertising Interval should not be the same value (as stated
     * in Bluetooth Core Spec 5.2, section 7.8.5)
     * Range: 0x0020 to 0x4000
     */
    uint32_t interval_min;

    /** Maximum Advertising Interval (N * 0.625 milliseconds)
     * Minimum Advertising Interval shall be less than or equal to the
     * Maximum Advertising Interval. The Minimum Advertising Interval and
     * Maximum Advertising Interval should not be the same value (as stated
     * in Bluetooth Core Spec 5.2, section 7.8.5)
     * Range: 0x0020 to 0x4000
     */
    uint32_t interval_max;

    /**
     * @brief Directed advertising to peer
     *
     * When this parameter is set the advertiser will send directed
     * advertising to the remote device.
     *
     * The advertising type will either be high duty cycle, or low duty
     * cycle if the BT_LE_ADV_OPT_DIR_MODE_LOW_DUTY option is enabled.
     * When using @ref BT_LE_ADV_OPT_EXT_ADV then only low duty cycle is
     * allowed.
     *
     * In case of connectable high duty cycle if the connection could not
     * be established within the timeout the connected() callback will be
     * called with the status set to @ref BT_HCI_ERR_ADV_TIMEOUT.
     */
    const bt_addr_le_t *peer;
};

struct bt_le_ext_adv_start_param {
    /**
     * @brief Advertiser timeout (N * 10 ms).
     *
     * Application will be notified by the advertiser sent callback.
     * Set to zero for no timeout.
     *
     * When using high duty cycle directed connectable advertising then
     * this parameters must be set to a non-zero value less than or equal
     * to the maximum of @ref BT_GAP_ADV_HIGH_DUTY_CYCLE_MAX_TIMEOUT.
     *
     * If privacy @kconfig{CONFIG_BT_PRIVACY} is enabled then the timeout
     * must be less than @kconfig{CONFIG_BT_RPA_TIMEOUT}.
     */
    uint16_t timeout;
    /**
     * @brief Number of advertising events.
     *
     * Application will be notified by the advertiser sent callback.
     * Set to zero for no limit.
     */
    uint8_t  num_events;
};

#endif /* __ADAPTATION_H__ */
