// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>

#include "Common/Common.h"
#include "Common/CPUDetect.h"
#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/TextureDecoder.h"
//#include "VideoCommon/VideoCommon.h" // to get debug logs
#include "VideoCommon/VideoConfig.h"

// GameCube/Wii texture decoder

// Decodes all known GameCube/Wii texture formats.
// by ector

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

//inline void decodebytesC4(u32 *dst, const u8 *src, int numbytes, int tlutaddr, int tlutfmt)
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

//inline void decodebytesC8(u32 *dst, const u8 *src, int numbytes, int tlutaddr, int tlutfmt)
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

void decodeDXTBlock(u32 *dst, const DXTBlock *src, int pitch)
{
	// S3TC Decoder (Note: GCN decodes differently from PC so we can't use native support)
	// Needs more speed.
	u16 c1 = Common::swap16(src->color1);
	u16 c2 = Common::swap16(src->color2);
	int blue1 = Convert5To8(c1 & 0x1F);
	int blue2 = Convert5To8(c2 & 0x1F);
	int green1 = Convert6To8((c1 >> 5) & 0x3F);
	int green2 = Convert6To8((c2 >> 5) & 0x3F);
	int red1 = Convert5To8((c1 >> 11) & 0x1F);
	int red2 = Convert5To8((c2 >> 11) & 0x1F);
	int colors[4];
	colors[0] = makecol(red1, green1, blue1, 255);
	colors[1] = makecol(red2, green2, blue2, 255);
	if (c1 > c2)
	{
		int blue3 = ((blue2 - blue1) >> 1) - ((blue2 - blue1) >> 3);
		int green3 = ((green2 - green1) >> 1) - ((green2 - green1) >> 3);
		int red3 = ((red2 - red1) >> 1) - ((red2 - red1) >> 3);
		colors[2] = makecol(red1 + red3, green1 + green3, blue1 + blue3, 255);
		colors[3] = makecol(red2 - red3, green2 - green3, blue2 - blue3, 255);
	}
	else
	{
		colors[2] = makecol((red1 + red2 + 1) / 2, // Average
							(green1 + green2 + 1) / 2,
							(blue1 + blue2 + 1) / 2, 255);
		colors[3] = makecol(red2, green2, blue2, 0);  // Color2 but transparent
	}

	for (int y = 0; y < 4; y++)
	{
		int val = src->lines[y];
		for (int x = 0; x < 4; x++)
		{
			dst[x] = colors[(val >> 6) & 3];
			val <<= 2;
		}
		dst += pitch;
	}
}

void decodeDXTBlockRGBA(u32 *dst, const DXTBlock *src, int pitch)
{
	// S3TC Decoder (Note: GCN decodes differently from PC so we can't use native support)
	// Needs more speed.
	u16 c1 = Common::swap16(src->color1);
	u16 c2 = Common::swap16(src->color2);
	int blue1 = Convert5To8(c1 & 0x1F);
	int blue2 = Convert5To8(c2 & 0x1F);
	int green1 = Convert6To8((c1 >> 5) & 0x3F);
	int green2 = Convert6To8((c2 >> 5) & 0x3F);
	int red1 = Convert5To8((c1 >> 11) & 0x1F);
	int red2 = Convert5To8((c2 >> 11) & 0x1F);
	int colors[4];
	colors[0] = makeRGBA(red1, green1, blue1, 255);
	colors[1] = makeRGBA(red2, green2, blue2, 255);
	if (c1 > c2)
	{
		int blue3 = ((blue2 - blue1) >> 1) - ((blue2 - blue1) >> 3);
		int green3 = ((green2 - green1) >> 1) - ((green2 - green1) >> 3);
		int red3 = ((red2 - red1) >> 1) - ((red2 - red1) >> 3);
		colors[2] = makeRGBA(red1 + red3, green1 + green3, blue1 + blue3, 255);
		colors[3] = makeRGBA(red2 - red3, green2 - green3, blue2 - blue3, 255);
	}
	else
	{
		colors[2] = makeRGBA((red1 + red2 + 1) / 2, // Average
							(green1 + green2 + 1) / 2,
							(blue1 + blue2 + 1) / 2, 255);
		colors[3] = makeRGBA(red2, green2, blue2, 0);  // Color2 but transparent
	}

	for (int y = 0; y < 4; y++)
	{
		int val = src->lines[y];
		for (int x = 0; x < 4; x++)
		{
			dst[x] = colors[(val >> 6) & 3];
			val <<= 2;
		}
		dst += pitch;
	}
}

