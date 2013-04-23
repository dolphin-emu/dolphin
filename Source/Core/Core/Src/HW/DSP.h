// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _DSPINTERFACE_H
#define _DSPINTERFACE_H

#include "Common.h"
class PointerWrap;
class DSPEmulator;

namespace DSP
{

enum DSPInterruptType
{
	INT_DSP		= 0,
	INT_ARAM	= 1,
	INT_AID		= 2
};

// aram size and mask
enum
{
	ARAM_SIZE	= 0x01000000,	// 16 MB
	ARAM_MASK	= 0x00FFFFFF,
};

void Init(bool hle);
void Shutdown();

DSPEmulator *GetDSPEmulator();

void DoState(PointerWrap &p);

void GenerateDSPInterrupt(DSPInterruptType _DSPInterruptType, bool _bSet = true);
void GenerateDSPInterruptFromDSPEmu(DSPInterruptType _DSPInterruptType, bool _bSet = true);

// Read32
void Read16(u16& _uReturnValue, const u32 _uAddress);
void Read32(u32& _uReturnValue, const u32 _uAddress);	

// Write
void Write16(const u16 _uValue, const u32 _uAddress);
void Write32(const u32 _uValue, const u32 _uAddress);

// Audio/DSP Helper
u8 ReadARAM(const u32 _uAddress);
void WriteARAM(u8 value, u32 _uAddress);

// Debugger Helper
u8* GetARAMPtr();

void UpdateAudioDMA();
void UpdateDSPSlice(int cycles);

}// end of namespace DSP

#endif


