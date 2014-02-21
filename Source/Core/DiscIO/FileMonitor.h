// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"

namespace FileMon
{

void ShowSound(std::string File);
void ReadGC(std::string File);
void CheckFile(std::string File, u64 Size);
void FindFilename(u64 Offset);
void Close();

}
