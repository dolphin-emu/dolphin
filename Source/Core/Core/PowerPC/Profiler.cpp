// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/Profiler.h"

namespace Profiler
{

bool g_ProfileBlocks;

void WriteProfileResults(const std::string& filename)
{
	JitInterface::WriteProfileResults(filename);
}

}  // namespace
