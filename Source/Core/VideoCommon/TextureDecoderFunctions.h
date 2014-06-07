// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once
#include "Common/Common.h"
#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/TextureDecoder.h"

// Texture decoding helper functions
static inline u32 decode5A3(u16 val)
{
	int r,g,b,a;
	if ((val & 0x8000))
	{
		a = 0xFF;
		r = Convert5To8((val >> 10) & 0x1F);
		g = Convert5To8((val >> 5) & 0x1F);
		b = Convert5To8(val & 0x1F);
	}
	else
	{
		a = Convert3To8((val >> 12) & 0x7);
		r = Convert4To8((val >> 8) & 0xF);
		g = Convert4To8((val >> 4) & 0xF);
		b = Convert4To8(val & 0xF);
	}
	return (a << 24) | (r << 16) | (g << 8) | b;
}

static inline u32 decode5A3RGBA(u16 val)
{
	int r,g,b,a;
	if ((val&0x8000))
	{
		r=Convert5To8((val>>10) & 0x1f);
		g=Convert5To8((val>>5 ) & 0x1f);
		b=Convert5To8((val    ) & 0x1f);
		a=0xFF;
	}
	else
	{
		a=Convert3To8((val>>12) & 0x7);
		r=Convert4To8((val>>8 ) & 0xf);
		g=Convert4To8((val>>4 ) & 0xf);
		b=Convert4To8((val    ) & 0xf);
	}
	return r | (g<<8) | (b << 16) | (a << 24);
}

static inline u32 decode565RGBA(u16 val)
{
	int r,g,b,a;
	r=Convert5To8((val>>11) & 0x1f);
	g=Convert6To8((val>>5 ) & 0x3f);
	b=Convert5To8((val    ) & 0x1f);
	a=0xFF;
	return  r | (g<<8) | (b << 16) | (a << 24);
}

static inline u32 decodeIA8Swapped(u16 val)
{
	int a = val & 0xFF;
	int i = val >> 8;
	return i | (i<<8) | (i<<16) | (a<<24);
}

struct DXTBlock
{
	u16 color1;
	u16 color2;
	u8 lines[4];
};

inline void decodebytesC4_5A3_To_BGRA32(u32 *dst, const u8 *src, int tlutaddr)
{
	u16 *tlut = (u16*)(texMem + tlutaddr);
	for (int x = 0; x < 4; x++)
	{
		u8 val = src[x];
		*dst++ = decode5A3(Common::swap16(tlut[val >> 4]));
		*dst++ = decode5A3(Common::swap16(tlut[val & 0xF]));
	}
}

inline void decodebytesC4_5A3_To_rgba32(u32 *dst, const u8 *src, int tlutaddr)
{
	u16 *tlut = (u16*)(texMem + tlutaddr);
	for (int x = 0; x < 4; x++)
	{
		u8 val = src[x];
		*dst++ = decode5A3RGBA(Common::swap16(tlut[val >> 4]));
		*dst++ = decode5A3RGBA(Common::swap16(tlut[val & 0xF]));
	}
}

inline void decodebytesC4_To_Raw16(u16* dst, const u8* src, int tlutaddr)
{
	u16* tlut = (u16*)(texMem+tlutaddr);
	for (int x = 0; x < 4; x++)
	{
		u8 val = src[x];
		*dst++ = Common::swap16(tlut[val >> 4]);
		*dst++ = Common::swap16(tlut[val & 0xF]);
	}
}

inline void decodebytesC4IA8_To_RGBA(u32* dst, const u8* src, int tlutaddr)
{
	u16* tlut = (u16*)(texMem+tlutaddr);
	for (int x = 0; x < 4; x++)
	{
		u8 val = src[x];
		*dst++ = decodeIA8Swapped(tlut[val >> 4]);
		*dst++ = decodeIA8Swapped(tlut[val & 0xF]);
	}
}

