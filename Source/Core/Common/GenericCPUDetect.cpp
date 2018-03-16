// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CPUDetect.h"

const CPUInfo cpu_info;

CPUInfo::CPUInfo()
{
}

std::string CPUInfo::Summarize() const
{
  return "Generic";
}
