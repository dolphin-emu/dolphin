// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
#include <memory.h>

#ifdef _WIN32
#include <intrin.h>
#else

//#include <config/i386/cpuid.h>
#include <xmmintrin.h>

// if you are on linux and this doesn't build, plz fix :)
static inline void do_cpuid(unsigned int *eax, unsigned int *ebx,
						    unsigned int *ecx, unsigned int *edx)
{
	// Note: EBX is reserved on Mac OS X, so it has to be restored at the end
	//       of the asm block.
	__asm__(
		"pushl  %%ebx;"
		"cpuid;"
		"movl   %%ebx,%1;"
		"popl   %%ebx;"
		: "=a" (*eax),
		  "=r" (*ebx),
		  "=c" (*ecx),
		  "=d" (*edx));
}

void __cpuid(int info[4], int x)
{
	unsigned int eax = x, ebx = 0, ecx = 0, edx = 0;
	do_cpuid(&eax, &ebx, &ecx, &edx);
	info[0] = eax;
	info[1] = ebx;
	info[2] = ecx;
	info[3] = edx;
}

#endif

#include "Common.h"
#include "CPUDetect.h"
#include "StringUtil.h"

CPUInfo cpu_info;

void CPUInfo::Detect()
{
	memset(this, 0, sizeof(*this));
#ifdef _M_IX86
	Mode64bit = false;
#elif defined (_M_X64)
	Mode64bit = true;
	OS64bit = true;
#endif
	num_cores = 1;

#ifdef _WIN32
#ifdef _M_IX86
	BOOL f64 = FALSE;
	OS64bit = IsWow64Process(GetCurrentProcess(), &f64) && f64;
#endif
#endif

	// Set obvious defaults, for extra safety
	if (Mode64bit)
	{
		bSSE = true;
		bSSE2 = true;
		bLongMode = true;
	}

	// Assume CPU supports the CPUID instruction. Those that don't can barely boot modern OS:es anyway.
	int cpu_id[4];
	memset(cpu_string, 0, sizeof(cpu_string));

	// Detect CPU's CPUID capabilities, and grab cpu string
	__cpuid(cpu_id, 0x00000000);
	u32 max_std_fn = cpu_id[0];  // EAX
	*((int *)cpu_string) = cpu_id[1];
	*((int *)(cpu_string + 4)) = cpu_id[3];
	*((int *)(cpu_string + 8)) = cpu_id[2];
	__cpuid(cpu_id, 0x80000000);
	u32 max_ex_fn = cpu_id[0];
	if (!strcmp(cpu_string, "GenuineIntel"))
		vendor = VENDOR_INTEL;
	else if (!strcmp(cpu_string, "AuthenticAMD"))
		vendor = VENDOR_AMD;
	else
		vendor = VENDOR_OTHER;

	// Set reasonable default brand string even if brand string not available.
	strcpy(brand_string, cpu_string);

	// Detect family and other misc stuff.
	bool HTT = false;
	int logical_cpu_count = 1;
	if (max_std_fn >= 1) {
		__cpuid(cpu_id, 0x00000001);
		logical_cpu_count = (cpu_id[1] >> 16) & 0xFF;
		if ((cpu_id[3] >> 28) & 1) {
			// wtf, we get here on my core 2
			HTT = true;
		}
		if ((cpu_id[3] >> 25) & 1) bSSE = true;
		if ((cpu_id[3] >> 26) & 1) bSSE2 = true;
		if (cpu_id[2] & 1) bSSE3 = true;
		if ((cpu_id[2] >> 9) & 1) bSSSE3 = true;
		if ((cpu_id[2] >> 19) & 1) bSSE4_1 = true;
		if ((cpu_id[2] >> 20) & 1) bSSE4_2 = true;
	}
	if (max_ex_fn >= 0x80000004) {
		// Extract brand string
		__cpuid(cpu_id, 0x80000002);
		memcpy(brand_string, cpu_id, sizeof(cpu_id));
		__cpuid(cpu_id, 0x80000003);
		memcpy(brand_string + 16, cpu_id, sizeof(cpu_id));
		__cpuid(cpu_id, 0x80000004);
		memcpy(brand_string + 32, cpu_id, sizeof(cpu_id));
	}
	if (max_ex_fn >= 0x80000001) {
		// Check for more features.
		__cpuid(cpu_id, 0x80000001);
		bool cmp_legacy = false;
		if (cpu_id[2] & 1) bLAHFSAHF64 = true;
		if (cpu_id[2] & 2) cmp_legacy = true; //wtf is this?
		if ((cpu_id[3] >> 29) & 1) bLongMode = true;
	}
	if (max_ex_fn >= 0x80000008) {
		// Get number of cores. This is a bit complicated. Following AMD manual here.
		__cpuid(cpu_id, 0x80000008);
		int apic_id_core_id_size = (cpu_id[2] >> 12) & 0xF;
		if (apic_id_core_id_size == 0) {
			// Use what AMD calls the "legacy method" to determine # of cores.
			if (HTT) {
				num_cores = logical_cpu_count;
			} else {
				num_cores = 1;
			}
		} else {
			// Use AMD's new method.
			num_cores = (cpu_id[2] & 0xFF) + 1;
		}
	} else {
		// Wild guess
		if (logical_cpu_count)
			num_cores = logical_cpu_count;
	}
}

std::string CPUInfo::Summarize()
{
	std::string sum = StringFromFormat("%s : %i cores. ", cpu_string, num_cores);
	if (bSSE) sum += "SSE";
	if (bSSE2) sum += ", SSE2";
	if (bSSE3) sum += ", SSE3";
	if (bSSSE3) sum += ", SSSE3";
	if (bSSE4_1) sum += ", SSE4.1";
	if (bSSE4_2) sum += ", SSE4.2";
	if (bLongMode) sum += ", 64-bit support";
	sum += "  (wrong? report)";
	return sum;
}