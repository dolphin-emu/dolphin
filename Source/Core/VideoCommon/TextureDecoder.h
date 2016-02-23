// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

enum
{
	TMEM_SIZE = 1024 * 1024,
	TMEM_LINE_SIZE = 32,
};
alignas(16) extern u8 texMem[TMEM_SIZE];

enum TextureFormat
{
	// These are the texture formats that can be read by the texture mapper.
	GX_TF_I4     = 0x0,
	GX_TF_I8     = 0x1,
	GX_TF_IA4    = 0x2,
	GX_TF_IA8    = 0x3,
	GX_TF_RGB565 = 0x4,
	GX_TF_RGB5A3 = 0x5,
	GX_TF_RGBA8  = 0x6,
	GX_TF_C4     = 0x8,
	GX_TF_C8     = 0x9,
	GX_TF_C14X2  = 0xA,
	GX_TF_CMPR   = 0xE,

	_GX_TF_ZTF = 0x10,  // flag for Z texture formats (used internally by dolphin)

	// Depth texture formats (which directly map to the equivalent colour format above.)
	GX_TF_Z8     = 0x1 | _GX_TF_ZTF,
	GX_TF_Z16    = 0x3 | _GX_TF_ZTF,
	GX_TF_Z24X8  = 0x6 | _GX_TF_ZTF,

	_GX_TF_CTF   = 0x20,  // flag for copy-texture-format only (used internally by dolphin)

	// These are extra formats that can be used when copying from efb,
	// they use one of texel formats from above, but pack diffrent data into them.
	GX_CTF_R4    = 0x0 | _GX_TF_CTF,
	GX_CTF_RA4   = 0x2 | _GX_TF_CTF,
	GX_CTF_RA8   = 0x3 | _GX_TF_CTF,
	GX_CTF_YUVA8 = 0x6 | _GX_TF_CTF, // YUV 4:4:4 - Dolphin doesn't implement this format as no commercial games use it
	GX_CTF_A8    = 0x7 | _GX_TF_CTF,
	GX_CTF_R8    = 0x8 | _GX_TF_CTF,
	GX_CTF_G8    = 0x9 | _GX_TF_CTF,
	GX_CTF_B8    = 0xA | _GX_TF_CTF,
	GX_CTF_RG8   = 0xB | _GX_TF_CTF,
	GX_CTF_GB8   = 0xC | _GX_TF_CTF,

	// extra depth texture formats that can be used for efb copies.
	GX_CTF_Z4    = 0x0 | _GX_TF_ZTF | _GX_TF_CTF,
	GX_CTF_Z8H   = 0x8 | _GX_TF_ZTF | _GX_TF_CTF, // This produces an identical result to to GX_TF_Z8
	GX_CTF_Z8M   = 0x9 | _GX_TF_ZTF | _GX_TF_CTF,
	GX_CTF_Z8L   = 0xA | _GX_TF_ZTF | _GX_TF_CTF,
	GX_CTF_Z16R  = 0xB | _GX_TF_ZTF | _GX_TF_CTF, // Reversed version of GX_TF_Z16
	GX_CTF_Z16L  = 0xC | _GX_TF_ZTF | _GX_TF_CTF,
};

enum TlutFormat
{
	GX_TL_IA8    = 0x0,
	GX_TL_RGB565 = 0x1,
	GX_TL_RGB5A3 = 0x2,
};

int TexDecoder_GetTexelSizeInNibbles(int format);
int TexDecoder_GetTextureSizeInBytes(int width, int height, int format);
int TexDecoder_GetBlockWidthInTexels(u32 format);
int TexDecoder_GetBlockHeightInTexels(u32 format);
int TexDecoder_GetPaletteSize(int fmt);
int TexDecoder_GetEfbCopyBaseFormat(int format);

void TexDecoder_Decode(u8 *dst, const u8 *src, int width, int height, int texformat, const u8* tlut, TlutFormat tlutfmt);
void TexDecoder_DecodeRGBA8FromTmem(u8* dst, const u8 *src_ar, const u8 *src_gb, int width, int height);
void TexDecoder_DecodeTexel(u8 *dst, const u8 *src, int s, int t, int imageWidth, int texformat, const u8* tlut, TlutFormat tlutfmt);
void TexDecoder_DecodeTexelRGBA8FromTmem(u8 *dst, const u8 *src_ar, const u8* src_gb, int s, int t, int imageWidth);

void TexDecoder_SetTexFmtOverlayOptions(bool enable, bool center);

/* Internal method, implemented by TextureDecoder_Generic and TextureDecoder_x64. */
void _TexDecoder_DecodeImpl(u32 * dst, const u8 * src, int width, int height, int texformat, const u8* tlut, TlutFormat tlutfmt);
