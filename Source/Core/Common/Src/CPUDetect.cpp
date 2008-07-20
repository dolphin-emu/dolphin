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

#ifdef _WIN32
#include <intrin.h>
#endif

#ifdef __linux__
//#include <config/i386/cpuid.h>
#include <xmmintrin.h>
void __cpuid(int info[4], int x) {}


#endif

#include <memory.h>

#include "Common.h"
#include "CPUDetect.h"

// This code was adapted from an example in MSDN:
CPUInfoStruct cpu_info;

void CPUInfoStruct::Detect()
{
#ifdef _M_IX86
	Mode64bit = false;
#elif defined (_M_X64)
	Mode64bit = true;
	OS64bit = true;
#endif
	numCores = 1;

#ifdef _WIN32
#ifdef _M_IX86
	BOOL f64 = FALSE;
	OS64bit = IsWow64Process(GetCurrentProcess(), &f64) && f64;
#endif
#endif

	// __cpuid with an InfoType argument of 0 returns the number of
	// valid Ids in CPUInfo[0] and the CPU identification string in
	// the other three array elements. The CPU identification string is
	// not in linear order. The code below arranges the information
	// in a human readable form.
	__cpuid(CPUInfo, 0);
	nIds = CPUInfo[0];
	memset(CPUString, 0, sizeof(CPUString));
	*((int*)CPUString) = CPUInfo[1];
	*((int*)(CPUString + 4)) = CPUInfo[3];
	*((int*)(CPUString + 8)) = CPUInfo[2];

	// Assume that everything non-intel is AMD
	if (memcmp(CPUString, "GenuineIntel", 12) == 0)
	{
		isAMD = false;
	}
	else
	{
		isAMD = true;
	}

	// Get the information associated with each valid Id
	for (unsigned int i = 0; i <= nIds; ++i)
	{
		__cpuid(CPUInfo, i);

		// Interpret CPU feature information.
		if  (i == 1)
		{
			nSteppingID = CPUInfo[0] & 0xf;
			nModel  = (CPUInfo[0] >> 4) & 0xf;
			nFamily = (CPUInfo[0] >> 8) & 0xf;
			nProcessorType  = (CPUInfo[0] >> 12) & 0x3;
			nExtendedmodel  = (CPUInfo[0] >> 16) & 0xf;
			nExtendedfamily = (CPUInfo[0] >> 20) & 0xff;
			nBrandIndex = CPUInfo[1] & 0xff;
			nCLFLUSHcachelinesize = ((CPUInfo[1] >> 8) & 0xff) * 8;
			nAPICPhysicalID = (CPUInfo[1] >> 24) & 0xff;
			bSSE3NewInstructions = (CPUInfo[2] & 0x1) || false;
			bMONITOR_MWAIT = (CPUInfo[2] & 0x8) || false;
			bCPLQualifiedDebugStore = (CPUInfo[2] & 0x10) || false;
			bThermalMonitor2 = (CPUInfo[2] & 0x100) || false;
			nFeatureInfo = CPUInfo[3];

			if (CPUInfo[2] & (1 << 23))
			{
				bPOPCNT = true;
			}

			if (CPUInfo[2] & (1 << 19))
			{
				bSSE4_1 = true;
			}

			if (CPUInfo[2] & (1 << 20))
			{
				bSSE4_2 = true;
			}
		}
	}

	// Calling __cpuid with 0x80000000 as the InfoType argument
	// gets the number of valid extended IDs.
	__cpuid(CPUInfo, 0x80000000);
	nExIds = CPUInfo[0];
	memset(CPUBrandString, 0, sizeof(CPUBrandString));

	// Get the information associated with each extended ID.
	for (unsigned int i = 0x80000000; i <= nExIds; ++i)
	{
		__cpuid(CPUInfo, i);

		// Interpret CPU brand string and cache information.
		if (i == 0x80000001)
		{
			// This block seems bugged.
			nFeatureInfo2 = CPUInfo[1]; // ECX
			bSSE5  = (nFeatureInfo2 & (1 << 11)) ? true : false;
			bLZCNT = (nFeatureInfo2 & (1 << 5)) ? true : false;
			bSSE4A = (nFeatureInfo2 & (1 << 6)) ? true : false;
			bLAHFSAHF64 = (nFeatureInfo2 & (1 << 0)) ? true : false;

			CPU64bit = (CPUInfo[2] & (1 << 29)) ? true : false;
		}
		else if  (i == 0x80000002)
		{
			memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
		}
		else if  (i == 0x80000003)
		{
			memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
		}
		else if  (i == 0x80000004)
		{
			memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
		}
		else if  (i == 0x80000006)
		{
			nCacheLineSize   = CPUInfo[2] & 0xff;
			nL2Associativity = (CPUInfo[2] >> 12) & 0xf;
			nCacheSizeK = (CPUInfo[2] >> 16) & 0xffff;
		}
		else if (i == 0x80000008)
		{
			int numLSB = (CPUInfo[2] >> 12) & 0xF;
			numCores = 1 << numLSB;
			//int coresPerDie = CPUInfo[2] & 0xFF;
			// numCores = coresPerDie;
		}
	}

	// Display all the information in user-friendly format.
	// printf_s("\n\nCPU String: %s\n", CPUString);

	if (nIds < 1)
	{
		bOldCPU = true;
	}

	nIds = 1;
	bx87FPUOnChip = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bVirtual_8086ModeEnhancement = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bDebuggingExtensions = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bPageSizeExtensions = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bTimeStampCounter = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bRDMSRandWRMSRSupport = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bPhysicalAddressExtensions = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bMachineCheckException = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bCMPXCHG8BInstruction = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bAPICOnChip = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bUnknown1 = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bSYSENTERandSYSEXIT = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bMemoryTypeRangeRegisters = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bPTEGlobalBit = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bMachineCheckArchitecture = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bConditionalMove_CompareInstruction = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bPageAttributeTable = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bPageSizeExtension = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bProcessorSerialNumber = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bCFLUSHExtension = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bUnknown2 = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bDebugStore = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bThermalMonitorandClockCtrl = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bMMXTechnology = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bFXSAVE_FXRSTOR = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bSSEExtensions = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bSSE2Extensions = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bSelfSnoop = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bHyper_threadingTechnology = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bThermalMonitor = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bUnknown4 = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;
	bPendBrkEN = (nFeatureInfo & nIds) ? true : false;
	nIds <<= 1;

	if  (nExIds < 0x80000004)
	{
		strcpy(CPUBrandString, "(unknown)");
	}
}


