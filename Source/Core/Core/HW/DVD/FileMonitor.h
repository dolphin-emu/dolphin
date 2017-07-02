// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

namespace DiscIO
{
struct Partition;
class Volume;
}

namespace FileMonitor
{
// Can be called with nullptr to set the file system to nothing. When not called
// with nullptr, the volume must remain valid until the next SetFileSystem call.
void SetFileSystem(const DiscIO::Volume* volume);
// Logs access to files in the file system set by SetFileSystem
void Log(u64 offset, const DiscIO::Partition& partition);
}
