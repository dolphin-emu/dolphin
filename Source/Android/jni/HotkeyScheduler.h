// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

namespace HotkeyScheduler
{
void Start();

void Stop();

void SetBackgroundExecutionAllowed(bool allowed);

void RefreshActiveHotkeys();

const std::vector<int>& GetSupportedHotkeys();
}  // namespace HotkeyScheduler
