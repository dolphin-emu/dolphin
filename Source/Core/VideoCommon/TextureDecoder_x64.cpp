// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>

#include "Common/Common.h"
//#include "VideoCommon.h" // to get debug logs
#include "Common/CPUDetect.h"

#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoConfig.h"

#ifdef _OPENMP
#include <omp.h>
#elif defined __GNUC__
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#endif

#if _M_SSE >= 0x401
#include <smmintrin.h>
#include <emmintrin.h>
#elif _M_SSE >= 0x301 && !(defined __GNUC__ && !defined __SSSE3__)
#include <tmmintrin.h>
#endif

// This avoids a harmless warning from a system header in Clang;
// see http://llvm.org/bugs/show_bug.cgi?id=16093
#ifdef __clang__
#pragma clang diagnostic ignored "-Wshadow"
#endif

bool TexFmt_Overlay_Enable=false;
bool TexFmt_Overlay_Center=false;

extern const char* texfmt[];
extern const unsigned char sfont_map[];
extern const unsigned char sfont_raw[][9*10];

// TRAM
// STATE_TO_SAVE
 GC_ALIGNED16(u8 texMem[TMEM_SIZE]);


// Gamecube/Wii texture decoder

// Decodes all known Gamecube/Wii texture formats.
// by ector

int TexDecoder_GetTexelSizeInNibbles(int format)
{
	switch (format & 0x3f) {
	case GX_TF_I4: return 1;
	case GX_TF_I8: return 2;
	case GX_TF_IA4: return 2;
	case GX_TF_IA8: return 4;
	case GX_TF_RGB565: return 4;
	case GX_TF_RGB5A3: return 4;
	case GX_TF_RGBA8: return 8;
	case GX_TF_C4: return 1;
	case GX_TF_C8: return 2;
	case GX_TF_C14X2: return 4;
	case GX_TF_CMPR: return 1;
	case GX_CTF_R4:    return 1;
	case GX_CTF_RA4:   return 2;
	case GX_CTF_RA8:   return 4;
	case GX_CTF_YUVA8: return 8;
	case GX_CTF_A8:    return 2;
	case GX_CTF_R8:    return 2;
	case GX_CTF_G8:    return 2;
	case GX_CTF_B8:    return 2;
	case GX_CTF_RG8:   return 4;
	case GX_CTF_GB8:   return 4;

	case GX_TF_Z8:     return 2;
	case GX_TF_Z16:    return 4;
	case GX_TF_Z24X8:  return 8;

	case GX_CTF_Z4:    return 1;
	case GX_CTF_Z8M:   return 2;
	case GX_CTF_Z8L:   return 2;
	case GX_CTF_Z16L:  return 4;
	default: return 1;
	}
}

int TexDecoder_GetTextureSizeInBytes(int width, int height, int format)
{
	return (width * height * TexDecoder_GetTexelSizeInNibbles(format)) / 2;
}

int TexDecoder_GetBlockWidthInTexels(u32 format)
{
	switch (format)
	{
	case GX_TF_I4: return 8;
	case GX_TF_I8: return 8;
	case GX_TF_IA4: return 8;
	case GX_TF_IA8: return 4;
	case GX_TF_RGB565: return 4;
	case GX_TF_RGB5A3: return 4;
	case GX_TF_RGBA8: return  4;
	case GX_TF_C4: return 8;
	case GX_TF_C8: return 8;
	case GX_TF_C14X2: return 4;
	case GX_TF_CMPR: return 8;
	case GX_CTF_R4: return 8;
	case GX_CTF_RA4: return 8;
	case GX_CTF_RA8: return 4;
	case GX_CTF_A8: return 8;
	case GX_CTF_R8: return 8;
	case GX_CTF_G8: return 8;
	case GX_CTF_B8: return 8;
	case GX_CTF_RG8: return 4;
	case GX_CTF_GB8: return 4;
	case GX_TF_Z8: return 8;
	case GX_TF_Z16: return 4;
	case GX_TF_Z24X8: return 4;
	case GX_CTF_Z4: return 8;
	case GX_CTF_Z8M: return 8;
	case GX_CTF_Z8L: return 8;
	case GX_CTF_Z16L: return 4;
	default:
		ERROR_LOG(VIDEO, "Unsupported Texture Format (%08x)! (GetBlockWidthInTexels)", format);
		return 8;
	}
}

int TexDecoder_GetBlockHeightInTexels(u32 format)
{
	switch (format)
	{
	case GX_TF_I4: return 8;
	case GX_TF_I8: return 4;
	case GX_TF_IA4: return 4;
	case GX_TF_IA8: return 4;
	case GX_TF_RGB565: return 4;
	case GX_TF_RGB5A3: return 4;
	case GX_TF_RGBA8: return  4;
	case GX_TF_C4: return 8;
	case GX_TF_C8: return 4;
	case GX_TF_C14X2: return 4;
	case GX_TF_CMPR: return 8;
	case GX_CTF_R4: return 8;
	case GX_CTF_RA4: return 4;
	case GX_CTF_RA8: return 4;
	case GX_CTF_A8: return 4;
	case GX_CTF_R8: return 4;
	case GX_CTF_G8: return 4;
	case GX_CTF_B8: return 4;
	case GX_CTF_RG8: return 4;
	case GX_CTF_GB8: return 4;
	case GX_TF_Z8: return 4;
	case GX_TF_Z16: return 4;
	case GX_TF_Z24X8: return 4;
	case GX_CTF_Z4: return 8;
	case GX_CTF_Z8M: return 4;
	case GX_CTF_Z8L: return 4;
	case GX_CTF_Z16L: return 4;
	default:
		ERROR_LOG(VIDEO, "Unsupported Texture Format (%08x)! (GetBlockHeightInTexels)", format);
		return 4;
	}
}

//returns bytes
int TexDecoder_GetPaletteSize(int format)
{
	switch (format)
	{
	case GX_TF_C4: return 16 * 2;
	case GX_TF_C8: return 256 * 2;
	case GX_TF_C14X2: return 16384 * 2;
	default:
		return 0;
	}
}

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

#if _M_SSE >= 0x301
static const __m128i kMaskSwap16 = _mm_set_epi32(0x0E0F0C0DL, 0x0A0B0809L, 0x06070405L, 0x02030001L);

inline void decodebytesC8_To_Raw16_SSSE3(u16* dst, const u8* src, int tlutaddr)
{
	u16* tlut = (u16*)(texMem + tlutaddr);

	// Make 8 16-bits unsigned integer values
	__m128i a = _mm_setzero_si128();
	a = _mm_insert_epi16(a, tlut[src[0]], 0);
	a = _mm_insert_epi16(a, tlut[src[1]], 1);
	a = _mm_insert_epi16(a, tlut[src[2]], 2);
	a = _mm_insert_epi16(a, tlut[src[3]], 3);
	a = _mm_insert_epi16(a, tlut[src[4]], 4);
	a = _mm_insert_epi16(a, tlut[src[5]], 5);
	a = _mm_insert_epi16(a, tlut[src[6]], 6);
	a = _mm_insert_epi16(a, tlut[src[7]], 7);

	// Apply Common::swap16() to 16-bits unsigned integers at once
	const __m128i b = _mm_shuffle_epi8(a, kMaskSwap16);

	// Store values to dst without polluting the caches
	_mm_stream_si128((__m128i*)dst, b);
}
#endif

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

inline void SetOpenMPThreadCount(int width, int height)
{
#ifdef _OPENMP
	// Don't use multithreading in small Textures
	if (g_ActiveConfig.bOMPDecoder && width > 127 && height > 127)
	{
		// don't span to many threads they will kill the rest of the emu :)
		omp_set_num_threads((omp_get_num_procs() + 2) / 3);
	}
	else
	{
		omp_set_num_threads(1);
	}
#endif
}

