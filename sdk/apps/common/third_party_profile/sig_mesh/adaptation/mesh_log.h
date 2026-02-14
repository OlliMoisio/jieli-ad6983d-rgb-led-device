#ifndef __MESH_LOG_H__
#define __MESH_LOG_H__

#include "system/debug.h"

#ifndef LOG_TAG_CONST

#undef log_info
#undef log_debug
#undef log_warn
#undef log_error
#undef log_info_hexdump
#undef log_char

#ifdef LOG_INFO_ENABLE
#define log_info(format, ...)       log_print(__LOG_INFO, NULL, "[Info] :" _LOG_TAG format "\r\n", ## __VA_ARGS__)
#else
#define log_info(...)
#endif /* LOG_INFO_ENABLE */

#ifdef LOG_DEBUG_ENABLE
#define log_debug(format, ...)      log_print(__LOG_DEBUG, NULL, "[Debug] :" _LOG_TAG format "\r\n", ## __VA_ARGS__)
#else
#define log_debug(...)
#endif /* LOG_DEBUG_ENABLE */

#ifdef LOG_WARN_ENABLE
#define log_warn(format, ...)       log_print(__LOG_WARN, NULL, "[Warn] :" _LOG_TAG format "\r\n", ## __VA_ARGS__)
#else
#define log_warn(...)
#endif /* LOG_WARN_ENABLE */

#ifdef LOG_ERROR_ENABLE
#define log_error(format, ...)      log_print(__LOG_ERROR, NULL, "[Error] :" _LOG_TAG format "\r\n", ## __VA_ARGS__)
#else
#define log_error(...)
#endif /* LOG_ERROR_ENABLE */

#ifdef LOG_DUMP_ENABLE
#define log_info_hexdump(x,y)       printf_buf(x,y)
#else
#define log_info_hexdump(...)
#endif /* LOG_DUMP_ENABLE */

#ifdef LOG_CHAR_ENABLE
#define log_char(x)                 putchar(x)
#else
#define log_char(x)
#endif /* LOG_CHAR_ENABLE */

#endif /* LOG_TAG_CONST */

//< debug remap
#if MESH_CODE_LOG_DEBUG_EN
#define LOG_INF             log_info
#define LOG_HEXDUMP_INF     log_info_hexdump
#define LOG_DBG             log_debug
#define LOG_WRN             log_warn
#define LOG_ERR             log_error
#else
#define LOG_INF
#define LOG_HEXDUMP_INF
#define LOG_DBG
#define LOG_WRN
#define LOG_ERR             log_error
#endif /* MESH_CODE_LOG_DEBUG_EN */

#endif /* __MESH_LOG_H__ */