#if 0   // TODO - currently does not handle transparency correctly and causes problems when texture dimensions are not multiples of 8
static void copyDXTBlock(u8* dst, const u8* src)
{
	((u16*)dst)[0] = Common::swap16(((u16*)src)[0]);
	((u16*)dst)[1] = Common::swap16(((u16*)src)[1]);
	u32 pixels = ((u32*)src)[1];
	// A bit of trickiness here: the row are in the same order
	// between the two formats, but the ordering within the rows
	// is reversed.
	pixels = ((pixels >> 4) & 0x0F0F0F0F) | ((pixels << 4) & 0xF0F0F0F0);
	pixels = ((pixels >> 2) & 0x33333333) | ((pixels << 2) & 0xCCCCCCCC);
	((u32*)dst)[1] = pixels;
}
#endif

static PC_TexFormat GetPCFormatFromTLUTFormat(int tlutfmt)
{
	switch (tlutfmt)
	{
	case 0: return PC_TEX_FMT_IA8;    // IA8
	case 1: return PC_TEX_FMT_RGB565; // RGB565
	case 2: return PC_TEX_FMT_BGRA32; // RGB5A3: This TLUT format requires
									  // extra work to decode.
	}
	return PC_TEX_FMT_NONE; // Error
}

PC_TexFormat GetPC_TexFormat(int texformat, int tlutfmt)
{
	switch (texformat)
	{
	case GX_TF_C4:
		return GetPCFormatFromTLUTFormat(tlutfmt);
	case GX_TF_I4:
		return PC_TEX_FMT_IA8;
	case GX_TF_I8:  // speed critical
		return PC_TEX_FMT_IA8;
	case GX_TF_C8:
		return GetPCFormatFromTLUTFormat(tlutfmt);
	case GX_TF_IA4:
		return PC_TEX_FMT_IA4_AS_IA8;
	case GX_TF_IA8:
		return PC_TEX_FMT_IA8;
	case GX_TF_C14X2:
		return GetPCFormatFromTLUTFormat(tlutfmt);
	case GX_TF_RGB565:
		return PC_TEX_FMT_RGB565;
	case GX_TF_RGB5A3:
		return PC_TEX_FMT_BGRA32;
	case GX_TF_RGBA8:  // speed critical
		return PC_TEX_FMT_BGRA32;
	case GX_TF_CMPR:  // speed critical
		// The metroid games use this format almost exclusively.
		{
			return PC_TEX_FMT_BGRA32;
		}
	}

	// The "copy" texture formats, too?
	return PC_TEX_FMT_NONE;
}


