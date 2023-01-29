// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Software/SWRenderer.h"

#include <string>

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"

#include "Core/HW/Memmap.h"
#include "Core/System.h"

#include "VideoBackends/Software/EfbInterface.h"

#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/VideoBackendBase.h"

namespace SW
{
u32 SWRenderer::AccessEFB(EFBAccessType type, u32 x, u32 y, u32 InputData)
{
  u32 value = 0;

  switch (type)
  {
  case EFBAccessType::PeekZ:
  {
    value = EfbInterface::GetDepth(x, y);
    break;
  }
  case EFBAccessType::PeekColor:
  {
    const u32 color = EfbInterface::GetColor(x, y);

    // rgba to argb
    value = (color >> 8) | (color & 0xff) << 24;

    // check what to do with the alpha channel (GX_PokeAlphaRead)
    PixelEngine::AlphaReadMode alpha_read_mode =
        Core::System::GetInstance().GetPixelEngine().GetAlphaReadMode();

    if (alpha_read_mode == PixelEngine::AlphaReadMode::ReadNone)
    {
      // value is OK as it is
    }
    else if (alpha_read_mode == PixelEngine::AlphaReadMode::ReadFF)
    {
      value |= 0xFF000000;
    }
    else
    {
      if (alpha_read_mode != PixelEngine::AlphaReadMode::Read00)
      {
        PanicAlertFmt("Invalid PE alpha read mode: {}", static_cast<u16>(alpha_read_mode));
      }
      value &= 0x00FFFFFF;
    }

    break;
  }
  default:
    break;
  }

  return value;
}

}  // namespace SW
