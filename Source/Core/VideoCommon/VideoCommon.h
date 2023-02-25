// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <bit>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"

#include "VideoCommon/BPMemory.h"

// These are accurate (disregarding AA modes).
constexpr u32 EFB_WIDTH = 640u;
constexpr u32 EFB_HEIGHT = 528u;

// Max XFB width is 720. You can only copy out 640 wide areas of efb to XFB
// so you need multiple copies to do the full width.
// The VI can do horizontal scaling (TODO: emulate).
constexpr u32 MAX_XFB_WIDTH = 720u;

// Although EFB height is 528, 576-line XFB's can be created either with
// vertical scaling by the EFB copy operation or copying to multiple XFB's
// that are next to each other in memory (TODO: handle that situation).
constexpr u32 MAX_XFB_HEIGHT = 576u;

#define PRIM_LOG(t, ...) DEBUG_LOG_FMT(VIDEO, t __VA_OPT__(, ) __VA_ARGS__)

// warning: mapping buffer should be disabled to use this
// #define LOG_VTX() DEBUG_LOG_FMT(VIDEO, "vtx: {} {} {}, ",
//                                 ((float*)g_vertex_manager_write_ptr)[-3],
//                                 ((float*)g_vertex_manager_write_ptr)[-2],
//                                 ((float*)g_vertex_manager_write_ptr)[-1]);

#define LOG_VTX()

enum class APIType
{
  OpenGL,
  D3D,
  Vulkan,
  Metal,
  Nothing
};

inline u32 RGBA8ToRGBA6ToRGBA8(u32 src)
{
  u32 color = src;
  color &= 0xFCFCFCFC;
  color |= (color >> 6) & 0x03030303;
  return color;
}

inline u32 RGBA8ToRGB565ToRGBA8(u32 src)
{
  u32 color = (src & 0xF8FCF8);
  color |= (color >> 5) & 0x070007;
  color |= (color >> 6) & 0x000300;
  color |= 0xFF000000;
  return color;
}

inline u32 Z24ToZ16ToZ24(u32 src)
{
  return (src & 0xFFFF00) | (src >> 16);
}

inline u32 CompressZ16(u32 z24depth, DepthFormat format)
{
  // Flipper offers a number of choices for 16bit Z formats that adjust
  // where the bulk of the precision lies.

  if (format == DepthFormat::ZLINEAR)
  {
    // This is just a linear depth buffer with 16 bits of precision
    return z24depth >> 8;
  }

  // ZNEAR/ZMID/ZFAR are custom floating point formats with 2/3/4 bits of exponent
  // The exponent is simply the number of leading ones that have been removed
  // The first zero bit is skipped and not stored. The mantissa contains the next 14/13/12 bits
  // If exponent is at the MAX (3, 7, or 12) then the next bit might still be a one, and can't
  // be skipped, so the mantissa simply contains the next 14/13/12 bits

  u32 leading_ones = static_cast<u32>(std::countl_one(z24depth << 8));
  bool next_bit_is_one = false;  // AKA: Did we clamp leading_ones?
  u32 exp_bits;

  switch (format)
  {
  case DepthFormat::ZNEAR:
    exp_bits = 2;
    if (leading_ones >= 3u)
    {
      leading_ones = 3u;
      next_bit_is_one = true;
    }
    break;
  case DepthFormat::ZMID:
    exp_bits = 3;
    if (leading_ones >= 7u)
    {
      leading_ones = 7u;
      next_bit_is_one = true;
    }
    break;
  case DepthFormat::ZFAR:
    exp_bits = 4;
    if (leading_ones >= 12u)
    {
      // The hardware implementation only uses values 0 to 12 in the exponent
      leading_ones = 12u;
      next_bit_is_one = true;
    }
    break;
  default:
    return z24depth >> 8;
  }

  u32 mantissa_bits = 16 - exp_bits;

  // Calculate which bits we need to extract from z24depth for our mantissa
  u32 top = std::max<u32>(24 - leading_ones, mantissa_bits);
  if (!next_bit_is_one)
  {
    top -= 1;  // We know the next bit is zero, so we don't need to include it.
  }
  u32 bottom = top - mantissa_bits;

  u32 exponent = leading_ones << mantissa_bits;  // Upper bits contain exponent
  u32 mantissa = Common::ExtractBits(z24depth, bottom, top - 1);

  return exponent | mantissa;
}
