// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"

namespace DiscIO
{
struct Partition;
}

namespace FileMon
{
bool IsSoundFile(const std::string& filename);
void ReadFileSystem(const std::string& file, const DiscIO::Partition& partition);
void CheckFile(const std::string& file, u64 size);
void FindFilename(u64 offset, const DiscIO::Partition& partition);
void Close();
}
