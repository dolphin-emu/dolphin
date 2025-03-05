// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/TextureDecoder.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#ifdef CHECK
#include "Common/Assert.h"
#endif

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/Intrinsics.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"

#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/TextureDecoder_Util.h"

// GameCube/Wii texture decoder

// Decodes all known GameCube/Wii texture formats.
// by ector

static inline u32 DecodePixel_IA8(u16 val)
{
  int a = val & 0xFF;
  int i = val >> 8;
  return i | (i << 8) | (i << 16) | (a << 24);
}

static inline u32 DecodePixel_RGB565(u16 val)
{
  int r = Convert5To8((val >> 11) & 0x1f);
  int g = Convert6To8((val >> 5) & 0x3f);
  int b = Convert5To8((val) & 0x1f);
  int a = 0xFF;
  return r | (g << 8) | (b << 16) | (a << 24);
}

static inline u32 DecodePixel_RGB5A3(u16 val)
{
  int r, g, b, a;
  if ((val & 0x8000))
  {
    r = Convert5To8((val >> 10) & 0x1f);
    g = Convert5To8((val >> 5) & 0x1f);
    b = Convert5To8((val)&0x1f);
    a = 0xFF;
  }
  else
  {
    a = Convert3To8((val >> 12) & 0x7);
    r = Convert4To8((val >> 8) & 0xf);
    g = Convert4To8((val >> 4) & 0xf);
    b = Convert4To8((val)&0xf);
  }
  return r | (g << 8) | (b << 16) | (a << 24);
}

static inline void DecodeBytes_C4_IA8(u32* dst, const u8* src, const u8* tlut_)
{
  const u16* tlut = (u16*)tlut_;
  for (int x = 0; x < 4; x++)
  {
    u8 val = src[x];
    *dst++ = DecodePixel_IA8(tlut[val >> 4]);
    *dst++ = DecodePixel_IA8(tlut[val & 0xF]);
  }
}

static inline void DecodeBytes_C4_RGB565(u32* dst, const u8* src, const u8* tlut_)
{
  const u16* tlut = (u16*)tlut_;
  for (int x = 0; x < 4; x++)
  {
    u8 val = src[x];
    *dst++ = DecodePixel_RGB565(Common::swap16(tlut[val >> 4]));
    *dst++ = DecodePixel_RGB565(Common::swap16(tlut[val & 0xF]));
  }
}

static inline void DecodeBytes_C4_RGB5A3(u32* dst, const u8* src, const u8* tlut_)
{
  const u16* tlut = (u16*)tlut_;
  for (int x = 0; x < 4; x++)
  {
    u8 val = src[x];
    *dst++ = DecodePixel_RGB5A3(Common::swap16(tlut[val >> 4]));
    *dst++ = DecodePixel_RGB5A3(Common::swap16(tlut[val & 0xF]));
  }
}

static inline void DecodeBytes_C8_IA8(u32* dst, const u8* src, const u8* tlut_)
{
  const u16* tlut = (u16*)tlut_;
  for (int x = 0; x < 8; x++)
  {
    *dst++ = DecodePixel_IA8(tlut[src[x]]);
  }
}

static inline void DecodeBytes_C8_RGB565(u32* dst, const u8* src, const u8* tlut_)
{
  const u16* tlut = (u16*)tlut_;
  for (int x = 0; x < 8; x++)
  {
    u8 val = src[x];
    *dst++ = DecodePixel_RGB565(Common::swap16(tlut[val]));
  }
}

static inline void DecodeBytes_C8_RGB5A3(u32* dst, const u8* src, const u8* tlut_)
{
  const u16* tlut = (u16*)tlut_;
  for (int x = 0; x < 8; x++)
  {
    u8 val = src[x];
    *dst++ = DecodePixel_RGB5A3(Common::swap16(tlut[val]));
  }
}

static inline void DecodeBytes_C14X2_IA8(u32* dst, const u16* src, const u8* tlut_)
{
  const u16* tlut = (u16*)tlut_;
  for (int x = 0; x < 4; x++)
  {
    u16 val = Common::swap16(src[x]);
    *dst++ = DecodePixel_IA8(tlut[(val & 0x3FFF)]);
  }
}

static inline void DecodeBytes_C14X2_RGB565(u32* dst, const u16* src, const u8* tlut_)
{
  const u16* tlut = (u16*)tlut_;
  for (int x = 0; x < 4; x++)
  {
    u16 val = Common::swap16(src[x]);
    *dst++ = DecodePixel_RGB565(Common::swap16(tlut[(val & 0x3FFF)]));
  }
}

static inline void DecodeBytes_C14X2_RGB5A3(u32* dst, const u16* src, const u8* tlut_)
{
  const u16* tlut = (u16*)tlut_;
  for (int x = 0; x < 4; x++)
  {
    u16 val = Common::swap16(src[x]);
    *dst++ = DecodePixel_RGB5A3(Common::swap16(tlut[(val & 0x3FFF)]));
  }
}

static inline void DecodeBytes_IA4(u32* dst, const u8* src)
{
  for (int x = 0; x < 8; x++)
  {
    const u8 val = src[x];
    u8 a = Convert4To8(val >> 4);
    u8 l = Convert4To8(val & 0xF);
    dst[x] = (a << 24) | l << 16 | l << 8 | l;
  }
}

#ifdef CHECK
static void DecodeDXTBlock(u32* dst, const DXTBlock* src, int pitch)
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
  colors[0] = MakeRGBA(red1, green1, blue1, 255);
  colors[1] = MakeRGBA(red2, green2, blue2, 255);
  if (c1 > c2)
  {
    colors[2] =
        MakeRGBA(DXTBlend(red2, red1), DXTBlend(green2, green1), DXTBlend(blue2, blue1), 255);
    colors[3] =
        MakeRGBA(DXTBlend(red1, red2), DXTBlend(green1, green2), DXTBlend(blue1, blue2), 255);
  }
  else
  {
    // color[3] is the same as color[2] (average of both colors), but transparent.
    // This differs from DXT1 where color[3] is transparent black.
    colors[2] = MakeRGBA((red1 + red2) / 2, (green1 + green2) / 2, (blue1 + blue2) / 2, 255);
    colors[3] = MakeRGBA((red1 + red2) / 2, (green1 + green2) / 2, (blue1 + blue2) / 2, 0);
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
#endif

// JSD 01/06/11:
// TODO: we really should ensure BOTH the source and destination addresses are aligned to 16-byte
// boundaries to squeeze out a little more performance. _mm_loadu_si128/_mm_storeu_si128 is slower
// than _mm_load_si128/_mm_store_si128 because they work on unaligned addresses. The processor is
// free to make the assumption that addresses are multiples of 16 in the aligned case.
// TODO: complete SSE2 optimization of less often used texture formats.
// TODO: refactor algorithms using _mm_loadl_epi64 unaligned loads to prefer 128-bit aligned loads.
static void TexDecoder_DecodeImpl_C4(u32* dst, const u8* src, int width, int height,
                                     TextureFormat texformat, const u8* tlut, TLUTFormat tlutfmt,
                                     int Wsteps4, int Wsteps8)
{
  switch (tlutfmt)
  {
  case TLUTFormat::RGB5A3:
  {
    for (int y = 0; y < height; y += 8)
      for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8, yStep++)
        for (int iy = 0, xStep = 8 * yStep; iy < 8; iy++, xStep++)
          DecodeBytes_C4_RGB5A3(dst + (y + iy) * width + x, src + 4 * xStep, tlut);
  }
  break;

  case TLUTFormat::IA8:
  {
    for (int y = 0; y < height; y += 8)
      for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8, yStep++)
        for (int iy = 0, xStep = 8 * yStep; iy < 8; iy++, xStep++)
          DecodeBytes_C4_IA8(dst + (y + iy) * width + x, src + 4 * xStep, tlut);
  }
  break;

  case TLUTFormat::RGB565:
  {
    for (int y = 0; y < height; y += 8)
      for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8, yStep++)
        for (int iy = 0, xStep = 8 * yStep; iy < 8; iy++, xStep++)
          DecodeBytes_C4_RGB565(dst + (y + iy) * width + x, src + 4 * xStep, tlut);
  }
  break;

  default:
    break;
  }
}

