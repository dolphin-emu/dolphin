// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <expected>

#include "Core/ActionReplay.h"

namespace Cheats
{
class CheatSearchSessionBase;

enum class GenerateActionReplayCodeErrorCode
{
  IndexOutOfRange,
  NotVirtualMemory,
  InvalidAddress,
};

std::expected<ActionReplay::ARCode, GenerateActionReplayCodeErrorCode>
GenerateActionReplayCode(const Cheats::CheatSearchSessionBase& session, size_t index);
}  // namespace Cheats
