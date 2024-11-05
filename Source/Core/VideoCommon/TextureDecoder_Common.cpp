// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/SpanUtils.h"
#include "Common/Swap.h"

#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/TextureDecoder_Util.h"
#include "VideoCommon/sfont.inc"

static bool TexFmt_Overlay_Enable = false;
static bool TexFmt_Overlay_Center = false;

// TRAM
// STATE_TO_SAVE
alignas(16) std::array<u8, TMEM_SIZE> s_tex_mem;

int TexDecoder_GetTexelSizeInNibbles(TextureFormat format)
{
  switch (format)
  {
  // 4-bit formats
  case TextureFormat::I4:
  case TextureFormat::C4:
    return 1;
  // 8-bit formats
  case TextureFormat::I8:
  case TextureFormat::IA4:
  case TextureFormat::C8:
    return 2;
  // 16-bit formats
  case TextureFormat::IA8:
  case TextureFormat::RGB565:
  case TextureFormat::RGB5A3:
  case TextureFormat::C14X2:
    return 4;
  // 32-bit formats
  case TextureFormat::RGBA8:
    return 8;
  // Compressed format
  case TextureFormat::CMPR:
    return 1;
  // Special formats
  case TextureFormat::XFB:
    return 4;
  default:
    PanicAlertFmt("Invalid Texture Format {}! (GetTexelSizeInNibbles)", format);
    return 1;
  }
}

int TexDecoder_GetTextureSizeInBytes(int width, int height, TextureFormat format)
{
  return (width * height * TexDecoder_GetTexelSizeInNibbles(format)) / 2;
}

int TexDecoder_GetBlockWidthInTexels(TextureFormat format)
{
  switch (format)
  {
  // 4-bit formats
  case TextureFormat::I4:
  case TextureFormat::C4:
    return 8;
  // 8-bit formats
  case TextureFormat::I8:
  case TextureFormat::IA4:
  case TextureFormat::C8:
    return 8;
  // 16-bit formats
  case TextureFormat::IA8:
  case TextureFormat::RGB565:
  case TextureFormat::RGB5A3:
  case TextureFormat::C14X2:
    return 4;
  // 32-bit formats
  case TextureFormat::RGBA8:
    return 4;
  // Compressed format
  case TextureFormat::CMPR:
    return 8;
  // Special formats
  case TextureFormat::XFB:
    return 16;
  default:
    PanicAlertFmt("Invalid Texture Format {}! (GetBlockWidthInTexels)", format);
    return 8;
  }
}

int TexDecoder_GetBlockHeightInTexels(TextureFormat format)
{
  switch (format)
  {
  // 4-bit formats
  case TextureFormat::I4:
  case TextureFormat::C4:
    return 8;
  // 8-bit formats
  case TextureFormat::I8:
  case TextureFormat::IA4:
  case TextureFormat::C8:
    return 4;
  // 16-bit formats
  case TextureFormat::IA8:
  case TextureFormat::RGB565:
  case TextureFormat::RGB5A3:
  case TextureFormat::C14X2:
    return 4;
  // 32-bit formats
  case TextureFormat::RGBA8:
    return 4;
  // Compressed format
  case TextureFormat::CMPR:
    return 8;
  // Special formats
  case TextureFormat::XFB:
    return 1;
  default:
    PanicAlertFmt("Invalid Texture Format {}! (GetBlockHeightInTexels)", format);
    return 4;
  }
}

int TexDecoder_GetEFBCopyBlockWidthInTexels(EFBCopyFormat format)
{
  switch (format)
  {
  // 4-bit formats
  case EFBCopyFormat::R4:
    return 8;
  // 8-bit formats
  case EFBCopyFormat::RA4:
  case EFBCopyFormat::A8:
  case EFBCopyFormat::R8_0x1:
  case EFBCopyFormat::R8:
  case EFBCopyFormat::G8:
  case EFBCopyFormat::B8:
    return 8;
  // 16-bit formats
  case EFBCopyFormat::RA8:
  case EFBCopyFormat::RGB565:
  case EFBCopyFormat::RGB5A3:
  case EFBCopyFormat::RG8:
  case EFBCopyFormat::GB8:
    return 4;
  // 32-bit formats
  case EFBCopyFormat::RGBA8:
    return 4;
  // Special formats
  case EFBCopyFormat::XFB:
    return 16;
  default:
    PanicAlertFmt("Invalid EFB Copy Format {}! (GetEFBCopyBlockWidthInTexels)", format);
    return 8;
  }
}

