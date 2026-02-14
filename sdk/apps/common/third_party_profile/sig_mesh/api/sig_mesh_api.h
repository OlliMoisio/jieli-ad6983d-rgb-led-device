#ifndef __SIG_MESH_API_H__
#define __SIG_MESH_API_H__

#include "api/basic_depend.h"
#include "api/mesh_config.h"
#include "kernel/atomic_h.h"
#include "net/buf.h"
#include "api/access.h"
#include "api/main.h"
#include "api/proxy.h"
#include "api/cfg.h"
#include "api/cfg_cli.h"
#include "api/cfg_srv.h"
#include "api/health_cli.h"
#include "api/health_srv.h"
#include "api/keys.h"
#include "api/cdb.h"
#include "api/sar_cfg.h"
#include "api/rpr.h"
#include "api/heartbeat.h"
#include "api/blob.h"
#include "api/blob_srv.h"
#include "api/blob_cli.h"
#include "api/dfd.h"
#include "api/dfd_srv.h"
#include "api/dfu.h"
#include "api/dfu_cli.h"
#include "api/dfu_metadata.h"
#include "api/dfu_srv.h"
#include "api/blob_io_flash.h"
#include "api/large_comp_data_cli.h"
#include "api/large_comp_data_srv.h"
#include "api/od_priv_proxy_cli.h"
#include "api/od_priv_proxy_srv.h"
#include "api/op_agg_cli.h"
#include "api/op_agg_srv.h"
#include "api/priv_beacon_cli.h"
#include "api/priv_beacon_srv.h"
#include "api/rpr_cli.h"
#include "api/rpr_srv.h"
#include "api/sar_cfg_cli.h"
#include "api/sar_cfg_srv.h"
#include "api/sol_pdu_rpl_cli.h"
#include "api/sol_pdu_rpl_srv.h"
#include "api/scene.h"
#include "api/scene_cli.h"
#include "api/statistic.h"
#include "system/debug.h"


/*******************************************************************/
/*
 *-------------------   common
 */
#define __noinit

#define ADV_SCAN_UNIT(_ms) ((_ms) * 8 / 5)

#define BT_MESH_ADDR_IS_UNICAST(addr) ((addr) && (addr) < 0x8000)
#define BT_MESH_ADDR_IS_GROUP(addr) ((addr) >= 0xc000 && (addr) <= 0xff00)
#define BT_MESH_ADDR_IS_VIRTUAL(addr) ((addr) >= 0x8000 && (addr) < 0xc000)
#define BT_MESH_ADDR_IS_RFU(addr) ((addr) >= 0xff00 && (addr) <= 0xfffb)

/** Shorthand macro for defining a model list directly in the element. */
#define BT_MESH_MODEL_LIST(...) ((struct bt_mesh_model[]){ __VA_ARGS__ })