inline void decodebytesC4RGB565_To_RGBA(u32* dst, const u8* src, int tlutaddr)
{
	u16* tlut = (u16*)(texMem+tlutaddr);
	for (int x = 0; x < 4; x++)
	{
		u8 val = src[x];
		*dst++ = decode565RGBA(Common::swap16(tlut[val >> 4]));
		*dst++ = decode565RGBA(Common::swap16(tlut[val & 0xF]));
	}
}

inline void decodebytesC8_5A3_To_BGRA32(u32 *dst, const u8 *src, int tlutaddr)
{
	u16 *tlut = (u16*)(texMem + tlutaddr);
	for (int x = 0; x < 8; x++)
	{
		u8 val = src[x];
		*dst++ = decode5A3(Common::swap16(tlut[val]));
	}
}

inline void decodebytesC8_5A3_To_RGBA32(u32 *dst, const u8 *src, int tlutaddr)
{
	u16 *tlut = (u16*)(texMem + tlutaddr);
	for (int x = 0; x < 8; x++)
	{
		u8 val = src[x];
		*dst++ = decode5A3RGBA(Common::swap16(tlut[val]));
	}
}

inline void decodebytesC8_To_Raw16(u16* dst, const u8* src, int tlutaddr)
{
	u16* tlut = (u16*)(texMem + tlutaddr);
	for (int x = 0; x < 8; x++)
	{
		u8 val = src[x];
		*dst++ = Common::swap16(tlut[val]);
	}
}

inline void decodebytesC8IA8_To_RGBA(u32* dst, const u8* src, int tlutaddr)
{
	u16* tlut = (u16*)(texMem + tlutaddr);
	for (int x = 0; x < 8; x++)
	{
		*dst++ = decodeIA8Swapped(tlut[src[x]]);
	}
}

inline void decodebytesC8RGB565_To_RGBA(u32* dst, const u8* src, int tlutaddr)
{
	u16* tlut = (u16*)(texMem + tlutaddr);
	for (int x = 0; x < 8; x++)
	{
		*dst++ = decode565RGBA(Common::swap16(tlut[src[x]]));
	}
}

inline void decodebytesC14X2_5A3_To_BGRA32(u32 *dst, const u16 *src, int tlutaddr)
{
	u16 *tlut = (u16*)(texMem + tlutaddr);
	for (int x = 0; x < 4; x++)
	{
		u16 val = Common::swap16(src[x]);
		*dst++ = decode5A3(Common::swap16(tlut[(val & 0x3FFF)]));
	}
}

inline void decodebytesC14X2_5A3_To_RGBA(u32 *dst, const u16 *src, int tlutaddr)
{
	u16 *tlut = (u16*)(texMem + tlutaddr);
	for (int x = 0; x < 4; x++)
	{
		u16 val = Common::swap16(src[x]);
		*dst++ = decode5A3RGBA(Common::swap16(tlut[(val & 0x3FFF)]));
	}
}

inline void decodebytesC14X2_To_Raw16(u16* dst, const u16* src, int tlutaddr)
{
	u16* tlut = (u16*)(texMem + tlutaddr);
	for (int x = 0; x < 4; x++)
	{
		u16 val = Common::swap16(src[x]);
		*dst++ = Common::swap16(tlut[(val & 0x3FFF)]);
	}
}

inline void decodebytesC14X2IA8_To_RGBA(u32* dst, const u16* src, int tlutaddr)
{
	u16* tlut = (u16*)(texMem + tlutaddr);
	for (int x = 0; x < 4; x++)
	{
		u16 val = Common::swap16(src[x]);
		*dst++ = decodeIA8Swapped(tlut[(val & 0x3FFF)]);
	}
}

inline void decodebytesC14X2rgb565_To_RGBA(u32* dst, const u16* src, int tlutaddr)
{
	u16* tlut = (u16*)(texMem + tlutaddr);
	for (int x = 0; x < 4; x++)
	{
		u16 val = Common::swap16(src[x]);
		*dst++ = decode565RGBA(Common::swap16(tlut[(val & 0x3FFF)]));
	}
}

