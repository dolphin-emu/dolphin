// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/TimeUtil.h"

#include <ctime>
#include <optional>

namespace Common
{
std::optional<std::tm> Localtime(std::time_t time)
{
  std::tm local_time;
#ifdef _MSC_VER
  if (localtime_s(&local_time, &time) != 0)
    return std::nullopt;
#else
  std::tm* result = localtime_r(&time, &local_time);
  if (result != &local_time)
    return std::nullopt;
#endif
  return local_time;
}
}  // Namespace Common
