// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

namespace DiscIO
{
struct Partition;
class Volume;
}  // namespace DiscIO

namespace FileMonitor
{
void Log(const DiscIO::Volume& volume, const DiscIO::Partition& partition, u64 offset);
}
