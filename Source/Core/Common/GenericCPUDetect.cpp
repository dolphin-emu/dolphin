// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/CPUDetect.h"

CPUInfo cpu_info;

CPUInfo::CPUInfo()
{
}

std::string CPUInfo::Summarize()
{
  return "Generic";
}
