// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/CPUDetect.h"

#ifdef _WIN32
#include <windows.h>
#include <processthreadsapi.h>
#endif

#include <algorithm>
#include <cstring>
#include <string>
#include <thread>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/Intrinsics.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#ifndef _WIN32

#ifdef __FreeBSD__
#include <unistd.h>

#include <machine/cpufunc.h>
#include <sys/types.h>
#endif

static inline void __cpuidex(int info[4], int function_id, int subfunction_id)
{
#ifdef __FreeBSD__
  // Despite the name, this is just do_cpuid() with ECX as second input.
  cpuid_count((u_int)function_id, (u_int)subfunction_id, (u_int*)info);
#else
  info[0] = function_id;     // eax
  info[2] = subfunction_id;  // ecx
  __asm__("cpuid"
          : "=a"(info[0]), "=b"(info[1]), "=c"(info[2]), "=d"(info[3])
          : "a"(function_id), "c"(subfunction_id));
#endif
}

constexpr u32 XCR_XFEATURE_ENABLED_MASK = 0;

static u64 xgetbv(u32 index)
{
  u32 eax, edx;
  __asm__ __volatile__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));
  return ((u64)edx << 32) | eax;
}

#else

constexpr u32 XCR_XFEATURE_ENABLED_MASK = _XCR_XFEATURE_ENABLED_MASK;

static u64 xgetbv(u32 index)
{
  return _xgetbv(index);
}

static void WarnIfRunningUnderEmulation()
{
  // Starting with win11, arm64 windows can run x64 processes under emulation.
  // This detects such a scenario and informs the user they probably want to run a native build.
  PROCESS_MACHINE_INFORMATION info{};
  if (!GetProcessInformation(GetCurrentProcess(), ProcessMachineTypeInfo, &info, sizeof(info)))
  {
    // Possibly we are running on version of windows which doesn't support ProcessMachineTypeInfo.
    return;
  }
  if (info.MachineAttributes & MACHINE_ATTRIBUTES::KernelEnabled)
  {
    // KernelEnabled will be set if process arch matches the kernel arch - how we want people to run
    // dolphin.
    return;
  }

  // The process is not native; could use IsWow64Process2 to get native machine type, but for now
  // we can assume it is arm64.
  PanicAlertFmtT("This build of Dolphin is not natively compiled for your CPU.\n"
                 "Please run the ARM64 build of Dolphin for a better experience.");
}

#endif  // ifdef _WIN32

struct CPUIDResult
{
  u32 eax{}, ebx{}, ecx{}, edx{};
};
static_assert(sizeof(CPUIDResult) == sizeof(u32) * 4);

static inline CPUIDResult cpuid(int function_id, int subfunction_id = 0)
{
  CPUIDResult info;
  __cpuidex((int*)&info, function_id, subfunction_id);
  return info;
}

CPUInfo cpu_info;

CPUInfo::CPUInfo()
{
  Detect();
}

