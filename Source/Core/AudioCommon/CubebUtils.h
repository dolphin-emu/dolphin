// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct cubeb;

namespace CubebUtils
{
std::shared_ptr<cubeb> GetContext();
std::vector<std::pair<std::string, std::string>> ListInputDevices();
const void* GetInputDeviceById(const std::string& id);
}  // namespace CubebUtils
