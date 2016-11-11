// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>
#include <utility>

#include "Common/CommonTypes.h"

namespace USBUtils
{
void Init();
void Shutdown();

std::map<std::pair<u16, u16>, std::string> GetInsertedDevices();
std::string GetDeviceName(u16 vid, u16 pid);
std::string GetDeviceName(const std::pair<u16, u16>& pair);
}  // namespace USBUtils
