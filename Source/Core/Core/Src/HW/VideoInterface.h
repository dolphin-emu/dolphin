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
// NTSC is 60 FPS, right?
// Wrong, it's about 59.94 FPS. The NTSC engineers had to slightly lower
// the field rate from 60 FPS when they added color to the standard.
// This was done to prevent analog interference between the video and
// audio signals. PAL has no similar reduction; it is exactly 50 FPS.
//#define NTSC_FIELD_RATE		(60.0f / 1.001f)
#define NTSC_FIELD_RATE		60
#define NTSC_LINE_COUNT		525
// These line numbers indicate the beginning of the "active video" in a frame.
// An NTSC frame has the lower field first followed by the upper field.
// TODO: Is this true for PAL-M? Is this true for EURGB60?
#define NTSC_LOWER_BEGIN		21
#define NTSC_UPPER_BEGIN		283

//#define PAL_FIELD_RATE		50.0f
#define PAL_FIELD_RATE		50
#define PAL_LINE_COUNT		625
// These line numbers indicate the beginning of the "active video" in a frame.
// A PAL frame has the upper field first followed by the lower field.
#define PAL_UPPER_BEGIN		23	// TODO: Actually 23.5!
#define PAL_LOWER_BEGIN		336

	// urgh, ugly externs.
	extern u32 TargetRefreshRate;

	// For BS2 HLE
	void PreInit(bool _bNTSC);
	void SetRegionReg(char _region);

	void Init();	
	void DoState(PointerWrap &p);

	void Read8(u8& _uReturnValue, const u32 _uAddress);
	void Read16(u16& _uReturnValue, const u32 _uAddress);
	void Read32(u32& _uReturnValue, const u32 _uAddress);
				
	void Write16(const u16 _uValue, const u32 _uAddress);
	void Write32(const u32 _uValue, const u32 _uAddress);	

	// returns a pointer to the current visible xfb
	u8* GetXFBPointerTop();
	u8* GetXFBPointerBottom();

    // Update and draw framebuffer(s)
    void Update();

	// UpdateInterrupts: check if we have to generate a new VI Interrupt
	void UpdateInterrupts();

    // Change values pertaining to video mode
    void UpdateTiming();

	int GetTicksPerLine();
	int GetTicksPerFrame();
};

#endif // _VIDEOINTERFACE_H
