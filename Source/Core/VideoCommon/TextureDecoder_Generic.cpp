// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/Swap.h"

#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/TextureDecoder_Util.h"
//#include "VideoCommon/VideoCommon.h" // to get debug logs
#include "VideoCommon/VideoConfig.h"

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
  int r, g, b, a;
  r = Convert5To8((val >> 11) & 0x1f);
  g = Convert6To8((val >> 5) & 0x3f);
  b = Convert5To8((val)&0x1f);
  a = 0xFF;
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

static inline u32 DecodePixel_Paletted(u16 pixel, TLUTFormat tlutfmt)
{
  switch (tlutfmt)
  {
  case TLUTFormat::IA8:
    return DecodePixel_IA8(pixel);
  case TLUTFormat::RGB565:
    return DecodePixel_RGB565(Common::swap16(pixel));
  case TLUTFormat::RGB5A3:
    return DecodePixel_RGB5A3(Common::swap16(pixel));
  default:
    return 0;
  }
}

static inline void DecodeBytes_C4(u32* dst, const u8* src, const u8* tlut_, TLUTFormat tlutfmt)
{
  const u16* tlut = (u16*)tlut_;
  for (int x = 0; x < 4; x++)
  {
    u8 val = src[x];
    *dst++ = DecodePixel_Paletted(tlut[val >> 4], tlutfmt);
    *dst++ = DecodePixel_Paletted(tlut[val & 0xF], tlutfmt);
  }
}

static inline void DecodeBytes_C8(u32* dst, const u8* src, const u8* tlut_, TLUTFormat tlutfmt)
{
  const u16* tlut = (u16*)tlut_;
  for (int x = 0; x < 8; x++)
  {
    u8 val = src[x];
    *dst++ = DecodePixel_Paletted(tlut[val], tlutfmt);
  }
}

