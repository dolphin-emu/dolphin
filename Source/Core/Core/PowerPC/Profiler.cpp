// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Profiler.h"

#include <string>
#include "Common/PerformanceCounter.h"
#include "Core/PowerPC/JitInterface.h"

namespace Profiler
{
bool g_ProfileBlocks = false;

void WriteProfileResults(const std::string& filename)
{
  JitInterface::WriteProfileResults(filename);
}

}  // namespace