FUNCTION_TARGET_SSSE3
static void TexDecoder_DecodeImpl_I4_SSSE3(u32* dst, const u8* src, int width, int height,
                                           TextureFormat texformat, const u8* tlut,
                                           TLUTFormat tlutfmt, int Wsteps4, int Wsteps8)
{
  const __m128i kMask_x0f = _mm_set1_epi32(0x0f0f0f0fL);
  const __m128i kMask_xf0 = _mm_set1_epi32(0xf0f0f0f0L);

  // xsacha optimized with SSSE3 intrinsics
  // Produces a ~40% speed improvement over SSE2 implementation
  const __m128i mask9180 = _mm_set_epi8(9, 9, 9, 9, 1, 1, 1, 1, 8, 8, 8, 8, 0, 0, 0, 0);
  const __m128i maskB3A2 = _mm_set_epi8(11, 11, 11, 11, 3, 3, 3, 3, 10, 10, 10, 10, 2, 2, 2, 2);
  const __m128i maskD5C4 = _mm_set_epi8(13, 13, 13, 13, 5, 5, 5, 5, 12, 12, 12, 12, 4, 4, 4, 4);
  const __m128i maskF7E6 = _mm_set_epi8(15, 15, 15, 15, 7, 7, 7, 7, 14, 14, 14, 14, 6, 6, 6, 6);
  for (int y = 0; y < height; y += 8)
  {
    for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8, yStep++)
    {
      for (int iy = 0, xStep = 4 * yStep; iy < 8; iy += 2, xStep++)
      {
        const __m128i r0 = _mm_loadl_epi64((const __m128i*)(src + 8 * xStep));
        // We want the hi 4 bits of each 8-bit word replicated to 32-bit words:
        // (00000000 00000000 HhGgFfEe DdCcBbAa) -> (00000000 00000000 HHGGFFEE DDCCBBAA)
        const __m128i i1 = _mm_and_si128(r0, kMask_xf0);
        const __m128i i11 = _mm_or_si128(i1, _mm_srli_epi16(i1, 4));

        // Now we do same as above for the second half of the byte
        const __m128i i2 = _mm_and_si128(r0, kMask_x0f);
        const __m128i i22 = _mm_or_si128(i2, _mm_slli_epi16(i2, 4));

        // Combine both sides
        const __m128i base = _mm_unpacklo_epi64(i11, i22);
        // Achieve the pattern visible in the masks.
        const __m128i o1 = _mm_shuffle_epi8(base, mask9180);
        const __m128i o2 = _mm_shuffle_epi8(base, maskB3A2);
        const __m128i o3 = _mm_shuffle_epi8(base, maskD5C4);
        const __m128i o4 = _mm_shuffle_epi8(base, maskF7E6);

        // Write row 0:
        _mm_storeu_si128((__m128i*)(dst + (y + iy) * width + x), o1);
        _mm_storeu_si128((__m128i*)(dst + (y + iy) * width + x + 4), o2);
        // Write row 1:
        _mm_storeu_si128((__m128i*)(dst + (y + iy + 1) * width + x), o3);
        _mm_storeu_si128((__m128i*)(dst + (y + iy + 1) * width + x + 4), o4);
      }
    }
  }
}

static void TexDecoder_DecodeImpl_I4(u32* dst, const u8* src, int width, int height,
                                     TextureFormat texformat, const u8* tlut, TLUTFormat tlutfmt,
                                     int Wsteps4, int Wsteps8)
{
  const __m128i kMask_x0f = _mm_set1_epi32(0x0f0f0f0fL);
  const __m128i kMask_xf0 = _mm_set1_epi32(0xf0f0f0f0L);

  // JSD optimized with SSE2 intrinsics.
  // Produces a ~76% speed improvement over reference C implementation.
  for (int y = 0; y < height; y += 8)
  {
    for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8, yStep++)
    {
      for (int iy = 0, xStep = 4 * yStep; iy < 8; iy += 2, xStep++)
      {
        const __m128i r0 = _mm_loadl_epi64((const __m128i*)(src + 8 * xStep));
        // Shuffle low 64-bits with itself to expand from (0000 0000 hgfe dcba) to (hhgg ffee
        // ddcc bbaa)
        const __m128i r1 = _mm_unpacklo_epi8(r0, r0);

        // We want the hi 4 bits of each 8-bit word replicated to 32-bit words:
        // (HhHhGgGg FfFfEeEe DdDdCcCc BbBbAaAa) & kMask_xf0 -> (H0H0G0G0 F0F0E0E0 D0D0C0C0
        // B0B0A0A0)
        const __m128i i1 = _mm_and_si128(r1, kMask_xf0);
        // -> (HHHHGGGG FFFFEEEE DDDDCCCC BBBBAAAA)
        const __m128i i11 = _mm_or_si128(i1, _mm_srli_epi16(i1, 4));

        // Shuffle low 64-bits with itself to expand from (HHHHGGGG FFFFEEEE DDDDCCCC BBBBAAAA)
        // to (DDDDDDDD CCCCCCCC BBBBBBBB AAAAAAAA)
        const __m128i i15 = _mm_unpacklo_epi8(i11, i11);
        // (DDDDDDDD CCCCCCCC BBBBBBBB AAAAAAAA) -> (BBBBBBBB BBBBBBBB AAAAAAAA AAAAAAAA)
        const __m128i i151 = _mm_unpacklo_epi8(i15, i15);
        // (DDDDDDDD CCCCCCCC BBBBBBBB AAAAAAAA) -> (DDDDDDDD DDDDDDDD CCCCCCCC CCCCCCCC)
        const __m128i i152 = _mm_unpackhi_epi8(i15, i15);

        // Shuffle hi  64-bits with itself to expand from (HHHHGGGG FFFFEEEE DDDDCCCC BBBBAAAA)
        // to (HHHHHHHH GGGGGGGG FFFFFFFF EEEEEEEE)
        const __m128i i16 = _mm_unpackhi_epi8(i11, i11);
        // (HHHHHHHH GGGGGGGG FFFFFFFF EEEEEEEE) -> (FFFFFFFF FFFFFFFF EEEEEEEE EEEEEEEE)
        const __m128i i161 = _mm_unpacklo_epi8(i16, i16);
        // (HHHHHHHH GGGGGGGG FFFFFFFF EEEEEEEE) -> (HHHHHHHH HHHHHHHH GGGGGGGG GGGGGGGG)
        const __m128i i162 = _mm_unpackhi_epi8(i16, i16);

        // Now find the lo 4 bits of each input 8-bit word:
        const __m128i i2 = _mm_and_si128(r1, kMask_x0f);
        const __m128i i22 = _mm_or_si128(i2, _mm_slli_epi16(i2, 4));

        const __m128i i25 = _mm_unpacklo_epi8(i22, i22);
        const __m128i i251 = _mm_unpacklo_epi8(i25, i25);
        const __m128i i252 = _mm_unpackhi_epi8(i25, i25);

        const __m128i i26 = _mm_unpackhi_epi8(i22, i22);
        const __m128i i261 = _mm_unpacklo_epi8(i26, i26);
        const __m128i i262 = _mm_unpackhi_epi8(i26, i26);

        // _mm_and_si128(i151, kMask_x00000000ffffffff) takes i151 and masks off 1st and 3rd
        // 32-bit words
        // (BBBBBBBB BBBBBBBB AAAAAAAA AAAAAAAA) -> (00000000 BBBBBBBB 00000000 AAAAAAAA)
        // _mm_and_si128(i251, kMask_xffffffff00000000) takes i251 and masks off 2nd and 4th
        // 32-bit words
        // (bbbbbbbb bbbbbbbb aaaaaaaa aaaaaaaa) -> (bbbbbbbb 00000000 aaaaaaaa 00000000)
        // And last but not least, _mm_or_si128 ORs those two together, giving us the
        // interleaving we desire:
        // (00000000 BBBBBBBB 00000000 AAAAAAAA) | (bbbbbbbb 00000000 aaaaaaaa 00000000) ->
        // (bbbbbbbb BBBBBBBB aaaaaaaa AAAAAAAA)
        const __m128i kMask_x00000000ffffffff =
            _mm_set_epi32(0x00000000L, 0xffffffffL, 0x00000000L, 0xffffffffL);
        const __m128i kMask_xffffffff00000000 =
            _mm_set_epi32(0xffffffffL, 0x00000000L, 0xffffffffL, 0x00000000L);
        const __m128i o1 = _mm_or_si128(_mm_and_si128(i151, kMask_x00000000ffffffff),
                                        _mm_and_si128(i251, kMask_xffffffff00000000));
        const __m128i o2 = _mm_or_si128(_mm_and_si128(i152, kMask_x00000000ffffffff),
                                        _mm_and_si128(i252, kMask_xffffffff00000000));

        // These two are for the next row; same pattern as above. We batched up two rows because
        // our input was 64 bits.
        const __m128i o3 = _mm_or_si128(_mm_and_si128(i161, kMask_x00000000ffffffff),
                                        _mm_and_si128(i261, kMask_xffffffff00000000));
        const __m128i o4 = _mm_or_si128(_mm_and_si128(i162, kMask_x00000000ffffffff),
                                        _mm_and_si128(i262, kMask_xffffffff00000000));
        // Write row 0:
        _mm_storeu_si128((__m128i*)(dst + (y + iy) * width + x), o1);
        _mm_storeu_si128((__m128i*)(dst + (y + iy) * width + x + 4), o2);
        // Write row 1:
        _mm_storeu_si128((__m128i*)(dst + (y + iy + 1) * width + x), o3);
        _mm_storeu_si128((__m128i*)(dst + (y + iy + 1) * width + x + 4), o4);
      }
    }
  }
}

FUNCTION_TARGET_SSSE3
static void TexDecoder_DecodeImpl_I8_SSSE3(u32* dst, const u8* src, int width, int height,
                                           TextureFormat texformat, const u8* tlut,
                                           TLUTFormat tlutfmt, int Wsteps4, int Wsteps8)
{
  // xsacha optimized with SSSE3 intrinsics
  // Produces a ~10% speed improvement over SSE2 implementation
  for (int y = 0; y < height; y += 4)
  {
    for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
    {
      for (int iy = 0, xStep = 4 * yStep; iy < 4; ++iy, xStep++)
      {
        const __m128i mask3210 = _mm_set_epi8(3, 3, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0);

        const __m128i mask7654 = _mm_set_epi8(7, 7, 7, 7, 6, 6, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4);
        // Load 64 bits from `src` into an __m128i with upper 64 bits zeroed: (0000 0000 hgfe dcba)
        __m128i r = _mm_loadl_epi64((const __m128i*)(src + 8 * xStep));
        // Shuffle select bytes to expand from (0000 0000 hgfe dcba) to:
        __m128i rgba0 = _mm_shuffle_epi8(r, mask3210); // (dddd cccc bbbb aaaa)
        __m128i rgba1 = _mm_shuffle_epi8(r, mask7654); // (hhhh gggg ffff eeee)

        __m128i* quaddst = (__m128i*)(dst + (y + iy) * width + x);
        _mm_storeu_si128(quaddst, rgba0);
        _mm_storeu_si128(quaddst + 1, rgba1);
      }
    }
  }
}

