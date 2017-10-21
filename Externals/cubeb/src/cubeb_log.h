/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#ifndef CUBEB_LOG
#define CUBEB_LOG

#include "cubeb/cubeb.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PRINTF_FORMAT(fmt, args) __attribute__((format(printf, fmt, args)))
#else
#define PRINTF_FORMAT(fmt, args)
#endif

extern cubeb_log_level g_cubeb_log_level;
extern cubeb_log_callback g_cubeb_log_callback PRINTF_FORMAT(1, 2);
void cubeb_async_log(const char * fmt, ...);
void cubeb_async_log_reset_threads();

#ifdef __cplusplus
}
#endif

#define LOGV(msg, ...) LOG_INTERNAL(CUBEB_LOG_VERBOSE, msg, ##__VA_ARGS__)
#define LOG(msg, ...) LOG_INTERNAL(CUBEB_LOG_NORMAL, msg, ##__VA_ARGS__)

#define LOG_INTERNAL(level, fmt, ...) do {                                   \
    if (g_cubeb_log_callback && level <= g_cubeb_log_level) {                            \
      g_cubeb_log_callback("%s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    }                                                                        \
  } while(0)

/* Asynchronous verbose logging, to log in real-time callbacks. */
#define ALOGV(fmt, ...)                   \
do {                                      \
  cubeb_async_log(fmt, ##__VA_ARGS__);    \
} while(0)

#endif // CUBEB_LOG
