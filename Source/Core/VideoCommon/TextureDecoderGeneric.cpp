// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/TextureDecoderFunctions.h"
#include "VideoCommon/TextureDecoderGeneric.h"

// Texture decoding
PC_TexFormat TextureDecoderGeneric::Decode_C4(u32 *dst, const u8 *src, int width, int height, int tlutaddr, int tlutfmt)
{
	const int Wsteps8 = (width + 7) / 8;
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
	return PC_TEX_FMT_RGBA32;
}

PC_TexFormat TextureDecoderGeneric::Decode_C8(u32 *dst, const u8 *src, int width, int height, int tlutaddr, int tlutfmt)
{
	const int Wsteps8 = (width + 7) / 8;
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
	return PC_TEX_FMT_RGBA32;
}

PC_TexFormat TextureDecoderGeneric::Decode_I4(u32 *dst, const u8 *src, int width, int height)
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
	return PC_TEX_FMT_RGBA32;
}

PC_TexFormat TextureDecoderGeneric::Decode_I8(u32 *dst, const u8 *src, int width, int height)
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
	return PC_TEX_FMT_RGBA32;
}

PC_TexFormat TextureDecoderGeneric::Decode_IA4(u32 *dst, const u8 *src, int width, int height)
{
	const int Wsteps8 = (width + 7) / 8;
	for (int y = 0; y < height; y += 4)
		for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
			for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
				decodebytesIA4RGBA(dst + (y + iy) * width + x, src + 8 * xStep);
	return PC_TEX_FMT_RGBA32;
}

PC_TexFormat TextureDecoderGeneric::Decode_IA8(u32 *dst, const u8 *src, int width, int height)
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
	return PC_TEX_FMT_RGBA32;
}

PC_TexFormat TextureDecoderGeneric::Decode_C14X2(u32 *dst, const u8 *src, int width, int height, int tlutaddr, int tlutfmt)
{
	const int Wsteps4 = (width + 3) / 4;
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
	return PC_TEX_FMT_RGBA32;
}

PC_TexFormat TextureDecoderGeneric::Decode_RGB565(u32 *dst, const u8 *src, int width, int height)
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
	return PC_TEX_FMT_RGBA32;
}

PC_TexFormat TextureDecoderGeneric::Decode_RGB5A3(u32 *dst, const u8 *src, int width, int height)
{
	// Reference C implementation:
	for (int y = 0; y < height; y += 4)
		for (int x = 0; x < width; x += 4)
			for (int iy = 0; iy < 4; iy++, src += 8)
				decodebytesRGB5A3rgba(dst+(y+iy)*width+x, (u16*)src);
	return PC_TEX_FMT_RGBA32;
}

PC_TexFormat TextureDecoderGeneric::Decode_RGBA8(u32 *dst, const u8 *src, int width, int height)
{
	// Reference C implementation.
	for (int y = 0; y < height; y += 4)
		for (int x = 0; x < width; x += 4)
		{
			for (int iy = 0; iy < 4; iy++)
				decodebytesARGB8_4ToRgba(dst + (y+iy)*width + x, (u16*)src + 4 * iy, (u16*)src + 4 * iy + 16);
			src += 64;
		}
	return PC_TEX_FMT_RGBA32;
}

PC_TexFormat TextureDecoderGeneric::Decode_CMPR(u32 *dst, const u8 *src, int width, int height)
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
	return PC_TEX_FMT_RGBA32;
}

