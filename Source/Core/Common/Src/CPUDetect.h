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

#ifndef _CPUDETECT_H
#define _CPUDETECT_H

struct CPUInfoStruct
{
	bool isAMD;

	bool OS64bit;
	bool CPU64bit;
	bool Mode64bit;
	int numCores;

	char CPUString[0x20];
	char CPUBrandString[0x40];
	int CPUInfo[4];
	int nSteppingID;
	int nModel;
	int nFamily;
	int nProcessorType;
	int nExtendedmodel;
	int nExtendedfamily;
	int nBrandIndex;
	int nCLFLUSHcachelinesize;
	int nAPICPhysicalID;
	int nFeatureInfo;
	int nFeatureInfo2;
	int nCacheLineSize;
	int nL2Associativity;
	int nCacheSizeK;
	int nRet;
	unsigned int nIds, nExIds;

	bool bMONITOR_MWAIT;
	bool bCPLQualifiedDebugStore;
	bool bThermalMonitor2;

	bool bOldCPU;
	bool bx87FPUOnChip;
	bool bVirtual_8086ModeEnhancement;
	bool bDebuggingExtensions;
	bool bPageSizeExtensions;
	bool bTimeStampCounter;
	bool bRDMSRandWRMSRSupport;
	bool bPhysicalAddressExtensions;
	bool bMachineCheckException;
	bool bCMPXCHG8BInstruction;
	bool bAPICOnChip;
	bool bUnknown1;
	bool bSYSENTERandSYSEXIT;
	bool bMemoryTypeRangeRegisters;
	bool bPTEGlobalBit;
	bool bMachineCheckArchitecture;
	bool bConditionalMove_CompareInstruction;
	bool bPageAttributeTable;
	bool bPageSizeExtension;
	bool bProcessorSerialNumber;
	bool bCFLUSHExtension;
	bool bUnknown2;
	bool bDebugStore;
	bool bThermalMonitorandClockCtrl;
	bool bMMXTechnology;
	bool bFXSAVE_FXRSTOR;
	bool bSSEExtensions;
	bool bSSE2Extensions;
	bool bSSE3NewInstructions;
	bool bSelfSnoop;
	bool bHyper_threadingTechnology;
	bool bThermalMonitor;
	bool bUnknown4;
	bool bPendBrkEN;

	bool bPOPCNT;
	bool bSSE4_1;
	bool bSSE4_2;
	bool bSSE5;
	bool bLZCNT;
	bool bSSE4A;
	bool bLAHFSAHF64;

	void Detect();
};


extern CPUInfoStruct cpu_info;

inline void DetectCPU() {cpu_info.Detect();}


#endif

