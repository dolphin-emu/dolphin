// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Software/EfbCopy.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/HW/Memmap.h"
#include "VideoBackends/Software/EfbInterface.h"
#include "VideoBackends/Software/TextureEncoder.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/Fifo.h"

static const float s_gammaLUT[] = {1.0f, 1.7f, 2.2f, 1.0f};

namespace EfbCopy
{
static void CopyToXfb(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc,
                      float Gamma)
{
  DEBUG_LOG(VIDEO, "xfbaddr: %x, fbwidth: %i, fbheight: %i, source: (%i, %i, %i, %i), Gamma %f",
            xfbAddr, fbWidth, fbHeight, sourceRc.top, sourceRc.left, sourceRc.bottom,
            sourceRc.right, Gamma);

  EfbInterface::yuv422_packed* xfb_in_ram =
      (EfbInterface::yuv422_packed*)Memory::GetPointer(xfbAddr);

  EfbInterface::CopyToXFB(xfb_in_ram, fbWidth, fbHeight, sourceRc, Gamma);
}

static void CopyToRam()
{
  u8* dest_ptr = Memory::GetPointer(bpmem.copyTexDest << 5);

  TextureEncoder::Encode(dest_ptr);
}

void ClearEfb()
{
  u32 clearColor = (bpmem.clearcolorAR & 0xff) << 24 | bpmem.clearcolorGB << 8 |
                   (bpmem.clearcolorAR & 0xff00) >> 8;

  int left = bpmem.copyTexSrcXY.x;
  int top = bpmem.copyTexSrcXY.y;
  int right = left + bpmem.copyTexSrcWH.x;
  int bottom = top + bpmem.copyTexSrcWH.y;

  for (u16 y = top; y <= bottom; y++)
  {
    for (u16 x = left; x <= right; x++)
    {
      EfbInterface::SetColor(x, y, (u8*)(&clearColor));
      EfbInterface::SetDepth(x, y, bpmem.clearZValue);
    }
  }
}

void CopyEfb()
{
  EFBRectangle rc;
  rc.left = (int)bpmem.copyTexSrcXY.x;
  rc.top = (int)bpmem.copyTexSrcXY.y;

  // flipper represents the widths internally as last pixel minus starting pixel, so
  // these are zero indexed.
  rc.right = rc.left + (int)bpmem.copyTexSrcWH.x + 1;
  rc.bottom = rc.top + (int)bpmem.copyTexSrcWH.y + 1;

  if (bpmem.triggerEFBCopy.copy_to_xfb)
  {
    float yScale;
    if (bpmem.triggerEFBCopy.scale_invert)
      yScale = 256.0f / (float)bpmem.dispcopyyscale;
    else
      yScale = (float)bpmem.dispcopyyscale / 256.0f;

    float xfbLines = ((bpmem.copyTexSrcWH.y + 1.0f) * yScale);

    if (yScale != 1.0)
      WARN_LOG(VIDEO, "yScale of %f is currently unsupported", yScale);

    if ((u32)xfbLines > MAX_XFB_HEIGHT)
    {
      INFO_LOG(VIDEO, "Tried to scale EFB to too many XFB lines (%f)", xfbLines);
      xfbLines = MAX_XFB_HEIGHT;
    }

    CopyToXfb(bpmem.copyTexDest << 5, bpmem.copyMipMapStrideChannels << 4, (u32)xfbLines, rc,
              s_gammaLUT[bpmem.triggerEFBCopy.gamma]);
  }
  else
  {
    CopyToRam();  // FIXME: should use the rectangle we have already created above
  }
}
}
