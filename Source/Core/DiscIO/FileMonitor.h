// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"

namespace FileMon
{

bool IsSoundFile(const std::string& filename);
void ReadFileSystem(const std::string& file);
void CheckFile(const std::string& file, u64 size);
void FindFilename(u64 offset);
void Close();

}
