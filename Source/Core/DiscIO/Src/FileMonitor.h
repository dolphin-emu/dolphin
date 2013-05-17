// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <iostream> // System: For std

#include "Common.h" // Common: For u64


namespace FileMon
{

void ShowSound(std::string File);
void ReadGC(std::string File);
void CheckFile(std::string File, u64 Size);
void FindFilename(u64 Offset);
void Close();

}
