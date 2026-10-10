// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/TextureDecoder.h"

#include <array>

#include <arm_neon.h>

#include "Common/CommonTypes.h"
#include "Common/Inline.h"
#include "Common/Swap.h"

template <typename T>
struct RGBA
{
  T r;
  T g;
  T b;
  T a;
};

using RGBAx8 = RGBA<uint8x8_t>;
using RGBAx16 = RGBA<uint8x16_t>;

static DOLPHIN_FORCE_INLINE void Store8x1RGBA(u32* dst, int x, int y, int width, RGBAx8 rgba)
{
  // Store one row containing 8 texels (32 bytes).
  uint8x8x4_t value{rgba.r, rgba.g, rgba.b, rgba.a};
  vst4_u8(reinterpret_cast<u8*>(dst + y * width + x), value);
}

static DOLPHIN_FORCE_INLINE void Store8x2RGBA(u32* dst, int x, int y, int width, RGBAx16 rgba)
{
  // Store 2 rows containing 8 texels (32 bytes) each, for a total of 16 texels (64 bytes).
  RGBAx8 low{vget_low_u8(rgba.r), vget_low_u8(rgba.g), vget_low_u8(rgba.b), vget_low_u8(rgba.a)};
  Store8x1RGBA(dst, x, y + 0, width, low);
  RGBAx8 high{vget_high_u8(rgba.r), vget_high_u8(rgba.g), vget_high_u8(rgba.b),
              vget_high_u8(rgba.a)};
  Store8x1RGBA(dst, x, y + 1, width, high);
}

static DOLPHIN_FORCE_INLINE void Store4x4RGBA(u32* dst, int x, int y, int width, RGBAx16 rgba)
{
  // Store 4 rows containing 4 texels (16 bytes) each, for a total of 16 texels (64 bytes).
  // vst4_u8 can't store less than 32 bytes, so instead we zip and use vst2_u16.
  uint16x8_t rg0 = vreinterpretq_u16_u8(vzip1q_u8(rgba.r, rgba.g));
  uint16x8_t rg1 = vreinterpretq_u16_u8(vzip2q_u8(rgba.r, rgba.g));
  uint16x8_t ba0 = vreinterpretq_u16_u8(vzip1q_u8(rgba.b, rgba.a));
  uint16x8_t ba1 = vreinterpretq_u16_u8(vzip2q_u8(rgba.b, rgba.a));

  uint16x4x2_t rgba0{vget_low_u16(rg0), vget_low_u16(ba0)};
  vst2_u16(reinterpret_cast<u16*>(dst + (y + 0) * width + x), rgba0);

  uint16x4x2_t rgba1{vget_high_u16(rg0), vget_high_u16(ba0)};
  vst2_u16(reinterpret_cast<u16*>(dst + (y + 1) * width + x), rgba1);

  uint16x4x2_t rgba2{vget_low_u16(rg1), vget_low_u16(ba1)};
  vst2_u16(reinterpret_cast<u16*>(dst + (y + 2) * width + x), rgba2);

  uint16x4x2_t rgba3{vget_high_u16(rg1), vget_high_u16(ba1)};
  vst2_u16(reinterpret_cast<u16*>(dst + (y + 3) * width + x), rgba3);
}

static DOLPHIN_FORCE_INLINE void TexDecoder_DecodeImpl_I4(u32* dst, const u8* src, int width,
                                                          int height, int Wsteps4, int Wsteps8)
{
  for (int y = 0; y < height; y += 8)
  {
    for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8, yStep++)
    {
      uint8x16x2_t i4 = vld1q_u8_x2(src + 32 * yStep);
      for (int i = 0, iy = 0; i < 2; i++, iy += 4)
      {
        // TODO: vluti4q_laneq_u8 should be faster, but it requires an optional extension
        uint8x16_t i8_even = vsriq_n_u8(i4.val[i], i4.val[i], 4);
        uint8x16_t i8_odd = vsliq_n_u8(i4.val[i], i4.val[i], 4);
        uint8x16x2_t i8 = vzipq_u8(i8_even, i8_odd);
        Store8x2RGBA(dst, x, y + iy + 0, width, {i8.val[0], i8.val[0], i8.val[0], i8.val[0]});
        Store8x2RGBA(dst, x, y + iy + 2, width, {i8.val[1], i8.val[1], i8.val[1], i8.val[1]});
      }
    }
  }
}