int TexDecoder_GetEFBCopyBlockHeightInTexels(EFBCopyFormat format)
{
  switch (format)
  {
  // 4-bit formats
  case EFBCopyFormat::R4:
    return 8;
  // 8-bit formats
  case EFBCopyFormat::RA4:
  case EFBCopyFormat::A8:
  case EFBCopyFormat::R8_0x1:
  case EFBCopyFormat::R8:
  case EFBCopyFormat::G8:
  case EFBCopyFormat::B8:
    return 4;
  // 16-bit formats
  case EFBCopyFormat::RA8:
  case EFBCopyFormat::RGB565:
  case EFBCopyFormat::RGB5A3:
  case EFBCopyFormat::RG8:
  case EFBCopyFormat::GB8:
    return 4;
  // 32-bit formats
  case EFBCopyFormat::RGBA8:
    return 4;
  // Special formats
  case EFBCopyFormat::XFB:
    return 1;
  default:
    PanicAlertFmt("Invalid EFB Copy Format {}! (GetEFBCopyBlockHeightInTexels)", format);
    return 4;
  }
}

// returns bytes
int TexDecoder_GetPaletteSize(TextureFormat format)
{
  switch (format)
  {
  case TextureFormat::C4:
    return 16 * 2;
  case TextureFormat::C8:
    return 256 * 2;
  case TextureFormat::C14X2:
    return 16384 * 2;
  default:
    return 0;
  }
}

// Get the "in memory" texture format of an EFB copy's format.
// With the exception of c4/c8/c14 paletted texture formats (which are handled elsewhere)
// this is the format the game should be using when it is drawing an EFB copy back.
TextureFormat TexDecoder_GetEFBCopyBaseFormat(EFBCopyFormat format)
{
  switch (format)
  {
  case EFBCopyFormat::R4:
    return TextureFormat::I4;
  case EFBCopyFormat::A8:
  case EFBCopyFormat::R8_0x1:
  case EFBCopyFormat::R8:
  case EFBCopyFormat::G8:
  case EFBCopyFormat::B8:
    return TextureFormat::I8;
  case EFBCopyFormat::RA4:
    return TextureFormat::IA4;
  case EFBCopyFormat::RA8:
  case EFBCopyFormat::RG8:
  case EFBCopyFormat::GB8:
    return TextureFormat::IA8;
  case EFBCopyFormat::RGB565:
    return TextureFormat::RGB565;
  case EFBCopyFormat::RGB5A3:
    return TextureFormat::RGB5A3;
  case EFBCopyFormat::RGBA8:
    return TextureFormat::RGBA8;
  case EFBCopyFormat::XFB:
    return TextureFormat::XFB;
  default:
    PanicAlertFmt("Invalid EFB Copy Format {}! (GetEFBCopyBaseFormat)", format);
    return static_cast<TextureFormat>(format);
  }
}

void TexDecoder_SetTexFmtOverlayOptions(bool enable, bool center)
{
  TexFmt_Overlay_Enable = enable;
  TexFmt_Overlay_Center = center;
}

static void TexDecoder_DrawOverlay(u8* dst, int width, int height, TextureFormat texformat)
{
  int w = std::min(width, 40);
  int h = std::min(height, 10);

  int xoff = (width - w) >> 1;
  int yoff = (height - h) >> 1;

  if (!TexFmt_Overlay_Center)
  {
    xoff = 0;
    yoff = 0;
  }

  const auto fmt_str = fmt::to_string(texformat);
  for (char ch : fmt_str)
  {
    int xcnt = 0;
    int nchar = sfont_map[static_cast<u8>(ch)];

    const unsigned char* ptr = sfont_raw[nchar];  // each char is up to 9x10

    for (int x = 0; x < 9; x++)
    {
      if (ptr[x] == 0x78)
        break;
      xcnt++;
    }

    for (int y = 0; y < 10; y++)
    {
      for (int x = 0; x < xcnt; x++)
      {
        int* dtp = (int*)dst;
        dtp[(y + yoff) * width + x + xoff] = ptr[x] ? 0xFFFFFFFF : 0xFF000000;
      }
      ptr += 9;
    }
    xoff += xcnt;
  }
}