static void TexDecoder_DecodeImpl_I8(u32* dst, const u8* src, int width, int height,
                                     TextureFormat texformat, const u8* tlut, TLUTFormat tlutfmt,
                                     int Wsteps4, int Wsteps8)
{
  // JSD optimized with SSE2 intrinsics.
  // Produces an ~86% speed improvement over reference C implementation.
  for (int y = 0; y < height; y += 4)
  {
    for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
    {
      // Each loop iteration processes 4 rows from 4 64-bit reads.
      const u8* src2 = src + 32 * yStep;
      // TODO: is it more efficient to group the loads together sequentially and also the stores
      // at the end? _mm_stream instead of _mm_store on my AMD Phenom II x410 made performance
      // significantly WORSE, so I went with _mm_stores. Perhaps there is some edge case here
      // creating the terrible performance or we're not aligned to 16-byte boundaries. I don't know.
      __m128i* quaddst;

      // Load 64 bits from `src` into an __m128i with upper 64 bits zeroed: (0000 0000 hgfe dcba)
      const __m128i r0 = _mm_loadl_epi64((const __m128i*)src2);
      // Shuffle low 64-bits with itself to expand from (0000 0000 hgfe dcba) to (hhgg ffee ddcc
      // bbaa)
      const __m128i r1 = _mm_unpacklo_epi8(r0, r0);

      // Shuffle low 64-bits with itself to expand from (hhgg ffee ddcc bbaa) to (dddd cccc bbbb
      // aaaa)
      const __m128i rgba0 = _mm_unpacklo_epi8(r1, r1);
      // Shuffle hi 64-bits with itself to expand from (hhgg ffee ddcc bbaa) to (hhhh gggg ffff
      // eeee)
      const __m128i rgba1 = _mm_unpackhi_epi8(r1, r1);

      // Store (dddd cccc bbbb aaaa) out:
      quaddst = (__m128i*)(dst + (y + 0) * width + x);
      _mm_storeu_si128(quaddst, rgba0);
      // Store (hhhh gggg ffff eeee) out:
      _mm_storeu_si128(quaddst + 1, rgba1);

      // Load 64 bits from `src` into an __m128i with upper 64 bits zeroed: (0000 0000 hgfe dcba)
      src2 += 8;
      const __m128i r2 = _mm_loadl_epi64((const __m128i*)src2);
      // Shuffle low 64-bits with itself to expand from (0000 0000 hgfe dcba) to (hhgg ffee ddcc
      // bbaa)
      const __m128i r3 = _mm_unpacklo_epi8(r2, r2);

      // Shuffle low 64-bits with itself to expand from (hhgg ffee ddcc bbaa) to (dddd cccc bbbb
      // aaaa)
      const __m128i rgba2 = _mm_unpacklo_epi8(r3, r3);
      // Shuffle hi 64-bits with itself to expand from (hhgg ffee ddcc bbaa) to (hhhh gggg ffff
      // eeee)
      const __m128i rgba3 = _mm_unpackhi_epi8(r3, r3);

      // Store (dddd cccc bbbb aaaa) out:
      quaddst = (__m128i*)(dst + (y + 1) * width + x);
      _mm_storeu_si128(quaddst, rgba2);
      // Store (hhhh gggg ffff eeee) out:
      _mm_storeu_si128(quaddst + 1, rgba3);

      // Load 64 bits from `src` into an __m128i with upper 64 bits zeroed: (0000 0000 hgfe dcba)
      src2 += 8;
      const __m128i r4 = _mm_loadl_epi64((const __m128i*)src2);
      // Shuffle low 64-bits with itself to expand from (0000 0000 hgfe dcba) to (hhgg ffee ddcc
      // bbaa)
      const __m128i r5 = _mm_unpacklo_epi8(r4, r4);

      // Shuffle low 64-bits with itself to expand from (hhgg ffee ddcc bbaa) to (dddd cccc bbbb
      // aaaa)
      const __m128i rgba4 = _mm_unpacklo_epi8(r5, r5);
      // Shuffle hi 64-bits with itself to expand from (hhgg ffee ddcc bbaa) to (hhhh gggg ffff
      // eeee)
      const __m128i rgba5 = _mm_unpackhi_epi8(r5, r5);

      // Store (dddd cccc bbbb aaaa) out:
      quaddst = (__m128i*)(dst + (y + 2) * width + x);
      _mm_storeu_si128(quaddst, rgba4);
      // Store (hhhh gggg ffff eeee) out:
      _mm_storeu_si128(quaddst + 1, rgba5);

      // Load 64 bits from `src` into an __m128i with upper 64 bits zeroed: (0000 0000 hgfe dcba)
      src2 += 8;
      const __m128i r6 = _mm_loadl_epi64((const __m128i*)src2);
      // Shuffle low 64-bits with itself to expand from (0000 0000 hgfe dcba) to (hhgg ffee ddcc
      // bbaa)
      const __m128i r7 = _mm_unpacklo_epi8(r6, r6);

      // Shuffle low 64-bits with itself to expand from (hhgg ffee ddcc bbaa) to (dddd cccc bbbb
      // aaaa)
      const __m128i rgba6 = _mm_unpacklo_epi8(r7, r7);
      // Shuffle hi 64-bits with itself to expand from (hhgg ffee ddcc bbaa) to (hhhh gggg ffff
      // eeee)
      const __m128i rgba7 = _mm_unpackhi_epi8(r7, r7);

      // Store (dddd cccc bbbb aaaa) out:
      quaddst = (__m128i*)(dst + (y + 3) * width + x);
      _mm_storeu_si128(quaddst, rgba6);
      // Store (hhhh gggg ffff eeee) out:
      _mm_storeu_si128(quaddst + 1, rgba7);
    }
  }
}

static void TexDecoder_DecodeImpl_C8(u32* dst, const u8* src, int width, int height,
                                     TextureFormat texformat, const u8* tlut, TLUTFormat tlutfmt,
                                     int Wsteps4, int Wsteps8)
{
  switch (tlutfmt)
  {
  case TLUTFormat::RGB5A3:
  {
    for (int y = 0; y < height; y += 4)
      for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
        for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
          DecodeBytes_C8_RGB5A3((u32*)dst + (y + iy) * width + x, src + 8 * xStep, tlut);
  }
  break;

  case TLUTFormat::IA8:
  {
    for (int y = 0; y < height; y += 4)
      for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
        for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
          DecodeBytes_C8_IA8(dst + (y + iy) * width + x, src + 8 * xStep, tlut);
  }
  break;

  case TLUTFormat::RGB565:
  {
    for (int y = 0; y < height; y += 4)
      for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
        for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
          DecodeBytes_C8_RGB565(dst + (y + iy) * width + x, src + 8 * xStep, tlut);
  }
  break;

  default:
    break;
  }
}

static void TexDecoder_DecodeImpl_IA4(u32* dst, const u8* src, int width, int height,
                                      TextureFormat texformat, const u8* tlut, TLUTFormat tlutfmt,
                                      int Wsteps4, int Wsteps8)
{
  for (int y = 0; y < height; y += 4)
  {
    for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
    {
      for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
      {
        DecodeBytes_IA4(dst + (y + iy) * width + x, src + 8 * xStep);
      }
    }
  }
}

FUNCTION_TARGET_SSSE3
static void TexDecoder_DecodeImpl_IA8_SSSE3(u32* dst, const u8* src, int width, int height,
                                            TextureFormat texformat, const u8* tlut,
                                            TLUTFormat tlutfmt, int Wsteps4, int Wsteps8)
{
  // xsacha optimized with SSSE3 intrinsics.
  // Produces an ~50% speed improvement over SSE2 implementation.
  for (int y = 0; y < height; y += 4)
  {
    for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
    {
      for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
      {
        const __m128i mask = _mm_set_epi8(6, 7, 7, 7, 4, 5, 5, 5, 2, 3, 3, 3, 0, 1, 1, 1);
        // Load 4x 16-bit IA8 samples from `src` into an __m128i with upper 64 bits zeroed:
        // (0000 0000 hgfe dcba)
        const __m128i r0 = _mm_loadl_epi64((const __m128i*)(src + 8 * xStep));
        // Shuffle to (ghhh efff cddd abbb)
        const __m128i r1 = _mm_shuffle_epi8(r0, mask);
        _mm_storeu_si128((__m128i*)(dst + (y + iy) * width + x), r1);
      }
    }
  }
}

static void TexDecoder_DecodeImpl_IA8(u32* dst, const u8* src, int width, int height,
                                      TextureFormat texformat, const u8* tlut, TLUTFormat tlutfmt,
                                      int Wsteps4, int Wsteps8)
{
  // JSD optimized with SSE2 intrinsics.
  // Produces an ~80% speed improvement over reference C implementation.
  const __m128i kMask_xf0 = _mm_set_epi32(0x00000000L, 0x00000000L, 0xff00ff00L, 0xff00ff00L);
  const __m128i kMask_x0f = _mm_set_epi32(0x00000000L, 0x00000000L, 0x00ff00ffL, 0x00ff00ffL);
  const __m128i kMask_xf000 = _mm_set_epi32(0xff000000L, 0xff000000L, 0xff000000L, 0xff000000L);
  const __m128i kMask_x0fff = _mm_set_epi32(0x00ffffffL, 0x00ffffffL, 0x00ffffffL, 0x00ffffffL);
  for (int y = 0; y < height; y += 4)
  {
    for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
    {
      for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
      {
        // Expands a 16-bit "IA" to a 32-bit "AIII". Each char is an 8-bit value.

        // Load 4x 16-bit IA8 samples from `src` into an __m128i with upper 64 bits zeroed:
        // (0000 0000 hgfe dcba)
        const __m128i r0 = _mm_loadl_epi64((const __m128i*)(src + 8 * xStep));

        // Logical shift all 16-bit words right by 8 bits (0000 0000 hgfe dcba) to (0000 0000
        // 0h0f 0d0b). This gets us only the I components.
        const __m128i i0 = _mm_srli_epi16(r0, 8);

        // Now join up the I components from their original positions but mask out the A
        // components.
        // (0000 0000 hgfe dcba) &      kMask_xFF00      -> (0000 0000 h0f0 d0b0)
        // (0000 0000 h0f0 d0b0) | (0000 0000 0h0f 0d0b) -> (0000 0000 hhff ddbb)
        const __m128i i1 = _mm_or_si128(_mm_and_si128(r0, kMask_xf0), i0);

        // Shuffle low 64-bits with itself to expand from (0000 0000 hhff ddbb) to (hhhh ffff
        // dddd bbbb)
        const __m128i i2 = _mm_unpacklo_epi8(i1, i1);
        // (hhhh ffff dddd bbbb) & kMask_x0fff -> (0hhh 0fff 0ddd 0bbb)
        const __m128i i3 = _mm_and_si128(i2, kMask_x0fff);

        // Now that we have the I components in 32-bit word form, time work out the A components
        // into their final positions.

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
        _mm_storeu_si128((__m128i*)(dst + (y + iy) * width + x), r1);
      }
    }
  }
}

