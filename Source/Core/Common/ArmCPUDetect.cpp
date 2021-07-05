// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#if !defined(_WIN32) && !defined(__APPLE__)
#ifndef __FreeBSD__
#include <asm/hwcap.h>
#endif
#include <sys/auxv.h>
#include <unistd.h>
#endif

#include <fmt/format.h>

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"

#ifndef WIN32

const char procfile[] = "/proc/cpuinfo";

static std::string GetCPUString()
{
  const std::string marker = "Hardware\t: ";
  std::string cpu_string = "Unknown";

  std::string line;
  std::ifstream file;
  File::OpenFStream(file, procfile, std::ios_base::in);

  if (!file)
    return cpu_string;

  while (std::getline(file, line))
  {
    if (line.find(marker) != std::string::npos)
    {
      cpu_string = line.substr(marker.length());
      break;
    }
  }

  return cpu_string;
}

#endif

CPUInfo cpu_info;

CPUInfo::CPUInfo()
{
  Detect();
}

// Detects the various CPU features
void CPUInfo::Detect()
{
  // Set some defaults here
  HTT = false;
  OS64bit = true;
  CPU64bit = true;
  Mode64bit = true;
  vendor = CPUVendor::ARM;
  bFMA = true;
  bFlushToZero = true;
  bAFP = false;

#ifdef __APPLE__
  num_cores = std::thread::hardware_concurrency();

  // M-series CPUs have all of these
  bFP = true;
  bASIMD = true;
  bAES = true;
  bSHA1 = true;
  bSHA2 = true;
  bCRC32 = true;
#elif defined(_WIN32)
  num_cores = std::thread::hardware_concurrency();

  // Windows does not provide any mechanism for querying the system registers on ARMv8, unlike Linux
  // which traps the register reads and emulates them in the kernel. There are environment variables
  // containing some of the CPU-specific values, which we could use for a lookup table in the
  // future. For now, assume all features are present as all known devices which are Windows-on-ARM
  // compatible also support these extensions.
  bFP = true;
  bASIMD = true;
  bAES = true;
  bCRC32 = true;
  bSHA1 = true;
  bSHA2 = true;
#else
  // Get the information about the CPU
  num_cores = sysconf(_SC_NPROCESSORS_CONF);
  strncpy(cpu_string, GetCPUString().c_str(), sizeof(cpu_string));

#ifdef __FreeBSD__
  u_long hwcaps = 0;
  elf_aux_info(AT_HWCAP, &hwcaps, sizeof(u_long));
#else
  unsigned long hwcaps = getauxval(AT_HWCAP);
#endif
  bFP = hwcaps & HWCAP_FP;
  bASIMD = hwcaps & HWCAP_ASIMD;
  bAES = hwcaps & HWCAP_AES;
  bCRC32 = hwcaps & HWCAP_CRC32;
  bSHA1 = hwcaps & HWCAP_SHA1;
  bSHA2 = hwcaps & HWCAP_SHA2;
#endif
}

// Turn the CPU info into a string we can show
std::string CPUInfo::Summarize()
{
  std::string sum;
  if (num_cores == 1)
    sum = fmt::format("{}, 1 core", cpu_string);
  else
    sum = fmt::format("{}, {} cores", cpu_string, num_cores);

  if (bAES)
    sum += ", AES";
  if (bCRC32)
    sum += ", CRC32";
  if (bSHA1)
    sum += ", SHA1";
  if (bSHA2)
    sum += ", SHA2";
  if (CPU64bit)
    sum += ", 64-bit";

  return sum;
}