//switch endianness, unswizzle
//TODO: to save memory, don't blindly convert everything to argb8888
//also ARGB order needs to be swapped later, to accommodate modern hardware better
//need to add DXT support too
PC_TexFormat TexDecoder_Decode_real(u8 *dst, const u8 *src, int width, int height, int texformat, int tlutaddr, int tlutfmt)
{
	SetOpenMPThreadCount(width, height);

	const int Wsteps4 = (width + 3) / 4;
	const int Wsteps8 = (width + 7) / 8;

	switch (texformat)
	{
	case GX_TF_C4:
		if (tlutfmt == 2)
		{
			// Special decoding is required for TLUT format 5A3
			#pragma omp parallel for
			for (int y = 0; y < height; y += 8)
				for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8, yStep++)
					for (int iy = 0, xStep = yStep * 8; iy < 8; iy++, xStep++)
						decodebytesC4_5A3_To_BGRA32((u32*)dst + (y + iy) * width + x, src + 4 * xStep, tlutaddr);
		}
		else
		{
			#pragma omp parallel for
			for (int y = 0; y < height; y += 8)
				for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8, yStep++)
					for (int iy = 0, xStep = yStep * 8; iy < 8; iy++, xStep++)
						decodebytesC4_To_Raw16((u16*)dst + (y + iy) * width + x, src + 4 * xStep, tlutaddr);
		}
		return GetPCFormatFromTLUTFormat(tlutfmt);
	case GX_TF_I4:
		{
			#pragma omp parallel for
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
			#pragma omp parallel for
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
					{
						((u64*)(dst + (y + iy) * width + x))[0] = ((u64*)(src + 8 * xStep))[0];
					}
		}
		return PC_TEX_FMT_I8;
	case GX_TF_C8:
		if (tlutfmt == 2)
		{
			// Special decoding is required for TLUT format 5A3
			#pragma omp parallel for
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						decodebytesC8_5A3_To_BGRA32((u32*)dst + (y + iy) * width + x, src + 8 * xStep, tlutaddr);
		}
		else
		{

#if _M_SSE >= 0x301

			if (cpu_info.bSSSE3) {
				#pragma omp parallel for
				for (int y = 0; y < height; y += 4)
					for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
						for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
							decodebytesC8_To_Raw16_SSSE3((u16*)dst + (y + iy) * width + x, src + 8 * xStep, tlutaddr);
			} else
#endif
			{
				#pragma omp parallel for
				for (int y = 0; y < height; y += 4)
					for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
						for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
							decodebytesC8_To_Raw16((u16*)dst + (y + iy) * width + x, src  + 8 * xStep, tlutaddr);
			}
		}
		return GetPCFormatFromTLUTFormat(tlutfmt);
	case GX_TF_IA4:
		{
			#pragma omp parallel for
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						decodebytesIA4((u16*)dst + (y + iy) * width + x, src + 8 * xStep);
		}
		return PC_TEX_FMT_IA4_AS_IA8;
	case GX_TF_IA8:
		{
			#pragma omp parallel for
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
			#pragma omp parallel for
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						decodebytesC14X2_5A3_To_BGRA32((u32*)dst + (y + iy) * width + x, (u16*)(src + 8 * xStep), tlutaddr);
		}
		else
		{
			#pragma omp parallel for
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						decodebytesC14X2_To_Raw16((u16*)dst + (y + iy) * width + x,(u16*)(src + 8 * xStep), tlutaddr);
		}
		return GetPCFormatFromTLUTFormat(tlutfmt);
	case GX_TF_RGB565:
		{
			#pragma omp parallel for
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
			#pragma omp parallel for
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						//decodebytesRGB5A3((u32*)dst+(y+iy)*width+x, (u16*)src, 4);
						decodebytesRGB5A3((u32*)dst+(y+iy)*width+x, (u16*)(src + 8 * xStep));
		}
		return PC_TEX_FMT_BGRA32;
	case GX_TF_RGBA8:  // speed critical
		{

#if _M_SSE >= 0x301

			if (cpu_info.bSSSE3) {
				#pragma omp parallel for
				for (int y = 0; y < height; y += 4) {
					__m128i* p = (__m128i*)(src + y * width * 4);
					for (int x = 0; x < width; x += 4) {

						// We use _mm_loadu_si128 instead of _mm_load_si128
						// because "p" may not be aligned in 16-bytes alignment.
						// See Issue 3493.
						const __m128i a0 = _mm_loadu_si128(p++);
						const __m128i a1 = _mm_loadu_si128(p++);
						const __m128i a2 = _mm_loadu_si128(p++);
						const __m128i a3 = _mm_loadu_si128(p++);

						// Shuffle 16-bit integeres by _mm_unpacklo_epi16()/_mm_unpackhi_epi16(),
						// apply Common::swap32() by _mm_shuffle_epi8() and
						// store them by _mm_stream_si128().
						// See decodebytesARGB8_4() about the idea.

						static const __m128i kMaskSwap32 = _mm_set_epi32(0x0C0D0E0FL, 0x08090A0BL, 0x04050607L, 0x00010203L);

						const __m128i b0 = _mm_unpacklo_epi16(a0, a2);
						const __m128i c0 = _mm_shuffle_epi8(b0, kMaskSwap32);
						_mm_stream_si128((__m128i*)((u32*)dst + (y + 0) * width + x), c0);

						const __m128i b1 = _mm_unpackhi_epi16(a0, a2);
						const __m128i c1 = _mm_shuffle_epi8(b1, kMaskSwap32);
						_mm_stream_si128((__m128i*)((u32*)dst + (y + 1) * width + x), c1);

						const __m128i b2 = _mm_unpacklo_epi16(a1, a3);
						const __m128i c2 = _mm_shuffle_epi8(b2, kMaskSwap32);
						_mm_stream_si128((__m128i*)((u32*)dst + (y + 2) * width + x), c2);

						const __m128i b3 = _mm_unpackhi_epi16(a1, a3);
						const __m128i c3 = _mm_shuffle_epi8(b3, kMaskSwap32);
						_mm_stream_si128((__m128i*)((u32*)dst + (y + 3) * width + x), c3);
					}
				}
			} else

#endif

			{
				#pragma omp parallel for
				for (int y = 0; y < height; y += 4)
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
			#pragma omp parallel for
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
	SetOpenMPThreadCount(width, height);

	const int Wsteps4 = (width + 3) / 4;
	const int Wsteps8 = (width + 7) / 8;

	switch (texformat)
	{
	case GX_TF_C4:
		if (tlutfmt == 2)
		{
			// Special decoding is required for TLUT format 5A3
			#pragma omp parallel for
			for (int y = 0; y < height; y += 8)
				for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8,yStep++)
					for (int iy = 0, xStep =  8 * yStep; iy < 8; iy++,xStep++)
						decodebytesC4_5A3_To_rgba32(dst + (y + iy) * width + x, src + 4 * xStep, tlutaddr);
		}
		else if (tlutfmt == 0)
		{
			#pragma omp parallel for
			for (int y = 0; y < height; y += 8)
				for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8,yStep++)
					for (int iy = 0, xStep =  8 * yStep; iy < 8; iy++,xStep++)
						decodebytesC4IA8_To_RGBA(dst + (y + iy) * width + x, src + 4 * xStep, tlutaddr);

		}
		else
		{
			#pragma omp parallel for
			for (int y = 0; y < height; y += 8)
				for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8,yStep++)
					for (int iy = 0, xStep =  8 * yStep; iy < 8; iy++,xStep++)
						decodebytesC4RGB565_To_RGBA(dst + (y + iy) * width + x, src  + 4 * xStep, tlutaddr);
		}
		break;
	case GX_TF_I4:
		{
			const __m128i kMask_x0f = _mm_set1_epi32(0x0f0f0f0fL);
			const __m128i kMask_xf0 = _mm_set1_epi32(0xf0f0f0f0L);
#if _M_SSE >= 0x301
			// xsacha optimized with SSSE3 intrinsics
			// Produces a ~40% speed improvement over SSE2 implementation
			if (cpu_info.bSSSE3) {
				const __m128i mask9180 = _mm_set_epi8(9,9,9,9,1,1,1,1,8,8,8,8,0,0,0,0);
				const __m128i maskB3A2 = _mm_set_epi8(11,11,11,11,3,3,3,3,10,10,10,10,2,2,2,2);
				const __m128i maskD5C4 = _mm_set_epi8(13,13,13,13,5,5,5,5,12,12,12,12,4,4,4,4);
				const __m128i maskF7E6 = _mm_set_epi8(15,15,15,15,7,7,7,7,14,14,14,14,6,6,6,6);
				#pragma omp parallel for
				for (int y = 0; y < height; y += 8)
					for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8,yStep++)
						for (int iy = 0, xStep =  4 * yStep; iy < 8; iy += 2,xStep++)
						{
							const __m128i r0 = _mm_loadl_epi64((const __m128i *)(src + 8 * xStep));
							// We want the hi 4 bits of each 8-bit word replicated to 32-bit words:
							// (00000000 00000000 HhGgFfEe DdCcBbAa) -> (00000000 00000000 HHGGFFEE DDCCBBAA)
							const __m128i i1 = _mm_and_si128(r0, kMask_xf0);
							const __m128i i11 = _mm_or_si128(i1, _mm_srli_epi16(i1, 4));

							// Now we do same as above for the second half of the byte
							const __m128i i2 = _mm_and_si128(r0, kMask_x0f);
							const __m128i i22 = _mm_or_si128(i2, _mm_slli_epi16(i2,4));

							// Combine both sides
							const __m128i base = _mm_unpacklo_epi64(i11,i22);
							// Achieve the pattern visible in the masks.
							const __m128i o1 = _mm_shuffle_epi8(base, mask9180);
							const __m128i o2 = _mm_shuffle_epi8(base, maskB3A2);
							const __m128i o3 = _mm_shuffle_epi8(base, maskD5C4);
							const __m128i o4 = _mm_shuffle_epi8(base, maskF7E6);

							// Write row 0:
							_mm_storeu_si128( (__m128i*)( dst+(y + iy) * width + x ), o1 );
							_mm_storeu_si128( (__m128i*)( dst+(y + iy) * width + x + 4 ), o2 );
							// Write row 1:
							_mm_storeu_si128( (__m128i*)( dst+(y + iy+1) * width + x ), o3 );
							_mm_storeu_si128( (__m128i*)( dst+(y + iy+1) * width + x + 4 ), o4 );
						}
			} else
