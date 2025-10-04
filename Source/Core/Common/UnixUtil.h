// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <pthread.h>
#include <sys/eventfd.h>

#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"

namespace UnixUtil
{
inline int CreateEventFD(unsigned int count, int flags)
{
  const int result = eventfd(count, flags);
  if (result == -1)
  {
    ERROR_LOG_FMT(COMMON, "eventfd failed: {}", Common::LastStrerrorString());
    std::abort();
  }
  return result;
}

// Repeatedly call a function that can erroneously produce EINTR.
auto RetryOnEINTR(auto func, auto... args)
{
  while (true)
  {
    const int result = func(args...);
    if (result >= 0 || errno != EINTR)
      return result;
  }
}

// This is a very low-effort wrapper for pthread.
// It allows creating a pthread from any callable (e.g. a lambda).
// The wrapper object must exist for the lifetime of the thread.
template <typename Func>
struct PThreadWrapper
{
  Func func;
  pthread_t handle{};

  explicit PThreadWrapper(Func&& f) : func(std::move(f))
  {
    if (int result = pthread_create(
            &handle, nullptr,
            [](void* arg) -> void* {
              static_cast<PThreadWrapper*>(arg)->func();
              return nullptr;
            },
            this);
        result != 0)
    {
      ERROR_LOG_FMT(COMMON, "pthread_create: {}", Common::StrerrorString(result));
      std::abort();
    }
  }
};

}  // namespace UnixUtil