//switch endianness, unswizzle
//TODO: to save memory, don't blindly convert everything to argb8888
//also ARGB order needs to be swapped later, to accommodate modern hardware better
//need to add DXT support too
PC_TexFormat TexDecoder_Decode_real(u8 *dst, const u8 *src, int width, int height, int texformat, int tlutaddr, int tlutfmt)
{
	const int Wsteps4 = (width + 3) / 4;
	const int Wsteps8 = (width + 7) / 8;

	switch (texformat)
	{
	case GX_TF_C4:
		if (tlutfmt == 2)
		{
			// Special decoding is required for TLUT format 5A3
			for (int y = 0; y < height; y += 8)
				for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8, yStep++)
					for (int iy = 0, xStep = yStep * 8; iy < 8; iy++, xStep++)
						decodebytesC4_5A3_To_BGRA32((u32*)dst + (y + iy) * width + x, src + 4 * xStep, tlutaddr);
		}
		else
		{
			for (int y = 0; y < height; y += 8)
				for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8, yStep++)
					for (int iy = 0, xStep = yStep * 8; iy < 8; iy++, xStep++)
						decodebytesC4_To_Raw16((u16*)dst + (y + iy) * width + x, src + 4 * xStep, tlutaddr);
		}
		return GetPCFormatFromTLUTFormat(tlutfmt);
	case GX_TF_I4:
		{
			for (int y = 0; y < height; y += 8)
				for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8, yStep++)
					for (int iy = 0, xStep = yStep * 8 ; iy < 8; iy++,xStep++)
						for (int ix = 0; ix < 4; ix++)
						{
							int val = src[4 * xStep + ix];
							dst[(y + iy) * width + x + ix * 2] = Convert4To8(val >> 4);
							dst[(y + iy) * width + x + ix * 2 + 1] = Convert4To8(val & 0xF);
						}
		}
	   return PC_TEX_FMT_I4_AS_I8;
	case GX_TF_I8:  // speed critical
		{
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						((u64*)(dst + (y + iy) * width + x))[0] = ((u64*)(src + 8 * xStep))[0];
		}
		return PC_TEX_FMT_I8;
	case GX_TF_C8:
		if (tlutfmt == 2)
		{
			// Special decoding is required for TLUT format 5A3
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						decodebytesC8_5A3_To_BGRA32((u32*)dst + (y + iy) * width + x, src + 8 * xStep, tlutaddr);
		}
		else
		{

			{
				for (int y = 0; y < height; y += 4)
					for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
						for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
							decodebytesC8_To_Raw16((u16*)dst + (y + iy) * width + x, src  + 8 * xStep, tlutaddr);
			}
		}
		return GetPCFormatFromTLUTFormat(tlutfmt);
	case GX_TF_IA4:
		{
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						decodebytesIA4((u16*)dst + (y + iy) * width + x, src + 8 * xStep);
		}
		return PC_TEX_FMT_IA4_AS_IA8;
	case GX_TF_IA8:
		{
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
					for (int iy = 0, xStep = yStep * 4; iy < 4; iy++, xStep++)
					{
						u16 *ptr = (u16 *)dst + (y + iy) * width + x;
						u16 *s = (u16 *)(src + 8 * xStep);
						for (int j = 0; j < 4; j++)
							*ptr++ = Common::swap16(*s++);
					}
		}
		return PC_TEX_FMT_IA8;
	case GX_TF_C14X2:
		if (tlutfmt == 2)
		{
			// Special decoding is required for TLUT format 5A3
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						decodebytesC14X2_5A3_To_BGRA32((u32*)dst + (y + iy) * width + x, (u16*)(src + 8 * xStep), tlutaddr);
		}
		else
		{
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						decodebytesC14X2_To_Raw16((u16*)dst + (y + iy) * width + x,(u16*)(src + 8 * xStep), tlutaddr);
		}
		return GetPCFormatFromTLUTFormat(tlutfmt);
	case GX_TF_RGB565:
		{
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
					{
						u16 *ptr = (u16 *)dst + (y + iy) * width + x;
						u16 *s = (u16 *)(src + 8 * xStep);
						for (int j = 0; j < 4; j++)
							*ptr++ = Common::swap16(*s++);
					}
		}
		return PC_TEX_FMT_RGB565;
	case GX_TF_RGB5A3:
		{
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						//decodebytesRGB5A3((u32*)dst+(y+iy)*width+x, (u16*)src, 4);
						decodebytesRGB5A3((u32*)dst+(y+iy)*width+x, (u16*)(src + 8 * xStep));
		}
		return PC_TEX_FMT_BGRA32;
	case GX_TF_RGBA8:  // speed critical
		{
			for (int y = 0; y < height; y += 4)
			{
				for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
				{
					const u8* src2 = src + 64 * yStep;
					for (int iy = 0; iy < 4; iy++)
						decodebytesARGB8_4((u32*)dst + (y+iy)*width + x, (u16*)src2 + 4 * iy, (u16*)src2 + 4 * iy + 16);
				}
			}
		}
		return PC_TEX_FMT_BGRA32;
	case GX_TF_CMPR:  // speed critical
		// The metroid games use this format almost exclusively.
		{
#if 0   // TODO - currently does not handle transparency correctly and causes problems when texture dimensions are not multiples of 8
			// 11111111 22222222 55555555 66666666
			// 33333333 44444444 77777777 88888888
			for (int y = 0; y < height; y += 8)
			{
				for (int x = 0; x < width; x += 8)
				{
					copyDXTBlock(dst+(y/2)*width+x*2, src);
					src += 8;
					copyDXTBlock(dst+(y/2)*width+x*2+8, src);
					src += 8;
					copyDXTBlock(dst+(y/2+2)*width+x*2, src);
					src += 8;
					copyDXTBlock(dst+(y/2+2)*width+x*2+8, src);
					src += 8;
				}
			}
			return PC_TEX_FMT_DXT1;
#else
			for (int y = 0; y < height; y += 8)
			{
				for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8, yStep++)
				{
					const u8* src2 = src + 4 * sizeof(DXTBlock) * yStep;
					decodeDXTBlock((u32*)dst + y * width + x, (DXTBlock*)src2, width);
										src2 += sizeof(DXTBlock);
					decodeDXTBlock((u32*)dst + y * width + x + 4, (DXTBlock*)src2, width);
										src2 += sizeof(DXTBlock);
					decodeDXTBlock((u32*)dst + (y + 4) * width + x, (DXTBlock*)src2, width);
										src2 += sizeof(DXTBlock);
					decodeDXTBlock((u32*)dst + (y + 4) * width + x + 4, (DXTBlock*)src2, width);
				}
			}