void TexDecoder_Decode(u8* dst, const u8* src, int width, int height, TextureFormat texformat,
                       const u8* tlut, TLUTFormat tlutfmt)
{
  _TexDecoder_DecodeImpl((u32*)dst, src, width, height, texformat, tlut, tlutfmt);

  if (TexFmt_Overlay_Enable)
    TexDecoder_DrawOverlay(dst, width, height, texformat);
}

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

void TexDecoder_DecodeTexel(u8* dst, std::span<const u8> src, int s, int t, int imageWidth,
                            TextureFormat texformat, std::span<const u8> tlut_, TLUTFormat tlutfmt)
{
  /* General formula for computing texture offset
  //
  u16 sBlk = s / blockWidth;
  u16 tBlk = t / blockHeight;
  u16 widthBlks = (width / blockWidth) + 1;
  u32 base = (tBlk * widthBlks + sBlk) * blockWidth * blockHeight;
  u16 blkS = s & (blockWidth - 1);
  u16 blkT =  t & (blockHeight - 1);
  u32 blkOff = blkT * blockWidth + blkS;
  */

  switch (texformat)
  {
  case TextureFormat::C4:
  {
    u16 sBlk = s >> 3;
    u16 tBlk = t >> 3;
    u16 widthBlks = (imageWidth >> 3) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 5;
    u16 blkS = s & 7;
    u16 blkT = t & 7;
    u32 blkOff = (blkT << 3) + blkS;

    int rs = (blkOff & 1) ? 0 : 4;
    u32 offset = base + (blkOff >> 1);

    u8 val = (Common::SafeSpanRead<u8>(src, offset) >> rs) & 0xF;
    u16 pixel = Common::SafeSpanRead<u16>(tlut_, sizeof(u16) * val);

    *((u32*)dst) = DecodePixel_Paletted(pixel, tlutfmt);
  }
  break;
  case TextureFormat::I4:
  {
    u16 sBlk = s >> 3;
    u16 tBlk = t >> 3;
    u16 widthBlks = (imageWidth >> 3) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 5;
    u16 blkS = s & 7;
    u16 blkT = t & 7;
    u32 blkOff = (blkT << 3) + blkS;

    int rs = (blkOff & 1) ? 0 : 4;
    u32 offset = base + (blkOff >> 1);

    u8 val = (Common::SafeSpanRead<u8>(src, offset) >> rs) & 0xF;
    val = Convert4To8(val);
    dst[0] = val;
    dst[1] = val;
    dst[2] = val;
    dst[3] = val;
  }
  break;
  case TextureFormat::I8:
  {
    u16 sBlk = s >> 3;
    u16 tBlk = t >> 2;
    u16 widthBlks = (imageWidth >> 3) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 5;
    u16 blkS = s & 7;
    u16 blkT = t & 3;
    u32 blkOff = (blkT << 3) + blkS;

    u8 val = Common::SafeSpanRead<u8>(src, base + blkOff);
    dst[0] = val;
    dst[1] = val;
    dst[2] = val;
    dst[3] = val;
  }
  break;
  case TextureFormat::C8:
  {
    u16 sBlk = s >> 3;
    u16 tBlk = t >> 2;
    u16 widthBlks = (imageWidth >> 3) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 5;
    u16 blkS = s & 7;
    u16 blkT = t & 3;
    u32 blkOff = (blkT << 3) + blkS;

    u8 val = Common::SafeSpanRead<u8>(src, base + blkOff);
    u16 pixel = Common::SafeSpanRead<u16>(tlut_, sizeof(u16) * val);

    *((u32*)dst) = DecodePixel_Paletted(pixel, tlutfmt);
  }
  break;
  case TextureFormat::IA4:
  {
    u16 sBlk = s >> 3;
    u16 tBlk = t >> 2;
    u16 widthBlks = (imageWidth >> 3) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 5;
    u16 blkS = s & 7;
    u16 blkT = t & 3;
    u32 blkOff = (blkT << 3) + blkS;

    u8 val = Common::SafeSpanRead<u8>(src, base + blkOff);
    const u8 a = Convert4To8(val >> 4);
    const u8 l = Convert4To8(val & 0xF);
    dst[0] = l;
    dst[1] = l;
    dst[2] = l;
    dst[3] = a;
  }
  break;
  case TextureFormat::IA8:
  {
    u16 sBlk = s >> 2;
    u16 tBlk = t >> 2;
    u16 widthBlks = (imageWidth >> 2) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 4;
    u16 blkS = s & 3;
    u16 blkT = t & 3;
    u32 blkOff = (blkT << 2) + blkS;

    u32 offset = (base + blkOff) << 1;
    u16 val = Common::SafeSpanRead<u16>(src, offset);

    *((u32*)dst) = DecodePixel_IA8(val);
  }
  break;
  case TextureFormat::C14X2:
  {
    u16 sBlk = s >> 2;
    u16 tBlk = t >> 2;
    u16 widthBlks = (imageWidth >> 2) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 4;
    u16 blkS = s & 3;
    u16 blkT = t & 3;
    u32 blkOff = (blkT << 2) + blkS;

    u32 offset = (base + blkOff) << 1;
    u16 val = Common::swap16(Common::SafeSpanRead<u16>(src, offset)) & 0x3FFF;
    u16 pixel = Common::SafeSpanRead<u16>(tlut_, sizeof(u16) * val);

    *((u32*)dst) = DecodePixel_Paletted(pixel, tlutfmt);
  }
  break;
  case TextureFormat::RGB565:
  {
    u16 sBlk = s >> 2;
    u16 tBlk = t >> 2;
    u16 widthBlks = (imageWidth >> 2) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 4;
    u16 blkS = s & 3;
    u16 blkT = t & 3;
    u32 blkOff = (blkT << 2) + blkS;

    u32 offset = (base + blkOff) << 1;
    u16 val = Common::SafeSpanRead<u16>(src, offset);

    *((u32*)dst) = DecodePixel_RGB565(Common::swap16(val));
  }
  break;
  case TextureFormat::RGB5A3:
  {
    u16 sBlk = s >> 2;
    u16 tBlk = t >> 2;
    u16 widthBlks = (imageWidth >> 2) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 4;
    u16 blkS = s & 3;
    u16 blkT = t & 3;
    u32 blkOff = (blkT << 2) + blkS;

    u32 offset = (base + blkOff) << 1;
    u16 val = Common::SafeSpanRead<u16>(src, offset);

    *((u32*)dst) = DecodePixel_RGB5A3(Common::swap16(val));
  }
  break;
  case TextureFormat::RGBA8:
  {
    u16 sBlk = s >> 2;
    u16 tBlk = t >> 2;
    u16 widthBlks = (imageWidth >> 2) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 5;  // shift by 5 is correct
    u16 blkS = s & 3;
    u16 blkT = t & 3;
    u32 blkOff = (blkT << 2) + blkS;

    u32 offset = (base + blkOff) << 1;

    dst[3] = Common::SafeSpanRead<u8>(src, offset);
    dst[0] = Common::SafeSpanRead<u8>(src, offset + 1);
    dst[1] = Common::SafeSpanRead<u8>(src, offset + 32);
    dst[2] = Common::SafeSpanRead<u8>(src, offset + 33);
  }
  break;
  case TextureFormat::CMPR:
  {
    u16 sDxt = s >> 2;
    u16 tDxt = t >> 2;

    u16 sBlk = sDxt >> 1;
    u16 tBlk = tDxt >> 1;
    u16 widthBlks = (imageWidth >> 3) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 2;
    u16 blkS = sDxt & 1;
    u16 blkT = tDxt & 1;
    u32 blkOff = (blkT << 1) + blkS;

    u32 offset = (base + blkOff) << 3;

    DXTBlock dxtBlock = Common::SafeSpanRead<DXTBlock>(src, offset);

    u16 c1 = Common::swap16(dxtBlock.color1);
    u16 c2 = Common::swap16(dxtBlock.color2);
    int blue1 = Convert5To8(c1 & 0x1F);
    int blue2 = Convert5To8(c2 & 0x1F);
    int green1 = Convert6To8((c1 >> 5) & 0x3F);
    int green2 = Convert6To8((c2 >> 5) & 0x3F);
    int red1 = Convert5To8((c1 >> 11) & 0x1F);
    int red2 = Convert5To8((c2 >> 11) & 0x1F);

    u16 ss = s & 3;
    u16 tt = t & 3;

    int colorSel = dxtBlock.lines[tt];
    int rs = 6 - (ss << 1);
    colorSel = (colorSel >> rs) & 3;
    colorSel |= c1 > c2 ? 0 : 4;

    u32 color = 0;

    switch (colorSel)
    {
    case 0:
    case 4:
      color = MakeRGBA(red1, green1, blue1, 255);
      break;
    case 1:
    case 5:
      color = MakeRGBA(red2, green2, blue2, 255);
      break;
    case 2:
      color = MakeRGBA(DXTBlend(red2, red1), DXTBlend(green2, green1), DXTBlend(blue2, blue1), 255);
      break;
    case 3:
      color = MakeRGBA(DXTBlend(red1, red2), DXTBlend(green1, green2), DXTBlend(blue1, blue2), 255);
      break;
    case 6:
      color = MakeRGBA((red1 + red2) / 2, (green1 + green2) / 2, (blue1 + blue2) / 2, 255);
      break;
    case 7:
      // color[3] is the same as color[2] (average of both colors), but transparent.
      // This differs from DXT1 where color[3] is transparent black.
      color = MakeRGBA((red1 + red2) / 2, (green1 + green2) / 2, (blue1 + blue2) / 2, 0);
      break;
    default:
      color = 0;
      break;
    }

    *((u32*)dst) = color;
  }
  break;
  case TextureFormat::XFB:
  {
    size_t offset = (t * imageWidth + (s & (~1))) * 2;

    // We do this one color sample (aka 2 RGB pixles) at a time
    int Y = int((s & 1) == 0 ? src[offset] : src[offset + 2]) - 16;
    int U = int(src[offset + 1]) - 128;
    int V = int(src[offset + 3]) - 128;

    // We do the inverse BT.601 conversion for YCbCr to RGB
    // http://www.equasys.de/colorconversion.html#YCbCr-RGBColorFormatConversion
    // TODO: Use more precise numbers for this conversion (although on real hardware, the XFB isn't
    // in a real texture format, so does this conversion actually ever happen?)
    u8 R = std::clamp(int(1.164f * Y + 1.596f * V), 0, 255);
    u8 G = std::clamp(int(1.164f * Y - 0.392f * U - 0.813f * V), 0, 255);
    u8 B = std::clamp(int(1.164f * Y + 2.017f * U), 0, 255);
    dst[t * imageWidth + s] = 0xff000000 | B << 16 | G << 8 | R;
  }
  break;
  }
}

