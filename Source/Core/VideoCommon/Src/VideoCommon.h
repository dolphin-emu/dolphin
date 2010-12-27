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

#ifndef _VIDEOCOMMON_H
#define _VIDEOCOMMON_H

#include "Common.h"
#include "MathUtil.h"
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
	// XFB width is decided by EFB copy operation. The VI can do horizontal
	// scaling (TODO: emulate).
	MAX_XFB_WIDTH = EFB_WIDTH,

	// Although EFB height is 528, 574-line XFB's can be created either with
	// vertical scaling by the EFB copy operation or copying to multiple XFB's
	// that are next to each other in memory (TODO: handle that situation).
	MAX_XFB_HEIGHT = 574
};

// If this is enabled, bounding boxes will be computed for everything drawn.
// This can theoretically have a big speed hit in some geom heavy games. Needs more work.
// Helps some effects in Paper Mario (but they aren't quite right yet).
// Do testing to figure out if the speed hit is bad?
// #define BBOX_SUPPORT

extern SVideoInitialize g_VideoInitialize;

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


inline float Memory_Read_Float(u32 _uAddress)
{
	union {u32 i; float f;} temp;
	temp.i = Memory_Read_U32(_uAddress);
	return temp.f;
}


// Logging
// ----------
void HandleGLError();


// This structure should only be used to represent a rectangle in EFB
// coordinates, where the origin is at the upper left and the frame dimensions
// are 640 x 528.
struct EFBRectangle : public MathUtil::Rectangle<int>
{};

// This structure should only be used to represent a rectangle in standard target
// coordinates, where the origin is at the lower left and the frame dimensions
// depend on the resolution settings. Use Renderer::ConvertEFBRectangle to
// convert an EFBRectangle to a TargetRectangle.
struct TargetRectangle : public MathUtil::Rectangle<int>
{
#ifdef _WIN32
	// Only used by D3D plugin.
	const RECT *AsRECT() const
	{
		// The types are binary compatible so this works.
		return (const RECT *)this;
	}
	RECT *AsRECT()
	{
		// The types are binary compatible so this works.
		return (RECT *)this;
	}
#endif
};

#ifdef _WIN32
#define PRIM_LOG(...) {DEBUG_LOG(VIDEO, __VA_ARGS__)}
#else
#define PRIM_LOG(...) {DEBUG_LOG(VIDEO, ##__VA_ARGS__)}
#endif


// #define LOG_VTX() DEBUG_LOG(VIDEO, "vtx: %f %f %f, ", ((float*)VertexManager::s_pCurBufferPointer)[0], ((float*)VertexManager::s_pCurBufferPointer)[1], ((float*)VertexManager::s_pCurBufferPointer)[2]);

#define LOG_VTX()

typedef enum
{
	API_OPENGL,
	API_D3D9,
	API_D3D11,
	API_GLSL,
	API_NONE
} API_TYPE;

inline u32 RGBA8ToRGBA6ToRGBA8(u32 src)
{
	u32 color = src;
	color &= 0xFCFCFCFC;
	color |= (color >> 6) & 0x03030303;
	return color;
}

inline u32 RGBA8ToRGB565ToRGB8(u32 src)
{
	u32 color = src;
	u32 dstr5 = (color & 0xFF0000) >> 19;
	u32 dstg6 = (color & 0xFF00) >> 10;
	u32 dstb5 = (color & 0xFF) >> 3;
	u32 dstr8 = (dstr5 << 3) | (dstr5 >> 2);
	u32 dstg8 = (dstg6 << 2) | (dstg6 >> 4);
	u32 dstb8 = (dstb5 << 3) | (dstb5 >> 2);
	color = (dstr8 << 16) | (dstg8 << 8) | dstb8;
	return color;
}


#endif  // _VIDEOCOMMON_H
