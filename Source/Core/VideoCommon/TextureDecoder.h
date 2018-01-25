// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <tuple>
#include "Common/CommonTypes.h"

enum
{
  TMEM_SIZE = 1024 * 1024,
  TMEM_LINE_SIZE = 32,
};
extern GC_ALIGNED16(u8 texMem[TMEM_SIZE]);

enum class TextureFormat
{
  // These values represent texture format in GX registers.
  I4 = 0x0,
  I8 = 0x1,
  IA4 = 0x2,
  IA8 = 0x3,
  RGB565 = 0x4,
  RGB5A3 = 0x5,
  RGBA8 = 0x6,
  C4 = 0x8,
  C8 = 0x9,
  C14X2 = 0xA,
  CMPR = 0xE,
};

static inline bool IsColorIndexed(TextureFormat format)
{
  return format == TextureFormat::C4 || format == TextureFormat::C8 ||
         format == TextureFormat::C14X2;
}

// The EFB Copy pipeline looks like:
//
//   1. Read EFB -> 2. Select color/depth -> 3. Downscale (optional)
//   -> 4. YUV conversion (optional) -> 5. Encode Tiles -> 6. Write RAM
//
// The "Encode Tiles" stage receives RGBA8 texels from previous stages and encodes them to various
// formats. EFBCopyFormat is the tile encoder mode. Note that the tile encoder does not care about
// color vs. depth or intensity formats - it only sees RGBA8 texels.
enum class EFBCopyFormat
{
  // These values represent EFB copy format in GX registers.
  // Most (but not all) of these values correspond to values of TextureFormat.
  R4 = 0x0,  // R4, I4, Z4

  // FIXME: Does 0x1 (Z8) have identical results to 0x8 (Z8H)?
  //        Is either or both of 0x1 and 0x8 used in games?
  R8_0x1 = 0x1,  // R8, I8, Z8H (?)

  RA4 = 0x2,  // RA4, IA4

  // FIXME: Earlier versions of this file named the value 0x3 "GX_TF_Z16", which does not reflect
  //        the results one would expect when copying from the depth buffer with this format.
  //        For reference: When copying from the depth buffer, R should receive the top 8 bits of
  //                       the Z value, and A should be either 0xFF or 0 (please investigate).
  //        Please test original hardware and make sure dolphin-emu implements this format
  //        correctly.
  RA8 = 0x3,  // RA8, IA8, (FIXME: Z16 too?)

  RGB565 = 0x4,
  RGB5A3 = 0x5,
  RGBA8 = 0x6,  // RGBA8, Z24
  A8 = 0x7,
  R8 = 0x8,   // R8, I8, Z8H
  G8 = 0x9,   // G8, Z8M
  B8 = 0xA,   // B8, Z8L
  RG8 = 0xB,  // RG8, Z16R (Note: G and R are reversed)
  GB8 = 0xC,  // GB8, Z16L
};

enum class TLUTFormat
{
  // These values represent TLUT format in GX registers.
  IA8 = 0x0,
  RGB565 = 0x1,
  RGB5A3 = 0x2,
};

static inline bool IsValidTLUTFormat(TLUTFormat tlutfmt)
{
  return tlutfmt == TLUTFormat::IA8 || tlutfmt == TLUTFormat::RGB565 ||
         tlutfmt == TLUTFormat::RGB5A3;
}

int TexDecoder_GetTexelSizeInNibbles(TextureFormat format);
int TexDecoder_GetTextureSizeInBytes(int width, int height, TextureFormat format);
int TexDecoder_GetBlockWidthInTexels(TextureFormat format);
int TexDecoder_GetBlockHeightInTexels(TextureFormat format);
int TexDecoder_GetEFBCopyBlockWidthInTexels(EFBCopyFormat format);
int TexDecoder_GetEFBCopyBlockHeightInTexels(EFBCopyFormat format);
int TexDecoder_GetPaletteSize(TextureFormat fmt);
TextureFormat TexDecoder_GetEFBCopyBaseFormat(EFBCopyFormat format);

void TexDecoder_Decode(u8* dst, const u8* src, int width, int height, TextureFormat texformat,
                       const u8* tlut, TLUTFormat tlutfmt);
void TexDecoder_DecodeRGBA8FromTmem(u8* dst, const u8* src_ar, const u8* src_gb, int width,
                                    int height);
void TexDecoder_DecodeTexel(u8* dst, const u8* src, int s, int t, int imageWidth,
                            TextureFormat texformat, const u8* tlut, TLUTFormat tlutfmt);
void TexDecoder_DecodeTexelRGBA8FromTmem(u8* dst, const u8* src_ar, const u8* src_gb, int s, int t,
                                         int imageWidth);

void TexDecoder_SetTexFmtOverlayOptions(bool enable, bool center);

/* Internal method, implemented by TextureDecoder_Generic and TextureDecoder_x64. */
void _TexDecoder_DecodeImpl(u32* dst, const u8* src, int width, int height, TextureFormat texformat,
                            const u8* tlut, TLUTFormat tlutfmt);