#endif
			return PC_TEX_FMT_BGRA32;
		}
	}

	// The "copy" texture formats, too?
	return PC_TEX_FMT_NONE;
}



// JSD 01/06/11:
// TODO: we really should ensure BOTH the source and destination addresses are aligned to 16-byte boundaries to
// squeeze out a little more performance. _mm_loadu_si128/_mm_storeu_si128 is slower than _mm_load_si128/_mm_store_si128
// because they work on unaligned addresses. The processor is free to make the assumption that addresses are multiples
// of 16 in the aligned case.
// TODO: complete SSE2 optimization of less often used texture formats.
// TODO: refactor algorithms using _mm_loadl_epi64 unaligned loads to prefer 128-bit aligned loads.

PC_TexFormat TexDecoder_Decode_RGBA(u32 * dst, const u8 * src, int width, int height, int texformat, int tlutaddr, int tlutfmt)
{

	const int Wsteps4 = (width + 3) / 4;
	const int Wsteps8 = (width + 7) / 8;

	switch (texformat)
	{
	case GX_TF_C4:
		if (tlutfmt == 2)
		{
			// Special decoding is required for TLUT format 5A3
			for (int y = 0; y < height; y += 8)
				for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8,yStep++)
					for (int iy = 0, xStep =  8 * yStep; iy < 8; iy++,xStep++)
						decodebytesC4_5A3_To_rgba32(dst + (y + iy) * width + x, src + 4 * xStep, tlutaddr);
		}
		else if (tlutfmt == 0)
		{
			for (int y = 0; y < height; y += 8)
				for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8,yStep++)
					for (int iy = 0, xStep =  8 * yStep; iy < 8; iy++,xStep++)
						decodebytesC4IA8_To_RGBA(dst + (y + iy) * width + x, src + 4 * xStep, tlutaddr);
		}
		else
		{
			for (int y = 0; y < height; y += 8)
				for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8,yStep++)
					for (int iy = 0, xStep =  8 * yStep; iy < 8; iy++,xStep++)
						decodebytesC4RGB565_To_RGBA(dst + (y + iy) * width + x, src  + 4 * xStep, tlutaddr);
		}
		break;
	case GX_TF_I4:
		{
			// Reference C implementation:
			for (int y = 0; y < height; y += 8)
				for (int x = 0; x < width; x += 8)
					for (int iy = 0; iy < 8; iy++, src += 4)
						for (int ix = 0; ix < 4; ix++)
						{
							int val = src[ix];
							u8 i1 = Convert4To8(val >> 4);
							u8 i2 = Convert4To8(val & 0xF);
							memset(dst+(y + iy) * width + x + ix * 2 , i1,4);
							memset(dst+(y + iy) * width + x + ix * 2 + 1 , i2,4);
						}
		}
	   break;
	case GX_TF_I8:  // speed critical
		{
			// Reference C implementation
			for (int y = 0; y < height; y += 4)
				for (int x = 0; x < width; x += 8)
					for (int iy = 0; iy < 4; ++iy, src += 8)
					{
						u32 *  newdst = dst + (y + iy)*width+x;
						const u8 *  newsrc = src;
						u8 srcval;

						srcval = (newsrc++)[0]; (newdst++)[0] = srcval | (srcval << 8) | (srcval << 16) | (srcval << 24);
						srcval = (newsrc++)[0]; (newdst++)[0] = srcval | (srcval << 8) | (srcval << 16) | (srcval << 24);
						srcval = (newsrc++)[0]; (newdst++)[0] = srcval | (srcval << 8) | (srcval << 16) | (srcval << 24);
						srcval = (newsrc++)[0]; (newdst++)[0] = srcval | (srcval << 8) | (srcval << 16) | (srcval << 24);
						srcval = (newsrc++)[0]; (newdst++)[0] = srcval | (srcval << 8) | (srcval << 16) | (srcval << 24);
						srcval = (newsrc++)[0]; (newdst++)[0] = srcval | (srcval << 8) | (srcval << 16) | (srcval << 24);
						srcval = (newsrc++)[0]; (newdst++)[0] = srcval | (srcval << 8) | (srcval << 16) | (srcval << 24);
						srcval = newsrc[0]; newdst[0] = srcval | (srcval << 8) | (srcval << 16) | (srcval << 24);
					}
		}
		break;
	case GX_TF_C8:
		if (tlutfmt == 2)
		{
			// Special decoding is required for TLUT format 5A3
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						decodebytesC8_5A3_To_RGBA32((u32*)dst + (y + iy) * width + x, src + 8 * xStep, tlutaddr);
		}
		else if (tlutfmt == 0)
		{
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						decodebytesC8IA8_To_RGBA(dst + (y + iy) * width + x, src + 8 * xStep, tlutaddr);
		}
		else
		{
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						decodebytesC8RGB565_To_RGBA(dst + (y + iy) * width + x, src + 8 * xStep, tlutaddr);

		}
		break;
	case GX_TF_IA4:
		{
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						decodebytesIA4RGBA(dst + (y + iy) * width + x, src + 8 * xStep);
		}
		break;
	case GX_TF_IA8:
		{
			// Reference C implementation:
			for (int y = 0; y < height; y += 4)
				for (int x = 0; x < width; x += 4)
					for (int iy = 0; iy < 4; iy++, src += 8)
					{
						u32 *ptr = dst + (y + iy) * width + x;
						u16 *s = (u16 *)src;
						ptr[0] = decodeIA8Swapped(s[0]);
						ptr[1] = decodeIA8Swapped(s[1]);
						ptr[2] = decodeIA8Swapped(s[2]);
						ptr[3] = decodeIA8Swapped(s[3]);
					}
		}
		break;
	case GX_TF_C14X2:
		if (tlutfmt == 2)
		{
			// Special decoding is required for TLUT format 5A3
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						decodebytesC14X2_5A3_To_BGRA32(dst + (y + iy) * width + x, (u16*)(src + 8 * xStep), tlutaddr);
		}
		else if (tlutfmt == 0)
		{
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						decodebytesC14X2IA8_To_RGBA(dst + (y + iy) * width + x,  (u16*)(src + 8 * xStep), tlutaddr);
		}
		else
		{
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						decodebytesC14X2rgb565_To_RGBA(dst + (y + iy) * width + x, (u16*)(src + 8 * xStep), tlutaddr);
		}
		break;
	case GX_TF_RGB565:
		{
			// Reference C implementation.
			for (int y = 0; y < height; y += 4)
				for (int x = 0; x < width; x += 4)
					for (int iy = 0; iy < 4; iy++, src += 8)
					{
						u32 *ptr = dst + (y + iy) * width + x;
						u16 *s = (u16 *)src;
						for (int j = 0; j < 4; j++)
							*ptr++ = decode565RGBA(Common::swap16(*s++));
					}
		}
		break;
	case GX_TF_RGB5A3:
		{
			// Reference C implementation:
			for (int y = 0; y < height; y += 4)
				for (int x = 0; x < width; x += 4)
					for (int iy = 0; iy < 4; iy++, src += 8)
						decodebytesRGB5A3rgba(dst+(y+iy)*width+x, (u16*)src);
		}
		break;
	case GX_TF_RGBA8:  // speed critical
		{
			// Reference C implementation.
			for (int y = 0; y < height; y += 4)
				for (int x = 0; x < width; x += 4)
				{
					for (int iy = 0; iy < 4; iy++)
						decodebytesARGB8_4ToRgba(dst + (y+iy)*width + x, (u16*)src + 4 * iy, (u16*)src + 4 * iy + 16);
					src += 64;
				}
		}
		break;
	case GX_TF_CMPR:  // speed critical
		// The metroid games use this format almost exclusively.
		{
			for (int y = 0; y < height; y += 8)
			{
				for (int x = 0; x < width; x += 8)
				{
					decodeDXTBlockRGBA((u32*)dst + y * width + x, (DXTBlock*)src, width);
										src += sizeof(DXTBlock);
					decodeDXTBlockRGBA((u32*)dst + y * width + x + 4, (DXTBlock*)src, width);
										src += sizeof(DXTBlock);
					decodeDXTBlockRGBA((u32*)dst + (y + 4) * width + x, (DXTBlock*)src, width);
										src += sizeof(DXTBlock);
					decodeDXTBlockRGBA((u32*)dst + (y + 4) * width + x + 4, (DXTBlock*)src, width);
										src += sizeof(DXTBlock);
				}
			}
			break;
		}
	}

	// The "copy" texture formats, too?
	return PC_TEX_FMT_RGBA32;
}