void TexDecoder_DecodeTexelRGBA8FromTmem(u8* dst, std::span<const u8> src_ar,
                                         std::span<const u8> src_gb, int s, int t, int imageWidth)
{
  u16 sBlk = s >> 2;
  u16 tBlk = t >> 2;
  u16 widthBlks =
      (imageWidth >> 2) + 1;  // TODO: Looks wrong. Shouldn't this be ((imageWidth-1)>>2)+1 ?
  u32 base_ar = (tBlk * widthBlks + sBlk) << 4;
  u32 base_gb = (tBlk * widthBlks + sBlk) << 4;
  u16 blkS = s & 3;
  u16 blkT = t & 3;
  u32 blk_off = (blkT << 2) + blkS;

  u32 offset_ar = (base_ar + blk_off) << 1;
  u32 offset_gb = (base_gb + blk_off) << 1;

  dst[3] = Common::SafeSpanRead<u8>(src_ar, offset_ar);      // A
  dst[0] = Common::SafeSpanRead<u8>(src_ar, offset_ar + 1);  // R
  dst[1] = Common::SafeSpanRead<u8>(src_gb, offset_gb);      // G
  dst[2] = Common::SafeSpanRead<u8>(src_gb, offset_gb + 1);  // B
}

void TexDecoder_DecodeTexelRGBA8FromTmem(u8* dst, const u8* src_ar, const u8* src_gb, int s, int t,
                                         int imageWidth)
{
  u16 sBlk = s >> 2;
  u16 tBlk = t >> 2;
  u16 widthBlks =
      (imageWidth >> 2) + 1;  // TODO: Looks wrong. Shouldn't this be ((imageWidth-1)>>2)+1 ?
  u32 base_ar = (tBlk * widthBlks + sBlk) << 4;
  u32 base_gb = (tBlk * widthBlks + sBlk) << 4;
  u16 blkS = s & 3;
  u16 blkT = t & 3;
  u32 blk_off = (blkT << 2) + blkS;

  u32 offset_ar = (base_ar + blk_off) << 1;
  u32 offset_gb = (base_gb + blk_off) << 1;
  const u8* val_addr_ar = src_ar + offset_ar;
  const u8* val_addr_gb = src_gb + offset_gb;

  dst[3] = val_addr_ar[0];  // A
  dst[0] = val_addr_ar[1];  // R
  dst[1] = val_addr_gb[0];  // G
  dst[2] = val_addr_gb[1];  // B
}

