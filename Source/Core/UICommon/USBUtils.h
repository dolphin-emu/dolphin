// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
