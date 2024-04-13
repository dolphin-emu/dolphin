// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <span>
#include <tuple>

#include "Common/CommonTypes.h"
#include "Common/EnumFormatter.h"
#include "Common/SpanUtils.h"

enum
{
  TMEM_SIZE = 1024 * 1024,
  TMEM_LINE_SIZE = 32,
};
alignas(16) extern std::array<u8, TMEM_SIZE> s_tex_mem;

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

  // Special texture format used to represent YUVY xfb copies.
  // They aren't really textures, but they share so much hardware and usecases that it makes sense
  // to emulate them as part of texture cache.
  // This isn't a real value that can be used on console; it only exists for ease of implementation.
  XFB = 0xF,
};
template <>
struct fmt::formatter<TextureFormat> : EnumFormatter<TextureFormat::CMPR>
{
  static constexpr array_type names = {"I4",     "I8",    "IA4",   "IA8",   "RGB565",
                                       "RGB5A3", "RGBA8", nullptr, "C4",    "C8",
                                       "C14X2",  nullptr, nullptr, nullptr, "CMPR"};
  constexpr formatter() : EnumFormatter(names) {}
};

static inline bool IsColorIndexed(TextureFormat format)
{
  return format == TextureFormat::C4 || format == TextureFormat::C8 ||
         format == TextureFormat::C14X2;
}

static inline bool IsValidTextureFormat(TextureFormat format)
{
  return format <= TextureFormat::RGBA8 || IsColorIndexed(format) || format == TextureFormat::CMPR;
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

  // Special texture format used to represent YUVY xfb copies.
  // They aren't really textures, but they share so much hardware and usecases that it makes sense
  // to emulate them as part of texture cache.
  // This isn't a real value that can be used on console; it only exists for ease of implementation.
  XFB = 0xF,
};
template <>
struct fmt::formatter<EFBCopyFormat> : EnumFormatter<EFBCopyFormat::GB8>
{
  static constexpr array_type names = {
      "R4/I4/Z4",  "R8/I8/Z8H (?)", "RA4/IA4", "RA8/IA8 (Z16 too?)",
      "RGB565",    "RGB5A3",        "RGBA8",   "A8",
      "R8/I8/Z8H", "G8/Z8M",        "B8/Z8L",  "RG8/Z16R (Note: G and R are reversed)",
      "GB8/Z16L",
  };
  constexpr formatter() : EnumFormatter(names) {}
};

enum class TLUTFormat
{
  // These values represent TLUT format in GX registers.
  IA8 = 0x0,
  RGB565 = 0x1,
  RGB5A3 = 0x2,
};
template <>
struct fmt::formatter<TLUTFormat> : EnumFormatter<TLUTFormat::RGB5A3>
{
  constexpr formatter() : EnumFormatter({"IA8", "RGB565", "RGB5A3"}) {}
};

static inline bool IsValidTLUTFormat(TLUTFormat tlutfmt)
{
  return tlutfmt == TLUTFormat::IA8 || tlutfmt == TLUTFormat::RGB565 ||
         tlutfmt == TLUTFormat::RGB5A3;
}

static inline bool IsCompatibleTextureFormat(TextureFormat from_format, TextureFormat to_format)
{
  if (from_format == to_format)
    return true;

  // Indexed and paletted formats are "compatible", that is do not require conversion.
  switch (from_format)
  {
  case TextureFormat::I4:
  case TextureFormat::C4:
    return to_format == TextureFormat::I4 || to_format == TextureFormat::C4;

  case TextureFormat::I8:
  case TextureFormat::C8:
    return to_format == TextureFormat::I8 || to_format == TextureFormat::C8;

  default:
    return false;
  }
}

static inline bool CanReinterpretTextureOnGPU(TextureFormat from_format, TextureFormat to_format)
{
  // Currently, we can only reinterpret textures of the same width.
  switch (from_format)
  {
  case TextureFormat::I8:
  case TextureFormat::IA4:
    return to_format == TextureFormat::I8 || to_format == TextureFormat::IA4;

  case TextureFormat::IA8:
  case TextureFormat::RGB565:
  case TextureFormat::RGB5A3:
    return to_format == TextureFormat::IA8 || to_format == TextureFormat::RGB565 ||
           to_format == TextureFormat::RGB5A3;

  default:
    return false;
  }
}

inline std::span<u8> TexDecoder_GetTmemSpan(size_t offset = 0)
{
  return Common::SafeSubspan(std::span<u8>(s_tex_mem), offset);
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
void TexDecoder_DecodeTexel(u8* dst, std::span<const u8> src, int s, int t, int imageWidth,
                            TextureFormat texformat, std::span<const u8> tlut, TLUTFormat tlutfmt);
void TexDecoder_DecodeTexelRGBA8FromTmem(u8* dst, std::span<const u8> src_ar,
                                         std::span<const u8> src_gb, int s, int t, int imageWidth);
void TexDecoder_DecodeTexelRGBA8FromTmem(u8* dst, const u8* src_ar, const u8* src_gb, int s, int t,
                                         int imageWidth);
void TexDecoder_DecodeXFB(u8* dst, const u8* src, u32 width, u32 height, u32 stride);

void TexDecoder_SetTexFmtOverlayOptions(bool enable, bool center);

/* Internal method, implemented by TextureDecoder_Generic and TextureDecoder_x64. */
void _TexDecoder_DecodeImpl(u32* dst, const u8* src, int width, int height, TextureFormat texformat,
                            const u8* tlut, TLUTFormat tlutfmt);
