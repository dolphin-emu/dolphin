// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Software/EfbInterface.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/PerfQueryBase.h"

static u8 efb[EFB_WIDTH * EFB_HEIGHT * 6];

namespace EfbInterface
{
u32 perf_values[PQ_NUM_MEMBERS];

static inline u32 GetColorOffset(u16 x, u16 y)
{
  return (x + y * EFB_WIDTH) * 3;
}

static inline u32 GetDepthOffset(u16 x, u16 y)
{
  return (x + y * EFB_WIDTH) * 3 + DEPTH_BUFFER_START;
}

static void SetPixelAlphaOnly(u32 offset, u8 a)
{
  switch (bpmem.zcontrol.pixel_format)
  {
  case PEControl::RGB8_Z24:
  case PEControl::Z24:
  case PEControl::RGB565_Z16:
    // do nothing
    break;
  case PEControl::RGBA6_Z24:
  {
    u32 a32 = a;
    u32* dst = (u32*)&efb[offset];
    u32 val = *dst & 0xffffffc0;
    val |= (a32 >> 2) & 0x0000003f;
    *dst = val;
  }
  break;
  default:
    ERROR_LOG(VIDEO, "Unsupported pixel format: %i", static_cast<int>(bpmem.zcontrol.pixel_format));
  }
}

static void SetPixelColorOnly(u32 offset, u8* rgb)
{
  switch (bpmem.zcontrol.pixel_format)
  {
  case PEControl::RGB8_Z24:
  case PEControl::Z24:
  {
    u32 src = *(u32*)rgb;
    u32* dst = (u32*)&efb[offset];
    u32 val = *dst & 0xff000000;
    val |= src >> 8;
    *dst = val;
  }
  break;
  case PEControl::RGBA6_Z24:
  {
    u32 src = *(u32*)rgb;
    u32* dst = (u32*)&efb[offset];
    u32 val = *dst & 0xff00003f;
    val |= (src >> 4) & 0x00000fc0;  // blue
    val |= (src >> 6) & 0x0003f000;  // green
    val |= (src >> 8) & 0x00fc0000;  // red
    *dst = val;
  }
  break;
  case PEControl::RGB565_Z16:
  {
    INFO_LOG(VIDEO, "RGB565_Z16 is not supported correctly yet");
    u32 src = *(u32*)rgb;
    u32* dst = (u32*)&efb[offset];
    u32 val = *dst & 0xff000000;
    val |= src >> 8;
    *dst = val;
  }
  break;
  default:
    ERROR_LOG(VIDEO, "Unsupported pixel format: %i", static_cast<int>(bpmem.zcontrol.pixel_format));
  }
}

static void SetPixelAlphaColor(u32 offset, u8* color)
{
  switch (bpmem.zcontrol.pixel_format)
  {
  case PEControl::RGB8_Z24:
  case PEControl::Z24:
  {
    u32 src = *(u32*)color;
    u32* dst = (u32*)&efb[offset];
    u32 val = *dst & 0xff000000;
    val |= src >> 8;
    *dst = val;
  }
  break;
  case PEControl::RGBA6_Z24:
  {
    u32 src = *(u32*)color;
    u32* dst = (u32*)&efb[offset];
    u32 val = *dst & 0xff000000;
    val |= (src >> 2) & 0x0000003f;  // alpha
    val |= (src >> 4) & 0x00000fc0;  // blue
    val |= (src >> 6) & 0x0003f000;  // green
    val |= (src >> 8) & 0x00fc0000;  // red
    *dst = val;
  }
  break;
  case PEControl::RGB565_Z16:
  {
    INFO_LOG(VIDEO, "RGB565_Z16 is not supported correctly yet");
    u32 src = *(u32*)color;
    u32* dst = (u32*)&efb[offset];
    u32 val = *dst & 0xff000000;
    val |= src >> 8;
    *dst = val;
  }
  break;
  default:
    ERROR_LOG(VIDEO, "Unsupported pixel format: %i", static_cast<int>(bpmem.zcontrol.pixel_format));
  }
}

static u32 GetPixelColor(u32 offset)
{
  u32 src;
  std::memcpy(&src, &efb[offset], sizeof(u32));

  switch (bpmem.zcontrol.pixel_format)
  {
  case PEControl::RGB8_Z24:
  case PEControl::Z24:
    return 0xff | ((src & 0x00ffffff) << 8);

  case PEControl::RGBA6_Z24:
    return Convert6To8(src & 0x3f) |                // Alpha
           Convert6To8((src >> 6) & 0x3f) << 8 |    // Blue
           Convert6To8((src >> 12) & 0x3f) << 16 |  // Green
           Convert6To8((src >> 18) & 0x3f) << 24;   // Red

  case PEControl::RGB565_Z16:
    INFO_LOG(VIDEO, "RGB565_Z16 is not supported correctly yet");
    return 0xff | ((src & 0x00ffffff) << 8);

  default:
    ERROR_LOG(VIDEO, "Unsupported pixel format: %i", static_cast<int>(bpmem.zcontrol.pixel_format));
    return 0;
  }
}

static void SetPixelDepth(u32 offset, u32 depth)
{
  switch (bpmem.zcontrol.pixel_format)
  {
  case PEControl::RGB8_Z24:
  case PEControl::RGBA6_Z24:
  case PEControl::Z24:
  {
    u32* dst = (u32*)&efb[offset];
    u32 val = *dst & 0xff000000;
    val |= depth & 0x00ffffff;
    *dst = val;
  }
  break;
  case PEControl::RGB565_Z16:
  {
    INFO_LOG(VIDEO, "RGB565_Z16 is not supported correctly yet");
    u32* dst = (u32*)&efb[offset];
    u32 val = *dst & 0xff000000;
    val |= depth & 0x00ffffff;
    *dst = val;
  }
  break;
  default:
    ERROR_LOG(VIDEO, "Unsupported pixel format: %i", static_cast<int>(bpmem.zcontrol.pixel_format));
  }
}

static u32 GetPixelDepth(u32 offset)
{
  u32 depth = 0;

  switch (bpmem.zcontrol.pixel_format)
  {
  case PEControl::RGB8_Z24:
  case PEControl::RGBA6_Z24:
  case PEControl::Z24:
  {
    depth = (*(u32*)&efb[offset]) & 0x00ffffff;
  }
  break;
  case PEControl::RGB565_Z16:
  {
    INFO_LOG(VIDEO, "RGB565_Z16 is not supported correctly yet");
    depth = (*(u32*)&efb[offset]) & 0x00ffffff;
  }
  break;
  default:
    ERROR_LOG(VIDEO, "Unsupported pixel format: %i", static_cast<int>(bpmem.zcontrol.pixel_format));
  }

  return depth;
}

static u32 GetSourceFactor(u8* srcClr, u8* dstClr, BlendMode::BlendFactor mode)
{
  switch (mode)
  {
  case BlendMode::ZERO:
    return 0;
  case BlendMode::ONE:
    return 0xffffffff;
  case BlendMode::DSTCLR:
    return *(u32*)dstClr;
  case BlendMode::INVDSTCLR:
    return 0xffffffff - *(u32*)dstClr;
  case BlendMode::SRCALPHA:
  {
    u8 alpha = srcClr[ALP_C];
    u32 factor = alpha << 24 | alpha << 16 | alpha << 8 | alpha;
    return factor;
  }
  case BlendMode::INVSRCALPHA:
  {
    u8 alpha = 0xff - srcClr[ALP_C];
    u32 factor = alpha << 24 | alpha << 16 | alpha << 8 | alpha;
    return factor;
  }
  case BlendMode::DSTALPHA:
  {
    u8 alpha = dstClr[ALP_C];
    u32 factor = alpha << 24 | alpha << 16 | alpha << 8 | alpha;
    return factor;
  }
  case BlendMode::INVDSTALPHA:
  {
    u8 alpha = 0xff - dstClr[ALP_C];
    u32 factor = alpha << 24 | alpha << 16 | alpha << 8 | alpha;
    return factor;
  }
  }

  return 0;
}

static u32 GetDestinationFactor(u8* srcClr, u8* dstClr, BlendMode::BlendFactor mode)
{
  switch (mode)
  {
  case BlendMode::ZERO:
    return 0;
  case BlendMode::ONE:
    return 0xffffffff;
  case BlendMode::SRCCLR:
    return *(u32*)srcClr;
  case BlendMode::INVSRCCLR:
    return 0xffffffff - *(u32*)srcClr;
  case BlendMode::SRCALPHA:
  {
    u8 alpha = srcClr[ALP_C];
    u32 factor = alpha << 24 | alpha << 16 | alpha << 8 | alpha;
    return factor;
  }
  case BlendMode::INVSRCALPHA:
  {
    u8 alpha = 0xff - srcClr[ALP_C];
    u32 factor = alpha << 24 | alpha << 16 | alpha << 8 | alpha;
    return factor;
  }
  case BlendMode::DSTALPHA:
  {
    u8 alpha = dstClr[ALP_C] & 0xff;
    u32 factor = alpha << 24 | alpha << 16 | alpha << 8 | alpha;
    return factor;
  }
  case BlendMode::INVDSTALPHA:
  {
    u8 alpha = 0xff - dstClr[ALP_C];
    u32 factor = alpha << 24 | alpha << 16 | alpha << 8 | alpha;
    return factor;
  }
  }

  return 0;
}

static void BlendColor(u8* srcClr, u8* dstClr)
{
  u32 srcFactor = GetSourceFactor(srcClr, dstClr, bpmem.blendmode.srcfactor);
  u32 dstFactor = GetDestinationFactor(srcClr, dstClr, bpmem.blendmode.dstfactor);

  for (int i = 0; i < 4; i++)
  {
    // add MSB of factors to make their range 0 -> 256
    u32 sf = (srcFactor & 0xff);
    sf += sf >> 7;

    u32 df = (dstFactor & 0xff);
    df += df >> 7;

    u32 color = (srcClr[i] * sf + dstClr[i] * df) >> 8;
    dstClr[i] = (color > 255) ? 255 : color;

    dstFactor >>= 8;
    srcFactor >>= 8;
  }
}

static void LogicBlend(u32 srcClr, u32* dstClr, BlendMode::LogicOp op)
{
  switch (op)
  {
  case BlendMode::CLEAR:
    *dstClr = 0;
    break;
  case BlendMode::AND:
    *dstClr = srcClr & *dstClr;
    break;
  case BlendMode::AND_REVERSE:
    *dstClr = srcClr & (~*dstClr);
    break;
  case BlendMode::COPY:
    *dstClr = srcClr;
    break;
  case BlendMode::AND_INVERTED:
    *dstClr = (~srcClr) & *dstClr;
    break;
  case BlendMode::NOOP:
    // Do nothing
    break;
  case BlendMode::XOR:
    *dstClr = srcClr ^ *dstClr;
    break;
  case BlendMode::OR:
    *dstClr = srcClr | *dstClr;
    break;
  case BlendMode::NOR:
    *dstClr = ~(srcClr | *dstClr);
    break;
  case BlendMode::EQUIV:
    *dstClr = ~(srcClr ^ *dstClr);
    break;
  case BlendMode::INVERT:
    *dstClr = ~*dstClr;
    break;
  case BlendMode::OR_REVERSE:
    *dstClr = srcClr | (~*dstClr);
    break;
  case BlendMode::COPY_INVERTED:
    *dstClr = ~srcClr;
    break;
  case BlendMode::OR_INVERTED:
    *dstClr = (~srcClr) | *dstClr;
    break;
  case BlendMode::NAND:
    *dstClr = ~(srcClr & *dstClr);
    break;
  case BlendMode::SET:
    *dstClr = 0xffffffff;
    break;
  }
}

static void SubtractBlend(u8* srcClr, u8* dstClr)
{
  for (int i = 0; i < 4; i++)
  {
    int c = (int)dstClr[i] - (int)srcClr[i];
    dstClr[i] = (c < 0) ? 0 : c;
  }
}

static void Dither(u16 x, u16 y, u8* color)
{
  // No blending for RGB8 mode
  if (!bpmem.blendmode.dither || bpmem.zcontrol.pixel_format != PEControl::PixelFormat::RGBA6_Z24)
    return;

  // Flipper uses a standard 2x2 Bayer Matrix for 6 bit dithering
  static const u8 dither[2][2] = {{0, 2}, {3, 1}};

  // Only the color channels are dithered?
  for (int i = BLU_C; i <= RED_C; i++)
    color[i] = ((color[i] - (color[i] >> 6)) + dither[y & 1][x & 1]) & 0xfc;
}

void BlendTev(u16 x, u16 y, u8* color)
{
  const u32 offset = GetColorOffset(x, y);
  u32 dstClr = GetPixelColor(offset);

  u8* dstClrPtr = (u8*)&dstClr;

  if (bpmem.blendmode.blendenable)
  {
    if (bpmem.blendmode.subtract)
      SubtractBlend(color, dstClrPtr);
    else
      BlendColor(color, dstClrPtr);
  }
  else if (bpmem.blendmode.logicopenable)
  {
    LogicBlend(*((u32*)color), &dstClr, bpmem.blendmode.logicmode);
  }
  else
  {
    dstClrPtr = color;
  }

  if (bpmem.dstalpha.enable)
    dstClrPtr[ALP_C] = bpmem.dstalpha.alpha;

  if (bpmem.blendmode.colorupdate)
  {
    Dither(x, y, dstClrPtr);
    if (bpmem.blendmode.alphaupdate)
      SetPixelAlphaColor(offset, dstClrPtr);
    else
      SetPixelColorOnly(offset, dstClrPtr);
  }
  else if (bpmem.blendmode.alphaupdate)
  {
    SetPixelAlphaOnly(offset, dstClrPtr[ALP_C]);
  }
}

void SetColor(u16 x, u16 y, u8* color)
{
  u32 offset = GetColorOffset(x, y);
  if (bpmem.blendmode.colorupdate)
  {
    if (bpmem.blendmode.alphaupdate)
      SetPixelAlphaColor(offset, color);
    else
      SetPixelColorOnly(offset, color);
  }
  else if (bpmem.blendmode.alphaupdate)
  {
    SetPixelAlphaOnly(offset, color[ALP_C]);
  }
}

void SetDepth(u16 x, u16 y, u32 depth)
{
  if (bpmem.zmode.updateenable)
    SetPixelDepth(GetDepthOffset(x, y), depth);
}

u32 GetColor(u16 x, u16 y)
{
  u32 offset = GetColorOffset(x, y);
  return GetPixelColor(offset);
}

// For internal used only, return a non-normalized value, which saves work later.
yuv444 GetColorYUV(u16 x, u16 y)
{
  const u32 color = GetColor(x, y);
  const u8 red = static_cast<u8>(color >> 24);
  const u8 green = static_cast<u8>(color >> 16);
  const u8 blue = static_cast<u8>(color >> 8);

  // GameCube/Wii uses the BT.601 standard algorithm for converting to YCbCr; see
  // http://www.equasys.de/colorconversion.html#YCbCr-RGBColorFormatConversion
  return {static_cast<u8>(0.257f * red + 0.504f * green + 0.098f * blue),
          static_cast<s8>(-0.148f * red + -0.291f * green + 0.439f * blue),
          static_cast<s8>(0.439f * red + -0.368f * green + -0.071f * blue)};
}

u32 GetDepth(u16 x, u16 y)
{
  u32 offset = GetDepthOffset(x, y);
  return GetPixelDepth(offset);
}

u8* GetPixelPointer(u16 x, u16 y, bool depth)
{
  if (depth)
    return &efb[GetDepthOffset(x, y)];
  return &efb[GetColorOffset(x, y)];
}

void CopyToXFB(yuv422_packed* xfb_in_ram, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc,
               float Gamma)
{
  // FIXME: We should do Gamma correction

  if (!xfb_in_ram)
  {
    WARN_LOG(VIDEO, "Tried to copy to invalid XFB address");
    return;
  }

  int left = sourceRc.left;
  int right = sourceRc.right;

  // this assumes copies will always start on an even (YU) pixel and the
  // copy always has an even width, which might not be true.
  if (left & 1 || right & 1)
  {
    WARN_LOG(VIDEO, "Trying to copy XFB to from unaligned EFB source");
    // this will show up as wrongly encoded
  }

  // Scanline buffer, leave room for borders
  yuv444 scanline[EFB_WIDTH + 2];

  // our internal yuv444 type is not normalized, so black is {0, 0, 0} instead of {16, 128, 128}
  yuv444 black;
  black.Y = 0;
  black.U = 0;
  black.V = 0;

  scanline[0] = black;          // black border at start
  scanline[right + 1] = black;  // black border at end

  for (u16 y = sourceRc.top; y < sourceRc.bottom; y++)
  {
    // Get a scanline of YUV pixels in 4:4:4 format

    for (int i = 1, x = left; x < right; i++, x++)
    {
      scanline[i] = GetColorYUV(x, y);
    }

    // And Downsample them to 4:2:2
    for (int i = 1, x = left; x < right; i += 2, x += 2)
    {
      // YU pixel
      xfb_in_ram[x].Y = scanline[i].Y + 16;
      // we mix our color differences in 10 bit space so it will round more accurately
      // U[i] = 1/4 * U[i-1] + 1/2 * U[i] + 1/4 * U[i+1]
      xfb_in_ram[x].UV =
          128 + ((scanline[i - 1].U + (scanline[i].U << 1) + scanline[i + 1].U) >> 2);

      // YV pixel
      xfb_in_ram[x + 1].Y = scanline[i + 1].Y + 16;
      // V[i] = 1/4 * V[i-1] + 1/2 * V[i] + 1/4 * V[i+1]
      xfb_in_ram[x + 1].UV =
          128 + ((scanline[i].V + (scanline[i + 1].V << 1) + scanline[i + 2].V) >> 2);
    }
    xfb_in_ram += fbWidth;
  }
}

// Like CopyToXFB, but we copy directly into the OpenGL color texture without going via GameCube
// main memory or doing a yuyv conversion
void BypassXFB(u8* texture, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc, float Gamma)
{
  if (fbWidth * fbHeight > MAX_XFB_WIDTH * MAX_XFB_HEIGHT)
  {
    ERROR_LOG(VIDEO, "Framebuffer is too large: %ix%i", fbWidth, fbHeight);
    return;
  }

  size_t textureAddress = 0;
  const int left = sourceRc.left;
  const int right = sourceRc.right;

  for (u16 y = sourceRc.top; y < sourceRc.bottom; y++)
  {
    for (u16 x = left; x < right; x++)
    {
      const u32 color = Common::swap32(GetColor(x, y) | 0xFF);

      std::memcpy(&texture[textureAddress], &color, sizeof(u32));
      textureAddress += sizeof(u32);
    }
  }
}

bool ZCompare(u16 x, u16 y, u32 z)
{
  u32 offset = GetDepthOffset(x, y);
  u32 depth = GetPixelDepth(offset);

  bool pass;

  switch (bpmem.zmode.func)
  {
  case ZMode::NEVER:
    pass = false;
    break;
  case ZMode::LESS:
    pass = z < depth;
    break;
  case ZMode::EQUAL:
    pass = z == depth;
    break;
  case ZMode::LEQUAL:
    pass = z <= depth;
    break;
  case ZMode::GREATER:
    pass = z > depth;
    break;
  case ZMode::NEQUAL:
    pass = z != depth;
    break;
  case ZMode::GEQUAL:
    pass = z >= depth;
    break;
  case ZMode::ALWAYS:
    pass = true;
    break;
  default:
    pass = false;
    ERROR_LOG(VIDEO, "Bad Z compare mode %i", (int)bpmem.zmode.func);
  }

  if (pass && bpmem.zmode.updateenable)
  {
    SetPixelDepth(offset, z);
  }

  return pass;
}
}