//< error type
#define ENONE           0  /* Err None */
#define EPERM 1         /**< Not owner */
#define ENOENT 2        /**< No such file or directory */
#define ESRCH 3         /**< No such context */
#define EINTR 4         /**< Interrupted system call */
#define EIO 5           /**< I/O error */
#define ENXIO 6         /**< No such device or address */
#define E2BIG 7         /**< Arg list too long */
#define ENOEXEC 8       /**< Exec format error */
#define EBADF 9         /**< Bad file number */
#define ECHILD 10       /**< No children */
#define EAGAIN 11       /**< No more contexts */
#define ENOMEM 12       /**< Not enough core */
#define EACCES 13       /**< Permission denied */
#define EFAULT 14       /**< Bad address */
#define ENOTBLK 15      /**< Block device required */
#define EBUSY 16        /**< Mount device busy */
#define EEXIST 17       /**< File exists */
#define EXDEV 18        /**< Cross-device link */
#define ENODEV 19       /**< No such device */
#define ENOTDIR 20      /**< Not a directory */
#define EISDIR 21       /**< Is a directory */
#define EINVAL 22       /**< Invalid argument */
#define ENFILE 23       /**< File table overflow */
#define EMFILE 24       /**< Too many open files */
#define ENOTTY 25       /**< Not a typewriter */
#define ETXTBSY 26      /**< Text file busy */
#define EFBIG 27        /**< File too large */
#define ENOSPC 28       /**< No space left on device */
#define ESPIPE 29       /**< Illegal seek */
#define EROFS 30        /**< Read-only file system */
#define EMLINK 31       /**< Too many links */
#define EPIPE 32        /**< Broken pipe */
#define EDOM 33         /**< Argument too large */
#define ERANGE 34       /**< Result too large */
#define ENOMSG 35       /**< Unexpected message type */
#define EDEADLK 45      /**< Resource deadlock avoided */
#define ENOLCK 46       /**< No locks available */
#define ENOSTR 60       /**< STREAMS device required */
#define ENODATA 61      /**< Missing expected message data */
#define ETIME 62        /**< STREAMS timeout occurred */
#define ENOSR 63        /**< Insufficient memory */
#define EPROTO 71       /**< Generic STREAMS error */
#define EBADMSG 77      /**< Invalid STREAMS message */
#define ENOSYS 88       /**< Function not implemented */
#define ENOTEMPTY 90    /**< Directory not empty */
#define ENAMETOOLONG 91 /**< File name too long */
#define ELOOP 92        /**< Too many levels of symbolic links */
#define EOPNOTSUPP 95   /**< Operation not supported on socket */
#define EPFNOSUPPORT 96 /**< Protocol family not supported */
#define ECONNRESET 104   /**< Connection reset by peer */
#define ENOBUFS 105      /**< No buffer space available */
#define EAFNOSUPPORT 106 /**< Addr family not supported */
#define EPROTOTYPE 107   /**< Protocol wrong type for socket */
#define ENOTSOCK 108     /**< Socket operation on non-socket */
#define ENOPROTOOPT 109  /**< Protocol not available */
#define ESHUTDOWN 110    /**< Can't send after socket shutdown */
#define ECONNREFUSED 111 /**< Connection refused */
#define EADDRINUSE 112   /**< Address already in use */
#define ECONNABORTED 113 /**< Software caused connection abort */
#define ENETUNREACH 114  /**< Network is unreachable */
#define ENETDOWN 115     /**< Network is down */
#define ETIMEDOUT 116    /**< Connection timed out */
#define EHOSTDOWN 117    /**< Host is down */
#define EHOSTUNREACH 118 /**< No route to host */
#define EINPROGRESS 119  /**< Operation now in progress */
#define EALREADY 120     /**< Operation already in progress */
#define EDESTADDRREQ 121 /**< Destination address required */
#define EMSGSIZE 122        /**< Message size */
#define EPROTONOSUPPORT 123 /**< Protocol not supported */
#define ESOCKTNOSUPPORT 124 /**< Socket type not supported */
#define EADDRNOTAVAIL 125   /**< Can't assign requested address */
#define ENETRESET 126       /**< Network dropped connection on reset */
#define EISCONN 127         /**< Socket is already connected */
#define ENOTCONN 128        /**< Socket is not connected */
#define ETOOMANYREFS 129    /**< Too many references: can't splice */
#define ENOTSUP 134         /**< Unsupported value */
#define EILSEQ 138          /**< Illegal byte sequence */
#define EOVERFLOW 139       /**< Value overflow */
#define ECANCELED 140       /**< Operation canceled */
#define SL_ERROR_BSD_EADDRNOTAVAIL          (-99L)  /* Cannot assign requested address */
#define EADDRNOTAVAIL                       SL_ERROR_BSD_EADDRNOTAVAIL

/**
 * @brief Check for macro definition in compiler-visible expressions
 *
 * This trick was pioneered in Linux as the config_enabled() macro.
 * The madness has the effect of taking a macro value that may be
 * defined to "1" (e.g. CONFIG_MYFEATURE), or may not be defined at
 * all and turning it into a literal expression that can be used at
 * "runtime".  That is, it works similarly to
 * "defined(CONFIG_MYFEATURE)" does except that it is an expansion
 * that can exist in a standard expression and be seen by the compiler
 * and optimizer.  Thus much ifdef usage can be replaced with cleaner
 * expressions like:
 *
 *     if (IS_ENABLED(CONFIG_MYFEATURE))
 *             myfeature_enable();
 *
 * INTERNAL
 * First pass just to expand any existing macros, we need the macro
 * value to be e.g. a literal "1" at expansion time in the next macro,
 * not "(1)", etc...  Standard recursive expansion does not work.
 */
#define IS_ENABLED(config_macro) _IS_ENABLED1(config_macro)

/* Now stick on a "_XXXX" prefix, it will now be "_XXXX1" if config_macro
 * is "1", or just "_XXXX" if it's undefined.
 *   ENABLED:   _IS_ENABLED2(_XXXX1)
 *   DISABLED   _IS_ENABLED2(_XXXX)
 */
#define _IS_ENABLED1(config_macro) _IS_ENABLED2(_XXXX##config_macro)

/* Here's the core trick, we map "_XXXX1" to "_YYYY," (i.e. a string
 * with a trailing comma), so it has the effect of making this a
 * two-argument tuple to the preprocessor only in the case where the
 * value is defined to "1"
 *   ENABLED:    _YYYY,    <--- note comma!
 *   DISABLED:   _XXXX
 *   DISABLED:   _XXXX0
 */
#define _XXXX1 _YYYY,
#define _XXXX0

/* Then we append an extra argument to fool the gcc preprocessor into
 * accepting it as a varargs macro.
 *                         arg1   arg2  arg3
 *   ENABLED:   _IS_ENABLED3(_YYYY,    1,    0)
 *   DISABLED   _IS_ENABLED3(_XXXX 1,  0)
 *   DISABLED   _IS_ENABLED3(_XXXX0 1,  0)
 */
