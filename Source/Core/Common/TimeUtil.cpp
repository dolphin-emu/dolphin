// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/TimeUtil.h"
#include "Common/Logging/Log.h"

#include <ctime>
#include <optional>

namespace Common
{
std::optional<std::tm> LocalTime(std::time_t time)
{
  std::tm local_time;
#ifdef _MSC_VER
  if (localtime_s(&local_time, &time) != 0)
#else
  if (localtime_r(&time, &local_time) == NULL)
#endif
  {
    ERROR_LOG_FMT(COMMON, "Failed to convert time to local time: {}", std::strerror(errno));
    return std::nullopt;
  }
  return local_time;
}
}  // Namespace Common