#endif
			// JSD optimized with SSE2 intrinsics.
			// Produces a ~76% speed improvement over reference C implementation.
			{
				#pragma omp parallel for
				for (int y = 0; y < height; y += 8)
					for (int x = 0, yStep = (y / 8) * Wsteps8 ; x < width; x += 8, yStep++)
						for (int iy = 0, xStep = 4 * yStep; iy < 8; iy += 2, xStep++)
						{
							const __m128i r0 = _mm_loadl_epi64((const __m128i *)(src + 8 * xStep));
							// Shuffle low 64-bits with itself to expand from (0000 0000 hgfe dcba) to (hhgg ffee ddcc bbaa)
							const __m128i r1 = _mm_unpacklo_epi8(r0, r0);

							// We want the hi 4 bits of each 8-bit word replicated to 32-bit words:
							// (HhHhGgGg FfFfEeEe DdDdCcCc BbBbAaAa) & kMask_xf0 -> (H0H0G0G0 F0F0E0E0 D0D0C0C0 B0B0A0A0)
							const __m128i i1 = _mm_and_si128(r1, kMask_xf0);
							// -> (HHHHGGGG FFFFEEEE DDDDCCCC BBBBAAAA)
							const __m128i i11 = _mm_or_si128(i1, _mm_srli_epi16(i1, 4));

							// Shuffle low 64-bits with itself to expand from (HHHHGGGG FFFFEEEE DDDDCCCC BBBBAAAA) to (DDDDDDDD CCCCCCCC BBBBBBBB AAAAAAAA)
							const __m128i i15 = _mm_unpacklo_epi8(i11, i11);
							// (DDDDDDDD CCCCCCCC BBBBBBBB AAAAAAAA) -> (BBBBBBBB BBBBBBBB AAAAAAAA AAAAAAAA)
							const __m128i i151 = _mm_unpacklo_epi8(i15, i15);
							// (DDDDDDDD CCCCCCCC BBBBBBBB AAAAAAAA) -> (DDDDDDDD DDDDDDDD CCCCCCCC CCCCCCCC)
							const __m128i i152 = _mm_unpackhi_epi8(i15, i15);

							// Shuffle hi  64-bits with itself to expand from (HHHHGGGG FFFFEEEE DDDDCCCC BBBBAAAA) to (HHHHHHHH GGGGGGGG FFFFFFFF EEEEEEEE)
							const __m128i i16 = _mm_unpackhi_epi8(i11, i11);
							// (HHHHHHHH GGGGGGGG FFFFFFFF EEEEEEEE) -> (FFFFFFFF FFFFFFFF EEEEEEEE EEEEEEEE)
							const __m128i i161 = _mm_unpacklo_epi8(i16, i16);
							// (HHHHHHHH GGGGGGGG FFFFFFFF EEEEEEEE) -> (HHHHHHHH HHHHHHHH GGGGGGGG GGGGGGGG)
							const __m128i i162 = _mm_unpackhi_epi8(i16, i16);

							// Now find the lo 4 bits of each input 8-bit word:
							const __m128i i2 = _mm_and_si128(r1, kMask_x0f);
							const __m128i i22 = _mm_or_si128(i2, _mm_slli_epi16(i2,4));

							const __m128i i25 = _mm_unpacklo_epi8(i22, i22);
							const __m128i i251 = _mm_unpacklo_epi8(i25, i25);
							const __m128i i252 = _mm_unpackhi_epi8(i25, i25);

							const __m128i i26 = _mm_unpackhi_epi8(i22, i22);
							const __m128i i261 = _mm_unpacklo_epi8(i26, i26);
							const __m128i i262 = _mm_unpackhi_epi8(i26, i26);

							// _mm_and_si128(i151, kMask_x00000000ffffffff) takes i151 and masks off 1st and 3rd 32-bit words
							// (BBBBBBBB BBBBBBBB AAAAAAAA AAAAAAAA) -> (00000000 BBBBBBBB 00000000 AAAAAAAA)
							// _mm_and_si128(i251, kMask_xffffffff00000000) takes i251 and masks off 2nd and 4th 32-bit words
							// (bbbbbbbb bbbbbbbb aaaaaaaa aaaaaaaa) -> (bbbbbbbb 00000000 aaaaaaaa 00000000)
							// And last but not least, _mm_or_si128 ORs those two together, giving us the interleaving we desire:
							// (00000000 BBBBBBBB 00000000 AAAAAAAA) | (bbbbbbbb 00000000 aaaaaaaa 00000000) -> (bbbbbbbb BBBBBBBB aaaaaaaa AAAAAAAA)
							const __m128i kMask_x00000000ffffffff = _mm_set_epi32(0x00000000L, 0xffffffffL, 0x00000000L, 0xffffffffL);
							const __m128i kMask_xffffffff00000000 = _mm_set_epi32(0xffffffffL, 0x00000000L, 0xffffffffL, 0x00000000L);
							const __m128i o1 = _mm_or_si128(_mm_and_si128(i151, kMask_x00000000ffffffff), _mm_and_si128(i251, kMask_xffffffff00000000));
							const __m128i o2 = _mm_or_si128(_mm_and_si128(i152, kMask_x00000000ffffffff), _mm_and_si128(i252, kMask_xffffffff00000000));

							// These two are for the next row; same pattern as above. We batched up two rows because our input was 64 bits.
							const __m128i o3 = _mm_or_si128(_mm_and_si128(i161, kMask_x00000000ffffffff), _mm_and_si128(i261, kMask_xffffffff00000000));
							const __m128i o4 = _mm_or_si128(_mm_and_si128(i162, kMask_x00000000ffffffff), _mm_and_si128(i262, kMask_xffffffff00000000));
							// Write row 0:
							_mm_storeu_si128( (__m128i*)( dst+(y + iy) * width + x ), o1 );
							_mm_storeu_si128( (__m128i*)( dst+(y + iy) * width + x + 4 ), o2 );
							// Write row 1:
							_mm_storeu_si128( (__m128i*)( dst+(y + iy+1) * width + x ), o3 );
							_mm_storeu_si128( (__m128i*)( dst+(y + iy+1) * width + x + 4 ), o4 );
						}
			}
		}
	   break;
	case GX_TF_I8:  // speed critical
		{
#if _M_SSE >= 0x301
			// xsacha optimized with SSSE3 intrinsics
			// Produces a ~10% speed improvement over SSE2 implementation
			if (cpu_info.bSSSE3)
			{
				#pragma omp parallel for
				for (int y = 0; y < height; y += 4)
					for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8,yStep++)
						for (int iy = 0, xStep = 4 * yStep; iy < 4; ++iy, xStep++)
						{
							const __m128i mask3210 = _mm_set_epi8(3, 3, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0);

							const __m128i mask7654 = _mm_set_epi8(7, 7, 7, 7, 6, 6, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4);
							__m128i *quaddst, r, rgba0, rgba1;
							// Load 64 bits from `src` into an __m128i with upper 64 bits zeroed: (0000 0000 hgfe dcba)
							r = _mm_loadl_epi64((const __m128i *)(src + 8 * xStep));
							// Shuffle select bytes to expand from (0000 0000 hgfe dcba) to:
							rgba0 = _mm_shuffle_epi8(r, mask3210); // (dddd cccc bbbb aaaa)
							rgba1 = _mm_shuffle_epi8(r, mask7654); // (hhhh gggg ffff eeee)

							quaddst = (__m128i *)(dst + (y + iy)*width + x);
							_mm_storeu_si128(quaddst, rgba0);
							_mm_storeu_si128(quaddst+1, rgba1);
						}

			} else