void CPUInfo::Detect()
{
#ifdef _WIN32
  WarnIfRunningUnderEmulation();
#endif

  // This should be much more reliable and easier than trying to get the number of cores out of the
  // CPUID data ourselves.
  num_cores = std::max(static_cast<int>(std::thread::hardware_concurrency()), 1);

  // Assume CPU supports the CPUID instruction. Those that don't can barely
  // boot modern OS anyway.

  // Detect CPU's CPUID capabilities and grab vendor string.
  auto info = cpuid(0);
  const u32 func_id_max = info.eax;

  std::string vendor_id;
  vendor_id.resize(sizeof(u32) * 3);
  std::memcpy(&vendor_id[0], &info.ebx, sizeof(u32));
  std::memcpy(&vendor_id[4], &info.edx, sizeof(u32));
  std::memcpy(&vendor_id[8], &info.ecx, sizeof(u32));
  TruncateToCString(&vendor_id);
  if (vendor_id == "GenuineIntel")
    vendor = CPUVendor::Intel;
  else if (vendor_id == "AuthenticAMD")
    vendor = CPUVendor::AMD;
  else
    vendor = CPUVendor::Other;

  // Detect family and other misc stuff.
  bool is_amd_family_17 = false;
  bool has_sse = false;
  if (func_id_max >= 1)
  {
    info = cpuid(1);
    const u32 version = info.eax;
    const u32 family = ((version >> 8) & 0xf) + ((version >> 20) & 0xff);
    const u32 model = ((version >> 4) & 0xf) + ((version >> 12) & 0xf0);
    const u32 stepping = version & 0xf;

    cpu_id = fmt::format("{:02X}:{:02X}:{:X}", family, model, stepping);

    // Detect people unfortunate enough to be running Dolphin on an Atom
    if (vendor == CPUVendor::Intel && family == 6 &&
        (model == 0x1C || model == 0x26 || model == 0x27 || model == 0x35 || model == 0x36 ||
         model == 0x37 || model == 0x4A || model == 0x4D || model == 0x5A || model == 0x5D))
      bAtom = true;

    // Detect AMD Zen1, Zen1+ and Zen2
    if (vendor == CPUVendor::AMD && family == 0x17)
      is_amd_family_17 = true;

    // AMD CPUs before Zen faked this flag and didn't actually
    // implement simultaneous multithreading (SMT; Intel calls it HTT)
    // but rather some weird middle-ground between 1-2 cores
    const bool ht = (info.edx >> 28) & 1;
    HTT = ht && (vendor == CPUVendor::Intel || (vendor == CPUVendor::AMD && family >= 0x17));

    if ((info.edx >> 25) & 1)
      has_sse = true;
    if (info.ecx & 1)
      bSSE3 = true;
    if ((info.ecx >> 9) & 1)
      bSSSE3 = true;
    if ((info.ecx >> 19) & 1)
      bSSE4_1 = true;
    if ((info.ecx >> 20) & 1)
      bSSE4_2 = true;
    if ((info.ecx >> 22) & 1)
      bMOVBE = true;
    if ((info.ecx >> 25) & 1)
      bAES = true;

    // AVX support requires 3 separate checks:
    //  - Is the AVX bit set in CPUID?
    //  - Is the XSAVE bit set in CPUID?
    //  - XGETBV result has the XCR bit set.
    if (((info.ecx >> 28) & 1) && ((info.ecx >> 27) & 1))
    {
      // Check that XSAVE can be used for SSE and AVX
      if ((xgetbv(XCR_XFEATURE_ENABLED_MASK) & 0b110) == 0b110)
      {
        bAVX = true;
        if ((info.ecx >> 12) & 1)
          bFMA = true;
      }
    }

    if (func_id_max >= 7)
    {
      info = cpuid(7);
      if ((info.ebx >> 3) & 1)
        bBMI1 = true;
      if ((info.ebx >> 8) & 1)
        bBMI2 = true;
      if ((info.ebx >> 29) & 1)
        bSHA1 = bSHA2 = true;
    }
  }

  info = cpuid(0x80000000);
  const u32 ext_func_id_max = info.eax;
  if (ext_func_id_max >= 0x80000004)
  {
    // Extract CPU model string
    model_name.resize(sizeof(info) * 3);
    for (u32 i = 0; i < 3; i++)
    {
      info = cpuid(0x80000002 + i);
      memcpy(&model_name[sizeof(info) * i], &info, sizeof(info));
    }
    TruncateToCString(&model_name);
    model_name = StripSpaces(model_name);
  }
  if (ext_func_id_max >= 0x80000001)
  {
    // Check for more features.
    info = cpuid(0x80000001);
    if ((info.ecx >> 5) & 1)
      bLZCNT = true;
    if ((info.ecx >> 16) & 1)
      bFMA4 = true;
  }

  // Computed flags
  bFlushToZero = has_sse;
  bBMI2FastParallelBitOps = bBMI2 && !is_amd_family_17;
  bCRC32 = bSSE4_2;

  model_name = ReplaceAll(model_name, ",", "_");
  cpu_id = ReplaceAll(cpu_id, ",", "_");
}

std::string CPUInfo::Summarize()
{
  std::vector<std::string> sum;
  sum.push_back(model_name);
  sum.push_back(cpu_id);

  if (bSSE3)
    sum.push_back("SSE3");
  if (bSSSE3)
    sum.push_back("SSSE3");
  if (bSSE4_1)
    sum.push_back("SSE4.1");
  if (bSSE4_2)
    sum.push_back("SSE4.2");
  if (HTT)
    sum.push_back("HTT");
  if (bAVX)
    sum.push_back("AVX");
  if (bBMI1)
    sum.push_back("BMI1");
  if (bBMI2)
    sum.push_back("BMI2");
  if (bFMA)
    sum.push_back("FMA");
  if (bMOVBE)
    sum.push_back("MOVBE");
  if (bAES)
    sum.push_back("AES");
  if (bCRC32)
    sum.push_back("CRC32");
  if (bSHA1)
    sum.push_back("SHA1");
  if (bSHA2)
    sum.push_back("SHA2");

  return JoinStrings(sum, ",");
}