static void TexDecoder_DecodeImpl_C14X2(u32* dst, const u8* src, int width, int height,
                                        TextureFormat texformat, const u8* tlut, TLUTFormat tlutfmt,
                                        int Wsteps4, int Wsteps8)
{
  switch (tlutfmt)
  {
  case TLUTFormat::RGB5A3:
  {
    for (int y = 0; y < height; y += 4)
      for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
        for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
          DecodeBytes_C14X2_RGB5A3(dst + (y + iy) * width + x, (u16*)(src + 8 * xStep), tlut);
  }
  break;

  case TLUTFormat::IA8:
  {
    for (int y = 0; y < height; y += 4)
      for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
        for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
          DecodeBytes_C14X2_IA8(dst + (y + iy) * width + x, (u16*)(src + 8 * xStep), tlut);
  }
  break;

  case TLUTFormat::RGB565:
  {
    for (int y = 0; y < height; y += 4)
      for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
        for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
          DecodeBytes_C14X2_RGB565(dst + (y + iy) * width + x, (u16*)(src + 8 * xStep), tlut);
  }
  break;

  default:
    break;
  }
}

static void TexDecoder_DecodeImpl_RGB565(u32* dst, const u8* src, int width, int height,
                                         TextureFormat texformat, const u8* tlut,
                                         TLUTFormat tlutfmt, int Wsteps4, int Wsteps8)
{
  // JSD optimized with SSE2 intrinsics.
  // Produces an ~78% speed improvement over reference C implementation.
  const __m128i kMaskR0 = _mm_set1_epi32(0x000000F8);
  const __m128i kMaskG0 = _mm_set1_epi32(0x0000FC00);
  const __m128i kMaskG1 = _mm_set1_epi32(0x00000300);
  const __m128i kMaskB0 = _mm_set1_epi32(0x00F80000);
  const __m128i kAlpha = _mm_set1_epi32(0xFF000000);
  for (int y = 0; y < height; y += 4)
  {
    for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
    {
      for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
      {
        __m128i* dxtsrc = (__m128i*)(src + 8 * xStep);
        // Load 4x 16-bit colors: (0000 0000 hgfe dcba)
        // where hg, fe, ba, and dc are 16-bit colors in big-endian order
        const __m128i rgb565x4 = _mm_loadl_epi64(dxtsrc);

        // The big-endian 16-bit colors `ba` and `dc` look like 0b_gggBBBbb_RRRrrGGg in a little
        // endian xmm register Unpack `hgfe dcba` to `hhgg ffee ddcc bbaa`, where each 32-bit word
        // is now 0b_gggBBBbb_RRRrrGGg_gggBBBbb_RRRrrGGg
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
        const __m128i abgr888x4 =
            _mm_or_si128(_mm_or_si128(_mm_or_si128(r0, r1), _mm_or_si128(g0, g1)),
                         _mm_or_si128(_mm_or_si128(b0, b1), kAlpha));

        __m128i* ptr = (__m128i*)(dst + (y + iy) * width + x);
        _mm_storeu_si128(ptr, abgr888x4);
      }
    }
  }
}

FUNCTION_TARGET_SSSE3
static void TexDecoder_DecodeImpl_RGB5A3_SSSE3(u32* dst, const u8* src, int width, int height,
                                               TextureFormat texformat, const u8* tlut,
                                               TLUTFormat tlutfmt, int Wsteps4, int Wsteps8)
{
  const __m128i kMask_x1f = _mm_set1_epi32(0x0000001fL);
  const __m128i kMask_x0f = _mm_set1_epi32(0x0000000fL);
  const __m128i kMask_x07 = _mm_set1_epi32(0x00000007L);
  // This is the hard-coded 0xFF alpha constant that is ORed in place after the RGB are calculated
  // for the RGB555 case when (s[x] & 0x8000) is true for all pixels.
  const __m128i aVxff00 = _mm_set1_epi32(0xFF000000L);

  // xsacha optimized with SSSE3 intrinsics (2 in 4 cases)
  // Produces a ~10% speed improvement over SSE2 implementation
  for (int y = 0; y < height; y += 4)
  {
    for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
    {
      for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
      {
        u32* newdst = dst + (y + iy) * width + x;
        const __m128i mask =
            _mm_set_epi8(-128, -128, 6, 7, -128, -128, 4, 5, -128, -128, 2, 3, -128, -128, 0, 1);
        const __m128i valV =
            _mm_shuffle_epi8(_mm_loadl_epi64((const __m128i*)(src + 8 * xStep)), mask);
        int cmp = _mm_movemask_epi8(valV);  // MSB: 0x2 = val0; 0x20=val1; 0x200 = val2; 0x2000=val3
        if ((cmp & 0x2222) ==
            0x2222)  // SSSE3 case #1: all 4 pixels are in RGB555 and alpha = 0xFF.
        {
          // Swizzle bits: 00012345 -> 12345123

          // r0 = (((val0>>10) & 0x1f) << 3) | (((val0>>10) & 0x1f) >> 2);
          const __m128i tmprV = _mm_and_si128(_mm_srli_epi16(valV, 10), kMask_x1f);
          const __m128i rV = _mm_or_si128(_mm_slli_epi16(tmprV, 3), _mm_srli_epi16(tmprV, 2));

          // g0 = (((val0>>5 ) & 0x1f) << 3) | (((val0>>5 ) & 0x1f) >> 2);
          const __m128i tmpgV = _mm_and_si128(_mm_srli_epi16(valV, 5), kMask_x1f);
          const __m128i gV = _mm_or_si128(_mm_slli_epi16(tmpgV, 3), _mm_srli_epi16(tmpgV, 2));

          // b0 = (((val0    ) & 0x1f) << 3) | (((val0    ) & 0x1f) >> 2);
          const __m128i tmpbV = _mm_and_si128(valV, kMask_x1f);
          const __m128i bV = _mm_or_si128(_mm_slli_epi16(tmpbV, 3), _mm_srli_epi16(tmpbV, 2));

          // newdst[0] = r0 | (g0 << 8) | (b0 << 16) | (a0 << 24);
          const __m128i final = _mm_or_si128(_mm_or_si128(rV, _mm_slli_epi32(gV, 8)),
                                             _mm_or_si128(_mm_slli_epi32(bV, 16), aVxff00));
          _mm_storeu_si128((__m128i*)newdst, final);
        }
        else if (!(cmp & 0x2222))  // SSSE3 case #2: all 4 pixels are in RGBA4443.
        {
          // Swizzle bits: 00001234 -> 12341234

          // r0 = (((val0>>8 ) & 0xf) << 4) | ((val0>>8 ) & 0xf);
          const __m128i tmprV = _mm_and_si128(_mm_srli_epi16(valV, 8), kMask_x0f);
          const __m128i rV = _mm_or_si128(_mm_slli_epi16(tmprV, 4), tmprV);

          // g0 = (((val0>>4 ) & 0xf) << 4) | ((val0>>4 ) & 0xf);
          const __m128i tmpgV = _mm_and_si128(_mm_srli_epi16(valV, 4), kMask_x0f);
          const __m128i gV = _mm_or_si128(_mm_slli_epi16(tmpgV, 4), tmpgV);

          // b0 = (((val0    ) & 0xf) << 4) | ((val0    ) & 0xf);
          const __m128i tmpbV = _mm_and_si128(valV, kMask_x0f);
          const __m128i bV = _mm_or_si128(_mm_slli_epi16(tmpbV, 4), tmpbV);
          // a0 = (((val0>>12) & 0x7) << 5) | (((val0>>12) & 0x7) << 2) | (((val0>>12) & 0x7) >> 1);
          const __m128i tmpaV = _mm_and_si128(_mm_srli_epi16(valV, 12), kMask_x07);
          const __m128i aV =
              _mm_or_si128(_mm_slli_epi16(tmpaV, 5),
                           _mm_or_si128(_mm_slli_epi16(tmpaV, 2), _mm_srli_epi16(tmpaV, 1)));

          // newdst[0] = r0 | (g0 << 8) | (b0 << 16) | (a0 << 24);
          const __m128i final =
              _mm_or_si128(_mm_or_si128(rV, _mm_slli_epi32(gV, 8)),
                           _mm_or_si128(_mm_slli_epi32(bV, 16), _mm_slli_epi32(aV, 24)));
          _mm_storeu_si128((__m128i*)newdst, final);
        }
        else
        {
          // TODO: Vectorise (Either 4-way branch or do both and select is better than this)
          u32* vals = (u32*)&valV;
          int r, g, b, a;
          for (int i = 0; i < 4; ++i)
          {
            if (vals[i] & 0x8000)
            {
              // Swizzle bits: 00012345 -> 12345123
              r = (((vals[i] >> 10) & 0x1f) << 3) | (((vals[i] >> 10) & 0x1f) >> 2);
              g = (((vals[i] >> 5) & 0x1f) << 3) | (((vals[i] >> 5) & 0x1f) >> 2);
              b = (((vals[i]) & 0x1f) << 3) | (((vals[i]) & 0x1f) >> 2);
              a = 0xFF;
            }
            else
            {
              a = (((vals[i] >> 12) & 0x7) << 5) | (((vals[i] >> 12) & 0x7) << 2) |
                  (((vals[i] >> 12) & 0x7) >> 1);
              // Swizzle bits: 00001234 -> 12341234
              r = (((vals[i] >> 8) & 0xf) << 4) | ((vals[i] >> 8) & 0xf);
              g = (((vals[i] >> 4) & 0xf) << 4) | ((vals[i] >> 4) & 0xf);
              b = (((vals[i]) & 0xf) << 4) | ((vals[i]) & 0xf);
            }
            newdst[i] = r | (g << 8) | (b << 16) | (a << 24);
          }
        }
      }
    }
  }
}

