// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CPUDetect.h"

CPUInfo cpu_info;

CPUInfo::CPUInfo() {}

std::string CPUInfo::Summarize()
{
	return "Generic";
}