#endif
			// JSD optimized with SSE2 intrinsics.
			// Produces an ~86% speed improvement over reference C implementation.
			{
				#pragma omp parallel for
				for (int y = 0; y < height; y += 4)
					for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8,yStep++)
					{
						// Each loop iteration processes 4 rows from 4 64-bit reads.
						const u8* src2 = src + 32 * yStep;
						// TODO: is it more efficient to group the loads together sequentially and also the stores at the end?
						// _mm_stream instead of _mm_store on my AMD Phenom II x410 made performance significantly WORSE, so I
						// went with _mm_stores. Perhaps there is some edge case here creating the terrible performance or we're
						// not aligned to 16-byte boundaries. I don't know.
						__m128i *quaddst;

						// Load 64 bits from `src` into an __m128i with upper 64 bits zeroed: (0000 0000 hgfe dcba)
						const __m128i r0 = _mm_loadl_epi64((const __m128i *)src2);
						// Shuffle low 64-bits with itself to expand from (0000 0000 hgfe dcba) to (hhgg ffee ddcc bbaa)
						const __m128i r1 = _mm_unpacklo_epi8(r0, r0);

						// Shuffle low 64-bits with itself to expand from (hhgg ffee ddcc bbaa) to (dddd cccc bbbb aaaa)
						const __m128i rgba0 = _mm_unpacklo_epi8(r1, r1);
						// Shuffle hi 64-bits with itself to expand from (hhgg ffee ddcc bbaa) to (hhhh gggg ffff eeee)
						const __m128i rgba1 = _mm_unpackhi_epi8(r1, r1);

						// Store (dddd cccc bbbb aaaa) out:
						quaddst = (__m128i *)(dst + (y + 0)*width + x);
						_mm_storeu_si128(quaddst, rgba0);
						// Store (hhhh gggg ffff eeee) out:
						_mm_storeu_si128(quaddst+1, rgba1);

						// Load 64 bits from `src` into an __m128i with upper 64 bits zeroed: (0000 0000 hgfe dcba)
						src2 += 8;
						const __m128i r2 = _mm_loadl_epi64((const __m128i *)src2);
						// Shuffle low 64-bits with itself to expand from (0000 0000 hgfe dcba) to (hhgg ffee ddcc bbaa)
						const __m128i r3 = _mm_unpacklo_epi8(r2, r2);

						// Shuffle low 64-bits with itself to expand from (hhgg ffee ddcc bbaa) to (dddd cccc bbbb aaaa)
						const __m128i rgba2 = _mm_unpacklo_epi8(r3, r3);
						// Shuffle hi 64-bits with itself to expand from (hhgg ffee ddcc bbaa) to (hhhh gggg ffff eeee)
						const __m128i rgba3 = _mm_unpackhi_epi8(r3, r3);

						// Store (dddd cccc bbbb aaaa) out:
						quaddst = (__m128i *)(dst + (y + 1)*width + x);
						_mm_storeu_si128(quaddst, rgba2);
						// Store (hhhh gggg ffff eeee) out:
						_mm_storeu_si128(quaddst+1, rgba3);

						// Load 64 bits from `src` into an __m128i with upper 64 bits zeroed: (0000 0000 hgfe dcba)
						src2 += 8;
						const __m128i r4 = _mm_loadl_epi64((const __m128i *)src2);
						// Shuffle low 64-bits with itself to expand from (0000 0000 hgfe dcba) to (hhgg ffee ddcc bbaa)
						const __m128i r5 = _mm_unpacklo_epi8(r4, r4);

						// Shuffle low 64-bits with itself to expand from (hhgg ffee ddcc bbaa) to (dddd cccc bbbb aaaa)
						const __m128i rgba4 = _mm_unpacklo_epi8(r5, r5);
						// Shuffle hi 64-bits with itself to expand from (hhgg ffee ddcc bbaa) to (hhhh gggg ffff eeee)
						const __m128i rgba5 = _mm_unpackhi_epi8(r5, r5);

						// Store (dddd cccc bbbb aaaa) out:
						quaddst = (__m128i *)(dst + (y + 2)*width + x);
						_mm_storeu_si128(quaddst, rgba4);
						// Store (hhhh gggg ffff eeee) out:
						_mm_storeu_si128(quaddst+1, rgba5);

						// Load 64 bits from `src` into an __m128i with upper 64 bits zeroed: (0000 0000 hgfe dcba)
						src2 += 8;
						const __m128i r6 = _mm_loadl_epi64((const __m128i *)src2);
						// Shuffle low 64-bits with itself to expand from (0000 0000 hgfe dcba) to (hhgg ffee ddcc bbaa)
						const __m128i r7 = _mm_unpacklo_epi8(r6, r6);

						// Shuffle low 64-bits with itself to expand from (hhgg ffee ddcc bbaa) to (dddd cccc bbbb aaaa)
						const __m128i rgba6 = _mm_unpacklo_epi8(r7, r7);
						// Shuffle hi 64-bits with itself to expand from (hhgg ffee ddcc bbaa) to (hhhh gggg ffff eeee)
						const __m128i rgba7 = _mm_unpackhi_epi8(r7, r7);

						// Store (dddd cccc bbbb aaaa) out:
						quaddst = (__m128i *)(dst + (y + 3)*width + x);
						_mm_storeu_si128(quaddst, rgba6);
						// Store (hhhh gggg ffff eeee) out:
						_mm_storeu_si128(quaddst+1, rgba7);

					}
			}
		}
		break;
	case GX_TF_C8:
		if (tlutfmt == 2)
		{
			// Special decoding is required for TLUT format 5A3
			#pragma omp parallel for
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						decodebytesC8_5A3_To_RGBA32((u32*)dst + (y + iy) * width + x, src + 8 * xStep, tlutaddr);
		}
		else if (tlutfmt == 0)
		{
			#pragma omp parallel for
			for (int y = 0; y < height; y += 4)
					for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
						for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
							decodebytesC8IA8_To_RGBA(dst + (y + iy) * width + x, src + 8 * xStep, tlutaddr);

		}
		else
		{
			#pragma omp parallel for
			for (int y = 0; y < height; y += 4)
					for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
						for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
							decodebytesC8RGB565_To_RGBA(dst + (y + iy) * width + x, src + 8 * xStep, tlutaddr);

		}
		break;
	case GX_TF_IA4:
		{
			#pragma omp parallel for
			for (int y = 0; y < height; y += 4)
					for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
						for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
							decodebytesIA4RGBA(dst + (y + iy) * width + x, src + 8 * xStep);
		}
		break;
	case GX_TF_IA8:
		{
#if _M_SSE >= 0x301
			// xsacha optimized with SSSE3 intrinsics.
			// Produces an ~50% speed improvement over SSE2 implementation.
			if (cpu_info.bSSSE3)
			{
				#pragma omp parallel for
				for (int y = 0; y < height; y += 4)
					for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
						for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						{
							const __m128i mask = _mm_set_epi8(6, 7, 7, 7, 4, 5, 5, 5, 2, 3, 3, 3, 0, 1, 1, 1);
							// Load 4x 16-bit IA8 samples from `src` into an __m128i with upper 64 bits zeroed: (0000 0000 hgfe dcba)
							const __m128i r0 = _mm_loadl_epi64((const __m128i *)(src + 8 * xStep));
							// Shuffle to (ghhh efff cddd abbb)
							const __m128i r1 = _mm_shuffle_epi8(r0, mask);
							_mm_storeu_si128( (__m128i*)(dst + (y + iy) * width + x), r1 );
						}
			} else
#endif
			// JSD optimized with SSE2 intrinsics.
			// Produces an ~80% speed improvement over reference C implementation.
			{
				const __m128i kMask_xf0 = _mm_set_epi32(0x00000000L, 0x00000000L, 0xff00ff00L, 0xff00ff00L);
				const __m128i kMask_x0f = _mm_set_epi32(0x00000000L, 0x00000000L, 0x00ff00ffL, 0x00ff00ffL);
				const __m128i kMask_xf000 = _mm_set_epi32(0xff000000L, 0xff000000L, 0xff000000L, 0xff000000L);
				const __m128i kMask_x0fff = _mm_set_epi32(0x00ffffffL, 0x00ffffffL, 0x00ffffffL, 0x00ffffffL);
				#pragma omp parallel for
				for (int y = 0; y < height; y += 4)
					for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
						for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						{
							// Expands a 16-bit "IA" to a 32-bit "AIII". Each char is an 8-bit value.

							// Load 4x 16-bit IA8 samples from `src` into an __m128i with upper 64 bits zeroed: (0000 0000 hgfe dcba)
							const __m128i r0 = _mm_loadl_epi64((const __m128i *)(src+ 8 * xStep));

							// Logical shift all 16-bit words right by 8 bits (0000 0000 hgfe dcba) to (0000 0000 0h0f 0d0b)
							// This gets us only the I components.
							const __m128i i0 = _mm_srli_epi16(r0, 8);

							// Now join up the I components from their original positions but mask out the A components.
							// (0000 0000 hgfe dcba) &      kMask_xFF00      -> (0000 0000 h0f0 d0b0)
							// (0000 0000 h0f0 d0b0) | (0000 0000 0h0f 0d0b) -> (0000 0000 hhff ddbb)
							const __m128i i1 = _mm_or_si128(_mm_and_si128(r0, kMask_xf0), i0);

							// Shuffle low 64-bits with itself to expand from (0000 0000 hhff ddbb) to (hhhh ffff dddd bbbb)
							const __m128i i2 = _mm_unpacklo_epi8(i1, i1);
							// (hhhh ffff dddd bbbb) & kMask_x0fff -> (0hhh 0fff 0ddd 0bbb)
							const __m128i i3 = _mm_and_si128(i2, kMask_x0fff);

							// Now that we have the I components in 32-bit word form, time work out the A components into
							// their final positions.

							// (0000 0000 hgfe dcba) &      kMask_x00FF      -> (0000 0000 0g0e 0c0a)
							const __m128i a0 = _mm_and_si128(r0, kMask_x0f);
							// (0000 0000 0g0e 0c0a) -> (00gg 00ee 00cc 00aa)
							const __m128i a1 = _mm_unpacklo_epi8(a0, a0);
							// (00gg 00ee 00cc 00aa) << 16 -> (gg00 ee00 cc00 aa00)
							const __m128i a2 = _mm_slli_epi32(a1, 16);
							// (gg00 ee00 cc00 aa00) & kMask_xf000 -> (g000 e000 c000 a000)
							const __m128i a3 = _mm_and_si128(a2, kMask_xf000);

							// Simply OR up i3 and a3 now and that's our result:
							// (0hhh 0fff 0ddd 0bbb) | (g000 e000 c000 a000) -> (ghhh efff cddd abbb)
							const __m128i r1 = _mm_or_si128(i3, a3);

							// write out the 128-bit result:
							_mm_storeu_si128( (__m128i*)(dst + (y + iy) * width + x), r1 );
						}
			}
		}
		break;
	case GX_TF_C14X2:
		if (tlutfmt == 2)
		{
			// Special decoding is required for TLUT format 5A3
			#pragma omp parallel for
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						decodebytesC14X2_5A3_To_BGRA32(dst + (y + iy) * width + x, (u16*)(src + 8 * xStep), tlutaddr);
		}
		else if (tlutfmt == 0)
		{
			#pragma omp parallel for
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						decodebytesC14X2IA8_To_RGBA(dst + (y + iy) * width + x,  (u16*)(src + 8 * xStep), tlutaddr);
		}
		else
		{
			#pragma omp parallel for
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						decodebytesC14X2rgb565_To_RGBA(dst + (y + iy) * width + x, (u16*)(src + 8 * xStep), tlutaddr);
		}
		break;
	case GX_TF_RGB565:
		{
			// JSD optimized with SSE2 intrinsics.
			// Produces an ~78% speed improvement over reference C implementation.
			const __m128i kMaskR0 = _mm_set1_epi32(0x000000F8);
			const __m128i kMaskG0 = _mm_set1_epi32(0x0000FC00);
			const __m128i kMaskG1 = _mm_set1_epi32(0x00000300);
			const __m128i kMaskB0 = _mm_set1_epi32(0x00F80000);
			const __m128i kAlpha  = _mm_set1_epi32(0xFF000000);
			#pragma omp parallel for
			for (int y = 0; y < height; y += 4)
				for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
					for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
					{
						__m128i *dxtsrc = (__m128i *)(src + 8 * xStep);
						// Load 4x 16-bit colors: (0000 0000 hgfe dcba)
						// where hg, fe, ba, and dc are 16-bit colors in big-endian order
						const __m128i rgb565x4 = _mm_loadl_epi64(dxtsrc);

						// The big-endian 16-bit colors `ba` and `dc` look like 0b_gggBBBbb_RRRrrGGg in a little endian xmm register
						// Unpack `hgfe dcba` to `hhgg ffee ddcc bbaa`, where each 32-bit word is now 0b_gggBBBbb_RRRrrGGg_gggBBBbb_RRRrrGGg
						const __m128i c0 = _mm_unpacklo_epi16(rgb565x4, rgb565x4);

						// swizzle 0b_gggBBBbb_RRRrrGGg_gggBBBbb_RRRrrGGg
						//      to 0b_11111111_BBBbbBBB_GGggggGG_RRRrrRRR

						// 0b_gggBBBbb_RRRrrGGg_gggBBBbb_RRRrrGGg &
						// 0b_00000000_00000000_00000000_11111000 =
						// 0b_00000000_00000000_00000000_RRRrr000
						const __m128i r0 = _mm_and_si128(c0, kMaskR0);
						// 0b_00000000_00000000_00000000_RRRrr000 >> 5 [32] =
						// 0b_00000000_00000000_00000000_00000RRR
						const __m128i r1 = _mm_srli_epi32(r0, 5);

						// 0b_gggBBBbb_RRRrrGGg_gggBBBbb_RRRrrGGg >> 3 [32] =
						// 0b_000gggBB_BbbRRRrr_GGggggBB_BbbRRRrr &
						// 0b_00000000_00000000_11111100_00000000 =
						// 0b_00000000_00000000_GGgggg00_00000000
						const __m128i gtmp = _mm_srli_epi32(c0, 3);
						const __m128i g0 = _mm_and_si128(gtmp, kMaskG0);
						// 0b_GGggggBB_BbbRRRrr_GGggggBB_Bbb00000 >> 6 [32] =
						// 0b_000000GG_ggggBBBb_bRRRrrGG_ggggBBBb &
						// 0b_00000000_00000000_00000011_00000000 =
						// 0b_00000000_00000000_000000GG_00000000 =
						const __m128i g1 = _mm_and_si128(_mm_srli_epi32(gtmp, 6), kMaskG1);

						// 0b_gggBBBbb_RRRrrGGg_gggBBBbb_RRRrrGGg >> 5 [32] =
						// 0b_00000ggg_BBBbbRRR_rrGGgggg_BBBbbRRR &
						// 0b_00000000_11111000_00000000_00000000 =
						// 0b_00000000_BBBbb000_00000000_00000000
						const __m128i b0 = _mm_and_si128(_mm_srli_epi32(c0, 5), kMaskB0);
						// 0b_00000000_BBBbb000_00000000_00000000 >> 5 [16] =
						// 0b_00000000_00000BBB_00000000_00000000
						const __m128i b1 = _mm_srli_epi16(b0, 5);

						// OR together the final RGB bits and the alpha component:
						const __m128i abgr888x4 = _mm_or_si128(
							_mm_or_si128(
								_mm_or_si128(r0, r1),
								_mm_or_si128(g0, g1)
							),
							_mm_or_si128(
								_mm_or_si128(b0, b1),
								kAlpha
							)
						);

						__m128i *ptr = (__m128i *)(dst + (y + iy) * width + x);
						_mm_storeu_si128(ptr, abgr888x4);
					}
		}
		break;
	case GX_TF_RGB5A3:
		{
			const __m128i kMask_x1f = _mm_set1_epi32(0x0000001fL);
			const __m128i kMask_x0f = _mm_set1_epi32(0x0000000fL);
			const __m128i kMask_x07 = _mm_set1_epi32(0x00000007L);
			// This is the hard-coded 0xFF alpha constant that is ORed in place after the RGB are calculated
			// for the RGB555 case when (s[x] & 0x8000) is true for all pixels.
			const __m128i aVxff00   = _mm_set1_epi32(0xFF000000L);

#if _M_SSE >= 0x301
			// xsacha optimized with SSSE3 intrinsics (2 in 4 cases)
			// Produces a ~10% speed improvement over SSE2 implementation
			if (cpu_info.bSSSE3)
			{
				#pragma omp parallel for
				for (int y = 0; y < height; y += 4)
					for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
						for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						{
							u32 *newdst = dst+(y+iy)*width+x;
							const __m128i mask = _mm_set_epi8(-128,-128,6,7,-128,-128,4,5,-128,-128,2,3,-128,-128,0,1);
								const __m128i valV = _mm_shuffle_epi8(_mm_loadl_epi64((const __m128i*)(src + 8 * xStep)),mask);
								int cmp = _mm_movemask_epi8(valV); //MSB: 0x2 = val0; 0x20=val1; 0x200 = val2; 0x2000=val3
								if ((cmp&0x2222)==0x2222) // SSSE3 case #1: all 4 pixels are in RGB555 and alpha = 0xFF.
								{
									// Swizzle bits: 00012345 -> 12345123

									//r0 = (((val0>>10) & 0x1f) << 3) | (((val0>>10) & 0x1f) >> 2);
									const __m128i tmprV = _mm_and_si128(_mm_srli_epi16(valV, 10), kMask_x1f);
									const __m128i rV = _mm_or_si128( _mm_slli_epi16(tmprV, 3), _mm_srli_epi16(tmprV, 2) );

									//g0 = (((val0>>5 ) & 0x1f) << 3) | (((val0>>5 ) & 0x1f) >> 2);
									const __m128i tmpgV = _mm_and_si128(_mm_srli_epi16(valV, 5), kMask_x1f);
									const __m128i gV = _mm_or_si128( _mm_slli_epi16(tmpgV, 3), _mm_srli_epi16(tmpgV, 2) );

									//b0 = (((val0    ) & 0x1f) << 3) | (((val0    ) & 0x1f) >> 2);
									const __m128i tmpbV = _mm_and_si128(valV, kMask_x1f);
									const __m128i bV = _mm_or_si128( _mm_slli_epi16(tmpbV, 3), _mm_srli_epi16(tmpbV, 2) );

									//newdst[0] = r0 | (g0 << 8) | (b0 << 16) | (a0 << 24);
									const __m128i final = _mm_or_si128(_mm_or_si128(rV,_mm_slli_epi32(gV, 8)),
														_mm_or_si128(_mm_slli_epi32(bV, 16), aVxff00));
									_mm_storeu_si128( (__m128i*)newdst, final );
								}
								else if (!(cmp&0x2222)) // SSSE3 case #2: all 4 pixels are in RGBA4443.
								{
									// Swizzle bits: 00001234 -> 12341234

									//r0 = (((val0>>8 ) & 0xf) << 4) | ((val0>>8 ) & 0xf);
									const __m128i tmprV = _mm_and_si128(_mm_srli_epi16(valV, 8), kMask_x0f);
									const __m128i rV = _mm_or_si128( _mm_slli_epi16(tmprV, 4), tmprV );

									//g0 = (((val0>>4 ) & 0xf) << 4) | ((val0>>4 ) & 0xf);
									const __m128i tmpgV = _mm_and_si128(_mm_srli_epi16(valV, 4), kMask_x0f);
									const __m128i gV = _mm_or_si128( _mm_slli_epi16(tmpgV, 4), tmpgV );

									//b0 = (((val0    ) & 0xf) << 4) | ((val0    ) & 0xf);
									const __m128i tmpbV = _mm_and_si128(valV, kMask_x0f);
									const __m128i bV = _mm_or_si128( _mm_slli_epi16(tmpbV, 4), tmpbV );
									//a0 = (((val0>>12) & 0x7) << 5) | (((val0>>12) & 0x7) << 2) | (((val0>>12) & 0x7) >> 1);
									const __m128i tmpaV = _mm_and_si128(_mm_srli_epi16(valV, 12), kMask_x07);
									const __m128i aV = _mm_or_si128(
										_mm_slli_epi16(tmpaV, 5),
										_mm_or_si128(
											_mm_slli_epi16(tmpaV, 2),
											_mm_srli_epi16(tmpaV, 1)
										)
									);

									//newdst[0] = r0 | (g0 << 8) | (b0 << 16) | (a0 << 24);
									const __m128i final = _mm_or_si128(_mm_or_si128(rV,_mm_slli_epi32(gV, 8)),
																_mm_or_si128(_mm_slli_epi32(bV, 16), _mm_slli_epi32(aV, 24)));
									_mm_storeu_si128( (__m128i*)newdst, final );
								}
								else
								{
									// TODO: Vectorise (Either 4-way branch or do both and select is better than this)
									u32 *vals = (u32*) &valV;
									int r,g,b,a;
									for (int i=0; i < 4; ++i)
									{
										if (vals[i] & 0x8000)
										{
											// Swizzle bits: 00012345 -> 12345123
											r = (((vals[i]>>10) & 0x1f) << 3) | (((vals[i]>>10) & 0x1f) >> 2);
											g = (((vals[i]>>5 ) & 0x1f) << 3) | (((vals[i]>>5 ) & 0x1f) >> 2);
											b = (((vals[i]    ) & 0x1f) << 3) | (((vals[i]    ) & 0x1f) >> 2);
											a = 0xFF;
										}
										else
										{
											a = (((vals[i]>>12) & 0x7) << 5) | (((vals[i]>>12) & 0x7) << 2) | (((vals[i]>>12) & 0x7) >> 1);
											// Swizzle bits: 00001234 -> 12341234
											r = (((vals[i]>>8 ) & 0xf) << 4) | ((vals[i]>>8 ) & 0xf);
											g = (((vals[i]>>4 ) & 0xf) << 4) | ((vals[i]>>4 ) & 0xf);
											b = (((vals[i]    ) & 0xf) << 4) | ((vals[i]    ) & 0xf);
										}
										newdst[i] = r | (g << 8) | (b << 16) | (a << 24);
									}
								}
						}
			} else
#endif
			// JSD optimized with SSE2 intrinsics (2 in 4 cases)
			// Produces a ~25% speed improvement over reference C implementation.
			{
				#pragma omp parallel for
				for (int y = 0; y < height; y += 4)
					for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
						for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
						{
							u32 *newdst = dst+(y+iy)*width+x;
							const u16 *newsrc = (const u16*)(src + 8 * xStep);

							// TODO: weak point
							const u16 val0 = Common::swap16(newsrc[0]);
							const u16 val1 = Common::swap16(newsrc[1]);
							const u16 val2 = Common::swap16(newsrc[2]);
							const u16 val3 = Common::swap16(newsrc[3]);

							const __m128i valV = _mm_set_epi16(0, val3, 0, val2, 0, val1, 0, val0);

							// Need to check all 4 pixels' MSBs to ensure we can do data-parallelism:
							if (((val0 & 0x8000) & (val1 & 0x8000) & (val2 & 0x8000) & (val3 & 0x8000)) == 0x8000)
							{
								// SSE2 case #1: all 4 pixels are in RGB555 and alpha = 0xFF.

								// Swizzle bits: 00012345 -> 12345123

								//r0 = (((val0>>10) & 0x1f) << 3) | (((val0>>10) & 0x1f) >> 2);
								const __m128i tmprV = _mm_and_si128(_mm_srli_epi16(valV, 10), kMask_x1f);
								const __m128i rV = _mm_or_si128( _mm_slli_epi16(tmprV, 3), _mm_srli_epi16(tmprV, 2) );

								//g0 = (((val0>>5 ) & 0x1f) << 3) | (((val0>>5 ) & 0x1f) >> 2);
								const __m128i tmpgV = _mm_and_si128(_mm_srli_epi16(valV, 5), kMask_x1f);
								const __m128i gV = _mm_or_si128( _mm_slli_epi16(tmpgV, 3), _mm_srli_epi16(tmpgV, 2) );

								//b0 = (((val0    ) & 0x1f) << 3) | (((val0    ) & 0x1f) >> 2);
								const __m128i tmpbV = _mm_and_si128(valV, kMask_x1f);
								const __m128i bV = _mm_or_si128( _mm_slli_epi16(tmpbV, 3), _mm_srli_epi16(tmpbV, 2) );

								//newdst[0] = r0 | (g0 << 8) | (b0 << 16) | (a0 << 24);
								const __m128i final = _mm_or_si128(_mm_or_si128(rV,_mm_slli_epi32(gV, 8)),
													_mm_or_si128(_mm_slli_epi32(bV, 16), aVxff00));

								// write the final result:
								_mm_storeu_si128( (__m128i*)newdst, final );
							}
							else if (((val0 & 0x8000) | (val1 & 0x8000) | (val2 & 0x8000) | (val3 & 0x8000)) == 0x0000)
							{
								// SSE2 case #2: all 4 pixels are in RGBA4443.

								// Swizzle bits: 00001234 -> 12341234

								//r0 = (((val0>>8 ) & 0xf) << 4) | ((val0>>8 ) & 0xf);
								const __m128i tmprV = _mm_and_si128(_mm_srli_epi16(valV, 8), kMask_x0f);
								const __m128i rV = _mm_or_si128( _mm_slli_epi16(tmprV, 4), tmprV );

								//g0 = (((val0>>4 ) & 0xf) << 4) | ((val0>>4 ) & 0xf);
								const __m128i tmpgV = _mm_and_si128(_mm_srli_epi16(valV, 4), kMask_x0f);
								const __m128i gV = _mm_or_si128( _mm_slli_epi16(tmpgV, 4), tmpgV );

								//b0 = (((val0    ) & 0xf) << 4) | ((val0    ) & 0xf);
								const __m128i tmpbV = _mm_and_si128(valV, kMask_x0f);
								const __m128i bV = _mm_or_si128( _mm_slli_epi16(tmpbV, 4), tmpbV );

								//a0 = (((val0>>12) & 0x7) << 5) | (((val0>>12) & 0x7) << 2) | (((val0>>12) & 0x7) >> 1);
								const __m128i tmpaV = _mm_and_si128(_mm_srli_epi16(valV, 12), kMask_x07);
								const __m128i aV = _mm_or_si128(
									_mm_slli_epi16(tmpaV, 5),
									_mm_or_si128(
										_mm_slli_epi16(tmpaV, 2),
										_mm_srli_epi16(tmpaV, 1)
									)
								);

								//newdst[0] = r0 | (g0 << 8) | (b0 << 16) | (a0 << 24);
								const __m128i final = _mm_or_si128(_mm_or_si128(rV,_mm_slli_epi32(gV, 8)),
													_mm_or_si128(_mm_slli_epi32(bV, 16), _mm_slli_epi32(aV, 24)));

								// write the final result:
								_mm_storeu_si128( (__m128i*)newdst, final );
							}
							else
							{
								// TODO: Vectorise (Either 4-way branch or do both and select is better than this)
								u32 *vals = (u32*) &valV;
								int r,g,b,a;
								for (int i=0; i < 4; ++i)
								{
									if (vals[i] & 0x8000)
									{
										// Swizzle bits: 00012345 -> 12345123
										r = (((vals[i]>>10) & 0x1f) << 3) | (((vals[i]>>10) & 0x1f) >> 2);
										g = (((vals[i]>>5 ) & 0x1f) << 3) | (((vals[i]>>5 ) & 0x1f) >> 2);
										b = (((vals[i]    ) & 0x1f) << 3) | (((vals[i]    ) & 0x1f) >> 2);
										a = 0xFF;
									}
									else
									{
										a = (((vals[i]>>12) & 0x7) << 5) | (((vals[i]>>12) & 0x7) << 2) | (((vals[i]>>12) & 0x7) >> 1);
										// Swizzle bits: 00001234 -> 12341234
										r = (((vals[i]>>8 ) & 0xf) << 4) | ((vals[i]>>8 ) & 0xf);
										g = (((vals[i]>>4 ) & 0xf) << 4) | ((vals[i]>>4 ) & 0xf);
										b = (((vals[i]    ) & 0xf) << 4) | ((vals[i]    ) & 0xf);
									}
									newdst[i] = r | (g << 8) | (b << 16) | (a << 24);
								}
							}
						}
				}
		}
		break;
	case GX_TF_RGBA8:  // speed critical
		{
#if _M_SSE >= 0x301
			// xsacha optimized with SSSE3 instrinsics
			// Produces a ~30% speed improvement over SSE2 implementation
			if (cpu_info.bSSSE3)
			{
				#pragma omp parallel for
				for (int y = 0; y < height; y += 4)
					for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
					{
						const u8* src2 = src + 64 * yStep;
						const __m128i mask0312 = _mm_set_epi8(12,15,13,14,8,11,9,10,4,7,5,6,0,3,1,2);
						const __m128i ar0 = _mm_loadu_si128((__m128i*)src2);
						const __m128i ar1 = _mm_loadu_si128((__m128i*)src2+1);
						const __m128i gb0 = _mm_loadu_si128((__m128i*)src2+2);
						const __m128i gb1 = _mm_loadu_si128((__m128i*)src2+3);


						const __m128i rgba00 = _mm_shuffle_epi8(_mm_unpacklo_epi8(ar0,gb0),mask0312);
						const __m128i rgba01 = _mm_shuffle_epi8(_mm_unpackhi_epi8(ar0,gb0),mask0312);
						const __m128i rgba10 = _mm_shuffle_epi8(_mm_unpacklo_epi8(ar1,gb1),mask0312);
						const __m128i rgba11 = _mm_shuffle_epi8(_mm_unpackhi_epi8(ar1,gb1),mask0312);

						__m128i *dst128 = (__m128i*)( dst + (y + 0) * width + x );
						_mm_storeu_si128(dst128, rgba00);
						dst128 = (__m128i*)( dst + (y + 1) * width + x );
						_mm_storeu_si128(dst128, rgba01);
						dst128 = (__m128i*)( dst + (y + 2) * width + x );
						_mm_storeu_si128(dst128, rgba10);
						dst128 = (__m128i*)( dst + (y + 3) * width + x );
						_mm_storeu_si128(dst128, rgba11);
					}
			} else
#endif
			// JSD optimized with SSE2 intrinsics
			// Produces a ~68% speed improvement over reference C implementation.
			{
				#pragma omp parallel for
				for (int y = 0; y < height; y += 4)
					for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
					{
						// Input is divided up into 16-bit words. The texels are split up into AR and GB components where all
						// AR components come grouped up first in 32 bytes followed by the GB components in 32 bytes. We are
						// processing 16 texels per each loop iteration, numbered from 0-f.
						//
						// Convention is:
						//   one byte is [component-name texel-number]
						//    __m128i is (4-bytes 4-bytes 4-bytes 4-bytes)
						//
						// Input  is ([A 7][R 7][A 6][R 6] [A 5][R 5][A 4][R 4] [A 3][R 3][A 2][R 2] [A 1][R 1][A 0][R 0])
						//           ([A f][R f][A e][R e] [A d][R d][A c][R c] [A b][R b][A a][R a] [A 9][R 9][A 8][R 8])
						//           ([G 7][B 7][G 6][B 6] [G 5][B 5][G 4][B 4] [G 3][B 3][G 2][B 2] [G 1][B 1][G 0][B 0])
						//           ([G f][B f][G e][B e] [G d][B d][G c][B c] [G b][B b][G a][B a] [G 9][B 9][G 8][B 8])
						//
						// Output is (RGBA3 RGBA2 RGBA1 RGBA0)
						//           (RGBA7 RGBA6 RGBA5 RGBA4)
						//           (RGBAb RGBAa RGBA9 RGBA8)
						//           (RGBAf RGBAe RGBAd RGBAc)
						const u8* src2 = src + 64 * yStep;
						// Loads the 1st half of AR components ([A 7][R 7][A 6][R 6] [A 5][R 5][A 4][R 4] [A 3][R 3][A 2][R 2] [A 1][R 1][A 0][R 0])
						const __m128i ar0 = _mm_loadu_si128((__m128i*)src2);
						// Loads the 2nd half of AR components ([A f][R f][A e][R e] [A d][R d][A c][R c] [A b][R b][A a][R a] [A 9][R 9][A 8][R 8])
						const __m128i ar1 = _mm_loadu_si128((__m128i*)src2+1);
						// Loads the 1st half of GB components ([G 7][B 7][G 6][B 6] [G 5][B 5][G 4][B 4] [G 3][B 3][G 2][B 2] [G 1][B 1][G 0][B 0])
						const __m128i gb0 = _mm_loadu_si128((__m128i*)src2+2);
						// Loads the 2nd half of GB components ([G f][B f][G e][B e] [G d][B d][G c][B c] [G b][B b][G a][B a] [G 9][B 9][G 8][B 8])
						const __m128i gb1 = _mm_loadu_si128((__m128i*)src2+3);
						__m128i rgba00, rgba01, rgba10, rgba11;
						const __m128i kMask_x000f = _mm_set_epi32(0x000000FFL, 0x000000FFL, 0x000000FFL, 0x000000FFL);
						const __m128i kMask_xf000 = _mm_set_epi32(0xFF000000L, 0xFF000000L, 0xFF000000L, 0xFF000000L);
						const __m128i kMask_x0ff0 = _mm_set_epi32(0x00FFFF00L, 0x00FFFF00L, 0x00FFFF00L, 0x00FFFF00L);
						// Expand the AR components to fill out 32-bit words:
						// ([A 7][R 7][A 6][R 6] [A 5][R 5][A 4][R 4] [A 3][R 3][A 2][R 2] [A 1][R 1][A 0][R 0]) -> ([A 3][A 3][R 3][R 3] [A 2][A 2][R 2][R 2] [A 1][A 1][R 1][R 1] [A 0][A 0][R 0][R 0])
						const __m128i aarr00 = _mm_unpacklo_epi8(ar0, ar0);
						// ([A 7][R 7][A 6][R 6] [A 5][R 5][A 4][R 4] [A 3][R 3][A 2][R 2] [A 1][R 1][A 0][R 0]) -> ([A 7][A 7][R 7][R 7] [A 6][A 6][R 6][R 6] [A 5][A 5][R 5][R 5] [A 4][A 4][R 4][R 4])
						const __m128i aarr01 = _mm_unpackhi_epi8(ar0, ar0);
						// ([A f][R f][A e][R e] [A d][R d][A c][R c] [A b][R b][A a][R a] [A 9][R 9][A 8][R 8]) -> ([A b][A b][R b][R b] [A a][A a][R a][R a] [A 9][A 9][R 9][R 9] [A 8][A 8][R 8][R 8])
						const __m128i aarr10 = _mm_unpacklo_epi8(ar1, ar1);
						// ([A f][R f][A e][R e] [A d][R d][A c][R c] [A b][R b][A a][R a] [A 9][R 9][A 8][R 8]) -> ([A f][A f][R f][R f] [A e][A e][R e][R e] [A d][A d][R d][R d] [A c][A c][R c][R c])
						const __m128i aarr11 = _mm_unpackhi_epi8(ar1, ar1);

						// Move A right 16 bits and mask off everything but the lowest  8 bits to get A in its final place:
						const __m128i ___a00 = _mm_and_si128(_mm_srli_epi32(aarr00, 16), kMask_x000f);
						// Move R left  16 bits and mask off everything but the highest 8 bits to get R in its final place:
						const __m128i r___00 = _mm_and_si128(_mm_slli_epi32(aarr00, 16), kMask_xf000);
						// OR the two together to get R and A in their final places:
						const __m128i r__a00 = _mm_or_si128(r___00, ___a00);

						const __m128i ___a01 = _mm_and_si128(_mm_srli_epi32(aarr01, 16), kMask_x000f);
						const __m128i r___01 = _mm_and_si128(_mm_slli_epi32(aarr01, 16), kMask_xf000);
						const __m128i r__a01 = _mm_or_si128(r___01, ___a01);

						const __m128i ___a10 = _mm_and_si128(_mm_srli_epi32(aarr10, 16), kMask_x000f);
						const __m128i r___10 = _mm_and_si128(_mm_slli_epi32(aarr10, 16), kMask_xf000);
						const __m128i r__a10 = _mm_or_si128(r___10, ___a10);

						const __m128i ___a11 = _mm_and_si128(_mm_srli_epi32(aarr11, 16), kMask_x000f);
						const __m128i r___11 = _mm_and_si128(_mm_slli_epi32(aarr11, 16), kMask_xf000);
						const __m128i r__a11 = _mm_or_si128(r___11, ___a11);

						// Expand the GB components to fill out 32-bit words:
						// ([G 7][B 7][G 6][B 6] [G 5][B 5][G 4][B 4] [G 3][B 3][G 2][B 2] [G 1][B 1][G 0][B 0]) -> ([G 3][G 3][B 3][B 3] [G 2][G 2][B 2][B 2] [G 1][G 1][B 1][B 1] [G 0][G 0][B 0][B 0])
						const __m128i ggbb00 = _mm_unpacklo_epi8(gb0, gb0);
						// ([G 7][B 7][G 6][B 6] [G 5][B 5][G 4][B 4] [G 3][B 3][G 2][B 2] [G 1][B 1][G 0][B 0]) -> ([G 7][G 7][B 7][B 7] [G 6][G 6][B 6][B 6] [G 5][G 5][B 5][B 5] [G 4][G 4][B 4][B 4])
						const __m128i ggbb01 = _mm_unpackhi_epi8(gb0, gb0);
						// ([G f][B f][G e][B e] [G d][B d][G c][B c] [G b][B b][G a][B a] [G 9][B 9][G 8][B 8]) -> ([G b][G b][B b][B b] [G a][G a][B a][B a] [G 9][G 9][B 9][B 9] [G 8][G 8][B 8][B 8])
						const __m128i ggbb10 = _mm_unpacklo_epi8(gb1, gb1);
						// ([G f][B f][G e][B e] [G d][B d][G c][B c] [G b][B b][G a][B a] [G 9][B 9][G 8][B 8]) -> ([G f][G f][B f][B f] [G e][G e][B e][B e] [G d][G d][B d][B d] [G c][G c][B c][B c])
						const __m128i ggbb11 = _mm_unpackhi_epi8(gb1, gb1);

						// G and B are already in perfect spots in the center, just remove the extra copies in the 1st and 4th positions:
						const __m128i _gb_00 = _mm_and_si128(ggbb00, kMask_x0ff0);
						const __m128i _gb_01 = _mm_and_si128(ggbb01, kMask_x0ff0);
						const __m128i _gb_10 = _mm_and_si128(ggbb10, kMask_x0ff0);
						const __m128i _gb_11 = _mm_and_si128(ggbb11, kMask_x0ff0);

						// Now join up R__A and _GB_ to get RGBA!
						rgba00 = _mm_or_si128(r__a00, _gb_00);
						rgba01 = _mm_or_si128(r__a01, _gb_01);
						rgba10 = _mm_or_si128(r__a10, _gb_10);
						rgba11 = _mm_or_si128(r__a11, _gb_11);
						// Write em out!
						__m128i *dst128 = (__m128i*)( dst + (y + 0) * width + x );
						_mm_storeu_si128(dst128, rgba00);
						dst128 = (__m128i*)( dst + (y + 1) * width + x );
						_mm_storeu_si128(dst128, rgba01);
						dst128 = (__m128i*)( dst + (y + 2) * width + x );
						_mm_storeu_si128(dst128, rgba10);
						dst128 = (__m128i*)( dst + (y + 3) * width + x );
						_mm_storeu_si128(dst128, rgba11);
					}
			}
		}
		break;
	case GX_TF_CMPR:  // speed critical
		// The metroid games use this format almost exclusively.
		{
			// JSD optimized with SSE2 intrinsics.
			// Produces a ~50% improvement for x86 and a ~40% improvement for x64 in speed over reference C implementation.
			// The x64 compiled reference C code is faster than the x86 compiled reference C code, but the SSE2 is
			// faster than both.
			#pragma omp parallel for
			for (int y = 0; y < height; y += 8)
			{
				for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8,yStep++)
				{
					// We handle two DXT blocks simultaneously to take full advantage of SSE2's 128-bit registers.
					// This is ideal because a single DXT block contains 2 RGBA colors when decoded from their 16-bit.
					// Two DXT blocks therefore contain 4 RGBA colors to be processed. The processing is parallelizable
					// at this level, so we do.
					for (int z = 0, xStep = 2 * yStep; z < 2; ++z, xStep++)
					{
						// JSD NOTE: You may see many strange patterns of behavior in the below code, but they
						// are for performance reasons. Sometimes, calculating what should be obvious hard-coded
						// constants is faster than loading their values from memory. Unfortunately, there is no
						// way to inline 128-bit constants from opcodes so they must be loaded from memory. This
						// seems a little ridiculous to me in that you can't even generate a constant value of 1 without
						// having to load it from memory. So, I stored the minimal constant I could, 128-bits worth
						// of 1s :). Then I use sequences of shifts to squash it to the appropriate size and bit
						// positions that I need.

						const __m128i allFFs128 = _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128());

						// Load 128 bits, i.e. two DXTBlocks (64-bits each)
						const __m128i dxt = _mm_loadu_si128((__m128i *)(src + sizeof(struct DXTBlock) * 2 * xStep));

						// Copy the 2-bit indices from each DXT block:
						GC_ALIGNED16( u32 dxttmp[4] );
						_mm_store_si128((__m128i *)dxttmp, dxt);

						u32 dxt0sel = dxttmp[1];
						u32 dxt1sel = dxttmp[3];

						__m128i argb888x4;
						__m128i c1 = _mm_unpackhi_epi16(dxt, dxt);
						c1 = _mm_slli_si128(c1, 8);
						const __m128i c0 = _mm_or_si128(c1, _mm_srli_si128(_mm_slli_si128(_mm_unpacklo_epi16(dxt, dxt), 8), 8));

						// Compare rgb0 to rgb1:
						// Each 32-bit word will contain either 0xFFFFFFFF or 0x00000000 for true/false.
						const __m128i c0cmp = _mm_srli_epi32(_mm_slli_epi32(_mm_srli_epi64(c0, 8), 16), 16);
						const __m128i c0shr = _mm_srli_epi64(c0cmp, 32);
						const __m128i cmprgb0rgb1 = _mm_cmpgt_epi32(c0cmp, c0shr);

						int cmp0 = _mm_extract_epi16(cmprgb0rgb1, 0);
						int cmp1 = _mm_extract_epi16(cmprgb0rgb1, 4);

						// green:
						// NOTE: We start with the larger number of bits (6) firts for G and shift the mask down 1 bit to get a 5-bit mask
						// later for R and B components.
						// low6mask == _mm_set_epi32(0x0000FC00, 0x0000FC00, 0x0000FC00, 0x0000FC00)
						const __m128i low6mask = _mm_slli_epi32( _mm_srli_epi32(allFFs128, 24 + 2), 8 + 2);
						const __m128i gtmp = _mm_srli_epi32(c0, 3);
						const __m128i g0 = _mm_and_si128(gtmp, low6mask);
						// low3mask == _mm_set_epi32(0x00000300, 0x00000300, 0x00000300, 0x00000300)
						const __m128i g1 = _mm_and_si128(_mm_srli_epi32(gtmp, 6), _mm_set_epi32(0x00000300, 0x00000300, 0x00000300, 0x00000300));
						argb888x4 = _mm_or_si128(g0, g1);
						// red:
						// low5mask == _mm_set_epi32(0x000000F8, 0x000000F8, 0x000000F8, 0x000000F8)
						const __m128i low5mask = _mm_slli_epi32( _mm_srli_epi32(low6mask, 8 + 3), 3);
						const __m128i r0 = _mm_and_si128(c0, low5mask);
						const __m128i r1 = _mm_srli_epi32(r0, 5);
						argb888x4 = _mm_or_si128(argb888x4, _mm_or_si128(r0, r1));
						// blue:
						// _mm_slli_epi32(low5mask, 16) == _mm_set_epi32(0x00F80000, 0x00F80000, 0x00F80000, 0x00F80000)
						const __m128i b0 = _mm_and_si128(_mm_srli_epi32(c0, 5), _mm_slli_epi32(low5mask, 16));
						const __m128i b1 = _mm_srli_epi16(b0, 5);
						// OR in the fixed alpha component
						// _mm_slli_epi32( allFFs128, 24 ) == _mm_set_epi32(0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000)
						argb888x4 = _mm_or_si128(_mm_or_si128(argb888x4, _mm_slli_epi32( allFFs128, 24 ) ), _mm_or_si128(b0, b1));
						// calculate RGB2 and RGB3:
						const __m128i rgb0 = _mm_shuffle_epi32(argb888x4, _MM_SHUFFLE(2, 2, 0, 0));
						const __m128i rgb1 = _mm_shuffle_epi32(argb888x4, _MM_SHUFFLE(3, 3, 1, 1));
						const __m128i rrggbb0 = _mm_and_si128(_mm_unpacklo_epi8(rgb0, rgb0), _mm_srli_epi16( allFFs128, 8 ));
						const __m128i rrggbb1 = _mm_and_si128(_mm_unpacklo_epi8(rgb1, rgb1), _mm_srli_epi16( allFFs128, 8 ));
						const __m128i rrggbb01 = _mm_and_si128(_mm_unpackhi_epi8(rgb0, rgb0), _mm_srli_epi16( allFFs128, 8 ));
						const __m128i rrggbb11 = _mm_and_si128(_mm_unpackhi_epi8(rgb1, rgb1), _mm_srli_epi16( allFFs128, 8 ));

						__m128i rgb2, rgb3;

						// if (rgb0 > rgb1):
						if (cmp0 != 0)
						{
							// RGB2a = ((RGB1 - RGB0) >> 1) - ((RGB1 - RGB0) >> 3)  using arithmetic shifts to extend sign (not logical shifts)
							const __m128i rrggbbsub = _mm_subs_epi16(rrggbb1, rrggbb0);
							const __m128i rrggbbsubshr1 = _mm_srai_epi16(rrggbbsub, 1);
							const __m128i rrggbbsubshr3 = _mm_srai_epi16(rrggbbsub, 3);
							const __m128i shr1subshr3 = _mm_sub_epi16(rrggbbsubshr1, rrggbbsubshr3);
							// low8mask16 == _mm_set_epi16(0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff)
							const __m128i low8mask16 = _mm_srli_epi16( allFFs128, 8 );
							const __m128i rrggbbdelta = _mm_and_si128(shr1subshr3, low8mask16);
							const __m128i rgbdeltadup = _mm_packus_epi16(rrggbbdelta, rrggbbdelta);
							const __m128i rgbdelta = _mm_srli_si128(_mm_slli_si128(rgbdeltadup, 8), 8);

							rgb2 = _mm_and_si128(_mm_add_epi8(rgb0, rgbdelta), _mm_srli_si128(allFFs128, 8));
							rgb3 = _mm_and_si128(_mm_sub_epi8(rgb1, rgbdelta), _mm_srli_si128(allFFs128, 8));
						}
						else
						{
							// RGB2b = avg(RGB0, RGB1)
							const __m128i rrggbb21  = _mm_avg_epu16(rrggbb0, rrggbb1);
							const __m128i rgb210 = _mm_srli_si128(_mm_packus_epi16(rrggbb21, rrggbb21), 8);
							rgb2 = rgb210;
							rgb3 = _mm_and_si128(_mm_srli_si128(_mm_shuffle_epi32(argb888x4, _MM_SHUFFLE(1, 1, 1, 1)), 8), _mm_srli_epi32( allFFs128, 8 ));
						}

						// if (rgb0 > rgb1):
						if (cmp1 != 0)
						{
							// RGB2a = ((RGB1 - RGB0) >> 1) - ((RGB1 - RGB0) >> 3)  using arithmetic shifts to extend sign (not logical shifts)
							const __m128i rrggbbsub1 = _mm_subs_epi16(rrggbb11, rrggbb01);
							const __m128i rrggbbsubshr11 = _mm_srai_epi16(rrggbbsub1, 1);
							const __m128i rrggbbsubshr31 = _mm_srai_epi16(rrggbbsub1, 3);
							const __m128i shr1subshr31 = _mm_sub_epi16(rrggbbsubshr11, rrggbbsubshr31);
							// low8mask16 == _mm_set_epi16(0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff)
							const __m128i low8mask16 = _mm_srli_epi16( allFFs128, 8 );
							const __m128i rrggbbdelta1 = _mm_and_si128(shr1subshr31, low8mask16);
							__m128i rgbdelta1 = _mm_packus_epi16(rrggbbdelta1, rrggbbdelta1);
							rgbdelta1 = _mm_slli_si128(rgbdelta1, 8);

							rgb2 = _mm_or_si128(rgb2, _mm_and_si128(_mm_add_epi8(rgb0, rgbdelta1), _mm_slli_si128(allFFs128, 8)));
							rgb3 = _mm_or_si128(rgb3, _mm_and_si128(_mm_sub_epi8(rgb1, rgbdelta1), _mm_slli_si128(allFFs128, 8)));
						}
						else
						{
							// RGB2b = avg(RGB0, RGB1)
							const __m128i rrggbb211 = _mm_avg_epu16(rrggbb01, rrggbb11);
							const __m128i rgb211 = _mm_slli_si128(_mm_packus_epi16(rrggbb211, rrggbb211), 8);
							rgb2 = _mm_or_si128(rgb2, rgb211);

							// _mm_srli_epi32( allFFs128, 8 ) == _mm_set_epi32(0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF)
							// Make this color fully transparent:
							rgb3 = _mm_or_si128(rgb3, _mm_and_si128(_mm_and_si128(rgb1, _mm_srli_epi32( allFFs128, 8 ) ), _mm_slli_si128(allFFs128, 8)));
						}

						// Create an array for color lookups for DXT0 so we can use the 2-bit indices:
						const __m128i mmcolors0 = _mm_or_si128(
							_mm_or_si128(
								_mm_srli_si128(_mm_slli_si128(argb888x4, 8), 8),
								_mm_slli_si128(_mm_srli_si128(_mm_slli_si128(rgb2, 8), 8 + 4), 8)
							),
							_mm_slli_si128(_mm_srli_si128(rgb3, 4), 8 + 4)
						);

						// Create an array for color lookups for DXT1 so we can use the 2-bit indices:
						const __m128i mmcolors1 = _mm_or_si128(
							_mm_or_si128(
								_mm_srli_si128(argb888x4, 8),
								_mm_slli_si128(_mm_srli_si128(rgb2, 8 + 4), 8)
							),
							_mm_slli_si128(_mm_srli_si128(rgb3, 8 + 4), 8 + 4)
						);

						// The #ifdef CHECKs here and below are to compare correctness of output against the reference code.
						// Don't use them in a normal build.
#ifdef CHECK
						// REFERENCE:
						u32 tmp0[4][4], tmp1[4][4];

						decodeDXTBlockRGBA(&(tmp0[0][0]), (const DXTBlock *)src, 4);
						decodeDXTBlockRGBA(&(tmp1[0][0]), (const DXTBlock *)(src + 8), 4);
#endif

						u32 *dst32 = ( dst + (y + z*4) * width + x );

						// Copy the colors here:
						GC_ALIGNED16( u32 colors0[4] );
						GC_ALIGNED16( u32 colors1[4] );
						_mm_store_si128((__m128i *)colors0, mmcolors0);
						_mm_store_si128((__m128i *)colors1, mmcolors1);

						// Row 0:
						dst32[(width * 0) + 0] = colors0[(dxt0sel >> ((0*8)+6)) & 3];
						dst32[(width * 0) + 1] = colors0[(dxt0sel >> ((0*8)+4)) & 3];
						dst32[(width * 0) + 2] = colors0[(dxt0sel >> ((0*8)+2)) & 3];
						dst32[(width * 0) + 3] = colors0[(dxt0sel >> ((0*8)+0)) & 3];
						dst32[(width * 0) + 4] = colors1[(dxt1sel >> ((0*8)+6)) & 3];
						dst32[(width * 0) + 5] = colors1[(dxt1sel >> ((0*8)+4)) & 3];
						dst32[(width * 0) + 6] = colors1[(dxt1sel >> ((0*8)+2)) & 3];
						dst32[(width * 0) + 7] = colors1[(dxt1sel >> ((0*8)+0)) & 3];
#ifdef CHECK
						assert( memcmp(&(tmp0[0]), &dst32[(width * 0)], 16) == 0 );
						assert( memcmp(&(tmp1[0]), &dst32[(width * 0) + 4], 16) == 0 );
#endif
						// Row 1:
						dst32[(width * 1) + 0] = colors0[(dxt0sel >> ((1*8)+6)) & 3];
						dst32[(width * 1) + 1] = colors0[(dxt0sel >> ((1*8)+4)) & 3];
						dst32[(width * 1) + 2] = colors0[(dxt0sel >> ((1*8)+2)) & 3];
						dst32[(width * 1) + 3] = colors0[(dxt0sel >> ((1*8)+0)) & 3];
						dst32[(width * 1) + 4] = colors1[(dxt1sel >> ((1*8)+6)) & 3];
						dst32[(width * 1) + 5] = colors1[(dxt1sel >> ((1*8)+4)) & 3];
						dst32[(width * 1) + 6] = colors1[(dxt1sel >> ((1*8)+2)) & 3];
						dst32[(width * 1) + 7] = colors1[(dxt1sel >> ((1*8)+0)) & 3];
#ifdef CHECK
						assert( memcmp(&(tmp0[1]), &dst32[(width * 1)], 16) == 0 );
						assert( memcmp(&(tmp1[1]), &dst32[(width * 1) + 4], 16) == 0 );
#endif
						// Row 2:
						dst32[(width * 2) + 0] = colors0[(dxt0sel >> ((2*8)+6)) & 3];
						dst32[(width * 2) + 1] = colors0[(dxt0sel >> ((2*8)+4)) & 3];
						dst32[(width * 2) + 2] = colors0[(dxt0sel >> ((2*8)+2)) & 3];
						dst32[(width * 2) + 3] = colors0[(dxt0sel >> ((2*8)+0)) & 3];
						dst32[(width * 2) + 4] = colors1[(dxt1sel >> ((2*8)+6)) & 3];
						dst32[(width * 2) + 5] = colors1[(dxt1sel >> ((2*8)+4)) & 3];
						dst32[(width * 2) + 6] = colors1[(dxt1sel >> ((2*8)+2)) & 3];
						dst32[(width * 2) + 7] = colors1[(dxt1sel >> ((2*8)+0)) & 3];
#ifdef CHECK
						assert( memcmp(&(tmp0[2]), &dst32[(width * 2)], 16) == 0 );
						assert( memcmp(&(tmp1[2]), &dst32[(width * 2) + 4], 16) == 0 );
#endif
						// Row 3:
						dst32[(width * 3) + 0] = colors0[(dxt0sel >> ((3*8)+6)) & 3];
						dst32[(width * 3) + 1] = colors0[(dxt0sel >> ((3*8)+4)) & 3];
						dst32[(width * 3) + 2] = colors0[(dxt0sel >> ((3*8)+2)) & 3];
						dst32[(width * 3) + 3] = colors0[(dxt0sel >> ((3*8)+0)) & 3];
						dst32[(width * 3) + 4] = colors1[(dxt1sel >> ((3*8)+6)) & 3];
						dst32[(width * 3) + 5] = colors1[(dxt1sel >> ((3*8)+4)) & 3];
						dst32[(width * 3) + 6] = colors1[(dxt1sel >> ((3*8)+2)) & 3];
						dst32[(width * 3) + 7] = colors1[(dxt1sel >> ((3*8)+0)) & 3];
#ifdef CHECK
						assert( memcmp(&(tmp0[3]), &dst32[(width * 3)], 16) == 0 );
						assert( memcmp(&(tmp1[3]), &dst32[(width * 3) + 4], 16) == 0 );
#endif
					}
				}
			}
			break;
		}
	}

	// The "copy" texture formats, too?
	return PC_TEX_FMT_RGBA32;
}




