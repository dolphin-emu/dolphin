// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
