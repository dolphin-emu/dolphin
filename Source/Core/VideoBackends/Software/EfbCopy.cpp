// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Software/EfbCopy.h"

#include <algorithm>

#include "Common/CommonTypes.h"
#include "VideoBackends/Software/EfbInterface.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/VideoCommon.h"

namespace EfbCopy
{
void ClearEfb()
{
  u32 clearColor = (bpmem.clearcolorAR & 0xff) << 24 | bpmem.clearcolorGB << 8 |
                   (bpmem.clearcolorAR & 0xff00) >> 8;

  const int left = bpmem.copyTexSrcXY.x;
  const int top = bpmem.copyTexSrcXY.y;
  const int right = std::min(left + bpmem.copyTexSrcWH.x, EFB_WIDTH - 1);
  const int bottom = std::min(top + bpmem.copyTexSrcWH.y, EFB_HEIGHT - 1);

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
