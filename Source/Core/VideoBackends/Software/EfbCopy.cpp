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

namespace EfbCopy
{
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
}  // namespace EfbCopy
