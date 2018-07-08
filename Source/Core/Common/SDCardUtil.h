// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "Common/CommonTypes.h"

namespace Common
{
bool SDCardCreate(u64 disk_size /*in MB*/, const std::string& filename);
}  // namespace Common
