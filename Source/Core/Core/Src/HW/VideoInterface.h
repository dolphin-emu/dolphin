// Copyright (C) 2003 Dolphin Project.

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
#ifndef _VIDEOINTERFACE_H
#define _VIDEOINTERFACE_H

#include "Common.h"
class PointerWrap;

namespace VideoInterface
{
	enum VIInterruptType
	{
		INT_PRERETRACE		= 0,
		INT_POSTRETRACE		= 1,
		INT_REG_2,
		INT_REG_3,
	};

	// For BIOS HLE
	void PreInit(bool _bNTSC);
	void SetRegionReg(char _region);

	void Init();	
	void DoState(PointerWrap &p);

	void Read8(u8& _uReturnValue, const u32 _uAddress);
	void Read16(u16& _uReturnValue, const u32 _uAddress);
	void Read32(u32& _uReturnValue, const u32 _uAddress);
				
	void Write16(const u16 _uValue, const u32 _uAddress);
	void Write32(const u32 _uValue, const u32 _uAddress);	
				
	void GenerateVIInterrupt(VIInterruptType _VIInterrupt);	

	// returns a pointer to the current visible xfb
	u8* GetXFBPointerTop();
	u8* GetXFBPointerBottom();

    // Update and draw framebuffer(s)
    void Update();

	// urgh, ugly externs.
	extern double ActualRefreshRate;
	extern double TargetRefreshRate;
	extern s64 SyncTicksProgress;

	// UpdateInterrupts: check if we have to generate a new VI Interrupt
	void UpdateInterrupts();

    // Change values pertaining to video mode
    void UpdateTiming();

	int getTicksPerLine();
};

#endif // _VIDEOINTERFACE_H