static DOLPHIN_FORCE_INLINE void TexDecoder_DecodeImpl_I8(u32* dst, const u8* src, int width,
                                                          int height, int Wsteps4, int Wsteps8)
{
  for (int y = 0; y < height; y += 4)
  {
    for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
    {
      uint8x8x4_t i8 = vld1_u8_x4(src + 32 * yStep);
      for (int iy = 0; iy < 4; ++iy)
        Store8x1RGBA(dst, x, y + iy, width, {i8.val[iy], i8.val[iy], i8.val[iy], i8.val[iy]});
    }
  }
}

static DOLPHIN_FORCE_INLINE void TexDecoder_DecodeImpl_IA4(u32* dst, const u8* src, int width,
                                                           int height, int Wsteps4, int Wsteps8)
{
  for (int y = 0; y < height; y += 4)
  {
    for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
    {
      uint8x16x2_t ia4 = vld1q_u8_x2(src + 32 * yStep);
      for (int i = 0, iy = 0; i < 2; i++, iy += 2)
      {
        uint8x16_t a8 = vsriq_n_u8(ia4.val[i], ia4.val[i], 4);
        uint8x16_t i8 = vsliq_n_u8(ia4.val[i], ia4.val[i], 4);
        Store8x2RGBA(dst, x, y + iy + 0, width, {i8, i8, i8, a8});
      }
    }
  }
}

static DOLPHIN_FORCE_INLINE RGBAx16 Decode_IA8(uint8x16x2_t ia)
{
  return {ia.val[1], ia.val[1], ia.val[1], ia.val[0]};
}

static DOLPHIN_FORCE_INLINE void TexDecoder_DecodeImpl_IA8(u32* dst, const u8* src, int width,
                                                           int height, int Wsteps4, int Wsteps8)
{
  for (int y = 0; y < height; y += 4)
  {
    for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
    {
      uint8x16x2_t ia = vld2q_u8(src + 32 * yStep);
      Store4x4RGBA(dst, x, y, width, Decode_IA8(ia));
    }
  }
}

static DOLPHIN_FORCE_INLINE RGBAx8 Decode_RGB565(uint16x8_t rgb565, uint16x8_t rgb565_byteswapped)
{
  // rgb565 contains gggbbbbb rrrrrggg, rgb565_byteswapped contains rrrrrggg gggbbbbb.
  //
  // We're assuming that rgb565_byteswapped is derived from rgb565 and that reading from rgb565
  // therefore leads to a shorter critical path than reading from rgb565_byteswapped. If it wasn't
  // for that, this function would only need rgb565_byteswapped as a parameter.
  uint8x8_t r5 = vmovn_u16(rgb565);
  uint8x8_t r8 = vsri_n_u8(r5, r5, 5);
  uint8x8_t g6 = vshrn_n_u16(rgb565_byteswapped, 3);
  uint8x8_t g8 = vsri_n_u8(g6, g6, 6);
  uint8x8_t b5 = vshrn_n_u16(rgb565, 5);
  uint8x8_t b8 = vsri_n_u8(b5, b5, 5);
  uint8x8_t a8 = vdup_n_u8(0xFF);
  return {r8, g8, b8, a8};
}

