// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>
#include <utility>

#include "Common/CommonTypes.h"

namespace USBUtils
{
std::map<std::pair<u16, u16>, std::string> GetInsertedDevices();
std::string GetDeviceName(std::pair<u16, u16> vid_pid);
}  // namespace USBUtils
