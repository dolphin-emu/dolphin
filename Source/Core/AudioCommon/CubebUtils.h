// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct cubeb;

namespace CubebUtils
{
std::shared_ptr<cubeb> GetContext();
std::vector<std::pair<std::string, std::string>> ListInputDevices();
const void* GetInputDeviceById(std::string_view id);
}  // namespace CubebUtils
