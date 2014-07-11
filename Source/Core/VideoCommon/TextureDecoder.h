// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/Hash.h"

enum
{
	TMEM_SIZE = 1024*1024,
	TMEM_LINE_SIZE = 32,
};
extern  GC_ALIGNED16(u8 texMem[TMEM_SIZE]);

enum TextureFormat
{
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

	_GX_TF_CTF   = 0x20,  // copy-texture-format only (simply means linear?)
	_GX_TF_ZTF   = 0x10,  // Z-texture-format

	// these formats are also valid when copying targets
	GX_CTF_R4    = 0x0 | _GX_TF_CTF,
	GX_CTF_RA4   = 0x2 | _GX_TF_CTF,
	GX_CTF_RA8   = 0x3 | _GX_TF_CTF,
	GX_CTF_YUVA8 = 0x6 | _GX_TF_CTF,
	GX_CTF_A8    = 0x7 | _GX_TF_CTF,
	GX_CTF_R8    = 0x8 | _GX_TF_CTF,
	GX_CTF_G8    = 0x9 | _GX_TF_CTF,
	GX_CTF_B8    = 0xA | _GX_TF_CTF,
	GX_CTF_RG8   = 0xB | _GX_TF_CTF,
	GX_CTF_GB8   = 0xC | _GX_TF_CTF,

	GX_TF_Z8     = 0x1 | _GX_TF_ZTF,
	GX_TF_Z16    = 0x3 | _GX_TF_ZTF,
	GX_TF_Z24X8  = 0x6 | _GX_TF_ZTF,

	GX_CTF_Z4    = 0x0 | _GX_TF_ZTF | _GX_TF_CTF,
	GX_CTF_Z8M   = 0x9 | _GX_TF_ZTF | _GX_TF_CTF,
	GX_CTF_Z8L   = 0xA | _GX_TF_ZTF | _GX_TF_CTF,
	GX_CTF_Z16L  = 0xC | _GX_TF_ZTF | _GX_TF_CTF,
};

extern const char* texfmt[];
extern const unsigned char sfont_map[];
extern const unsigned char sfont_raw[][9*10];

int TexDecoder_GetTexelSizeInNibbles(int format);
int TexDecoder_GetTextureSizeInBytes(int width, int height, int format);
int TexDecoder_GetBlockWidthInTexels(u32 format);
int TexDecoder_GetBlockHeightInTexels(u32 format);
int TexDecoder_GetPaletteSize(int fmt);

enum PC_TexFormat
{
	PC_TEX_FMT_NONE = 0,
	PC_TEX_FMT_BGRA32,
	PC_TEX_FMT_RGBA32,
	PC_TEX_FMT_I4_AS_I8,
	PC_TEX_FMT_IA4_AS_IA8,
	PC_TEX_FMT_I8,
	PC_TEX_FMT_IA8,
	PC_TEX_FMT_RGB565,
	PC_TEX_FMT_DXT1,
};

PC_TexFormat TexDecoder_Decode(u8 *dst, const u8 *src, int width, int height, int texformat, int tlutaddr, int tlutfmt,bool rgbaOnly = false);
PC_TexFormat GetPC_TexFormat(int texformat, int tlutfmt);
void TexDecoder_DecodeTexel(u8 *dst, const u8 *src, int s, int t, int imageWidth, int texformat, int tlutaddr, int tlutfmt);
void TexDecoder_DecodeTexelRGBA8FromTmem(u8 *dst, const u8 *src_ar, const u8* src_gb, int s, int t, int imageWidth);
PC_TexFormat TexDecoder_DecodeRGBA8FromTmem(u8* dst, const u8 *src_ar, const u8 *src_gb, int width, int height);
void TexDecoder_SetTexFmtOverlayOptions(bool enable, bool center);