void TexDecoder_SetTexFmtOverlayOptions(bool enable, bool center)
{
	TexFmt_Overlay_Enable = enable;
	TexFmt_Overlay_Center = center;
}

PC_TexFormat TexDecoder_Decode(u8 *dst, const u8 *src, int width, int height, int texformat, int tlutaddr, int tlutfmt,bool rgbaOnly)
{
	PC_TexFormat retval = rgbaOnly ? TexDecoder_Decode_RGBA((u32*)dst, src,
			width, height, texformat, tlutaddr, tlutfmt)
		: TexDecoder_Decode_real(dst, src,
			width, height, texformat, tlutaddr, tlutfmt);

	if ((!TexFmt_Overlay_Enable) || (retval == PC_TEX_FMT_NONE))
		return retval;

	int w = std::min(width, 40);
	int h = std::min(height, 10);

	int xoff = (width - w) >> 1;
	int yoff = (height - h) >> 1;

	if (!TexFmt_Overlay_Center)
	{
		xoff=0;
		yoff=0;
	}

	const char* fmt = texfmt[texformat&15];
	while (*fmt)
	{
		int xcnt = 0;
		int nchar = sfont_map[(int)*fmt];

		const unsigned char *ptr = sfont_raw[nchar]; // each char is up to 9x10

		for (int x = 0; x < 9;x++)
		{
			if (ptr[x] == 0x78)
				break;
			xcnt++;
		}

		for (int y=0; y < 10; y++)
		{
			for (int x=0; x < xcnt; x++)
			{
				switch (retval)
				{
				case PC_TEX_FMT_I8:
					{
						// TODO: Is this an acceptable way to draw in I8?
						u8  *dtp = (u8*)dst;
						dtp[(y + yoff) * width + x + xoff] = ptr[x] ? 0xFF : 0x88;
						break;
					}
				case PC_TEX_FMT_IA8:
				case PC_TEX_FMT_IA4_AS_IA8:
					{
						u16  *dtp = (u16*)dst;
						dtp[(y + yoff) * width + x + xoff] = ptr[x] ? 0xFFFF : 0xFF00;
						break;
					}
				case PC_TEX_FMT_RGB565:
					{
						u16  *dtp = (u16*)dst;
						dtp[(y + yoff)*width + x + xoff] = ptr[x] ? 0xFFFF : 0x0000;
						break;
					}
				default:
				case PC_TEX_FMT_BGRA32:
					{
						int  *dtp = (int*)dst;
						dtp[(y + yoff) * width + x + xoff] = ptr[x] ? 0xFFFFFFFF : 0xFF000000;
						break;
					}
				}
			}
			ptr += 9;
		}
		xoff += xcnt;
		fmt++;
	}

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

const char* texfmt[] = {
	// pixel
	"I4",      "I8",      "IA4",      "IA8",
	"RGB565",  "RGB5A3",  "RGBA8",    "0x07",
	"C4",      "C8",      "C14X2",    "0x0B",
	"0x0C",    "0x0D",    "CMPR",     "0x0F",
	// Z-buffer
	"0x10",    "Z8",      "0x12",     "Z16",
	"0x14",    "0x15",    "Z24X8",    "0x17",
	"0x18",    "0x19",    "0x1A",     "0x1B",
	"0x1C",    "0x1D",    "0x1E",     "0x1F",
	// pixel + copy
	"CR4",     "0x21",    "CRA4",    "CRA8",
	"0x24",    "0x25",    "CYUVA8",  "CA8",
	"CR8",     "CG8",     "CB8",     "CRG8",
	"CGB8",    "0x2D",    "0x2E",    "0x2F",
	// Z + copy
	"CZ4",     "0x31",    "0x32",    "0x33",
	"0x34",    "0x35",    "0x36",    "0x37",
	"0x38",    "CZ8M",    "CZ8L",    "0x3B",
	"CZ16L",   "0x3D",    "0x3E",    "0x3F",
};

const unsigned char sfont_map[] = {
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
	 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,10,10,10,10,10,
	10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,
	26,27,28,29,30,31,32,33,34,35,36,10,10,10,10,10,
	10,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,
	52,53,54,55,56,57,58,59,60,61,62,10,10,10,10,10,
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
};

const unsigned char sfont_raw[][9*10] = {
	{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff,
	0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0xff,
	0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0xff,
	0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0xff,
	0xff, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0x00, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	},{
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78,
	},
};