// Needs more speed.
inline void decodebytesIA4(u16 *dst, const u8 *src)
{
	for (int x = 0; x < 8; x++)
	{
		const u8 val = src[x];
		u8 a = Convert4To8(val >> 4);
		u8 l = Convert4To8(val & 0xF);
		dst[x] = (a << 8) | l;
	}
}

inline void decodebytesIA4RGBA(u32 *dst, const u8 *src)
{
	for (int x = 0; x < 8; x++)
	{
		const u8 val = src[x];
		u8 a = Convert4To8(val >> 4);
		u8 l = Convert4To8(val & 0xF);
		dst[x] = (a << 24) | l << 16 | l << 8 | l;
	}
}

inline void decodebytesRGB5A3(u32 *dst, const u16 *src)
{
#if 0
	for (int x = 0; x < 4; x++)
		dst[x] = decode5A3(Common::swap16(src[x]));
#else
	dst[0] = decode5A3(Common::swap16(src[0]));
	dst[1] = decode5A3(Common::swap16(src[1]));
	dst[2] = decode5A3(Common::swap16(src[2]));
	dst[3] = decode5A3(Common::swap16(src[3]));
#endif
}

inline void decodebytesRGB5A3rgba(u32 *dst, const u16 *src)
{
#if 0
	for (int x = 0; x < 4; x++)
		dst[x] = decode5A3RGBA(Common::swap16(src[x]));
#else
	dst[0] = decode5A3RGBA(Common::swap16(src[0]));
	dst[1] = decode5A3RGBA(Common::swap16(src[1]));
	dst[2] = decode5A3RGBA(Common::swap16(src[2]));
	dst[3] = decode5A3RGBA(Common::swap16(src[3]));
#endif
}

// This one is used by many video formats. It'd therefore be good if it was fast.
// Needs more speed.
inline void decodebytesARGB8_4(u32 *dst, const u16 *src, const u16 *src2)
{
#if 0
	for (int x = 0; x < 4; x++)
		dst[x] = Common::swap32((src2[x] << 16) | src[x]);
#else
	dst[0] = Common::swap32((src2[0] << 16) | src[0]);
	dst[1] = Common::swap32((src2[1] << 16) | src[1]);
	dst[2] = Common::swap32((src2[2] << 16) | src[2]);
	dst[3] = Common::swap32((src2[3] << 16) | src[3]);
#endif

	// This can probably be done in a few SSE pack/unpack instructions + pshufb
	// some unpack instruction x2:
	// ABABABABABABABAB 1212121212121212 ->
	// AB12AB12AB12AB12 AB12AB12AB12AB12
	// 2x pshufb->
	// 21BA21BA21BA21BA 21BA21BA21BA21BA
	// and we are done.
}

inline void decodebytesARGB8_4ToRgba(u32 *dst, const u16 *src, const u16 * src2)
{
#if 0
	for (int x = 0; x < 4; x++) {
		dst[x] =  ((src[x] & 0xFF) << 24) | ((src[x] & 0xFF00)>>8)  | (src2[x] << 8);
	}
#else
	dst[0] =  ((src[0] & 0xFF) << 24) | ((src[0] & 0xFF00)>>8)  | (src2[0] << 8);
	dst[1] =  ((src[1] & 0xFF) << 24) | ((src[1] & 0xFF00)>>8)  | (src2[1] << 8);
	dst[2] =  ((src[2] & 0xFF) << 24) | ((src[2] & 0xFF00)>>8)  | (src2[2] << 8);
	dst[3] =  ((src[3] & 0xFF) << 24) | ((src[3] & 0xFF00)>>8)  | (src2[3] << 8);
#endif
}

inline u32 makecol(int r, int g, int b, int a)
{
	return (a << 24)|(r << 16)|(g << 8)|b;
}

inline u32 makeRGBA(int r, int g, int b, int a)
{
	return (a<<24)|(b<<16)|(g<<8)|r;
}

void decodeDXTBlock(u32 *dst, const DXTBlock *src, int pitch);
void decodeDXTBlockRGBA(u32 *dst, const DXTBlock *src, int pitch);