PC_TexFormat TexDecoder_Decode(u8 *dst, const u8 *src, int width, int height, int texformat, int tlutaddr, int tlutfmt,bool rgbaOnly)
{
	PC_TexFormat retval = rgbaOnly ? TexDecoder_Decode_RGBA((u32*)dst, src,
			width, height, texformat, tlutaddr, tlutfmt)
		: TexDecoder_Decode_real(dst, src,
			width, height, texformat, tlutaddr, tlutfmt);

	TexDecoder_DrawOverlay(dst, width, height, texformat, retval);
	return retval;
}

void TexDecoder_DecodeTexel(u8 *dst, const u8 *src, int s, int t, int imageWidth, int texformat, int tlutaddr, int tlutfmt)
{
	/* General formula for computing texture offset
	//
	u16 sBlk = s / blockWidth;
	u16 tBlk = t / blockHeight;
	u16 widthBlks = (width / blockWidth) + 1;
	u32 base = (tBlk * widthBlks + sBlk) * blockWidth * blockHeight;
	u16 blkS = s & (blockWidth - 1);
	u16 blkT =  t & (blockHeight - 1);
	u32 blkOff = blkT * blockWidth + blkS;
	*/

	switch (texformat)
	{
	case GX_TF_C4:
		{
			u16 sBlk = s >> 3;
			u16 tBlk = t >> 3;
			u16 widthBlks = (imageWidth >> 3) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 5;
			u16 blkS = s & 7;
			u16 blkT =  t & 7;
			u32 blkOff = (blkT << 3) + blkS;

			int rs = (blkOff & 1)?0:4;
			u32 offset = base + (blkOff >> 1);

			u8 val = (*(src + offset) >> rs) & 0xF;
			u16 *tlut = (u16*)(texMem + tlutaddr);

			switch (tlutfmt)
			{
			case 0:
				*((u32*)dst) = decodeIA8Swapped(tlut[val]);
				break;
			case 1:
				*((u32*)dst) = decode565RGBA(Common::swap16(tlut[val]));
				break;
			case 2:
				*((u32*)dst) = decode5A3RGBA(Common::swap16(tlut[val]));
				break;
			}
		}
		break;
	case GX_TF_I4:
		{
			u16 sBlk = s >> 3;
			u16 tBlk = t >> 3;
			u16 widthBlks = (imageWidth >> 3) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 5;
			u16 blkS = s & 7;
			u16 blkT =  t & 7;
			u32 blkOff = (blkT << 3) + blkS;

			int rs = (blkOff & 1)?0:4;
			u32 offset = base + (blkOff >> 1);

			u8 val = (*(src + offset) >> rs) & 0xF;
			val = Convert4To8(val);
			dst[0] = val;
			dst[1] = val;
			dst[2] = val;
			dst[3] = val;
		}
	   break;
	case GX_TF_I8:
		{
			u16 sBlk = s >> 3;
			u16 tBlk = t >> 2;
			u16 widthBlks = (imageWidth >> 3) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 5;
			u16 blkS = s & 7;
			u16 blkT =  t & 3;
			u32 blkOff = (blkT << 3) + blkS;

			u8 val = *(src + base + blkOff);
			dst[0] = val;
			dst[1] = val;
			dst[2] = val;
			dst[3] = val;
		}
		break;
	case GX_TF_C8:
		{
			u16 sBlk = s >> 3;
			u16 tBlk = t >> 2;
			u16 widthBlks = (imageWidth >> 3) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 5;
			u16 blkS = s & 7;
			u16 blkT =  t & 3;
			u32 blkOff = (blkT << 3) + blkS;

			u8 val = *(src + base + blkOff);
			u16 *tlut = (u16*)(texMem + tlutaddr);

			switch (tlutfmt)
			{
			case 0:
				*((u32*)dst) = decodeIA8Swapped(tlut[val]);
				break;
			case 1:
				*((u32*)dst) = decode565RGBA(Common::swap16(tlut[val]));
				break;
			case 2:
				*((u32*)dst) = decode5A3RGBA(Common::swap16(tlut[val]));
				break;
			}
		}
		break;
	case GX_TF_IA4:
		{
			u16 sBlk = s >> 3;
			u16 tBlk = t >> 2;
			u16 widthBlks = (imageWidth >> 3) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 5;
			u16 blkS = s & 7;
			u16 blkT =  t & 3;
			u32 blkOff = (blkT << 3) + blkS;

			u8 val = *(src + base + blkOff);
			const u8 a = Convert4To8(val>>4);
			const u8 l = Convert4To8(val&0xF);
			dst[0] = l;
			dst[1] = l;
			dst[2] = l;
			dst[3] = a;
		}
		break;
	case GX_TF_IA8:
		{
			u16 sBlk = s >> 2;
			u16 tBlk = t >> 2;
			u16 widthBlks = (imageWidth >> 2) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 4;
			u16 blkS = s & 3;
			u16 blkT =  t & 3;
			u32 blkOff = (blkT << 2) + blkS;

			u32 offset = (base + blkOff) << 1;
			const u16* valAddr = (u16*)(src + offset);

			*((u32*)dst) = decodeIA8Swapped(*valAddr);
		}
		break;
	case GX_TF_C14X2:
		{
			u16 sBlk = s >> 2;
			u16 tBlk = t >> 2;
			u16 widthBlks = (imageWidth >> 2) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 4;
			u16 blkS = s & 3;
			u16 blkT =  t & 3;
			u32 blkOff = (blkT << 2) + blkS;

			u32 offset = (base + blkOff) << 1;
			const u16* valAddr = (u16*)(src + offset);

			u16 val = Common::swap16(*valAddr) & 0x3FFF;
			u16 *tlut = (u16*)(texMem + tlutaddr);

			switch (tlutfmt)
			{
			case 0:
				*((u32*)dst) = decodeIA8Swapped(tlut[val]);
				break;
			case 1:
				*((u32*)dst) = decode565RGBA(Common::swap16(tlut[val]));
				break;
			case 2:
				*((u32*)dst) = decode5A3RGBA(Common::swap16(tlut[val]));
				break;
			}
		}
		break;
	case GX_TF_RGB565:
		{
			u16 sBlk = s >> 2;
			u16 tBlk = t >> 2;
			u16 widthBlks = (imageWidth >> 2) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 4;
			u16 blkS = s & 3;
			u16 blkT =  t & 3;
			u32 blkOff = (blkT << 2) + blkS;

			u32 offset = (base + blkOff) << 1;
			const u16* valAddr = (u16*)(src + offset);

			*((u32*)dst) = decode565RGBA(Common::swap16(*valAddr));
		}
		break;
	case GX_TF_RGB5A3:
		{
			u16 sBlk = s >> 2;
			u16 tBlk = t >> 2;
			u16 widthBlks = (imageWidth >> 2) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 4;
			u16 blkS = s & 3;
			u16 blkT =  t & 3;
			u32 blkOff = (blkT << 2) + blkS;

			u32 offset = (base + blkOff) << 1;
			const u16* valAddr = (u16*)(src + offset);

			*((u32*)dst) = decode5A3RGBA(Common::swap16(*valAddr));
		}
		break;
	case GX_TF_RGBA8:
		{
			u16 sBlk = s >> 2;
			u16 tBlk = t >> 2;
			u16 widthBlks = (imageWidth >> 2) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 5; // shift by 5 is correct
			u16 blkS = s & 3;
			u16 blkT =  t & 3;
			u32 blkOff = (blkT << 2) + blkS;

			u32 offset = (base + blkOff) << 1 ;
			const u8* valAddr = src + offset;

			dst[3] = valAddr[0];
			dst[0] = valAddr[1];
			dst[1] = valAddr[32];
			dst[2] = valAddr[33];
		}
		break;
	case GX_TF_CMPR:
		{
			u16 sDxt = s >> 2;
			u16 tDxt = t >> 2;

			u16 sBlk = sDxt >> 1;
			u16 tBlk = tDxt >> 1;
			u16 widthBlks = (imageWidth >> 3) + 1;
			u32 base = (tBlk * widthBlks + sBlk) << 2;
			u16 blkS = sDxt & 1;
			u16 blkT =  tDxt & 1;
			u32 blkOff = (blkT << 1) + blkS;

			u32 offset = (base + blkOff) << 3;

			const DXTBlock* dxtBlock = (const DXTBlock*)(src + offset);

			u16 c1 = Common::swap16(dxtBlock->color1);
			u16 c2 = Common::swap16(dxtBlock->color2);
			int blue1 = Convert5To8(c1 & 0x1F);
			int blue2 = Convert5To8(c2 & 0x1F);
			int green1 = Convert6To8((c1 >> 5) & 0x3F);
			int green2 = Convert6To8((c2 >> 5) & 0x3F);
			int red1 = Convert5To8((c1 >> 11) & 0x1F);
			int red2 = Convert5To8((c2 >> 11) & 0x1F);

			u16 ss = s & 3;
			u16 tt = t & 3;

			int colorSel = dxtBlock->lines[tt];
			int rs = 6 - (ss << 1);
			colorSel = (colorSel >> rs) & 3;
			colorSel |= c1 > c2?0:4;

			u32 color = 0;

			switch (colorSel)
			{
				case 0:
				case 4:
					color = makeRGBA(red1, green1, blue1, 255);
					break;
				case 1:
				case 5:
					color = makeRGBA(red2, green2, blue2, 255);
					break;
				case 2:
					color = makeRGBA(red1+(red2-red1)/3, green1+(green2-green1)/3, blue1+(blue2-blue1)/3, 255);
					break;
				case 3:
					color = makeRGBA(red2+(red1-red2)/3, green2+(green1-green2)/3, blue2+(blue1-blue2)/3, 255);
					break;
				case 6:
					color = makeRGBA((int)ceil((float)(red1+red2)/2), (int)ceil((float)(green1+green2)/2), (int)ceil((float)(blue1+blue2)/2), 255);
					break;
				case 7:
					color = makeRGBA(red2, green2, blue2, 0);
					break;
			}

			*((u32*)dst) = color;
		}
		break;
	}
}

