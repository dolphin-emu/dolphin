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

#ifndef _COMMANDPROCESSOR_H
#define _COMMANDPROCESSOR_H

#include "Common.h"
#include "pluginspecs_video.h"
class PointerWrap;

#ifdef _WIN32
#include <windows.h>
#endif

extern bool MT;
namespace CommandProcessor
{
// internal hardware addresses
enum
{
	STATUS_REGISTER				= 0x00,
	CTRL_REGISTER				= 0x02,
	CLEAR_REGISTER				= 0x04,
	FIFO_TOKEN_REGISTER			= 0x0E,
	FIFO_BOUNDING_BOX_LEFT		= 0x10,
	FIFO_BOUNDING_BOX_RIGHT		= 0x12,
	FIFO_BOUNDING_BOX_TOP		= 0x14,
	FIFO_BOUNDING_BOX_BOTTOM	= 0x16,
	FIFO_BASE_LO				= 0x20,
	FIFO_BASE_HI				= 0x22,
	FIFO_END_LO					= 0x24,
	FIFO_END_HI					= 0x26,
	FIFO_HI_WATERMARK_LO		= 0x28,
	FIFO_HI_WATERMARK_HI		= 0x2a,
	FIFO_LO_WATERMARK_LO		= 0x2c,
	FIFO_LO_WATERMARK_HI		= 0x2e,
	FIFO_RW_DISTANCE_LO			= 0x30,
	FIFO_RW_DISTANCE_HI			= 0x32,
	FIFO_WRITE_POINTER_LO		= 0x34,
	FIFO_WRITE_POINTER_HI		= 0x36,
	FIFO_READ_POINTER_LO		= 0x38,
	FIFO_READ_POINTER_HI		= 0x3A,
	FIFO_BP_LO					= 0x3C,
	FIFO_BP_HI					= 0x3E
};

extern SCPFifoStruct fifo;

// Init
void Init();
void Shutdown();
void DoState(PointerWrap &p);

// Read
void HWCALL Read16(u16& _rReturnValue, const u32 _Address);
void HWCALL Write16(const u16 _Data, const u32 _Address);
void HWCALL Read32(u32& _rReturnValue, const u32 _Address);
void HWCALL Write32(const u32 _Data, const u32 _Address);

// for CGPFIFO
void CatchUpGPU();
void GatherPipeBursted();
void UpdateInterrupts();
void UpdateInterruptsFromVideoPlugin();

bool AllowIdleSkipping();

// for DC GP watchdog hack
void IncrementGPWDToken();
void WaitForFrameFinish();

} // end of namespace CommandProcessor

#endif