static void TexDecoder_DecodeImpl_RGB5A3(u32* dst, const u8* src, int width, int height,
                                         TextureFormat texformat, const u8* tlut,
                                         TLUTFormat tlutfmt, int Wsteps4, int Wsteps8)
{
  const __m128i kMask_x1f = _mm_set1_epi32(0x0000001fL);
  const __m128i kMask_x0f = _mm_set1_epi32(0x0000000fL);
  const __m128i kMask_x07 = _mm_set1_epi32(0x00000007L);
  // This is the hard-coded 0xFF alpha constant that is ORed in place after the RGB are calculated
  // for the RGB555 case when (s[x] & 0x8000) is true for all pixels.
  const __m128i aVxff00 = _mm_set1_epi32(0xFF000000L);

  // JSD optimized with SSE2 intrinsics (2 in 4 cases)
  // Produces a ~25% speed improvement over reference C implementation.
  for (int y = 0; y < height; y += 4)
  {
    for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
    {
      for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
      {
        u32* newdst = dst + (y + iy) * width + x;
        const u16* newsrc = (const u16*)(src + 8 * xStep);

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

          // r0 = (((val0>>10) & 0x1f) << 3) | (((val0>>10) & 0x1f) >> 2);
          const __m128i tmprV = _mm_and_si128(_mm_srli_epi16(valV, 10), kMask_x1f);
          const __m128i rV = _mm_or_si128(_mm_slli_epi16(tmprV, 3), _mm_srli_epi16(tmprV, 2));

          // g0 = (((val0>>5 ) & 0x1f) << 3) | (((val0>>5 ) & 0x1f) >> 2);
          const __m128i tmpgV = _mm_and_si128(_mm_srli_epi16(valV, 5), kMask_x1f);
          const __m128i gV = _mm_or_si128(_mm_slli_epi16(tmpgV, 3), _mm_srli_epi16(tmpgV, 2));

          // b0 = (((val0    ) & 0x1f) << 3) | (((val0    ) & 0x1f) >> 2);
          const __m128i tmpbV = _mm_and_si128(valV, kMask_x1f);
          const __m128i bV = _mm_or_si128(_mm_slli_epi16(tmpbV, 3), _mm_srli_epi16(tmpbV, 2));

          // newdst[0] = r0 | (g0 << 8) | (b0 << 16) | (a0 << 24);
          const __m128i final = _mm_or_si128(_mm_or_si128(rV, _mm_slli_epi32(gV, 8)),
                                             _mm_or_si128(_mm_slli_epi32(bV, 16), aVxff00));

          // write the final result:
          _mm_storeu_si128((__m128i*)newdst, final);
        }
        else if (((val0 & 0x8000) | (val1 & 0x8000) | (val2 & 0x8000) | (val3 & 0x8000)) == 0x0000)
        {
          // SSE2 case #2: all 4 pixels are in RGBA4443.

          // Swizzle bits: 00001234 -> 12341234

          // r0 = (((val0>>8 ) & 0xf) << 4) | ((val0>>8 ) & 0xf);
          const __m128i tmprV = _mm_and_si128(_mm_srli_epi16(valV, 8), kMask_x0f);
          const __m128i rV = _mm_or_si128(_mm_slli_epi16(tmprV, 4), tmprV);

          // g0 = (((val0>>4 ) & 0xf) << 4) | ((val0>>4 ) & 0xf);
          const __m128i tmpgV = _mm_and_si128(_mm_srli_epi16(valV, 4), kMask_x0f);
          const __m128i gV = _mm_or_si128(_mm_slli_epi16(tmpgV, 4), tmpgV);

          // b0 = (((val0    ) & 0xf) << 4) | ((val0    ) & 0xf);
          const __m128i tmpbV = _mm_and_si128(valV, kMask_x0f);
          const __m128i bV = _mm_or_si128(_mm_slli_epi16(tmpbV, 4), tmpbV);

          // a0 = (((val0>>12) & 0x7) << 5) | (((val0>>12) & 0x7) << 2) | (((val0>>12) & 0x7) >> 1);
          const __m128i tmpaV = _mm_and_si128(_mm_srli_epi16(valV, 12), kMask_x07);
          const __m128i aV =
              _mm_or_si128(_mm_slli_epi16(tmpaV, 5),
                           _mm_or_si128(_mm_slli_epi16(tmpaV, 2), _mm_srli_epi16(tmpaV, 1)));

          // newdst[0] = r0 | (g0 << 8) | (b0 << 16) | (a0 << 24);
          const __m128i final =
              _mm_or_si128(_mm_or_si128(rV, _mm_slli_epi32(gV, 8)),
                           _mm_or_si128(_mm_slli_epi32(bV, 16), _mm_slli_epi32(aV, 24)));

          // write the final result:
          _mm_storeu_si128((__m128i*)newdst, final);
        }
        else
        {
          // TODO: Vectorise (Either 4-way branch or do both and select is better than this)
          u32* vals = (u32*)&valV;
          int r, g, b, a;
          for (int i = 0; i < 4; ++i)
          {
            if (vals[i] & 0x8000)
            {
              // Swizzle bits: 00012345 -> 12345123
              r = (((vals[i] >> 10) & 0x1f) << 3) | (((vals[i] >> 10) & 0x1f) >> 2);
              g = (((vals[i] >> 5) & 0x1f) << 3) | (((vals[i] >> 5) & 0x1f) >> 2);
              b = (((vals[i]) & 0x1f) << 3) | (((vals[i]) & 0x1f) >> 2);
              a = 0xFF;
            }
            else
            {
              a = (((vals[i] >> 12) & 0x7) << 5) | (((vals[i] >> 12) & 0x7) << 2) |
                  (((vals[i] >> 12) & 0x7) >> 1);
              // Swizzle bits: 00001234 -> 12341234
              r = (((vals[i] >> 8) & 0xf) << 4) | ((vals[i] >> 8) & 0xf);
              g = (((vals[i] >> 4) & 0xf) << 4) | ((vals[i] >> 4) & 0xf);
              b = (((vals[i]) & 0xf) << 4) | ((vals[i]) & 0xf);
            }
            newdst[i] = r | (g << 8) | (b << 16) | (a << 24);
          }
        }
      }
    }
  }
}

FUNCTION_TARGET_SSSE3
static void TexDecoder_DecodeImpl_RGBA8_SSSE3(u32* dst, const u8* src, int width, int height,
                                              TextureFormat texformat, const u8* tlut,
                                              TLUTFormat tlutfmt, int Wsteps4, int Wsteps8)
{
  // xsacha optimized with SSSE3 instrinsics
  // Produces a ~30% speed improvement over SSE2 implementation
  for (int y = 0; y < height; y += 4)
  {
    for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
    {
      const u8* src2 = src + 64 * yStep;
      const __m128i mask0312 = _mm_set_epi8(12, 15, 13, 14, 8, 11, 9, 10, 4, 7, 5, 6, 0, 3, 1, 2);
      const __m128i ar0 = _mm_loadu_si128((__m128i*)src2);
      const __m128i ar1 = _mm_loadu_si128((__m128i*)src2 + 1);
      const __m128i gb0 = _mm_loadu_si128((__m128i*)src2 + 2);
      const __m128i gb1 = _mm_loadu_si128((__m128i*)src2 + 3);

      const __m128i rgba00 = _mm_shuffle_epi8(_mm_unpacklo_epi8(ar0, gb0), mask0312);
      const __m128i rgba01 = _mm_shuffle_epi8(_mm_unpackhi_epi8(ar0, gb0), mask0312);
      const __m128i rgba10 = _mm_shuffle_epi8(_mm_unpacklo_epi8(ar1, gb1), mask0312);
      const __m128i rgba11 = _mm_shuffle_epi8(_mm_unpackhi_epi8(ar1, gb1), mask0312);

      __m128i* dst128 = (__m128i*)(dst + (y + 0) * width + x);
      _mm_storeu_si128(dst128, rgba00);
      dst128 = (__m128i*)(dst + (y + 1) * width + x);
      _mm_storeu_si128(dst128, rgba01);
      dst128 = (__m128i*)(dst + (y + 2) * width + x);
      _mm_storeu_si128(dst128, rgba10);
      dst128 = (__m128i*)(dst + (y + 3) * width + x);
      _mm_storeu_si128(dst128, rgba11);
    }
  }
}