static DOLPHIN_FORCE_INLINE RGBAx16 Decode_RGB565(uint8x16x2_t rgb565)
{
  // rgb565.val[0] contains rrrrrggg, rgb565.val[1] contains gggbbbbb.
  uint8x16_t r5 = rgb565.val[0];
  uint8x16_t r8 = vsriq_n_u8(r5, r5, 5);
  uint8x16_t g6 = vsriq_n_u8(vshlq_n_u8(rgb565.val[0], 5), rgb565.val[1], 3);
  uint8x16_t g8 = vsriq_n_u8(g6, g6, 6);
  uint8x16_t b5 = vshlq_n_u8(rgb565.val[1], 3);
  uint8x16_t b8 = vsriq_n_u8(b5, b5, 5);
  uint8x16_t a8 = vdupq_n_u8(0xFF);
  return {r8, g8, b8, a8};
}

static DOLPHIN_FORCE_INLINE void TexDecoder_DecodeImpl_RGB565(u32* dst, const u8* src, int width,
                                                              int height, int Wsteps4, int Wsteps8)
{
  for (int y = 0; y < height; y += 4)
  {
    for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
    {
      uint8x16x2_t rgb565 = vld2q_u8(src + 32 * yStep);
      Store4x4RGBA(dst, x, y, width, Decode_RGB565(rgb565));
    }
  }
}

static DOLPHIN_FORCE_INLINE RGBAx16 Decode_RGB5A3(uint8x16x2_t rgb5a3)
{
  // Case with alpha:    rgb5a3.val[0] contains 0aaarrrr, rgb5a3.val[1] contains ggggbbbb
  // Case without alpha: rgb5a3.val[0] contains 1rrrrrgg, rgb5a3.val[1] contains gggbbbbb
  uint8x16_t r8_with_alpha = vsliq_n_u8(rgb5a3.val[0], rgb5a3.val[0], 4);
  uint8x16_t g8_with_alpha = vsriq_n_u8(rgb5a3.val[1], rgb5a3.val[1], 4);
  uint8x16_t b8_with_alpha = vsliq_n_u8(rgb5a3.val[1], rgb5a3.val[1], 4);
  uint8x16_t a3_with_alpha = vshlq_n_u8(rgb5a3.val[0], 1);
  uint8x16_t a6_with_alpha = vsriq_n_u8(a3_with_alpha, a3_with_alpha, 3);
  uint8x16_t a8_with_alpha = vsriq_n_u8(a6_with_alpha, a6_with_alpha, 6);

  uint8x16_t r5_without_alpha = vshlq_n_u8(rgb5a3.val[0], 1);
  uint8x16_t r8_without_alpha = vsriq_n_u8(r5_without_alpha, r5_without_alpha, 5);
  uint8x16_t g5_without_alpha = vsriq_n_u8(vshlq_n_u8(rgb5a3.val[0], 6), rgb5a3.val[1], 2);
  uint8x16_t g8_without_alpha = vsriq_n_u8(g5_without_alpha, g5_without_alpha, 5);
  uint8x16_t b5_without_alpha = vshlq_n_u8(rgb5a3.val[1], 3);
  uint8x16_t b8_without_alpha = vsriq_n_u8(b5_without_alpha, b5_without_alpha, 5);

  uint8x16_t is_without_alpha = vreinterpretq_u8_s8(vcltzq_s8(vreinterpretq_s8_u8(rgb5a3.val[0])));
  uint8x16_t r8 = vbslq_u8(is_without_alpha, r8_without_alpha, r8_with_alpha);
  uint8x16_t g8 = vbslq_u8(is_without_alpha, g8_without_alpha, g8_with_alpha);
  uint8x16_t b8 = vbslq_u8(is_without_alpha, b8_without_alpha, b8_with_alpha);
  uint8x16_t a8 = vorrq_u8(is_without_alpha, a8_with_alpha);

  return {r8, g8, b8, a8};
}

static DOLPHIN_FORCE_INLINE void TexDecoder_DecodeImpl_RGB5A3(u32* dst, const u8* src, int width,
                                                              int height, int Wsteps4, int Wsteps8)
{
  for (int y = 0; y < height; y += 4)
  {
    for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
    {
      uint8x16x2_t rgb5a3 = vld2q_u8(src + 32 * yStep);
      Store4x4RGBA(dst, x, y, width, Decode_RGB5A3(rgb5a3));
    }
  }
}

