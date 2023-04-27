// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

namespace VideoUtils
{
std::vector<std::string> GetAvailableAntialiasingModes(int& m_msaa_modes);
}  // namespace VideoUtils
