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

#ifndef _VIDEOCOMMON_H
#define _VIDEOCOMMON_H

#include "Common.h"
#include "pluginspecs_video.h"

extern SVideoInitialize g_VideoInitialize;

void DebugLog(const char* _fmt, ...);

inline u8 *Memory_GetPtr(u32 _uAddress)
{
	return g_VideoInitialize.pGetMemoryPointer(_uAddress);//&g_pMemory[_uAddress & RAM_MASK];
}

inline u8 Memory_Read_U8(u32 _uAddress)
{
	return *(u8*)g_VideoInitialize.pGetMemoryPointer(_uAddress);//g_pMemory[_uAddress & RAM_MASK];
}

inline u16 Memory_Read_U16(u32 _uAddress)
{
	return Common::swap16(*(u16*)g_VideoInitialize.pGetMemoryPointer(_uAddress));
}

inline u32 Memory_Read_U32(u32 _uAddress)
{
	return Common::swap32(*(u32*)g_VideoInitialize.pGetMemoryPointer(_uAddress));
}

inline float Memory_Read_Float(u32 _uAddress)
{
	union {u32 i; float f;} temp;
	temp.i = Memory_Read_U32(_uAddress);
	return temp.f;
}

struct TRectangle
{ 
	int left, top, right, bottom;
};

#endif  // _VIDEOCOMMON_H
