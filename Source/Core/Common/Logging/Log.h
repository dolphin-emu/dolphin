// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <fmt/format.h>
#include <string_view>
#include "Common/FormatUtil.h"

namespace Common::Log
{
enum class LogType : int
{
  ACHIEVEMENTS,
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
  HSP,
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
  SLIPPI,
  SLIPPI_ONLINE,
  SLIPPI_RUST_DEPENDENCIES,
  SLIPPI_RUST_ONLINE,
  SLIPPI_RUST_JUKEBOX,
  SP1,
  SYMBOLS,
  VIDEO,
  VIDEOINTERFACE,
  WII_IPC,
  WIIMOTE,

  NUMBER_OF_LOGS  // Must be last
};

constexpr LogType LAST_LOG_TYPE =
    static_cast<LogType>(static_cast<int>(LogType::NUMBER_OF_LOGS) - 1);

enum class LogLevel : int
{
  LNOTICE = 1,   // VERY important information that is NOT errors. Like startup and OSReports.
  LERROR = 2,    // Critical errors
  LWARNING = 3,  // Something is suspicious.
  LINFO = 4,     // General information.
  LDEBUG = 5,    // Detailed debugging - might make things slow.
};

// A "simple" logger that doesn't actually do any formatting at all, simply
// taking in a `level`, `file` name, `line` number, and the `msg` to log.
//
// The reason for this is that we now have some modules written in other languages
// that need to hop the boundary for logging, and we explicitly *do not* want to
// attempt to deal with who owns what and/or how to format with variable-length args
// from another language. Offering this just makes logging easier on some
// platforms (e.g, Windows). We also avoid needing to expose the LOG_LEVELS enum into
// other languages by just accepting an integer, which is shuffled into the correct type
// in the implementation.
//
// If you are calling this from another language and you need to log any form of structured
// data, you should do the formatting *on your side* and pass it over here. You are also
// responsible for ensuring that the msg string lives for an appropriate lifetime.
void SlippiRustLogger(int level, int slp_log_type, const char* file, int line, const char* msg);

#if defined(_DEBUG) || defined(DEBUGFAST)
constexpr auto MAX_LOGLEVEL = Common::Log::LogLevel::LDEBUG;
#else
constexpr auto MAX_LOGLEVEL = Common::Log::LogLevel::LINFO;
#endif  // logging

static const char LOG_LEVEL_TO_CHAR[7] = "-NEWID";

void GenericLogFmtImpl(LogLevel level, LogType type, const char* file, int line,
                       fmt::string_view format, const fmt::format_args& args);

template <std::size_t NumFields, typename S, typename... Args>
void GenericLogFmt(LogLevel level, LogType type, const char* file, int line, const S& format,
                   const Args&... args)
{
  static_assert(NumFields == sizeof...(args),
                "Unexpected number of replacement fields in format string; did you pass too few or "
                "too many arguments?");
  GenericLogFmtImpl(level, type, file, line, format, fmt::make_format_args(args...));
}
}  // namespace Common::Log

// fmtlib capable API

#define GENERIC_LOG_FMT(t, v, format, ...)                                                         \
  do                                                                                               \
  {                                                                                                \
    if (v <= Common::Log::MAX_LOGLEVEL)                                                            \
    {                                                                                              \
      /* Use a macro-like name to avoid shadowing warnings */                                      \
      constexpr auto GENERIC_LOG_FMT_N = Common::CountFmtReplacementFields(format);                \
      Common::Log::GenericLogFmt<GENERIC_LOG_FMT_N>(                                               \
          v, t, __FILE__, __LINE__, FMT_STRING(format) __VA_OPT__(, ) __VA_ARGS__);                \
    }                                                                                              \
  } while (0)

#define ERROR_LOG_FMT(t, ...)                                                                      \
  do                                                                                               \
  {                                                                                                \
    GENERIC_LOG_FMT(Common::Log::LogType::t,                                                       \
                    Common::Log::LogLevel::LERROR __VA_OPT__(, ) __VA_ARGS__);                     \
  } while (0)
#define WARN_LOG_FMT(t, ...)                                                                       \
  do                                                                                               \
  {                                                                                                \
    GENERIC_LOG_FMT(Common::Log::LogType::t,                                                       \
                    Common::Log::LogLevel::LWARNING __VA_OPT__(, ) __VA_ARGS__);                   \
  } while (0)
#define NOTICE_LOG_FMT(t, ...)                                                                     \
  do                                                                                               \
  {                                                                                                \
    GENERIC_LOG_FMT(Common::Log::LogType::t,                                                       \
                    Common::Log::LogLevel::LNOTICE __VA_OPT__(, ) __VA_ARGS__);                    \
  } while (0)
#define INFO_LOG_FMT(t, ...)                                                                       \
  do                                                                                               \
  {                                                                                                \
    GENERIC_LOG_FMT(Common::Log::LogType::t,                                                       \
                    Common::Log::LogLevel::LINFO __VA_OPT__(, ) __VA_ARGS__);                      \
  } while (0)
#define DEBUG_LOG_FMT(t, ...)                                                                      \
  do                                                                                               \
  {                                                                                                \
    GENERIC_LOG_FMT(Common::Log::LogType::t,                                                       \
                    Common::Log::LogLevel::LDEBUG __VA_OPT__(, ) __VA_ARGS__);                     \
  } while (0)
