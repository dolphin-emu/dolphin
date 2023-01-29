// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

namespace Common
{
// Returns a pointer to an array of strings with the device names
std::vector<std::string> GetCDDevices();

// Returns true if device is cdrom/dvd
bool IsCDROMDevice(std::string device);
}  // namespace Common
