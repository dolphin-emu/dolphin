// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Common.h"
#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#ifdef _WIN32
#define ASSERT_MSG(_t_, _a_, _fmt_, ...)                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(_a_))                                                                                    \
    {                                                                                              \
      if (!PanicYesNo(_fmt_, __VA_ARGS__))                                                         \
        Crash();                                                                                   \
    }                                                                                              \
  } while (0)

#define DEBUG_ASSERT_MSG(_t_, _a_, _msg_, ...)                                                     \
  do                                                                                               \
  {                                                                                                \
    if (MAX_LOGLEVEL >= LogTypes::LOG_LEVELS::LDEBUG && !(_a_))                                    \
    {                                                                                              \
      ERROR_LOG(_t_, _msg_, __VA_ARGS__);                                                          \
      if (!PanicYesNo(_msg_, __VA_ARGS__))                                                         \
        Crash();                                                                                   \
    }                                                                                              \
  } while (0)
#else
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
    if (MAX_LOGLEVEL >= LogTypes::LOG_LEVELS::LDEBUG && !(_a_))                                    \
    {                                                                                              \
      ERROR_LOG(_t_, _msg_, ##__VA_ARGS__);                                                        \
      if (!PanicYesNo(_msg_, ##__VA_ARGS__))                                                       \
        Crash();                                                                                   \
    }                                                                                              \
  } while (0)
#endif

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
    if (MAX_LOGLEVEL >= LogTypes::LOG_LEVELS::LDEBUG)                                              \
      ASSERT(_a_);                                                                                 \
  } while (0)


#ifdef ANDROID
//#undef ASSERT_MSG
#undef DEBUG_ASSERT_MSG
//#undef ASSERT
#undef DEBUG_ASSERT

//#define ASSERT_MSG(_t_, _a_, _fmt_, ...)  do {} while(0)
#define DEBUG_ASSERT_MSG(_t_, _a_, _msg_, ...)  do {} while(0)
//#define ASSERT(_a_)  do {} while(0)
#define DEBUG_ASSERT(_a_)  do {} while(0)
#endif
