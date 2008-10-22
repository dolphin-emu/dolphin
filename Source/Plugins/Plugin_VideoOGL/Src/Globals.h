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

// This file should DIE.

#ifndef _GLOBALS_H
#define _GLOBALS_H

// #define LOGGING

#include "Common.h"
#include "x64Emitter.h"

#define ERROR_LOG __Log

#if defined(_DEBUG) || defined(DEBUGFAST)
#define INFO_LOG if( g_Config.iLog & 1 ) __Log
#define PRIM_LOG if( g_Config.iLog & 2 ) __Log
#define DEBUG_LOG __Log
#else
#define INFO_LOG(...)
#define PRIM_LOG(...)
#define DEBUG_LOG(...)
#endif

void DebugLog(const char* _fmt, ...);
void __Log(const char *format, ...);
void __Log(int type, const char *format, ...);
void HandleGLError();

#if defined(_MSC_VER) && !defined(__x86_64__) && !defined(_M_X64)
void * memcpy_amd(void *dest, const void *src, size_t n);
unsigned char memcmp_mmx(const void* src1, const void* src2, int cmpsize);
#define memcpy_gc memcpy_amd
#define memcmp_gc memcmp_mmx
#else
#define memcpy_gc memcpy
#define memcmp_gc memcmp
#endif

#include "main.h"

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
    //return _byteswap_ushort(*(u16*)&g_pMemory[_uAddress & RAM_MASK]);
}

inline u32 Memory_Read_U32(u32 _uAddress)
{
    return Common::swap32(*(u32*)g_VideoInitialize.pGetMemoryPointer(_uAddress));
    //return _byteswap_ulong(*(u32*)&g_pMemory[_uAddress & RAM_MASK]);
}

inline float Memory_Read_Float(u32 _uAddress)
{
    union {u32 i; float f;} temp;
	temp.i = Memory_Read_U32(_uAddress);
    return temp.f;
}

#endif
