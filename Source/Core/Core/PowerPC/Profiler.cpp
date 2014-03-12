// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>
#include "Core/PowerPC/JitInterface.h"

namespace Profiler
{

bool g_ProfileBlocks;
bool g_ProfileInstructions;

void WriteProfileResults(const std::string& filename)
{
	JitInterface::WriteProfileResults(filename);
}

}  // namespace