void TexDecoder_DecodeTexelRGBA8FromTmem(u8 *dst, const u8 *src_ar, const u8* src_gb, int s, int t, int imageWidth)
{
	u16 sBlk = s >> 2;
	u16 tBlk = t >> 2;
	u16 widthBlks = (imageWidth >> 2) + 1; // TODO: Looks wrong. Shouldn't this be ((imageWidth-1)>>2)+1 ?
	u32 base_ar = (tBlk * widthBlks + sBlk) << 4;
	u32 base_gb = (tBlk * widthBlks + sBlk) << 4;
	u16 blkS = s & 3;
	u16 blkT =  t & 3;
	u32 blk_off = (blkT << 2) + blkS;

	u32 offset_ar = (base_ar + blk_off) << 1;
	u32 offset_gb = (base_gb + blk_off) << 1;
	const u8* val_addr_ar = src_ar + offset_ar;
	const u8* val_addr_gb = src_gb + offset_gb;

	dst[3] = val_addr_ar[0]; // A
	dst[0] = val_addr_ar[1]; // R
	dst[1] = val_addr_gb[0]; // G
	dst[2] = val_addr_gb[1]; // B
}

PC_TexFormat TexDecoder_DecodeRGBA8FromTmem(u8* dst, const u8 *src_ar, const u8 *src_gb, int width, int height)
{
	// TODO for someone who cares: Make this less slow!
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x)
		{
			TexDecoder_DecodeTexelRGBA8FromTmem(dst, src_ar, src_gb, x, y, width-1);
			dst += 4;
		}

	return PC_TEX_FMT_RGBA32;
}
