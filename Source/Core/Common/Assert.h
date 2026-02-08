// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Common.h"
#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#define ASSERT_MSG(_t_, _a_, _fmt_, ...)                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(_a_)) [[unlikely]]                                                                       \
    {                                                                                              \
      if (!PanicYesNoFmtAssert(_t_,                                                                \
                               "An error occurred.\n\n" _fmt_ "\n\n"                               \
                               "  Condition: {}\n  File: {}\n  Line: {}\n  Function: {}\n\n"       \
                               "Ignore and continue?",                                             \
                               __VA_ARGS__ __VA_OPT__(, ) #_a_, __FILE__, __LINE__, __func__))     \
        Crash();                                                                                   \
    }                                                                                              \
  } while (0)

#define DEBUG_ASSERT_MSG(_t_, _a_, _fmt_, ...)                                                     \
  do                                                                                               \
  {                                                                                                \
    if constexpr (Common::Log::MAX_LOGLEVEL >= Common::Log::LogLevel::LDEBUG)                      \
      ASSERT_MSG(_t_, _a_, _fmt_ __VA_OPT__(, ) __VA_ARGS__);                                      \
  } while (0)

#define ASSERT(_a_)                                                                                \
  do                                                                                               \
  {                                                                                                \
    if (!(_a_)) [[unlikely]]                                                                       \
    {                                                                                              \
      if (!PanicYesNoFmt("An error occurred.\n\n"                                                  \
                         "  Condition: {}\n  File: {}\n  Line: {}\n  Function: {}\n\n"             \
                         "Ignore and continue?",                                                   \
                         #_a_, __FILE__, __LINE__, __func__))                                      \
        Crash();                                                                                   \
    }                                                                                              \
  } while (0)

#define DEBUG_ASSERT(_a_)                                                                          \
  do                                                                                               \
  {                                                                                                \
    if constexpr (Common::Log::MAX_LOGLEVEL >= Common::Log::LogLevel::LDEBUG)                      \
      ASSERT(_a_);                                                                                 \
  } while (0)