static inline void DecodeBytes_C14X2(u32* dst, const u16* src, const u8* tlut_, TLUTFormat tlutfmt)
{
  const u16* tlut = (u16*)tlut_;
  for (int x = 0; x < 4; x++)
  {
    u16 val = Common::swap16(src[x]);
    *dst++ = DecodePixel_Paletted(tlut[(val & 0x3FFF)], tlutfmt);
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

static inline void DecodeBytes_RGB5A3(u32* dst, const u16* src)
{
#if 0
	for (int x = 0; x < 4; x++)
		dst[x] = DecodePixel_RGB5A3(Common::swap16(src[x]));
#else
  dst[0] = DecodePixel_RGB5A3(Common::swap16(src[0]));
  dst[1] = DecodePixel_RGB5A3(Common::swap16(src[1]));
  dst[2] = DecodePixel_RGB5A3(Common::swap16(src[2]));
  dst[3] = DecodePixel_RGB5A3(Common::swap16(src[3]));
#endif
}

static inline void DecodeBytes_RGBA8(u32* dst, const u16* src, const u16* src2)
{
#if 0
	for (int x = 0; x < 4; x++)
	{
		dst[x] =  ((src[x] & 0xFF) << 24) | ((src[x] & 0xFF00)>>8)  | (src2[x] << 8);
	}
#else
  dst[0] = ((src[0] & 0xFF) << 24) | ((src[0] & 0xFF00) >> 8) | (src2[0] << 8);
  dst[1] = ((src[1] & 0xFF) << 24) | ((src[1] & 0xFF00) >> 8) | (src2[1] << 8);
  dst[2] = ((src[2] & 0xFF) << 24) | ((src[2] & 0xFF00) >> 8) | (src2[2] << 8);
  dst[3] = ((src[3] & 0xFF) << 24) | ((src[3] & 0xFF00) >> 8) | (src2[3] << 8);
#endif
}

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

// JSD 01/06/11:
// TODO: we really should ensure BOTH the source and destination addresses are aligned to 16-byte
// boundaries to
// squeeze out a little more performance. _mm_loadu_si128/_mm_storeu_si128 is slower than
// _mm_load_si128/_mm_store_si128
// because they work on unaligned addresses. The processor is free to make the assumption that
// addresses are multiples
// of 16 in the aligned case.
// TODO: complete SSE2 optimization of less often used texture formats.
// TODO: refactor algorithms using _mm_loadl_epi64 unaligned loads to prefer 128-bit aligned loads.

void _TexDecoder_DecodeImpl(u32* dst, const u8* src, int width, int height, TextureFormat texformat,
                            const u8* tlut, TLUTFormat tlutfmt)
{
  const int Wsteps4 = (width + 3) / 4;
  const int Wsteps8 = (width + 7) / 8;

  switch (texformat)
  {
  case TextureFormat::C4:
    for (int y = 0; y < height; y += 8)
      for (int x = 0, yStep = (y / 8) * Wsteps8; x < width; x += 8, yStep++)
        for (int iy = 0, xStep = 8 * yStep; iy < 8; iy++, xStep++)
          DecodeBytes_C4(dst + (y + iy) * width + x, src + 4 * xStep, tlut, tlutfmt);
    break;
  case TextureFormat::I4:
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
            memset(dst + (y + iy) * width + x + ix * 2, i1, 4);
            memset(dst + (y + iy) * width + x + ix * 2 + 1, i2, 4);
          }
  }
  break;
  case TextureFormat::I8:  // speed critical
  {
    // Reference C implementation
    for (int y = 0; y < height; y += 4)
      for (int x = 0; x < width; x += 8)
        for (int iy = 0; iy < 4; ++iy, src += 8)
        {
          u32* newdst = dst + (y + iy) * width + x;
          const u8* newsrc = src;
          u8 srcval;

          srcval = (newsrc++)[0];
          (newdst++)[0] = srcval | (srcval << 8) | (srcval << 16) | (srcval << 24);
          srcval = (newsrc++)[0];
          (newdst++)[0] = srcval | (srcval << 8) | (srcval << 16) | (srcval << 24);
          srcval = (newsrc++)[0];
          (newdst++)[0] = srcval | (srcval << 8) | (srcval << 16) | (srcval << 24);
          srcval = (newsrc++)[0];
          (newdst++)[0] = srcval | (srcval << 8) | (srcval << 16) | (srcval << 24);
          srcval = (newsrc++)[0];
          (newdst++)[0] = srcval | (srcval << 8) | (srcval << 16) | (srcval << 24);
          srcval = (newsrc++)[0];
          (newdst++)[0] = srcval | (srcval << 8) | (srcval << 16) | (srcval << 24);
          srcval = (newsrc++)[0];
          (newdst++)[0] = srcval | (srcval << 8) | (srcval << 16) | (srcval << 24);
          srcval = newsrc[0];
          newdst[0] = srcval | (srcval << 8) | (srcval << 16) | (srcval << 24);
        }
  }
  break;
  case TextureFormat::C8:
    for (int y = 0; y < height; y += 4)
      for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
        for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
          DecodeBytes_C8((u32*)dst + (y + iy) * width + x, src + 8 * xStep, tlut, tlutfmt);
    break;
  case TextureFormat::IA4:
  {
    for (int y = 0; y < height; y += 4)
      for (int x = 0, yStep = (y / 4) * Wsteps8; x < width; x += 8, yStep++)
        for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
          DecodeBytes_IA4(dst + (y + iy) * width + x, src + 8 * xStep);
  }
  break;
  case TextureFormat::IA8:
  {
    // Reference C implementation:
    for (int y = 0; y < height; y += 4)
      for (int x = 0; x < width; x += 4)
        for (int iy = 0; iy < 4; iy++, src += 8)
        {
          u32* ptr = dst + (y + iy) * width + x;
          u16* s = (u16*)src;
          ptr[0] = DecodePixel_IA8(s[0]);
          ptr[1] = DecodePixel_IA8(s[1]);
          ptr[2] = DecodePixel_IA8(s[2]);
          ptr[3] = DecodePixel_IA8(s[3]);
        }
  }
  break;
  case TextureFormat::C14X2:
    for (int y = 0; y < height; y += 4)
      for (int x = 0, yStep = (y / 4) * Wsteps4; x < width; x += 4, yStep++)
        for (int iy = 0, xStep = 4 * yStep; iy < 4; iy++, xStep++)
          DecodeBytes_C14X2(dst + (y + iy) * width + x, (u16*)(src + 8 * xStep), tlut, tlutfmt);
    break;
  case TextureFormat::RGB565:
  {
    // Reference C implementation.
    for (int y = 0; y < height; y += 4)
      for (int x = 0; x < width; x += 4)
        for (int iy = 0; iy < 4; iy++, src += 8)
        {
          u32* ptr = dst + (y + iy) * width + x;
          u16* s = (u16*)src;
          for (int j = 0; j < 4; j++)
            *ptr++ = DecodePixel_RGB565(Common::swap16(*s++));
        }
  }
  break;
  case TextureFormat::RGB5A3:
  {
    // Reference C implementation:
    for (int y = 0; y < height; y += 4)
      for (int x = 0; x < width; x += 4)
        for (int iy = 0; iy < 4; iy++, src += 8)
          DecodeBytes_RGB5A3(dst + (y + iy) * width + x, (u16*)src);
  }
  break;
  case TextureFormat::RGBA8:  // speed critical
  {
    // Reference C implementation.
    for (int y = 0; y < height; y += 4)
      for (int x = 0; x < width; x += 4)
      {
        for (int iy = 0; iy < 4; iy++)
          DecodeBytes_RGBA8(dst + (y + iy) * width + x, (u16*)src + 4 * iy,
                            (u16*)src + 4 * iy + 16);
        src += 64;
      }
  }
  break;
  case TextureFormat::CMPR:  // speed critical
    // The metroid games use this format almost exclusively.
    {
      for (int y = 0; y < height; y += 8)
      {
        for (int x = 0; x < width; x += 8)
        {
          DecodeDXTBlock((u32*)dst + y * width + x, (DXTBlock*)src, width);
          src += sizeof(DXTBlock);
          DecodeDXTBlock((u32*)dst + y * width + x + 4, (DXTBlock*)src, width);
          src += sizeof(DXTBlock);
          DecodeDXTBlock((u32*)dst + (y + 4) * width + x, (DXTBlock*)src, width);
          src += sizeof(DXTBlock);
          DecodeDXTBlock((u32*)dst + (y + 4) * width + x + 4, (DXTBlock*)src, width);
          src += sizeof(DXTBlock);
        }
      }
      break;
    }
  case TextureFormat::XFB:
    TexDecoder_DecodeXFB(reinterpret_cast<u8*>(dst), src, width, height, width * 2);
    break;
  }
}