static DOLPHIN_FORCE_INLINE void TexDecoder_DecodeImpl_RGBA8(u32* dst, const u8* src, int width,
                                                             int height, int Wsteps4, int Wsteps8)
{
  for (int y = 0; y < height; y += 4)
  {
    for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
    {
      // The input texels are split up into AR and GB components where all AR components come
      // grouped up first in 32 bytes followed by the GB components in 32 bytes.
      uint8x16x2_t ar = vld2q_u8(src + 64 * yStep);
      uint8x16x2_t gb = vld2q_u8(src + 64 * yStep + 32);
      Store4x4RGBA(dst, x, y, width, {ar.val[1], gb.val[0], gb.val[1], ar.val[0]});
    }
  }
}

static DOLPHIN_FORCE_INLINE void TexDecoder_DecodeImpl_C4(u32* dst, const u8* src, int width,
                                                          int height, int Wsteps4, int Wsteps8,
                                                          RGBAx16 palette)
{
  uint8x16_t low_mask = vdupq_n_u8(0x0F);

  for (int y = 0; y < height; y += 8)
  {
    for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8, yStep++)
    {
      uint8x16x2_t c4 = vld1q_u8_x2(src + 32 * yStep);
      for (int i = 0, iy = 0; i < 2; i++, iy += 4)
      {
        // TODO: vluti4q_laneq_u8 should be faster, but it requires an optional extension
        uint8x16_t c8_even = vshrq_n_u8(c4.val[i], 4);
        uint8x16_t c8_odd = vandq_u8(c4.val[i], low_mask);

        uint8x16x2_t c8 = vzipq_u8(c8_even, c8_odd);

        RGBAx16 rgba_0 = {vqtbl1q_u8(palette.r, c8.val[0]), vqtbl1q_u8(palette.g, c8.val[0]),
                          vqtbl1q_u8(palette.b, c8.val[0]), vqtbl1q_u8(palette.a, c8.val[0])};
        RGBAx16 rgba_1 = {vqtbl1q_u8(palette.r, c8.val[1]), vqtbl1q_u8(palette.g, c8.val[1]),
                          vqtbl1q_u8(palette.b, c8.val[1]), vqtbl1q_u8(palette.a, c8.val[1])};

        Store8x2RGBA(dst, x, y + iy + 0, width, rgba_0);
        Store8x2RGBA(dst, x, y + iy + 2, width, rgba_1);
      }
    }
  }
}

static DOLPHIN_FORCE_INLINE void TexDecoder_DecodeImpl_C8(u32* dst, const u8* src, int width,
                                                          int height, int Wsteps4, int Wsteps8,
                                                          const u8* tlut,
                                                          RGBAx16 (*decoder_function)(uint8x16x2_t))
{
  // The current approach is about twice as fast as the TBL+TBX+TBX+TBX approach
  for (int y = 0; y < height; y += 4)
  {
    for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
    {
      for (int iy = 0, xStep = 4 * yStep; iy < 4; iy += 2, xStep += 2)
      {
        const u8* src2 = src + 8 * xStep;
        const u16* tlut2 = reinterpret_cast<const u16*>(tlut);

        const auto read_four_texels = [](const u16* tlut_, const u8* src_) {
          // No gather instruction, so we have to read each value one by one...
          // TODO: Newer CPUs could use SVE gather instructions
          u16 a = tlut_[src_[0]];
          u16 b = tlut_[src_[1]];
          u16 c = tlut_[src_[2]];
          u16 d = tlut_[src_[3]];
          u32 ab = (static_cast<u32>(b) << 16) | a;
          u32 cd = (static_cast<u32>(d) << 16) | c;
          return (static_cast<u64>(cd) << 32) | ab;
        };

        // We use the biggest possible GPR -> FPR moves, which are 64-bit
        uint64x1_t a = vdup_n_u64(read_four_texels(tlut2, src2 + 0));
        uint64x1_t b = vdup_n_u64(read_four_texels(tlut2, src2 + 4));
        uint64x1_t c = vdup_n_u64(read_four_texels(tlut2, src2 + 8));
        uint64x1_t d = vdup_n_u64(read_four_texels(tlut2, src2 + 12));

        uint64x2_t ab = vcombine_u64(a, b);
        uint64x2_t cd = vcombine_u64(c, d);

        uint8x16x2_t abcd = vuzpq_u8(vreinterpretq_u8_u64(ab), vreinterpretq_u8_u64(cd));
        Store8x2RGBA(dst, x, y + iy + 0, width, decoder_function(abcd));
      }
    }
  }
}

