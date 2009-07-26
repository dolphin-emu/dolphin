// Copyright (C) 2003-2009 Dolphin Project.

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

#include <stdarg.h>
#include <stdio.h>

#include "Globals.h"
#include "Common.h"

extern u8* g_pMemory;

// debugger externals that are needed even in Release builds
bool gSSBM = true;
bool gSSBMremedy1 = true;
bool gSSBMremedy2 = true;
bool gSequenced = true;
bool gVolume = true;
bool gReset = false;
float ratioFactor; // a global to get the ratio factor from MixAdd

// TODO: Wii support? Most likely audio data still must be in the old 24MB TRAM.
#define RAM_MASK 0x1FFFFFF

u8 Memory_Read_U8(u32 _uAddress)
{
	_uAddress &= RAM_MASK;
	return g_pMemory[_uAddress];
}

u16 Memory_Read_U16(u32 _uAddress)
{
	_uAddress &= RAM_MASK;
	return Common::swap16(*(u16*)&g_pMemory[_uAddress]);
}

u32 Memory_Read_U32(u32 _uAddress)
{
	_uAddress &= RAM_MASK;
	return Common::swap32(*(u32*)&g_pMemory[_uAddress]);
}

float Memory_Read_Float(u32 _uAddress)
{
	u32 uTemp = Memory_Read_U32(_uAddress);
	return *(float*)&uTemp;
}
