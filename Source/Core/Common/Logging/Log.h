// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

namespace LogTypes
{
enum LOG_TYPE
{
  ALL_LOGS,
#define TYPE(id, short_name, long_name) id,
#define CATEGORY(id, short_name, long_name) id,
#define END_CATEGORY(id) id##_END,
#include "Common/Logging/LogTypes.h"
#undef TYPE
#undef CATEGORY
#undef END_CATEGORY
  NUMBER_OF_LOGS  // Must be last
};

bool IsValid(LOG_TYPE type);
bool IsLeaf(LOG_TYPE type);
LOG_TYPE FirstChild(LOG_TYPE type);
LOG_TYPE NextSibling(LOG_TYPE type);
LOG_TYPE Parent(LOG_TYPE type);
size_t CountChildren(LOG_TYPE type);

enum LOG_LEVELS
{
  LSUPPRESS = 0,  // don't show any logs in this category (don't use this for generating logs, only
                  // in UI/config files)
  LNOTICE = 1,    // VERY important information that is NOT errors. Like startup and OSReports.
  LERROR = 2,     // Critical errors
  LWARNING = 3,   // Something is suspicious.
  LINFO = 4,      // General information.
  LDEBUG = 5,     // Detailed debugging - might make things slow.
  LINHERIT =
      6,  // Inherit the log level from parent (only use for unsetting log level in UI/config files)
};

static const char LOG_LEVEL_TO_CHAR[8] = "-NEWID?";

struct Info
{
  const char* short_name;
  const char* long_name;
  // 0 means leaf
  LOG_TYPE end;
};
extern const std::array<Info, NUMBER_OF_LOGS> INFO;

}  // namespace

void GenericLog(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char* file, int line,
                const char* fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 5, 6)))
#endif
    ;

#if defined(_DEBUG) || defined(DEBUGFAST)
#define MAX_LOGLEVEL LogTypes::LOG_LEVELS::LDEBUG
#else
#ifndef MAX_LOGLEVEL
#define MAX_LOGLEVEL LogTypes::LOG_LEVELS::LINFO
#endif  // loglevel
#endif  // logging

// Let the compiler optimize this out
#define GENERIC_LOG(t, v, ...)                                                                     \
  do                                                                                               \
  {                                                                                                \
    if (v <= MAX_LOGLEVEL)                                                                         \
      GenericLog(v, t, __FILE__, __LINE__, __VA_ARGS__);                                           \
  } while (0)

#define ERROR_LOG(t, ...)                                                                          \
  do                                                                                               \
  {                                                                                                \
    GENERIC_LOG(LogTypes::t, LogTypes::LERROR, __VA_ARGS__);                                       \
  } while (0)
#define WARN_LOG(t, ...)                                                                           \
  do                                                                                               \
  {                                                                                                \
    GENERIC_LOG(LogTypes::t, LogTypes::LWARNING, __VA_ARGS__);                                     \
  } while (0)
#define NOTICE_LOG(t, ...)                                                                         \
  do                                                                                               \
  {                                                                                                \
    GENERIC_LOG(LogTypes::t, LogTypes::LNOTICE, __VA_ARGS__);                                      \
  } while (0)
#define INFO_LOG(t, ...)                                                                           \
  do                                                                                               \
  {                                                                                                \
    GENERIC_LOG(LogTypes::t, LogTypes::LINFO, __VA_ARGS__);                                        \
  } while (0)
#define DEBUG_LOG(t, ...)                                                                          \
  do                                                                                               \
  {                                                                                                \
    GENERIC_LOG(LogTypes::t, LogTypes::LDEBUG, __VA_ARGS__);                                       \
  } while (0)
