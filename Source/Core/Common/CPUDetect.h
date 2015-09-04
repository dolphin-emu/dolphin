// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.


// Detect the CPU, so we'll know which optimizations to use
#pragma once

#include <string>

enum CPUVendor
{
	VENDOR_INTEL = 0,
	VENDOR_AMD = 1,
	VENDOR_ARM = 2,
	VENDOR_OTHER = 3,
};

struct CPUInfo
{
	CPUVendor vendor = VENDOR_INTEL;

	char cpu_string[0x41] = {};
	char brand_string[0x21] = {};
	bool OS64bit = false;
	bool CPU64bit = false;
	bool Mode64bit = false;

	bool HTT = false;
	int num_cores = 0;
	int logical_cpu_count = 0;

	bool bSSE    = false;
	bool bSSE2   = false;
	bool bSSE3   = false;
	bool bSSSE3  = false;
	bool bPOPCNT = false;
	bool bSSE4_1 = false;
	bool bSSE4_2 = false;
	bool bLZCNT  = false;
	bool bSSE4A  = false;
	bool bAVX    = false;
	bool bAVX2   = false;
	bool bBMI1   = false;
	bool bBMI2   = false;
	bool bFMA    = false;
	bool bFMA4   = false;
	bool bAES    = false;
	// FXSAVE/FXRSTOR
	bool bFXSR   = false;
	bool bMOVBE  = false;
	// This flag indicates that the hardware supports some mode
	// in which denormal inputs _and_ outputs are automatically set to (signed) zero.
	bool bFlushToZero = false;
	bool bLAHFSAHF64 = false;
	bool bLongMode = false;
	bool bAtom = false;

	// ARMv8 specific
	bool bFP    = false;
	bool bASIMD = false;
	bool bCRC32 = false;
	bool bSHA1  = false;
	bool bSHA2  = false;

	// Call Detect()
	explicit CPUInfo();

	// Turn the CPU info into a string we can show
	std::string Summarize();

private:
	// Detects the various CPU features
	void Detect();
};

extern CPUInfo cpu_info;
