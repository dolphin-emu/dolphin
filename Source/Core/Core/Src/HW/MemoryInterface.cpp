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

#include "Common.h"
#include "ChunkFile.h"

#include "../PowerPC/PowerPC.h"
#include "MemoryInterface.h"

namespace MemoryInterface
{

// internal hardware addresses
enum
{
	MEM_CHANNEL0_HI		= 0x000,
	MEM_CHANNEL0_LO		= 0x002,
	MEM_CHANNEL1_HI		= 0x004,
	MEM_CHANNEL1_LO		= 0x006,
	MEM_CHANNEL2_HI		= 0x008,
	MEM_CHANNEL2_LO		= 0x00A,
	MEM_CHANNEL3_HI		= 0x00C,
	MEM_CHANNEL3_LO		= 0x00E,
	MEM_CHANNEL_CTRL	= 0x010
};

struct MIMemStruct
{
	u32 Channel0_Addr;
	u32 Channel1_Addr;
	u32 Channel2_Addr;
	u32 Channel3_Addr;
	u32 Channel_Ctrl;
};

// STATE_TO_SAVE
static MIMemStruct miMem;

void DoState(PointerWrap &p)
{
	p.Do(miMem);
}

void Read16(u16& _uReturnValue, const u32 _iAddress)
{
	//0x30 -> 0x5a : gp memory metrics
	INFO_LOG(MEMMAP, "(r16) 0x%04x @ 0x%08x", 0, _iAddress);
	_uReturnValue = 0;
}

void Read32(u32& _uReturnValue, const u32 _iAddress)
{
	INFO_LOG(MEMMAP, "(r32) 0x%08x @ 0x%08x", 0, _iAddress);
	_uReturnValue = 0;
}

void Write32(const u32 _iValue, const u32 _iAddress)
{
	INFO_LOG(MEMMAP, "(w32) 0x%08x @ 0x%08x", _iValue, _iAddress);
}

//TODO : check
void Write16(const u16 _iValue, const u32 _iAddress) 
{
	INFO_LOG(MEMMAP, "(w16) 0x%04x @ 0x%08x", _iValue, _iAddress);
	switch(_iAddress & 0xFFF)
	{
	case MEM_CHANNEL0_HI: miMem.Channel0_Addr = (miMem.Channel0_Addr & 0xFFFF) | (_iValue<<16); return;
	case MEM_CHANNEL0_LO: miMem.Channel0_Addr = (miMem.Channel0_Addr & 0xFFFF0000) | (_iValue); return;
	case MEM_CHANNEL1_HI: miMem.Channel1_Addr = (miMem.Channel1_Addr & 0xFFFF) | (_iValue<<16); return;
	case MEM_CHANNEL1_LO: miMem.Channel1_Addr = (miMem.Channel1_Addr & 0xFFFF0000) | (_iValue); return;
	case MEM_CHANNEL2_HI: miMem.Channel2_Addr = (miMem.Channel2_Addr & 0xFFFF) | (_iValue<<16); return;
	case MEM_CHANNEL2_LO: miMem.Channel2_Addr = (miMem.Channel2_Addr & 0xFFFF0000) | (_iValue); return;
	case MEM_CHANNEL3_HI: miMem.Channel3_Addr = (miMem.Channel3_Addr & 0xFFFF) | (_iValue<<16); return;
	case MEM_CHANNEL3_LO: miMem.Channel3_Addr = (miMem.Channel3_Addr & 0xFFFF0000) | (_iValue); return;
	case MEM_CHANNEL_CTRL: miMem.Channel_Ctrl = _iValue; return;
	}
}

} // end of namespace MemoryInterface