template <bool VectorLoads>
static DOLPHIN_FORCE_INLINE void
TexDecoder_DecodeImpl_C14X2(u32* dst, const u8* src, int width, int height, int Wsteps4,
                            int Wsteps8, const u8* tlut, RGBAx16 (*decoder_function)(uint8x16x2_t))
{
  for (int y = 0; y < height; y += 4)
  {
    for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
    {
      const u16* src2 = reinterpret_cast<const u16*>(src + 32 * yStep);
      const u16* tlut2 = reinterpret_cast<const u16*>(tlut);

      // For C14X2 specifically: Reading src with vector loads is slightly faster for IA8 and RGB565
      // but significantly slower for RGB5A3 (tested on Snapdragon 6 Gen 3), so we let the caller
      // choose whether to use vector loads. (For C8, src vector loads are slower in all cases.)
      uint64x1_t a, b, c, d;
      if constexpr (VectorLoads)
      {
        uint16x8x2_t tlut_offsets = vld1q_u16_x2(src2);
        for (uint16x8_t& vec : tlut_offsets.val)
          vec = vreinterpretq_u16_u8(vrev16q_u8(vreinterpretq_u8_u16(vec)));

        constexpr auto read_four_texels = [](const u16* tlut_, u64 offsets) {
          // No gather instruction, so we have to read each value one by one...
          // TODO: Newer CPUs could use SVE gather instructions
          u16 a_ = tlut_[(offsets >> 0) & 0x3FFF];
          u16 b_ = tlut_[(offsets >> 16) & 0x3FFF];
          u16 c_ = tlut_[(offsets >> 32) & 0x3FFF];
          u16 d_ = tlut_[(offsets >> 48) & 0x3FFF];
          u32 ab = (static_cast<u32>(b_) << 16) | a_;
          u32 cd = (static_cast<u32>(d_) << 16) | c_;
          return (static_cast<u64>(cd) << 32) | ab;
        };

        // We use the biggest possible GPR <-> FPR moves, which are 64-bit
        a = vdup_n_u64(
            read_four_texels(tlut2, vgetq_lane_u64(vreinterpretq_u64_u16(tlut_offsets.val[0]), 0)));
        b = vdup_n_u64(
            read_four_texels(tlut2, vgetq_lane_u64(vreinterpretq_u64_u16(tlut_offsets.val[0]), 1)));
        c = vdup_n_u64(
            read_four_texels(tlut2, vgetq_lane_u64(vreinterpretq_u64_u16(tlut_offsets.val[1]), 0)));
        d = vdup_n_u64(
            read_four_texels(tlut2, vgetq_lane_u64(vreinterpretq_u64_u16(tlut_offsets.val[1]), 1)));
      }
      else
      {
        const auto read_four_texels = [](const u16* tlut_, const u16* src_) {
          u16 a_ = tlut_[Common::swap16(src_[0]) & 0x3FFF];
          u16 b_ = tlut_[Common::swap16(src_[1]) & 0x3FFF];
          u16 c_ = tlut_[Common::swap16(src_[2]) & 0x3FFF];
          u16 d_ = tlut_[Common::swap16(src_[3]) & 0x3FFF];
          u32 ab = (static_cast<u32>(b_) << 16) | a_;
          u32 cd = (static_cast<u32>(d_) << 16) | c_;
          return (static_cast<u64>(cd) << 32) | ab;
        };

        // We use the biggest possible GPR -> FPR moves, which are 64-bit
        a = vdup_n_u64(read_four_texels(tlut2, src2 + 0));
        b = vdup_n_u64(read_four_texels(tlut2, src2 + 4));
        c = vdup_n_u64(read_four_texels(tlut2, src2 + 8));
        d = vdup_n_u64(read_four_texels(tlut2, src2 + 12));
      }

      uint64x2_t ab = vcombine_u64(a, b);
      uint64x2_t cd = vcombine_u64(c, d);

      uint8x16x2_t abcd = vuzpq_u8(vreinterpretq_u8_u64(ab), vreinterpretq_u8_u64(cd));
      Store4x4RGBA(dst, x, y, width, decoder_function(abcd));
    }
  }
}

