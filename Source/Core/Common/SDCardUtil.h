// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

#include <string>

namespace Common
{
bool SDCardCreate(u64 disk_size /*in MB*/, const std::string& filename);
}  // namespace Common
