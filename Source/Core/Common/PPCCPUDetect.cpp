// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/CPUDetect.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#ifdef __linux__
#include <sys/auxv.h>
#endif

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"

#ifdef __linux__

static std::string ReadCpuinfoField(const std::string& field)
{
  std::string line;
  std::ifstream file;
  File::OpenFStream(file, "/proc/cpuinfo", std::ios_base::in);
  if (!file)
    return {};

  while (std::getline(file, line))
  {
    if (!line.starts_with(field))
      continue;
    auto non_tab = line.find_first_not_of("\t", field.length());
    if (non_tab == line.npos)
      continue;
    if (line[non_tab] != ':')
      continue;
    auto value_start = line.find_first_not_of(" ", non_tab + 1);
    if (value_start == line.npos)
      continue;
    return line.substr(value_start);
  }
  return {};
}

#endif

CPUInfo cpu_info;

CPUInfo::CPUInfo()
{
  Detect();
}

void CPUInfo::Detect()
{
  vendor = CPUVendor::Other;
  bFMA = true;

  num_cores = std::max(static_cast<int>(std::thread::hardware_concurrency()), 1);

#ifdef __linux__
  // e.g. "POWER9, altivec supported"
  model_name = ReadCpuinfoField("cpu");

  // e.g. "power9"
  if (const char* platform = reinterpret_cast<const char*>(getauxval(AT_PLATFORM)))
    cpu_id = platform;

  const unsigned long hwcap = getauxval(AT_HWCAP);
  const unsigned long hwcap2 = getauxval(AT_HWCAP2);
  bVSX = hwcap & PPC_FEATURE_HAS_VSX;
  bISA_2_07 = hwcap2 & PPC_FEATURE2_ARCH_2_07;
  bISA_3_0 = hwcap2 & PPC_FEATURE2_ARCH_3_00;
  bISA_3_1 = hwcap2 & PPC_FEATURE2_ARCH_3_1;
#endif
}

std::string CPUInfo::Summarize()
{
  std::vector<std::string> sum;
  sum.push_back(model_name);
  sum.push_back(cpu_id);

  if (bVSX)
    sum.push_back("VSX");
  if (bISA_3_1)
    sum.push_back("ISA 3.1");
  else if (bISA_3_0)
    sum.push_back("ISA 3.0");
  else if (bISA_2_07)
    sum.push_back("ISA 2.07");

  return fmt::to_string(fmt::join(sum, ","));
}
