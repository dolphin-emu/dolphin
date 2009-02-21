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

#if defined(_MSC_VER) && !defined(__x86_64__) && !defined(_M_X64)
void * memcpy_amd(void *dest, const void *src, size_t n);
unsigned char memcmp_mmx(const void* src1, const void* src2, int cmpsize);
#define memcpy_gc memcpy_amd
#define memcmp_gc memcmp_mmx
#else
#define memcpy_gc memcpy
#define memcmp_gc memcmp
#endif

enum {
	EFB_WIDTH = 640,
	EFB_HEIGHT = 528,
};

enum {
	XFB_WIDTH = 640,
	XFB_HEIGHT = 480, // 528 is max height ... ? or 538?
	// TODO: figure out what to do with PAL
};

extern SVideoInitialize g_VideoInitialize;
// (mb2) for XFB update hack. TODO: find a static better place
extern volatile u32 g_XFBUpdateRequested;

void DebugLog(const char* _fmt, ...);

//////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////
inline u8* Memory_Read_U8_Ptr(u32 _uAddress)
{
	return (u8*)g_VideoInitialize.pGetMemoryPointer(_uAddress);//g_pMemory[_uAddress & RAM_MASK];
}

inline u16* Memory_Read_U16_Unswapped_Ptr(u32 _uAddress)
{
	return (u16*)g_VideoInitialize.pGetMemoryPointer(_uAddress);
}

inline u32* Memory_Read_U32_Unswapped_Ptr(u32 _uAddress)
{
	return (u32*)g_VideoInitialize.pGetMemoryPointer(_uAddress);
}
//////////////////////////////////////////////////////////////////////////
inline u32 Memory_Read_U32_Unswapped(u32 _uAddress)
{
	return *(u32*)g_VideoInitialize.pGetMemoryPointer(_uAddress);
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



// Logging
// ¯¯¯¯¯¯¯¯¯¯
void DebugLog(const char* _fmt, ...); // This one goes to the main program
void HandleGLError();



#ifdef _WIN32
#define ERROR_LOG(...) {LOG(VIDEO, __VA_ARGS__)}
#define INFO_LOG(...) {LOG(VIDEO, __VA_ARGS__)}
#define PRIM_LOG(...) {LOG(VIDEO, __VA_ARGS__)}
#define DEBUG_LOG(...) {LOG(VIDEO, __VA_ARGS__)}

#else
#define ERROR_LOG(...) {LOG(VIDEO, ##__VA_ARGS__)}
#define INFO_LOG(...) {LOG(VIDEO, ##__VA_ARGS__)}
#define PRIM_LOG(...) {LOG(VIDEO, ##__VA_ARGS__)}
#define DEBUG_LOG(...) {LOG(VIDEO, ##__VA_ARGS__)}
#endif

#ifdef LOGGING
#define LOG_VTX() PRIM_LOG("vtx: %f %f %f, ", ((float*)VertexManager::s_pCurBufferPointer)[0], ((float*)VertexManager::s_pCurBufferPointer)[1], ((float*)VertexManager::s_pCurBufferPointer)[2]);
#else
#define LOG_VTX()
#endif

#endif  // _VIDEOCOMMON_H
