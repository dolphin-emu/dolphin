// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <string>

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/Intrinsics.h"

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

static inline void __cpuid(int info[4], int function_id)
{
  return __cpuidex(info, function_id, 0);
}

#define _XCR_XFEATURE_ENABLED_MASK 0
static u64 _xgetbv(u32 index)
{
  u32 eax, edx;
  __asm__ __volatile__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));
  return ((u64)edx << 32) | eax;
}

#endif  // ifndef _WIN32

CPUInfo cpu_info;

CPUInfo::CPUInfo()
{
  Detect();
}

// Detects the various CPU features
void CPUInfo::Detect()
{
#ifdef _M_X86_64
  Mode64bit = true;
  OS64bit = true;
#endif
  num_cores = 1;

  // Set obvious defaults, for extra safety
  if (Mode64bit)
  {
    bSSE = true;
    bSSE2 = true;
    bLongMode = true;
  }

  // Assume CPU supports the CPUID instruction. Those that don't can barely
  // boot modern OS:es anyway.
  int cpu_id[4];

  // Detect CPU's CPUID capabilities, and grab CPU string
  __cpuid(cpu_id, 0x00000000);
  u32 max_std_fn = cpu_id[0];  // EAX
  std::memcpy(&brand_string[0], &cpu_id[1], sizeof(int));
  std::memcpy(&brand_string[4], &cpu_id[3], sizeof(int));
  std::memcpy(&brand_string[8], &cpu_id[2], sizeof(int));
  __cpuid(cpu_id, 0x80000000);
  u32 max_ex_fn = cpu_id[0];
  if (!strcmp(brand_string, "GenuineIntel"))
    vendor = VENDOR_INTEL;
  else if (!strcmp(brand_string, "AuthenticAMD"))
    vendor = VENDOR_AMD;
  else
    vendor = VENDOR_OTHER;

  // Set reasonable default brand string even if brand string not available.
  strcpy(cpu_string, brand_string);

  // Detect family and other misc stuff.
  bool ht = false;
  HTT = ht;
  logical_cpu_count = 1;
  if (max_std_fn >= 1)
  {
    __cpuid(cpu_id, 0x00000001);
    int family = ((cpu_id[0] >> 8) & 0xf) + ((cpu_id[0] >> 20) & 0xff);
    int model = ((cpu_id[0] >> 4) & 0xf) + ((cpu_id[0] >> 12) & 0xf0);
    // Detect people unfortunate enough to be running Dolphin on an Atom
    if (family == 6 &&
        (model == 0x1C || model == 0x26 || model == 0x27 || model == 0x35 || model == 0x36 ||
         model == 0x37 || model == 0x4A || model == 0x4D || model == 0x5A || model == 0x5D))
      bAtom = true;
    logical_cpu_count = (cpu_id[1] >> 16) & 0xFF;
    ht = (cpu_id[3] >> 28) & 1;

    if ((cpu_id[3] >> 25) & 1)
      bSSE = true;
    if ((cpu_id[3] >> 26) & 1)
      bSSE2 = true;
    if ((cpu_id[2]) & 1)
      bSSE3 = true;
    if ((cpu_id[2] >> 9) & 1)
      bSSSE3 = true;
    if ((cpu_id[2] >> 19) & 1)
      bSSE4_1 = true;
    if ((cpu_id[2] >> 20) & 1)
      bSSE4_2 = true;
    if ((cpu_id[2] >> 22) & 1)
      bMOVBE = true;
    if ((cpu_id[2] >> 25) & 1)
      bAES = true;

    if ((cpu_id[3] >> 24) & 1)
    {
      // We can use FXSAVE.
      bFXSR = true;
    }

    // AVX support requires 3 separate checks:
    //  - Is the AVX bit set in CPUID?
    //  - Is the XSAVE bit set in CPUID?
    //  - XGETBV result has the XCR bit set.
    if (((cpu_id[2] >> 28) & 1) && ((cpu_id[2] >> 27) & 1))
    {
      if ((_xgetbv(_XCR_XFEATURE_ENABLED_MASK) & 0x6) == 0x6)
      {
        bAVX = true;
        if ((cpu_id[2] >> 12) & 1)
          bFMA = true;
      }
    }

    if (max_std_fn >= 7)
    {
      __cpuidex(cpu_id, 0x00000007, 0x00000000);
      // careful; we can't enable AVX2 unless the XSAVE/XGETBV checks above passed
      if ((cpu_id[1] >> 5) & 1)
        bAVX2 = bAVX;
      if ((cpu_id[1] >> 3) & 1)
        bBMI1 = true;
      if ((cpu_id[1] >> 8) & 1)
        bBMI2 = true;
    }
  }

  bFlushToZero = bSSE;

  if (max_ex_fn >= 0x80000004)
  {
    // Extract CPU model string
    __cpuid(cpu_id, 0x80000002);
    memcpy(cpu_string, cpu_id, sizeof(cpu_id));
    __cpuid(cpu_id, 0x80000003);
    memcpy(cpu_string + 16, cpu_id, sizeof(cpu_id));
    __cpuid(cpu_id, 0x80000004);
    memcpy(cpu_string + 32, cpu_id, sizeof(cpu_id));
  }
  if (max_ex_fn >= 0x80000001)
  {
    // Check for more features.
    __cpuid(cpu_id, 0x80000001);
    if (cpu_id[2] & 1)
      bLAHFSAHF64 = true;
    if ((cpu_id[2] >> 5) & 1)
      bLZCNT = true;
    if ((cpu_id[2] >> 16) & 1)
      bFMA4 = true;
    if ((cpu_id[3] >> 29) & 1)
      bLongMode = true;
  }

  num_cores = (logical_cpu_count == 0) ? 1 : logical_cpu_count;

  if (max_ex_fn >= 0x80000008)
  {
    // Get number of cores. This is a bit complicated. Following AMD manual here.
    __cpuid(cpu_id, 0x80000008);
    int apic_id_core_id_size = (cpu_id[2] >> 12) & 0xF;
    if (apic_id_core_id_size == 0)
    {
      if (ht)
      {
        // New mechanism for modern Intel CPUs.
        if (vendor == VENDOR_INTEL)
        {
          __cpuidex(cpu_id, 0x00000004, 0x00000000);
          int cores_x_package = ((cpu_id[0] >> 26) & 0x3F) + 1;
          HTT = (cores_x_package < logical_cpu_count);
          cores_x_package = ((logical_cpu_count % cores_x_package) == 0) ? cores_x_package : 1;
          num_cores = (cores_x_package > 1) ? cores_x_package : num_cores;
          logical_cpu_count /= cores_x_package;
        }
      }
    }
    else
    {
      // Use AMD's new method.
      num_cores = (cpu_id[2] & 0xFF) + 1;
    }
  }
}

// Turn the CPU info into a string we can show
std::string CPUInfo::Summarize()
{
  std::string sum(cpu_string);
  sum += " (";
  sum += brand_string;
  sum += ")";

  if (bSSE)
    sum += ", SSE";
  if (bSSE2)
  {
    sum += ", SSE2";
    if (!bFlushToZero)
      sum += " (but not DAZ!)";
  }
  if (bSSE3)
    sum += ", SSE3";
  if (bSSSE3)
    sum += ", SSSE3";
  if (bSSE4_1)
    sum += ", SSE4.1";
  if (bSSE4_2)
    sum += ", SSE4.2";
  if (HTT)
    sum += ", HTT";
  if (bAVX)
    sum += ", AVX";
  if (bAVX2)
    sum += ", AVX2";
  if (bBMI1)
    sum += ", BMI1";
  if (bBMI2)
    sum += ", BMI2";
  if (bFMA)
    sum += ", FMA";
  if (bAES)
    sum += ", AES";
  if (bMOVBE)
    sum += ", MOVBE";
  if (bLongMode)
    sum += ", 64-bit support";
  return sum;
}
