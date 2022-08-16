// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <string_view>

namespace Core::Debug
{
std::string HLEFormatString(const std::string_view message);
}  // namespace Core::Debug
