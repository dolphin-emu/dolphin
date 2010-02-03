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

#ifndef _TEXTUREDECODER_H
#define _TEXTUREDECODER_H

enum 
{
    TMEM_SIZE = 1024*1024,
    HALFTMEM_SIZE = 512*1024
};
extern u8 texMem[TMEM_SIZE];

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

PC_TexFormat TexDecoder_Decode(u8 *dst, const u8 *src, int width, int height, int texformat, int tlutaddr, int tlutfmt);
void TexDecoder_DirectDecode(u8 *dst, const u8 *src, int width, int height,int Pitch, int texformat, int tlutaddr, int tlutfmt);
PC_TexFormat GetPC_TexFormat(int texformat, int tlutfmt);
void TexDecoder_DecodeTexel(u8 *dst, const u8 *src, int s, int t, int imageWidth, int texformat, int tlutaddr, int tlutfmt);


u32 TexDecoder_GetSafeTextureHash(const u8 *src, int width, int height, int texformat, u32 seed=0);
u32 TexDecoder_GetTlutHash(const u8* src, int len);

void TexDecoder_SetTexFmtOverlayOptions(bool enable, bool center);

#endif