static void TexDecoder_DecodeImpl_RGBA8(u32* dst, const u8* src, int width, int height,
                                        TextureFormat texformat, const u8* tlut, TLUTFormat tlutfmt,
                                        int Wsteps4, int Wsteps8)
{
  // JSD optimized with SSE2 intrinsics
  // Produces a ~68% speed improvement over reference C implementation.
  for (int y = 0; y < height; y += 4)
  {
    for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
    {
      // Input is divided up into 16-bit words. The texels are split up into AR and GB
      // components where all AR components come grouped up first in 32 bytes followed by the GB
      // components in 32 bytes. We are processing 16 texels per each loop iteration, numbered from
      // 0-f.
      //
      // Convention is:
      //   one byte is [component-name texel-number]
      //    __m128i is (4-bytes 4-bytes 4-bytes 4-bytes)
      //
      // Input is:
      //   ([A 7][R 7][A 6][R 6] [A 5][R 5][A 4][R 4] [A 3][R 3][A 2][R 2] [A 1][R 1][A 0][R 0])
      //   ([A f][R f][A e][R e] [A d][R d][A c][R c] [A b][R b][A a][R a] [A 9][R 9][A 8][R 8])
      //   ([G 7][B 7][G 6][B 6] [G 5][B 5][G 4][B 4] [G 3][B 3][G 2][B 2] [G 1][B 1][G 0][B 0])
      //   ([G f][B f][G e][B e] [G d][B d][G c][B c] [G b][B b][G a][B a] [G 9][B 9][G 8][B 8])
      //
      // Output is:
      //   (RGBA3 RGBA2 RGBA1 RGBA0)
      //   (RGBA7 RGBA6 RGBA5 RGBA4)
      //   (RGBAb RGBAa RGBA9 RGBA8)
      //   (RGBAf RGBAe RGBAd RGBAc)
      const u8* src2 = src + 64 * yStep;
      // Loads the 1st half of AR components ([A 7][R 7][A 6][R 6] [A 5][R 5][A 4][R 4] [A 3][R
      // 3][A 2][R 2] [A 1][R 1][A 0][R 0])
      const __m128i ar0 = _mm_loadu_si128((__m128i*)src2);
      // Loads the 2nd half of AR components ([A f][R f][A e][R e] [A d][R d][A c][R c] [A b][R
      // b][A a][R a] [A 9][R 9][A 8][R 8])
      const __m128i ar1 = _mm_loadu_si128((__m128i*)src2 + 1);
      // Loads the 1st half of GB components ([G 7][B 7][G 6][B 6] [G 5][B 5][G 4][B 4] [G 3][B
      // 3][G 2][B 2] [G 1][B 1][G 0][B 0])
      const __m128i gb0 = _mm_loadu_si128((__m128i*)src2 + 2);
      // Loads the 2nd half of GB components ([G f][B f][G e][B e] [G d][B d][G c][B c] [G b][B
      // b][G a][B a] [G 9][B 9][G 8][B 8])
      const __m128i gb1 = _mm_loadu_si128((__m128i*)src2 + 3);
      __m128i rgba00, rgba01, rgba10, rgba11;
      const __m128i kMask_x000f = _mm_set_epi32(0x000000FFL, 0x000000FFL, 0x000000FFL, 0x000000FFL);
      const __m128i kMask_xf000 = _mm_set_epi32(0xFF000000L, 0xFF000000L, 0xFF000000L, 0xFF000000L);
      const __m128i kMask_x0ff0 = _mm_set_epi32(0x00FFFF00L, 0x00FFFF00L, 0x00FFFF00L, 0x00FFFF00L);
      // Expand the AR components to fill out 32-bit words:
      // ([A 7][R 7][A 6][R 6] [A 5][R 5][A 4][R 4] [A 3][R 3][A 2][R 2] [A 1][R 1][A 0][R 0])
      // -> ([A 3][A 3][R 3][R 3] [A 2][A 2][R 2][R 2] [A 1][A 1][R 1][R 1] [A 0][A 0][R 0][R 0])
      const __m128i aarr00 = _mm_unpacklo_epi8(ar0, ar0);
      // ([A 7][R 7][A 6][R 6] [A 5][R 5][A 4][R 4] [A 3][R 3][A 2][R 2] [A 1][R 1][A 0][R 0])
      // -> ([A 7][A 7][R 7][R 7] [A 6][A 6][R 6][R 6] [A 5][A 5][R 5][R 5] [A 4][A 4][R 4][R 4])
      const __m128i aarr01 = _mm_unpackhi_epi8(ar0, ar0);
      // ([A f][R f][A e][R e] [A d][R d][A c][R c] [A b][R b][A a][R a] [A 9][R 9][A 8][R 8])
      // -> ([A b][A b][R b][R b] [A a][A a][R a][R a] [A 9][A 9][R 9][R 9] [A 8][A 8][R 8][R 8])
      const __m128i aarr10 = _mm_unpacklo_epi8(ar1, ar1);
      // ([A f][R f][A e][R e] [A d][R d][A c][R c] [A b][R b][A a][R a] [A 9][R 9][A 8][R 8])
      // -> ([A f][A f][R f][R f] [A e][A e][R e][R e] [A d][A d][R d][R d] [A c][A c][R c][R c])
      const __m128i aarr11 = _mm_unpackhi_epi8(ar1, ar1);

      // Move A right 16 bits and mask off everything but the lowest  8 bits to get A in its
      // final place:
      const __m128i ___a00 = _mm_and_si128(_mm_srli_epi32(aarr00, 16), kMask_x000f);
      // Move R left  16 bits and mask off everything but the highest 8 bits to get R in its
      // final place:
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
      // ([G 7][B 7][G 6][B 6] [G 5][B 5][G 4][B 4] [G 3][B 3][G 2][B 2] [G 1][B 1][G 0][B 0])
      // -> ([G 3][G 3][B 3][B 3] [G 2][G 2][B 2][B 2] [G 1][G 1][B 1][B 1] [G 0][G 0][B 0][B 0])
      const __m128i ggbb00 = _mm_unpacklo_epi8(gb0, gb0);
      // ([G 7][B 7][G 6][B 6] [G 5][B 5][G 4][B 4] [G 3][B 3][G 2][B 2] [G 1][B 1][G 0][B 0])
      // -> ([G 7][G 7][B 7][B 7] [G 6][G 6][B 6][B 6] [G 5][G 5][B 5][B 5] [G 4][G 4][B 4][B 4])
      const __m128i ggbb01 = _mm_unpackhi_epi8(gb0, gb0);
      // ([G f][B f][G e][B e] [G d][B d][G c][B c] [G b][B b][G a][B a] [G 9][B 9][G 8][B 8])
      // -> ([G b][G b][B b][B b] [G a][G a][B a][B a] [G 9][G 9][B 9][B 9] [G 8][G 8][B 8][B 8])
      const __m128i ggbb10 = _mm_unpacklo_epi8(gb1, gb1);
      // ([G f][B f][G e][B e] [G d][B d][G c][B c] [G b][B b][G a][B a] [G 9][B 9][G 8][B 8])
      // -> ([G f][G f][B f][B f] [G e][G e][B e][B e] [G d][G d][B d][B d] [G c][G c][B c][B c])
      const __m128i ggbb11 = _mm_unpackhi_epi8(gb1, gb1);

      // G and B are already in perfect spots in the center, just remove the extra copies in the
      // 1st and 4th positions:
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
      __m128i* dst128 = (__m128i*)(dst + (y + 0) * width + x);
      _mm_storeu_si128(dst128, rgba00);
      dst128 = (__m128i*)(dst + (y + 1) * width + x);
      _mm_storeu_si128(dst128, rgba01);
      dst128 = (__m128i*)(dst + (y + 2) * width + x);
      _mm_storeu_si128(dst128, rgba10);
      dst128 = (__m128i*)(dst + (y + 3) * width + x);
      _mm_storeu_si128(dst128, rgba11);
    }
  }
}

