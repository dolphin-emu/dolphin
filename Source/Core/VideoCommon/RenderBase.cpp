// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// ---------------------------------------------------------------------------------------------
// GC graphics pipeline
// ---------------------------------------------------------------------------------------------
// 3d commands are issued through the fifo. The GPU draws to the 2MB EFB.
// The efb can be copied back into ram in two forms: as textures or as XFB.
// The XFB is the region in RAM that the VI chip scans out to the television.
// So, after all rendering to EFB is done, the image is copied into one of two XFBs in RAM.
// Next frame, that one is scanned out and the other one gets the copy. = double buffering.
// ---------------------------------------------------------------------------------------------

#include "VideoCommon/RenderBase.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <tuple>

#include <fmt/format.h>

#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "Core/ConfigManager.h"
#include "Core/System.h"

#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

std::unique_ptr<Renderer> g_renderer;

Renderer::~Renderer() = default;

void Renderer::ReinterpretPixelData(EFBReinterpretType convtype)
{
  g_framebuffer_manager->ReinterpretPixelData(convtype);
}

u32 Renderer::AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data)
{
  if (type == EFBAccessType::PeekColor)
  {
    u32 color = g_framebuffer_manager->PeekEFBColor(x, y);

    // a little-endian value is expected to be returned
    color = ((color & 0xFF00FF00) | ((color >> 16) & 0xFF) | ((color << 16) & 0xFF0000));

    if (bpmem.zcontrol.pixel_format == PixelFormat::RGBA6_Z24)
    {
      color = RGBA8ToRGBA6ToRGBA8(color);
    }
    else if (bpmem.zcontrol.pixel_format == PixelFormat::RGB565_Z16)
    {
      color = RGBA8ToRGB565ToRGBA8(color);
    }
    if (bpmem.zcontrol.pixel_format != PixelFormat::RGBA6_Z24)
    {
      color |= 0xFF000000;
    }

    // check what to do with the alpha channel (GX_PokeAlphaRead)
    PixelEngine::AlphaReadMode alpha_read_mode =
        Core::System::GetInstance().GetPixelEngine().GetAlphaReadMode();

    if (alpha_read_mode == PixelEngine::AlphaReadMode::ReadNone)
    {
      return color;
    }
    else if (alpha_read_mode == PixelEngine::AlphaReadMode::ReadFF)
    {
      return color | 0xFF000000;
    }
    else
    {
      if (alpha_read_mode != PixelEngine::AlphaReadMode::Read00)
      {
        PanicAlertFmt("Invalid PE alpha read mode: {}", static_cast<u16>(alpha_read_mode));
      }
      return color & 0x00FFFFFF;
    }
  }
  else  // if (type == EFBAccessType::PeekZ)
  {
    // Depth buffer is inverted for improved precision near far plane
    float depth = g_framebuffer_manager->PeekEFBDepth(x, y);
    if (!g_ActiveConfig.backend_info.bSupportsReversedDepthRange)
      depth = 1.0f - depth;

    // Convert to 24bit depth
    u32 z24depth = std::clamp<u32>(static_cast<u32>(depth * 16777216.0f), 0, 0xFFFFFF);

    if (bpmem.zcontrol.pixel_format == PixelFormat::RGB565_Z16)
    {
      // When in RGB565_Z16 mode, EFB Z peeks return a 16bit value, which is presumably a
      // resolved sample from the MSAA buffer.
      // Dolphin doesn't currently emulate the 3 sample MSAA mode (and potentially never will)
      // it just transparently upgrades the framebuffer to 24bit depth and color and whatever
      // level of MSAA and higher Internal Resolution the user has configured.

      // This is mostly transparent, unless the game does an EFB read.
      // But we can simply convert the 24bit depth on the fly to the 16bit depth the game expects.

      return CompressZ16(z24depth, bpmem.zcontrol.zformat);
    }

    return z24depth;
  }
}

void Renderer::PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points)
{
  if (type == EFBAccessType::PokeColor)
  {
    for (size_t i = 0; i < num_points; i++)
    {
      // Convert to expected format (BGRA->RGBA)
      // TODO: Check alpha, depending on mode?
      const EfbPokeData& point = points[i];
      u32 color = ((point.data & 0xFF00FF00) | ((point.data >> 16) & 0xFF) |
                   ((point.data << 16) & 0xFF0000));
      g_framebuffer_manager->PokeEFBColor(point.x, point.y, color);
    }
  }
  else  // if (type == EFBAccessType::PokeZ)
  {
    for (size_t i = 0; i < num_points; i++)
    {
      // Convert to floating-point depth.
      const EfbPokeData& point = points[i];
      float depth = float(point.data & 0xFFFFFF) / 16777216.0f;
      if (!g_ActiveConfig.backend_info.bSupportsReversedDepthRange)
        depth = 1.0f - depth;

      g_framebuffer_manager->PokeEFBDepth(point.x, point.y, depth);
    }
  }
}