#define _IS_ENABLED2(one_or_two_args) _IS_ENABLED3(one_or_two_args true, false)

/* And our second argument is thus now cooked to be 1 in the case
 * where the value is defined to 1, and 0 if not:
 */
#define _IS_ENABLED3(ignore_this, val, ...) val

/*******************************************************************/
/*
 *-------------------   models
 */

/** @def NET_BUF_SIMPLE_DEFINE
 *  @brief Define a net_buf_simple stack variable.
 *
 *  This is a helper macro which is used to define a net_buf_simple object
 *  on the stack.
 *
 *  @param _name Name of the net_buf_simple object.
 *  @param _size Maximum data storage for the buffer.
 */
#define NET_BUF_SIMPLE_DEFINE(_name, _size)     \
	u8_t net_buf_data_##_name[_size];       \
	struct net_buf_simple _name = {         \
		.data   = net_buf_data_##_name, \
		.len    = 0,                    \
		.size   = _size,                \
		.__buf  = net_buf_data_##_name, \
	}


/**
 * @brief Add a unsigned char variable at the tail of the buffer.
 *
 * @param buf Target buffer head address.
 *
 * @param val The variable will set.
 *
 * @return The tail address of the buffer.
 */
u8 *buffer_add_u8_at_tail(void *buf, u8 val);

/**
 * @brief Add a little-endian unsigned short variable at the tail of the buffer.
 *
 * @param buf Target buffer head address.
 *
 * @param val The variable will set.
 */
void buffer_add_le16_at_tail(void *buf, u16 val);

/**
 * @brief Add a big-endian unsigned short variable at the tail of the buffer.
 *
 * @param buf Target buffer head address.
 *
 * @param val The variable will set.
 */
void buffer_add_be16_at_tail(void *buf, u16 val);

/**
 * @brief Add a little-endian unsigned long variable at the tail of the buffer.
 *
 * @param buf Target buffer head address.
 *
 * @param val The variable will set.
 */
void buffer_add_le32_at_tail(void *buf, u32 val);

/**
 * @brief Add a big-endian unsigned long variable at the tail of the buffer.
 *
 * @param buf Target buffer head address.
 *
 * @param val The variable will set.
 */
void buffer_add_be32_at_tail(void *buf, u32 val);

/**
 * @brief Get the unsigned char variable from the buffer head address.
 *
 * @param buf Target buffer head address.
 *
 * @return Target variable.
 */
u8 buffer_pull_u8_from_head(void *buf);

/**
 * @brief Get the little-endian unsigned short variable from the buffer head address.
 *
 * @param buf Target buffer head address.
 *
 * @return Target variable.
 */
u16 buffer_pull_le16_from_head(void *buf);

/**
 * @brief Get the big-endian unsigned short variable from the buffer head address.
 *
 * @param buf Target buffer head address.
 *
 * @return Target variable.
 */
u16 buffer_pull_be16_from_head(void *buf);

/**
 * @brief Get the little-endian unsigned long variable from the buffer head address.
 *
 * @param buf Target buffer head address.
 *
 * @return Target variable.
 */
u32 buffer_pull_le32_from_head(void *buf);

/**
 * @brief Get the big-endian unsigned long variable from the buffer head address.
 *
 * @param buf Target buffer head address.
 *
 * @return Target variable.
 */
u32 buffer_pull_be32_from_head(void *buf);

/**
 * @brief Memcpy a array at the tail of the buffer.
 *
 * @param buf Target buffer head address.
 *
 * @param mem The source memory address.
 *
 * @param len The copy length.
 *
 * @return The result of the process : 0 is succ.
 */
void *buffer_memcpy(void *buf, const void *mem, u32 len);

/**
 * @brief Memset at the tail of the buffer.
 *
 * @param buf Target buffer head address.
 *
 * @param mem The set value.
 *
 * @param len The set length.
 *
 * @return The result of the process : 0 is succ.
 */
void *buffer_memset(struct net_buf_simple *buf, u8 val, u32 len);

/**
 * @brief Init the opcode at the head of the buffer.
 *
 * @param opcode Big-endian opcode.
 *
 * @return Little-endian opcode.
 */
u32 buffer_head_init(u32 opcode);

/**
 * @brief Loading the node info from storage (such as flash and so on).
 */
void settings_load(void);

/**
 * @brief
 *
 * @param buf Input & output buffer.
 * @param len Buffer length.
 *
 * @return 0.
 */
int bt_rand(void *buf, size_t len);

/**
 * @brief
 *
 * @param init_cb The callback function.
 *
 * @return void.
 */

void mesh_setup(void (*init_cb)(void));

#endif /* __SIG_MESH_API_H__ */
