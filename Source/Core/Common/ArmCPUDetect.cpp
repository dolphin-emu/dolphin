// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#ifdef ANDROID
#include <asm/hwcap.h>
#include <sys/auxv.h>
#include <unistd.h>
#endif

#ifdef IPHONEOS
#include <regex>
#include <sys/utsname.h>
#include <unistd.h>
#endif

#include <fmt/format.h>

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"

#ifdef ANDROID

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

#elif defined(IPHONEOS)

static std::string GetCPUString()
{
  return "Apple A-series CPU";
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
  // When ARMv8 CPUs come out, these need to be updated.
  HTT = false;
  OS64bit = true;
  CPU64bit = true;
  Mode64bit = true;
  vendor = CPUVendor::ARM;

#ifdef _WIN32
  num_cores = std::thread::hardware_concurrency();

  // Windows does not provide any mechanism for querying the system registers on ARMv8, unlike Linux
  // which traps the register reads and emulates them in the kernel. There are environment variables
  // containing some of the CPU-specific values, which we could use for a lookup table in the
  // future. For now, assume all features are present as all known devices which are Windows-on-ARM
  // compatible also support these extensions.
  bFP = true;
  bASIMD = true;
  bAES = true;
  bCRC32 = false;
  bSHA1 = true;
  bSHA2 = true;
#else
  // Get the information about the CPU
  num_cores = sysconf(_SC_NPROCESSORS_CONF);
  strncpy(cpu_string, GetCPUString().c_str(), sizeof(cpu_string));

#ifdef IPHONEOS
  // A-series CPUs have all of these
  bFP = true;
  bASIMD = true;
  bAES = true;

  // Need to tweak compiler settings to have these
  bSHA1 = false;
  bSHA2 = false;

  // Default to no CRC32 support
  bCRC32 = false;

  // Default to no APRR
  bCanAPRR = false;
  bAPRR = false;
  iAPRRVer = 0;
  
  // Check for CRC32 intrinsics support via the device's model.
  // (Apple doesn't provide a better way to do this.)
  // Devices with A10 processors and above should support this.
  // These include:
  //
  // iPhone9,1 and above
  // iPad7,1 and above
  // iPod9,1 and above
  // AppleTV6,2 and above

  // Different processors have different capabilities.
  //
  // The following devices with A10 processors and above have
  // CRC32 intrinsics support:
  // iPhone9,1 and above
  // iPad7,1 and above
  // iPod9,1 and above
  // AppleTV6,2 and above
  //
  // The following devices with A11 processors and above have
  // APRR support:
  // iPhone10,1 and above
  // iPad8,1 and above
  //
  // The following devices with A13 processors have APRR version
  // 2, which uses different registers than the A11 and A12:
  // iPhone 12,1 and above
  //
  utsname system_info;
  uname(&system_info);

  const std::string model = std::string(system_info.machine);
  std::regex number_regex("[0-9]+");
  std::smatch match;

  if (std::regex_search(model.begin(), model.end(), match, number_regex))
  {
    int model_number = std::stoi(match[0]);

    if (model.find("iPhone") != std::string::npos)
    {
      if (model_number >= 9) // iPhone 7
      {
        bCRC32 = true;
      }

      if (model_number >= 10) // iPhone 8, iPhone X
      {
        bAPRR = true;

        if (model_number >= 12) // iPhone 11, iPhone SE 2nd Gen
        {
          iAPRRVer = 2;
        }
        else
        {
          iAPRRVer = 1;
        }
      }
    }
    else if (model.find("iPad") != std::string::npos)
    {
      if (model_number >= 7)
      {
        bCRC32 = true;
      }

      if (model_number >= 8)
      {
        bAPRR = true;
        
        if (model_number >= 12) // guess for A13 iPads
        {
          iAPRRVer = 2;
        }
        else
        {
          iAPRRVer = 1;
        }
      }
    }
    else if (model.find("iPod") != std::string::npos)
    {
      if (model_number >= 9)
      {
        bCRC32 = true;
      }
    }
    else if (model.find("AppleTV") != std::string::npos)
    {
      if (model_number >= 6)
      {
        bCRC32 = true;
      }
    }
  }

  bCanAPRR = bAPRR;
  bAPRR = false;
#else
  unsigned long hwcaps = getauxval(AT_HWCAP);
  bFP = hwcaps & HWCAP_FP;
  bASIMD = hwcaps & HWCAP_ASIMD;
  bAES = hwcaps & HWCAP_AES;
  bCRC32 = hwcaps & HWCAP_CRC32;
  bSHA1 = hwcaps & HWCAP_SHA1;
  bSHA2 = hwcaps & HWCAP_SHA2;
#endif
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
  if (bAPRR)
    sum += fmt::format(", APRR version {}", iAPRRVer);
  if (CPU64bit)
    sum += ", 64-bit";

  return sum;
}
