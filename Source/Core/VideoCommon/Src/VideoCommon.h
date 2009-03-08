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

// These are accurate (disregarding AA modes).
enum
{
	EFB_WIDTH = 640,
	EFB_HEIGHT = 528,
};

enum
{
	XFB_WIDTH = 640,
	XFB_HEIGHT = 480, // 574 can be used with tricks (multi pass render and dual xfb copies, etc).
	// TODO: figure out what to do with PAL
};

extern SVideoInitialize g_VideoInitialize;
// (mb2) for XFB update hack. TODO: find a static better place
extern volatile u32 g_XFBUpdateRequested;

void DebugLog(const char* _fmt, ...);

//////////////////////////////////////////////////////////////////////////
inline u8 *Memory_GetPtr(u32 _uAddress)
{
	return g_VideoInitialize.pGetMemoryPointer(_uAddress);
}

inline u8 Memory_Read_U8(u32 _uAddress)
{
	return *(u8*)g_VideoInitialize.pGetMemoryPointer(_uAddress);
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
	return (u8*)g_VideoInitialize.pGetMemoryPointer(_uAddress);
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

inline float Memory_Read_Float(u32 _uAddress)
{
	union {u32 i; float f;} temp;
	temp.i = Memory_Read_U32(_uAddress);
	return temp.f;
}

struct TRectangle
{ 
	int left;
	int top;
	int right;
	int bottom;

	int GetWidth()  const { return right - left; }
	int GetHeight() const { return bottom - top; }

	void FlipYPosition(int y_height, TRectangle *dest) const
	{
		int offset = y_height - (bottom - top);
		dest->left = left;
		dest->top = top + offset;
		dest->right = right;
		dest->bottom = bottom + offset;
	}

	void FlipY(int y_height, TRectangle *dest) const {
		dest->left = left;
		dest->right = right;
		dest->bottom = y_height - bottom;
		dest->top = y_height - top;
	}

	void Scale(float factor_x, float factor_y, TRectangle *dest) const
	{
		dest->left   = (int)(factor_x * left);
		dest->right  = (int)(factor_x * right);
		dest->top    = (int)(factor_y * top);
		dest->bottom = (int)(factor_y * bottom);
	}
};

// Logging
// ¯¯¯¯¯¯¯¯¯¯
void DebugLog(const char *_fmt, ...); // This one goes to the main program
void HandleGLError();



#ifdef _WIN32
#define PRIM_LOG(...) {DEBUG_LOG(VIDEO, __VA_ARGS__)}
#else
#define PRIM_LOG(...) {DEBUG_LOG(VIDEO, ##__VA_ARGS__)}
#endif

#define LOG_VTX() DEBUG_LOG(VIDEO, "vtx: %f %f %f, ", ((float*)VertexManager::s_pCurBufferPointer)[0], ((float*)VertexManager::s_pCurBufferPointer)[1], ((float*)VertexManager::s_pCurBufferPointer)[2]);


#endif  // _VIDEOCOMMON_H