static void TexDecoder_DecodeImpl_CMPR(u32* dst, const u8* src, int width, int height,
                                       TextureFormat texformat, const u8* tlut, TLUTFormat tlutfmt,
                                       int Wsteps4, int Wsteps8)
{
  // The metroid games use this format almost exclusively.
  // JSD optimized with SSE2 intrinsics.
  // Produces a ~50% improvement for x86 and a ~40% improvement for x64 in speed over reference
  // C implementation. The x64 compiled reference C code is faster than the x86 compiled reference
  // C code, but the SSE2 is faster than both.
  for (int y = 0; y < height; y += 8)
  {
    for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8, yStep++)
    {
      // We handle two DXT blocks simultaneously to take full advantage of SSE2's 128-bit registers.
      // This is ideal because a single DXT block contains 2 RGBA colors when decoded from their
      // 16-bit. Two DXT blocks therefore contain 4 RGBA colors to be processed. The processing is
      // parallelizable at this level, so we do.
      for (int z = 0, xStep = 2 * yStep; z < 2; ++z, xStep++)
      {
        // JSD NOTE: You may see many strange patterns of behavior in the below code, but they
        // are for performance reasons. Sometimes, calculating what should be obvious hard-coded
        // constants is faster than loading their values from memory. Unfortunately, there is no
        // way to inline 128-bit constants from opcodes so they must be loaded from memory. This
        // seems a little ridiculous to me in that you can't even generate a constant value of 1
        // without having to load it from memory. So, I stored the minimal constant I could,
        // 128-bits worth of 1s :). Then I use sequences of shifts to squash it to the appropriate
        // size and bitpositions that I need.
        const __m128i allFFs128 = _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128());

        // Load 128 bits, i.e. two DXTBlocks (64-bits each)
        const __m128i dxt = _mm_loadu_si128((__m128i*)(src + sizeof(struct DXTBlock) * 2 * xStep));

        // Copy the 2-bit indices from each DXT block:
        alignas(16) u32 dxttmp[4];
        _mm_store_si128((__m128i*)dxttmp, dxt);

        u32 dxt0sel = dxttmp[1];
        u32 dxt1sel = dxttmp[3];

        __m128i argb888x4;
        __m128i c1 = _mm_unpackhi_epi16(dxt, dxt);
        c1 = _mm_slli_si128(c1, 8);
        const __m128i c0 =
            _mm_or_si128(c1, _mm_srli_si128(_mm_slli_si128(_mm_unpacklo_epi16(dxt, dxt), 8), 8));

        // Compare rgb0 to rgb1:
        // Each 32-bit word will contain either 0xFFFFFFFF or 0x00000000 for true/false.
        const __m128i c0cmp = _mm_srli_epi32(_mm_slli_epi32(_mm_srli_epi64(c0, 8), 16), 16);
        const __m128i c0shr = _mm_srli_epi64(c0cmp, 32);
        const __m128i cmprgb0rgb1 = _mm_cmpgt_epi32(c0cmp, c0shr);

        int cmp0 = _mm_extract_epi16(cmprgb0rgb1, 0);
        int cmp1 = _mm_extract_epi16(cmprgb0rgb1, 4);

        // green:
        // NOTE: We start with the larger number of bits (6) firts for G and shift the mask down
        // 1 bit to get a 5-bit mask later for R and B components.
        // low6mask == _mm_set_epi32(0x0000FC00, 0x0000FC00, 0x0000FC00, 0x0000FC00)
        const __m128i low6mask = _mm_slli_epi32(_mm_srli_epi32(allFFs128, 24 + 2), 8 + 2);
        const __m128i gtmp = _mm_srli_epi32(c0, 3);
        const __m128i g0 = _mm_and_si128(gtmp, low6mask);
        // low3mask == _mm_set_epi32(0x00000300, 0x00000300, 0x00000300, 0x00000300)
        const __m128i g1 = _mm_and_si128(
            _mm_srli_epi32(gtmp, 6), _mm_set_epi32(0x00000300, 0x00000300, 0x00000300, 0x00000300));
        argb888x4 = _mm_or_si128(g0, g1);
        // red:
        // low5mask == _mm_set_epi32(0x000000F8, 0x000000F8, 0x000000F8, 0x000000F8)
        const __m128i low5mask = _mm_slli_epi32(_mm_srli_epi32(low6mask, 8 + 3), 3);
        const __m128i r0 = _mm_and_si128(c0, low5mask);
        const __m128i r1 = _mm_srli_epi32(r0, 5);
        argb888x4 = _mm_or_si128(argb888x4, _mm_or_si128(r0, r1));
        // blue:
        // _mm_slli_epi32(low5mask, 16) == _mm_set_epi32(0x00F80000, 0x00F80000, 0x00F80000,
        // 0x00F80000)
        const __m128i b0 = _mm_and_si128(_mm_srli_epi32(c0, 5), _mm_slli_epi32(low5mask, 16));
        const __m128i b1 = _mm_srli_epi16(b0, 5);
        // OR in the fixed alpha component
        // _mm_slli_epi32( allFFs128, 24 ) == _mm_set_epi32(0xFF000000, 0xFF000000, 0xFF000000,
        // 0xFF000000)
        argb888x4 = _mm_or_si128(_mm_or_si128(argb888x4, _mm_slli_epi32(allFFs128, 24)),
                                 _mm_or_si128(b0, b1));
        // calculate RGB2 and RGB3:
        const __m128i rgb0 = _mm_shuffle_epi32(argb888x4, _MM_SHUFFLE(2, 2, 0, 0));
        const __m128i rgb1 = _mm_shuffle_epi32(argb888x4, _MM_SHUFFLE(3, 3, 1, 1));
        const __m128i rrggbb0 =
            _mm_and_si128(_mm_unpacklo_epi8(rgb0, rgb0), _mm_srli_epi16(allFFs128, 8));
        const __m128i rrggbb1 =
            _mm_and_si128(_mm_unpacklo_epi8(rgb1, rgb1), _mm_srli_epi16(allFFs128, 8));
        const __m128i rrggbb01 =
            _mm_and_si128(_mm_unpackhi_epi8(rgb0, rgb0), _mm_srli_epi16(allFFs128, 8));
        const __m128i rrggbb11 =
            _mm_and_si128(_mm_unpackhi_epi8(rgb1, rgb1), _mm_srli_epi16(allFFs128, 8));

        __m128i rgb2, rgb3;

        // if (rgb0 > rgb1):
        if (cmp0 != 0)
        {
          // RGB2 = (RGB0 * 5 + RGB1 * 3) / 8 = (RGB0 << 2 + RGB1 << 1 + (RGB0 + RGB1)) >> 3
          // RGB3 = (RGB0 * 3 + RGB1 * 5) / 8 = (RGB0 << 1 + RGB1 << 2 + (RGB0 + RGB1)) >> 3
          const __m128i rrggbbsum = _mm_add_epi16(rrggbb0, rrggbb1);

          const __m128i rrggbb0shl1 = _mm_slli_epi16(rrggbb0, 1);
          const __m128i rrggbb0shl2 = _mm_slli_epi16(rrggbb0, 2);

          const __m128i rrggbb1shl1 = _mm_slli_epi16(rrggbb1, 1);
          const __m128i rrggbb1shl2 = _mm_slli_epi16(rrggbb1, 2);

          const __m128i rrggbb2 =
              _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(rrggbb0shl2, rrggbb1shl1), rrggbbsum), 3);
          const __m128i rrggbb3 =
              _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(rrggbb0shl1, rrggbb1shl2), rrggbbsum), 3);

          const __m128i rgb2dup = _mm_packus_epi16(rrggbb2, rrggbb2);
          const __m128i rgb3dup = _mm_packus_epi16(rrggbb3, rrggbb3);

          rgb2 = _mm_and_si128(rgb2dup, _mm_srli_si128(allFFs128, 8));
          rgb3 = _mm_and_si128(rgb3dup, _mm_srli_si128(allFFs128, 8));
        }
        else
        {
          // RGB2b = avg(RGB0, RGB1)
          const __m128i rrggbb21 = _mm_srai_epi16(_mm_add_epi16(rrggbb0, rrggbb1), 1);
          const __m128i rgb210 = _mm_srli_si128(_mm_packus_epi16(rrggbb21, rrggbb21), 8);
          rgb2 = rgb210;
          rgb3 = _mm_and_si128(rgb210, _mm_srli_epi32(allFFs128, 8));
        }

        // if (rgb0 > rgb1):
        if (cmp1 != 0)
        {
          // RGB2 = (RGB0 * 5 + RGB1 * 3) / 8 = (RGB0 << 2 + RGB1 << 1 + (RGB0 + RGB1)) >> 3
          // RGB3 = (RGB0 * 3 + RGB1 * 5) / 8 = (RGB0 << 1 + RGB1 << 2 + (RGB0 + RGB1)) >> 3
          const __m128i rrggbbsum = _mm_add_epi16(rrggbb01, rrggbb11);

          const __m128i rrggbb0shl1 = _mm_slli_epi16(rrggbb01, 1);
          const __m128i rrggbb0shl2 = _mm_slli_epi16(rrggbb01, 2);

          const __m128i rrggbb1shl1 = _mm_slli_epi16(rrggbb11, 1);
          const __m128i rrggbb1shl2 = _mm_slli_epi16(rrggbb11, 2);

          const __m128i rrggbb2 =
              _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(rrggbb0shl2, rrggbb1shl1), rrggbbsum), 3);
          const __m128i rrggbb3 =
              _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(rrggbb0shl1, rrggbb1shl2), rrggbbsum), 3);

          const __m128i rgb2dup = _mm_packus_epi16(rrggbb2, rrggbb2);
          const __m128i rgb3dup = _mm_packus_epi16(rrggbb3, rrggbb3);

          rgb2 = _mm_or_si128(rgb2, _mm_and_si128(rgb2dup, _mm_slli_si128(allFFs128, 8)));
          rgb3 = _mm_or_si128(rgb3, _mm_and_si128(rgb3dup, _mm_slli_si128(allFFs128, 8)));
        }
        else
        {
          // RGB2b = avg(RGB0, RGB1)
          const __m128i rrggbb211 = _mm_srai_epi16(_mm_add_epi16(rrggbb01, rrggbb11), 1);
          const __m128i rgb211 = _mm_slli_si128(_mm_packus_epi16(rrggbb211, rrggbb211), 8);
          rgb2 = _mm_or_si128(rgb2, rgb211);

          // _mm_srli_epi32( allFFs128, 8 ) == _mm_set_epi32(0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF,
          // 0x00FFFFFF)
          // Make this color fully transparent:
          rgb3 = _mm_or_si128(rgb3, _mm_and_si128(_mm_and_si128(rgb2, _mm_srli_epi32(allFFs128, 8)),
                                                  _mm_slli_si128(allFFs128, 8)));
        }

        // Create an array for color lookups for DXT0 so we can use the 2-bit indices:
        const __m128i mmcolors0 = _mm_or_si128(
            _mm_or_si128(_mm_srli_si128(_mm_slli_si128(argb888x4, 8), 8),
                         _mm_slli_si128(_mm_srli_si128(_mm_slli_si128(rgb2, 8), 8 + 4), 8)),
            _mm_slli_si128(_mm_srli_si128(rgb3, 4), 8 + 4));

        // Create an array for color lookups for DXT1 so we can use the 2-bit indices:
        const __m128i mmcolors1 =
            _mm_or_si128(_mm_or_si128(_mm_srli_si128(argb888x4, 8),
                                      _mm_slli_si128(_mm_srli_si128(rgb2, 8 + 4), 8)),
                         _mm_slli_si128(_mm_srli_si128(rgb3, 8 + 4), 8 + 4));

