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
    if (!(_a_))                                                                                    \
    {                                                                                              \
      if (!PanicYesNo(_fmt_, ##__VA_ARGS__))                                                       \
        Crash();                                                                                   \
    }                                                                                              \
  } while (0)

#define DEBUG_ASSERT_MSG(_t_, _a_, _msg_, ...)                                                     \
  do                                                                                               \
  {                                                                                                \
    if constexpr (MAX_LOGLEVEL >= Common::Log::LOG_LEVELS::LDEBUG)                                 \
    {                                                                                              \
      if (!(_a_))                                                                                  \
      {                                                                                            \
        ERROR_LOG(_t_, _msg_, ##__VA_ARGS__);                                                      \
        if (!PanicYesNo(_msg_, ##__VA_ARGS__))                                                     \
          Crash();                                                                                 \
      }                                                                                            \
    }                                                                                              \
  } while (0)

#define ASSERT(_a_)                                                                                \
  do                                                                                               \
  {                                                                                                \
    ASSERT_MSG(MASTER_LOG, _a_,                                                                    \
               _trans("An error occurred.\n\n  Line: %d\n  File: %s\n\nIgnore and continue?"),     \
               __LINE__, __FILE__);                                                                \
  } while (0)

#define DEBUG_ASSERT(_a_)                                                                          \
  do                                                                                               \
  {                                                                                                \
    if constexpr (MAX_LOGLEVEL >= Common::Log::LOG_LEVELS::LDEBUG)                                 \
      ASSERT(_a_);                                                                                 \
  } while (0)
