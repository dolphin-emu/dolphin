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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "Common.h"
#include "MathUtil.h"
#include "VideoBackendBase.h"

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

// Logging
// ----------
void HandleGLError();


// This structure should only be used to represent a rectangle in EFB
// coordinates, where the origin is at the upper left and the frame dimensions
// are 640 x 528.
typedef MathUtil::Rectangle<int> EFBRectangle;

// This structure should only be used to represent a rectangle in standard target
// coordinates, where the origin is at the lower left and the frame dimensions
// depend on the resolution settings. Use Renderer::ConvertEFBRectangle to
// convert an EFBRectangle to a TargetRectangle.
struct TargetRectangle : public MathUtil::Rectangle<int>
{
#ifdef _WIN32
	// Only used by D3D backend.
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
	API_OPENGL = 1,
	API_D3D9_SM30 = 2,
	API_D3D9_SM20 = 4,
	API_D3D9 = 6,	
	API_D3D11 = 8,
	API_GLSL = 16,
	API_NONE = 32
} API_TYPE;

inline u32 RGBA8ToRGBA6ToRGBA8(u32 src)
{
	u32 color = src;
	color &= 0xFCFCFCFC;
	color |= (color >> 6) & 0x03030303;
	return color;
}

inline u32 RGBA8ToRGB565ToRGBA8(u32 src)
{
	u32 color = (src & 0xF8FCF8);
	color |= (color >> 5) & 0x070007;
	color |= (color >> 6) & 0x000300;
	color |= 0xFF000000;
	return color;
}

inline u32 Z24ToZ16ToZ24(u32 src)
{
	return (src & 0xFFFF00) | (src >> 16);
}

/* Returns the smallest power of 2 which is greater than or equal to num */
inline u32 MakePow2(u32 num)
{
	--num;
	num |= num >> 1;
	num |= num >> 2;
	num |= num >> 4;
	num |= num >> 8;
	num |= num >> 16;
	++num;
	return num;
}

// returns the exponent of the smallest power of two which is greater than val
inline unsigned int GetPow2(unsigned int val)
{
	unsigned int ret = 0;
	for (; val; val >>= 1)
		++ret;
	return ret;
}

#endif  // _VIDEOCOMMON_H
