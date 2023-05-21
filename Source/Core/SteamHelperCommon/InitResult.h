// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>

namespace Steam
{
enum class InitResult : uint8_t
{
  Success = 0,
  RestartingFromSteam = 1,
  Failure = 2
};
}  // namespace Steam
