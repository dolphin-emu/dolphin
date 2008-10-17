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

#ifndef _UTILS_H
#define _UTILS_H

#include "Common.h"
#include "main.h"
#include "LookUpTables.h"

extern int frameCount;

LRESULT CALLBACK AboutProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

//#define RAM_MASK 0x1FFFFFF

inline u8 *Memory_GetPtr(u32 _uAddress)
{
    return g_VideoInitialize.pGetMemoryPointer(_uAddress);
}

inline u8 Memory_Read_U8(u32 _uAddress)
{    
	return *g_VideoInitialize.pGetMemoryPointer(_uAddress);
}

inline u16 Memory_Read_U16(u32 _uAddress)
{
    return _byteswap_ushort(*(u16*)g_VideoInitialize.pGetMemoryPointer(_uAddress));
//	return _byteswap_ushort(*(u16*)&g_pMemory[_uAddress & RAM_MASK]);
}

inline u32 Memory_Read_U32(u32 _uAddress)
{
    if (_uAddress == 0x020143a8)
    {
        int i =0;
    }
    return _byteswap_ulong(*(u32*)g_VideoInitialize.pGetMemoryPointer(_uAddress));
//	return _byteswap_ulong(*(u32*)&g_pMemory[_uAddress & RAM_MASK]);
}

inline float Memory_Read_Float(u32 _uAddress)
{
	u32 uTemp = Memory_Read_U32(_uAddress);
	return *(float*)&uTemp;
}

#endif