// The #ifdef CHECKs here and below are to compare correctness of output against the reference code.
// Don't use them in a normal build.
#ifdef CHECK
        // REFERENCE:
        u32 tmp0[4][4], tmp1[4][4];

        DecodeDXTBlock(&(tmp0[0][0]),
                       reinterpret_cast<const DXTBlock*>(src + sizeof(DXTBlock) * 2 * xStep), 4);
        DecodeDXTBlock(&(tmp1[0][0]),
                       reinterpret_cast<const DXTBlock*>((src + sizeof(DXTBlock) * 2 * xStep) + 8),
                       4);
#endif

        u32* dst32 = (dst + (y + z * 4) * width + x);

        // Copy the colors here:
        alignas(16) u32 colors0[4];
        alignas(16) u32 colors1[4];
        _mm_store_si128((__m128i*)colors0, mmcolors0);
        _mm_store_si128((__m128i*)colors1, mmcolors1);

        // Row 0:
        dst32[(width * 0) + 0] = colors0[(dxt0sel >> ((0 * 8) + 6)) & 3];
        dst32[(width * 0) + 1] = colors0[(dxt0sel >> ((0 * 8) + 4)) & 3];
        dst32[(width * 0) + 2] = colors0[(dxt0sel >> ((0 * 8) + 2)) & 3];
        dst32[(width * 0) + 3] = colors0[(dxt0sel >> ((0 * 8) + 0)) & 3];
        dst32[(width * 0) + 4] = colors1[(dxt1sel >> ((0 * 8) + 6)) & 3];
        dst32[(width * 0) + 5] = colors1[(dxt1sel >> ((0 * 8) + 4)) & 3];
        dst32[(width * 0) + 6] = colors1[(dxt1sel >> ((0 * 8) + 2)) & 3];
        dst32[(width * 0) + 7] = colors1[(dxt1sel >> ((0 * 8) + 0)) & 3];
#ifdef CHECK
        ASSERT(memcmp(&(tmp0[0]), &dst32[(width * 0)], 16) == 0);
        ASSERT(memcmp(&(tmp1[0]), &dst32[(width * 0) + 4], 16) == 0);
#endif
        // Row 1:
        dst32[(width * 1) + 0] = colors0[(dxt0sel >> ((1 * 8) + 6)) & 3];
        dst32[(width * 1) + 1] = colors0[(dxt0sel >> ((1 * 8) + 4)) & 3];
        dst32[(width * 1) + 2] = colors0[(dxt0sel >> ((1 * 8) + 2)) & 3];
        dst32[(width * 1) + 3] = colors0[(dxt0sel >> ((1 * 8) + 0)) & 3];
        dst32[(width * 1) + 4] = colors1[(dxt1sel >> ((1 * 8) + 6)) & 3];
        dst32[(width * 1) + 5] = colors1[(dxt1sel >> ((1 * 8) + 4)) & 3];
        dst32[(width * 1) + 6] = colors1[(dxt1sel >> ((1 * 8) + 2)) & 3];
        dst32[(width * 1) + 7] = colors1[(dxt1sel >> ((1 * 8) + 0)) & 3];
#ifdef CHECK
        ASSERT(memcmp(&(tmp0[1]), &dst32[(width * 1)], 16) == 0);
        ASSERT(memcmp(&(tmp1[1]), &dst32[(width * 1) + 4], 16) == 0);
#endif
        // Row 2:
        dst32[(width * 2) + 0] = colors0[(dxt0sel >> ((2 * 8) + 6)) & 3];
        dst32[(width * 2) + 1] = colors0[(dxt0sel >> ((2 * 8) + 4)) & 3];
        dst32[(width * 2) + 2] = colors0[(dxt0sel >> ((2 * 8) + 2)) & 3];
        dst32[(width * 2) + 3] = colors0[(dxt0sel >> ((2 * 8) + 0)) & 3];
        dst32[(width * 2) + 4] = colors1[(dxt1sel >> ((2 * 8) + 6)) & 3];
        dst32[(width * 2) + 5] = colors1[(dxt1sel >> ((2 * 8) + 4)) & 3];
        dst32[(width * 2) + 6] = colors1[(dxt1sel >> ((2 * 8) + 2)) & 3];
        dst32[(width * 2) + 7] = colors1[(dxt1sel >> ((2 * 8) + 0)) & 3];
#ifdef CHECK
        ASSERT(memcmp(&(tmp0[2]), &dst32[(width * 2)], 16) == 0);
        ASSERT(memcmp(&(tmp1[2]), &dst32[(width * 2) + 4], 16) == 0);
#endif
        // Row 3:
        dst32[(width * 3) + 0] = colors0[(dxt0sel >> ((3 * 8) + 6)) & 3];
        dst32[(width * 3) + 1] = colors0[(dxt0sel >> ((3 * 8) + 4)) & 3];
        dst32[(width * 3) + 2] = colors0[(dxt0sel >> ((3 * 8) + 2)) & 3];
        dst32[(width * 3) + 3] = colors0[(dxt0sel >> ((3 * 8) + 0)) & 3];
        dst32[(width * 3) + 4] = colors1[(dxt1sel >> ((3 * 8) + 6)) & 3];
        dst32[(width * 3) + 5] = colors1[(dxt1sel >> ((3 * 8) + 4)) & 3];
        dst32[(width * 3) + 6] = colors1[(dxt1sel >> ((3 * 8) + 2)) & 3];
        dst32[(width * 3) + 7] = colors1[(dxt1sel >> ((3 * 8) + 0)) & 3];
#ifdef CHECK
        ASSERT(memcmp(&(tmp0[3]), &dst32[(width * 3)], 16) == 0);
        ASSERT(memcmp(&(tmp1[3]), &dst32[(width * 3) + 4], 16) == 0);
#endif
      }
    }
  }
}

void _TexDecoder_DecodeImpl(u32* dst, const u8* src, int width, int height, TextureFormat texformat,
                            const u8* tlut, TLUTFormat tlutfmt)
{
  int Wsteps4 = (width + 3) / 4;
  int Wsteps8 = (width + 7) / 8;

  switch (texformat)
  {
  case TextureFormat::C4:
    TexDecoder_DecodeImpl_C4(dst, src, width, height, texformat, tlut, tlutfmt, Wsteps4, Wsteps8);
    break;

  case TextureFormat::I4:
    if (cpu_info.bSSSE3)
      TexDecoder_DecodeImpl_I4_SSSE3(dst, src, width, height, texformat, tlut, tlutfmt, Wsteps4,
                                     Wsteps8);
    else
      TexDecoder_DecodeImpl_I4(dst, src, width, height, texformat, tlut, tlutfmt, Wsteps4, Wsteps8);
    break;

  case TextureFormat::I8:
    if (cpu_info.bSSSE3)
      TexDecoder_DecodeImpl_I8_SSSE3(dst, src, width, height, texformat, tlut, tlutfmt, Wsteps4,
                                     Wsteps8);
    else
      TexDecoder_DecodeImpl_I8(dst, src, width, height, texformat, tlut, tlutfmt, Wsteps4, Wsteps8);
    break;

  case TextureFormat::C8:
    TexDecoder_DecodeImpl_C8(dst, src, width, height, texformat, tlut, tlutfmt, Wsteps4, Wsteps8);
    break;

  case TextureFormat::IA4:
    TexDecoder_DecodeImpl_IA4(dst, src, width, height, texformat, tlut, tlutfmt, Wsteps4, Wsteps8);
    break;

  case TextureFormat::IA8:
    if (cpu_info.bSSSE3)
      TexDecoder_DecodeImpl_IA8_SSSE3(dst, src, width, height, texformat, tlut, tlutfmt, Wsteps4,
                                      Wsteps8);
    else
      TexDecoder_DecodeImpl_IA8(dst, src, width, height, texformat, tlut, tlutfmt, Wsteps4,
                                Wsteps8);
    break;

  case TextureFormat::C14X2:
    TexDecoder_DecodeImpl_C14X2(dst, src, width, height, texformat, tlut, tlutfmt, Wsteps4,
                                Wsteps8);
    break;

  case TextureFormat::RGB565:
    TexDecoder_DecodeImpl_RGB565(dst, src, width, height, texformat, tlut, tlutfmt, Wsteps4,
                                 Wsteps8);
    break;

  case TextureFormat::RGB5A3:
    if (cpu_info.bSSSE3)
      TexDecoder_DecodeImpl_RGB5A3_SSSE3(dst, src, width, height, texformat, tlut, tlutfmt, Wsteps4,
                                         Wsteps8);
    else
      TexDecoder_DecodeImpl_RGB5A3(dst, src, width, height, texformat, tlut, tlutfmt, Wsteps4,
                                   Wsteps8);
    break;

  case TextureFormat::RGBA8:
    if (cpu_info.bSSSE3)
      TexDecoder_DecodeImpl_RGBA8_SSSE3(dst, src, width, height, texformat, tlut, tlutfmt, Wsteps4,
                                        Wsteps8);
    else
      TexDecoder_DecodeImpl_RGBA8(dst, src, width, height, texformat, tlut, tlutfmt, Wsteps4,
                                  Wsteps8);
    break;

  case TextureFormat::CMPR:
    TexDecoder_DecodeImpl_CMPR(dst, src, width, height, texformat, tlut, tlutfmt, Wsteps4, Wsteps8);
    break;

  case TextureFormat::XFB:
    TexDecoder_DecodeXFB(reinterpret_cast<u8*>(dst), src, width, height, width * 2);
    break;

  default:
    PanicAlertFmt("Invalid Texture Format {}! (_TexDecoder_DecodeImpl)", texformat);
    break;
  }
}
