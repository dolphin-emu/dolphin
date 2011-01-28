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

// TODO: Get rid of this file.

#ifndef _GLOBALS_H
#define _GLOBALS_H

#include "Common.h"
#include "StringUtil.h"
#include "../Memmap.h"

inline u8 HLEMemory_Read_U8(u32 _uAddress)
{
	_uAddress &= Memory::RAM_MASK;
	return Memory::m_pRAM[_uAddress];
}

inline u16 HLEMemory_Read_U16(u32 _uAddress)
{
	_uAddress &= Memory::RAM_MASK;
	return Common::swap16(*(u16*)&Memory::m_pRAM[_uAddress]);
}

inline u32 HLEMemory_Read_U32(u32 _uAddress)
{
	_uAddress &= Memory::RAM_MASK;
	return Common::swap32(*(u32*)&Memory::m_pRAM[_uAddress]);
}

inline void* HLEMemory_Get_Pointer(u32 _uAddress)
{
	_uAddress &= Memory::RAM_MASK;
	return &Memory::m_pRAM[_uAddress];
}

#endif
