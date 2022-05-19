// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <string>

namespace UICommon::Steam
{
bool IsRelaunchRequired();
void Init();
void Shutdown();
}  // namespace Steam