static constexpr std::array<u32, 256> GenerateUnpack2BitTable()
{
  std::array<u32, 256> table;

  u32 index = 0;
  for (u32 i = 0; i < 4; ++i)
    for (u32 j = 0; j < 4; ++j)
      for (u32 k = 0; k < 4; ++k)
        for (u32 l = 0; l < 4; ++l)
          table[index++] = (l << 24) | (k << 16) | (j << 8) | i;

  return table;
}

static DOLPHIN_FORCE_INLINE void TexDecoder_DecodeImpl_CMPR(u32* dst, const u8* src, int width,
                                                            int height, int Wsteps4, int Wsteps8)
{
  static constexpr std::array<u32, 256> unpack_2bit_table = GenerateUnpack2BitTable();
  std::array<u32, 16> unpacked_indices;

  uint8x8_t dxt_blend_factors_0 = vreinterpret_u8_u16(vdup_n_u16(0x0305));
  uint8x8_t dxt_blend_factors_1 = vreinterpret_u8_u16(vdup_n_u16(0x0503));

  for (int y = 0; y < height; y += 8)
  {
    for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8, yStep++)
    {
      // Load four blocks at once

      const u32* src2 = reinterpret_cast<const u32*>(src + 32 * yStep);
      uint32x4x2_t values = vld2q_u32(src2);
      uint16x8_t rgb565 = vreinterpretq_u16_u32(values.val[0]);

      // Unpack colors 0 and 1

      uint16x8_t rgb565_byteswapped = vrev16q_u8(vreinterpretq_u8_u16(rgb565));
      RGBAx8 color_0_1 = Decode_RGB565(rgb565, rgb565_byteswapped);

      // Calculate colors 2 and 3

      const auto dxt_blend = [](uint8x8_t input, uint8x8_t factors) {
        return vshrn_n_u16(vpaddq_u16(vmull_u8(input, factors), vdupq_n_u16(0)), 3);
      };

      const auto average_blend = [](uint8x8_t input) {
        return vshrn_n_u16(vcombine_u16(vpaddl_u8(input), vdup_n_u16(0)), 1);
      };

      uint8x8_t first_case_r_2 = dxt_blend(color_0_1.r, dxt_blend_factors_0);
      uint8x8_t first_case_g_2 = dxt_blend(color_0_1.g, dxt_blend_factors_0);
      uint8x8_t first_case_b_2 = dxt_blend(color_0_1.b, dxt_blend_factors_0);

      uint8x8_t first_case_r_3 = dxt_blend(color_0_1.r, dxt_blend_factors_1);
      uint8x8_t first_case_g_3 = dxt_blend(color_0_1.g, dxt_blend_factors_1);
      uint8x8_t first_case_b_3 = dxt_blend(color_0_1.b, dxt_blend_factors_1);

      uint8x8_t first_case_r_2_3 = vzip1_u8(first_case_r_2, first_case_r_3);
      uint8x8_t first_case_g_2_3 = vzip1_u8(first_case_g_2, first_case_g_3);
      uint8x8_t first_case_b_2_3 = vzip1_u8(first_case_b_2, first_case_b_3);

      uint8x8_t second_case_r_2 = average_blend(color_0_1.r);
      uint8x8_t second_case_g_2 = average_blend(color_0_1.g);
      uint8x8_t second_case_b_2 = average_blend(color_0_1.b);

      uint8x8_t second_case_r_2_3 = vzip1_u8(second_case_r_2, second_case_r_2);
      uint8x8_t second_case_g_2_3 = vzip1_u8(second_case_g_2, second_case_g_2);
      uint8x8_t second_case_b_2_3 = vzip1_u8(second_case_b_2, second_case_b_2);

      uint16x4_t rgb565_0 = vget_low_u16(vuzp1q_u16(rgb565_byteswapped, vdupq_n_u16(0)));
      uint16x4_t rgb565_1 = vget_low_u16(vuzp2q_u16(rgb565_byteswapped, vdupq_n_u16(0)));
      uint16x4_t color_0_is_bigger = vcgt_u16(rgb565_0, rgb565_1);

      uint8x8_t r_2_3 =
          vbsl_u8(vreinterpret_u8_u16(color_0_is_bigger), first_case_r_2_3, second_case_r_2_3);
      uint8x8_t g_2_3 =
          vbsl_u8(vreinterpret_u8_u16(color_0_is_bigger), first_case_g_2_3, second_case_g_2_3);
      uint8x8_t b_2_3 =
          vbsl_u8(vreinterpret_u8_u16(color_0_is_bigger), first_case_b_2_3, second_case_b_2_3);
      // a_2 is always FF, a_3 is FF iff color 0 is bigger
      uint8x8_t a_2_3 = vorr_u16(vreinterpret_u8_u16(color_0_is_bigger), vdup_n_u16(0x00FF));

      // Put all four colors together

      const auto zip_colors = [](uint8x8_t color_0_1_, uint8x8_t color_2_3_) {
        return vzip1q_u16(vreinterpretq_u16_u8(vcombine_u8(color_0_1_, vdup_n_u8(0))),
                          vreinterpretq_u16_u8(vcombine_u8(color_2_3_, vdup_n_u8(0))));
      };

      RGBAx16 color{
          zip_colors(color_0_1.r, r_2_3),
          zip_colors(color_0_1.g, g_2_3),
          zip_colors(color_0_1.b, b_2_3),
          zip_colors(color_0_1.a, a_2_3),
      };

      // Unpack the indices so each one takes up one byte. We could do this entirely using SIMD,
      // but doing it using GPR units lets us offload the already busy FPR units.
      //
      // We rearrange the lines so every line from the first block is next to a line from the second
      // block, and every line from the third block is next to a line from the fourth block.
      // This lets us write 8 texels at a time to dst instead of just 4.
      //
      // TODO: vluti2q_laneq_u8 might be faster, but it requires an optional extension

      u32 first_block_indices = src2[1];
      u32 second_block_indices = src2[3];
      for (size_t i = 0; i < 4; ++i)
      {
        unpacked_indices[i * 2 + 0] = unpack_2bit_table[(first_block_indices >> i * 8) & 0xFF];
        unpacked_indices[i * 2 + 1] =
            unpack_2bit_table[(second_block_indices >> i * 8) & 0xFF] | 0x04040404;
      }

      u32 third_block_indices = src2[5];
      u32 fourth_block_indices = src2[7];
      for (size_t i = 0; i < 4; ++i)
      {
        unpacked_indices[i * 2 + 8] =
            unpack_2bit_table[(third_block_indices >> i * 8) & 0xFF] | 0x08080808;
        unpacked_indices[i * 2 + 9] =
            unpack_2bit_table[(fourth_block_indices >> i * 8) & 0xFF] | 0x0C0C0C0C;
      }

      // Finally, do the table lookup and store the result

      uint8x16x4_t table_indices =
          vld1q_u8_x4(reinterpret_cast<const u8*>(unpacked_indices.data()));
      for (int i = 0; i < 4; ++i)
      {
        RGBAx16 rgba = {
            vqtbl1q_u8(color.r, table_indices.val[i]),
            vqtbl1q_u8(color.g, table_indices.val[i]),
            vqtbl1q_u8(color.b, table_indices.val[i]),
            vqtbl1q_u8(color.a, table_indices.val[i]),
        };
        Store8x2RGBA(dst, x, y + i * 2, width, rgba);
      }
    }
  }
}