void TexDecoder_DecodeRGBA8FromTmem(u8* dst, const u8* src_ar, const u8* src_gb, int width,
                                    int height)
{
  // TODO for someone who cares: Make this less slow!
  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; ++x)
    {
      TexDecoder_DecodeTexelRGBA8FromTmem(dst, src_ar, src_gb, x, y, width - 1);
      dst += 4;
    }
  }
}

void TexDecoder_DecodeXFB(u8* dst, const u8* src, u32 width, u32 height, u32 stride)
{
  const u8* src_ptr = src;
  u8* dst_ptr = dst;

  for (u32 y = 0; y < height; y++)
  {
    const u8* row_ptr = src_ptr;
    for (u32 x = 0; x < width; x += 2)
    {
      // We do this one color sample (aka 2 RGB pixels) at a time
      int Y1 = int(*(row_ptr++)) - 16;
      int U = int(*(row_ptr++)) - 128;
      int Y2 = int(*(row_ptr++)) - 16;
      int V = int(*(row_ptr++)) - 128;

      // We do the inverse BT.601 conversion for YCbCr to RGB
      // http://www.equasys.de/colorconversion.html#YCbCr-RGBColorFormatConversion
      // TODO: Use more precise numbers for this conversion (although on real hardware, the XFB
      // isn't in a real texture format, so does this conversion actually ever happen?)
      u8 R1 = static_cast<u8>(std::clamp(int(1.164f * Y1 + 1.596f * V), 0, 255));
      u8 G1 = static_cast<u8>(std::clamp(int(1.164f * Y1 - 0.392f * U - 0.813f * V), 0, 255));
      u8 B1 = static_cast<u8>(std::clamp(int(1.164f * Y1 + 2.017f * U), 0, 255));

      u8 R2 = static_cast<u8>(std::clamp(int(1.164f * Y2 + 1.596f * V), 0, 255));
      u8 G2 = static_cast<u8>(std::clamp(int(1.164f * Y2 - 0.392f * U - 0.813f * V), 0, 255));
      u8 B2 = static_cast<u8>(std::clamp(int(1.164f * Y2 + 2.017f * U), 0, 255));

      u32 rgba = 0xff000000 | B1 << 16 | G1 << 8 | R1;
      std::memcpy(dst_ptr, &rgba, sizeof(rgba));
      dst_ptr += sizeof(rgba);
      rgba = 0xff000000 | B2 << 16 | G2 << 8 | R2;
      std::memcpy(dst_ptr, &rgba, sizeof(rgba));
      dst_ptr += sizeof(rgba);
    }

    src_ptr += stride;
  }
}
