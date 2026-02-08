// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Result.h"

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

Common::Result<GenerateActionReplayCodeErrorCode, ActionReplay::ARCode>
GenerateActionReplayCode(const Cheats::CheatSearchSessionBase& session, size_t index);
}  // namespace Cheats