void _TexDecoder_DecodeImpl(u32* dst, const u8* src, int width, int height, TextureFormat texformat,
                            const u8* tlut, TLUTFormat tlutfmt)
{
  const int Wsteps4 = (width + 3) / 4;
  const int Wsteps8 = (width + 7) / 8;

  switch (texformat)
  {
  case TextureFormat::I4:
    TexDecoder_DecodeImpl_I4(dst, src, width, height, Wsteps4, Wsteps8);
    break;
  case TextureFormat::I8:
    TexDecoder_DecodeImpl_I8(dst, src, width, height, Wsteps4, Wsteps8);
    break;
  case TextureFormat::IA4:
    TexDecoder_DecodeImpl_IA4(dst, src, width, height, Wsteps4, Wsteps8);
    break;
  case TextureFormat::IA8:
    TexDecoder_DecodeImpl_IA8(dst, src, width, height, Wsteps4, Wsteps8);
    break;
  case TextureFormat::RGB565:
    TexDecoder_DecodeImpl_RGB565(dst, src, width, height, Wsteps4, Wsteps8);
    break;
  case TextureFormat::RGB5A3:
    TexDecoder_DecodeImpl_RGB5A3(dst, src, width, height, Wsteps4, Wsteps8);
    break;
  case TextureFormat::RGBA8:
    TexDecoder_DecodeImpl_RGBA8(dst, src, width, height, Wsteps4, Wsteps8);
    break;
  case TextureFormat::C4:
  {
    RGBAx16 palette;
    switch (tlutfmt)
    {
    default:
    case TLUTFormat::IA8:
      palette = Decode_IA8(vld2q_u8(tlut));
      break;
    case TLUTFormat::RGB565:
      palette = Decode_RGB565(vld2q_u8(tlut));
      break;
    case TLUTFormat::RGB5A3:
      palette = Decode_RGB5A3(vld2q_u8(tlut));
      break;
    }
    TexDecoder_DecodeImpl_C4(dst, src, width, height, Wsteps4, Wsteps8, palette);
    break;
  }
  case TextureFormat::C8:
    switch (tlutfmt)
    {
    case TLUTFormat::IA8:
      TexDecoder_DecodeImpl_C8(dst, src, width, height, Wsteps4, Wsteps8, tlut, Decode_IA8);
      break;
    case TLUTFormat::RGB565:
      TexDecoder_DecodeImpl_C8(dst, src, width, height, Wsteps4, Wsteps8, tlut, Decode_RGB565);
      break;
    case TLUTFormat::RGB5A3:
      TexDecoder_DecodeImpl_C8(dst, src, width, height, Wsteps4, Wsteps8, tlut, Decode_RGB5A3);
      break;
    }
    break;
  case TextureFormat::C14X2:
    switch (tlutfmt)
    {
    case TLUTFormat::IA8:
      TexDecoder_DecodeImpl_C14X2<true>(dst, src, width, height, Wsteps4, Wsteps8, tlut,
                                        Decode_IA8);
      break;
    case TLUTFormat::RGB565:
      TexDecoder_DecodeImpl_C14X2<true>(dst, src, width, height, Wsteps4, Wsteps8, tlut,
                                        Decode_RGB565);
      break;
    case TLUTFormat::RGB5A3:
      TexDecoder_DecodeImpl_C14X2<false>(dst, src, width, height, Wsteps4, Wsteps8, tlut,
                                         Decode_RGB5A3);
      break;
    }
    break;
  case TextureFormat::CMPR:
    TexDecoder_DecodeImpl_CMPR(dst, src, width, height, Wsteps4, Wsteps8);
    break;
  case TextureFormat::XFB:
    TexDecoder_DecodeXFB(reinterpret_cast<u8*>(dst), src, width, height, width * 2);
    break;
  }
}
