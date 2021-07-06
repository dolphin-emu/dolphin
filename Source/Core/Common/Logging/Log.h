// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <fmt/format.h>
#include <string_view>
#include "Common/FormatUtil.h"

namespace Common::Log
{
enum LOG_TYPE
{
  ACTIONREPLAY,
  AUDIO,
  AUDIO_INTERFACE,
  BOOT,
  COMMANDPROCESSOR,
  COMMON,
  CONSOLE,
  CONTROLLERINTERFACE,
  CORE,
  DISCIO,
  DSPHLE,
  DSPLLE,
  DSP_MAIL,
  DSPINTERFACE,
  DVDINTERFACE,
  DYNA_REC,
  EXPANSIONINTERFACE,
  FILEMON,
  FRAMEDUMP,
  GDB_STUB,
  GPFIFO,
  HOST_GPU,
  IOS,
  IOS_DI,
  IOS_ES,
  IOS_FS,
  IOS_NET,
  IOS_SD,
  IOS_SSL,
  IOS_STM,
  IOS_USB,
  IOS_WC24,
  IOS_WFS,
  IOS_WIIMOTE,
  MASTER_LOG,
  MEMMAP,
  MEMCARD_MANAGER,
  NETPLAY,
  OSHLE,
  OSREPORT,
  OSREPORT_HLE,
  PIXELENGINE,
  PROCESSORINTERFACE,
  POWERPC,
  SERIALINTERFACE,
  SP1,
  SYMBOLS,
  VIDEO,
  VIDEOINTERFACE,
  WII_IPC,
  WIIMOTE,

  NUMBER_OF_LOGS  // Must be last
};

enum LOG_LEVELS
{
  LNOTICE = 1,   // VERY important information that is NOT errors. Like startup and OSReports.
  LERROR = 2,    // Critical errors
  LWARNING = 3,  // Something is suspicious.
  LINFO = 4,     // General information.
  LDEBUG = 5,    // Detailed debugging - might make things slow.
};

static const char LOG_LEVEL_TO_CHAR[7] = "-NEWID";

void GenericLogFmtImpl(LOG_LEVELS level, LOG_TYPE type, const char* file, int line,
                       fmt::string_view format, const fmt::format_args& args);

template <std::size_t NumFields, typename S, typename... Args>
void GenericLogFmt(LOG_LEVELS level, LOG_TYPE type, const char* file, int line, const S& format,
                   const Args&... args)
{
  static_assert(NumFields == sizeof...(args),
                "Unexpected number of replacement fields in format string; did you pass too few or "
                "too many arguments?");
  GenericLogFmtImpl(level, type, file, line, format,
                    fmt::make_args_checked<Args...>(format, args...));
}

void GenericLog(LOG_LEVELS level, LOG_TYPE type, const char* file, int line, const char* fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 5, 6)))
#endif
    ;
}  // namespace Common::Log

#if defined(_DEBUG) || defined(DEBUGFAST)
#define MAX_LOGLEVEL Common::Log::LOG_LEVELS::LDEBUG
#else
#ifndef MAX_LOGLEVEL
#define MAX_LOGLEVEL Common::Log::LOG_LEVELS::LINFO
#endif  // loglevel
#endif  // logging

// Let the compiler optimize this out
#define GENERIC_LOG(t, v, ...)                                                                     \
  do                                                                                               \
  {                                                                                                \
    if (v <= MAX_LOGLEVEL)                                                                         \
      Common::Log::GenericLog(v, t, __FILE__, __LINE__, __VA_ARGS__);                              \
  } while (0)

#define ERROR_LOG(t, ...)                                                                          \
  do                                                                                               \
  {                                                                                                \
    GENERIC_LOG(Common::Log::t, Common::Log::LERROR, __VA_ARGS__);                                 \
  } while (0)
#define WARN_LOG(t, ...)                                                                           \
  do                                                                                               \
  {                                                                                                \
    GENERIC_LOG(Common::Log::t, Common::Log::LWARNING, __VA_ARGS__);                               \
  } while (0)
#define NOTICE_LOG(t, ...)                                                                         \
  do                                                                                               \
  {                                                                                                \
    GENERIC_LOG(Common::Log::t, Common::Log::LNOTICE, __VA_ARGS__);                                \
  } while (0)
#define INFO_LOG(t, ...)                                                                           \
  do                                                                                               \
  {                                                                                                \
    GENERIC_LOG(Common::Log::t, Common::Log::LINFO, __VA_ARGS__);                                  \
  } while (0)
#define DEBUG_LOG(t, ...)                                                                          \
  do                                                                                               \
  {                                                                                                \
    GENERIC_LOG(Common::Log::t, Common::Log::LDEBUG, __VA_ARGS__);                                 \
  } while (0)

// fmtlib capable API

#define GENERIC_LOG_FMT(t, v, format, ...)                                                         \
  do                                                                                               \
  {                                                                                                \
    if (v <= MAX_LOGLEVEL)                                                                         \
    {                                                                                              \
      /* Use a macro-like name to avoid shadowing warnings */                                      \
      constexpr auto GENERIC_LOG_FMT_N = Common::CountFmtReplacementFields(format);                \
      Common::Log::GenericLogFmt<GENERIC_LOG_FMT_N>(v, t, __FILE__, __LINE__, FMT_STRING(format),  \
                                                    ##__VA_ARGS__);                                \
    }                                                                                              \
  } while (0)

#define ERROR_LOG_FMT(t, ...)                                                                      \
  do                                                                                               \
  {                                                                                                \
    GENERIC_LOG_FMT(Common::Log::t, Common::Log::LERROR, __VA_ARGS__);                             \
  } while (0)
#define WARN_LOG_FMT(t, ...)                                                                       \
  do                                                                                               \
  {                                                                                                \
    GENERIC_LOG_FMT(Common::Log::t, Common::Log::LWARNING, __VA_ARGS__);                           \
  } while (0)
#define NOTICE_LOG_FMT(t, ...)                                                                     \
  do                                                                                               \
  {                                                                                                \
    GENERIC_LOG_FMT(Common::Log::t, Common::Log::LNOTICE, __VA_ARGS__);                            \
  } while (0)
#define INFO_LOG_FMT(t, ...)                                                                       \
  do                                                                                               \
  {                                                                                                \
    GENERIC_LOG_FMT(Common::Log::t, Common::Log::LINFO, __VA_ARGS__);                              \
  } while (0)
#define DEBUG_LOG_FMT(t, ...)                                                                      \
  do                                                                                               \
  {                                                                                                \
    GENERIC_LOG_FMT(Common::Log::t, Common::Log::LDEBUG, __VA_ARGS__);                             \
  } while (0)
