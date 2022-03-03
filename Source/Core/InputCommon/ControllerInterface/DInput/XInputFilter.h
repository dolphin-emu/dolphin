// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Windows.h>
#include <unordered_set>

namespace ciface::DInput
{
std::unordered_set<DWORD> GetXInputGUIDS();
}  // namespace ciface::DInput
