// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BPFunctions.h"

#include <algorithm>
#include <string_view>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

namespace BPFunctions
{
// ----------------------------------------------
// State translation lookup tables
// Reference: Yet Another GameCube Documentation
// ----------------------------------------------

void FlushPipeline()
{
  g_vertex_manager->Flush();
}

void SetGenerationMode()
{
  g_vertex_manager->SetRasterizationStateChanged();
}

void SetScissor()
{
  /* NOTE: the minimum value here for the scissor rect is -342.
   * GX SDK functions internally add an offset of 342 to scissor coords to
   * ensure that the register was always unsigned.
   *
   * The code that was here before tried to "undo" this offset, but
   * since we always take the difference, the +342 added to both
   * sides cancels out. */

  /* NOTE: With a positive scissor offset, the scissor rect is shifted left and/or up;
   * With a negative scissor offset, the scissor rect is shifted right and/or down.
   *
   * GX SDK functions internally add an offset of 342 to scissor offset.
   * The scissor offset is always even, so to save space, the scissor offset register
   * is scaled down by 2. So, if somebody calls GX_SetScissorBoxOffset(20, 20);
   * the registers will be set to ((20 + 342) / 2 = 181, 181).
   *
   * The scissor offset register is 10bit signed [-512, 511].
   * e.g. In Super Mario Galaxy 1 and 2, during the "Boss roar effect",
   * for a scissor offset of (0, -464), the scissor offset register will be set to
   * (171, (-464 + 342) / 2 = -61).
   */
  s32 xoff = bpmem.scissorOffset.x * 2;
  s32 yoff = bpmem.scissorOffset.y * 2;

  MathUtil::Rectangle<int> native_rc(bpmem.scissorTL.x - xoff, bpmem.scissorTL.y - yoff,
                                     bpmem.scissorBR.x - xoff + 1, bpmem.scissorBR.y - yoff + 1);
  native_rc.ClampUL(0, 0, EFB_WIDTH, EFB_HEIGHT);

  auto target_rc = g_renderer->ConvertEFBRectangle(native_rc);
  auto converted_rc =
      g_renderer->ConvertFramebufferRectangle(target_rc, g_renderer->GetCurrentFramebuffer());
  g_renderer->SetScissorRect(converted_rc);
}

void SetViewport()
{
  s32 xoff = bpmem.scissorOffset.x * 2;
  s32 yoff = bpmem.scissorOffset.y * 2;
  float x = g_renderer->EFBToScaledXf(xfmem.viewport.xOrig - xfmem.viewport.wd - xoff);
  float y = g_renderer->EFBToScaledYf(xfmem.viewport.yOrig + xfmem.viewport.ht - yoff);

  float width = g_renderer->EFBToScaledXf(2.0f * xfmem.viewport.wd);
  float height = g_renderer->EFBToScaledYf(-2.0f * xfmem.viewport.ht);
  float min_depth = (xfmem.viewport.farZ - xfmem.viewport.zRange) / 16777216.0f;
  float max_depth = xfmem.viewport.farZ / 16777216.0f;
  if (width < 0.f)
  {
    x += width;
    width *= -1;
  }
  if (height < 0.f)
  {
    y += height;
    height *= -1;
  }

  // The maximum depth that is written to the depth buffer should never exceed this value.
  // This is necessary because we use a 2^24 divisor for all our depth values to prevent
  // floating-point round-trip errors. However the console GPU doesn't ever write a value
  // to the depth buffer that exceeds 2^24 - 1.
  constexpr float GX_MAX_DEPTH = 16777215.0f / 16777216.0f;
  if (!g_ActiveConfig.backend_info.bSupportsDepthClamp)
  {
    // There's no way to support oversized depth ranges in this situation. Let's just clamp the
    // range to the maximum value supported by the console GPU and hope for the best.
    min_depth = std::clamp(min_depth, 0.0f, GX_MAX_DEPTH);
    max_depth = std::clamp(max_depth, 0.0f, GX_MAX_DEPTH);
  }

  if (g_renderer->UseVertexDepthRange())
  {
    // We need to ensure depth values are clamped the maximum value supported by the console GPU.
    // Taking into account whether the depth range is inverted or not.
    if (xfmem.viewport.zRange < 0.0f && g_ActiveConfig.backend_info.bSupportsReversedDepthRange)
    {
      min_depth = GX_MAX_DEPTH;
      max_depth = 0.0f;
    }
    else
    {
      min_depth = 0.0f;
      max_depth = GX_MAX_DEPTH;
    }
  }

  float near_depth, far_depth;
  if (g_ActiveConfig.backend_info.bSupportsReversedDepthRange)
  {
    // Set the reversed depth range.
    near_depth = max_depth;
    far_depth = min_depth;
  }
  else
  {
    // We use an inverted depth range here to apply the Reverse Z trick.
    // This trick makes sure we match the precision provided by the 1:0
    // clipping depth range on the hardware.
    near_depth = 1.0f - max_depth;
    far_depth = 1.0f - min_depth;
  }

  // Clamp to size if oversized not supported. Required for D3D.
  if (!g_ActiveConfig.backend_info.bSupportsOversizedViewports)
  {
    const float max_width = static_cast<float>(g_renderer->GetCurrentFramebuffer()->GetWidth());
    const float max_height = static_cast<float>(g_renderer->GetCurrentFramebuffer()->GetHeight());
    x = std::clamp(x, 0.0f, max_width - 1.0f);
    y = std::clamp(y, 0.0f, max_height - 1.0f);
    width = std::clamp(width, 1.0f, max_width - x);
    height = std::clamp(height, 1.0f, max_height - y);
  }

  // Lower-left flip.
  if (g_ActiveConfig.backend_info.bUsesLowerLeftOrigin)
    y = static_cast<float>(g_renderer->GetCurrentFramebuffer()->GetHeight()) - y - height;

  g_renderer->SetViewport(x, y, width, height, near_depth, far_depth);
}

void SetDepthMode()
{
  g_vertex_manager->SetDepthStateChanged();
}

void SetBlendMode()
{
  g_vertex_manager->SetBlendingStateChanged();
}

/* Explanation of the magic behind ClearScreen:
  There's numerous possible formats for the pixel data in the EFB.
  However, in the HW accelerated backends we're always using RGBA8
  for the EFB format, which causes some problems:
  - We're using an alpha channel although the game doesn't
  - If the actual EFB format is RGBA6_Z24 or R5G6B5_Z16, we are using more bits per channel than the
  native HW

  To properly emulate the above points, we're doing the following:
  (1)
    - disable alpha channel writing of any kind of rendering if the actual EFB format doesn't use an
  alpha channel
    - NOTE: Always make sure that the EFB has been cleared to an alpha value of 0xFF in this case!
    - Same for color channels, these need to be cleared to 0x00 though.
  (2)
    - convert the RGBA8 color to RGBA6/RGB8/RGB565 and convert it to RGBA8 again
    - convert the Z24 depth value to Z16 and back to Z24
*/
void ClearScreen(const MathUtil::Rectangle<int>& rc)
{
  bool colorEnable = (bpmem.blendmode.colorupdate != 0);
  bool alphaEnable = (bpmem.blendmode.alphaupdate != 0);
  bool zEnable = (bpmem.zmode.updateenable != 0);
  auto pixel_format = bpmem.zcontrol.pixel_format;

  // (1): Disable unused color channels
  if (pixel_format == PixelFormat::RGB8_Z24 || pixel_format == PixelFormat::RGB565_Z16 ||
      pixel_format == PixelFormat::Z24)
  {
    alphaEnable = false;
  }

  if (colorEnable || alphaEnable || zEnable)
  {
    u32 color = (bpmem.clearcolorAR << 16) | bpmem.clearcolorGB;
    u32 z = bpmem.clearZValue;

    // (2) drop additional accuracy
    if (pixel_format == PixelFormat::RGBA6_Z24)
    {
      color = RGBA8ToRGBA6ToRGBA8(color);
    }
    else if (pixel_format == PixelFormat::RGB565_Z16)
    {
      color = RGBA8ToRGB565ToRGBA8(color);
      z = Z24ToZ16ToZ24(z);
    }
    g_renderer->ClearScreen(rc, colorEnable, alphaEnable, zEnable, color, z);
  }
}

void OnPixelFormatChange()
{
  // TODO : Check for Z compression format change
  // When using 16bit Z, the game may enable a special compression format which we might need to
  // handle. Only a few games like RS2 and RS3 even use z compression but it looks like they
  // always use ZFAR when using 16bit Z (on top of linear 24bit Z)

  // Besides, we currently don't even emulate 16bit depth and force it to 24bit.

  /*
   * When changing the EFB format, the pixel data won't get converted to the new format but stays
   * the same.
   * Since we are always using an RGBA8 buffer though, this causes issues in some games.
   * Thus, we reinterpret the old EFB data with the new format here.
   */
  if (!g_ActiveConfig.bEFBEmulateFormatChanges)
    return;

  const auto old_format = g_renderer->GetPrevPixelFormat();
  const auto new_format = bpmem.zcontrol.pixel_format;
  g_renderer->StorePixelFormat(new_format);

  DEBUG_LOG_FMT(VIDEO, "pixelfmt: pixel={}, zc={}", new_format, bpmem.zcontrol.zformat);

  // no need to reinterpret pixel data in these cases
  if (new_format == old_format || old_format == PixelFormat::INVALID_FMT)
    return;

  // Check for pixel format changes
  switch (old_format)
  {
  case PixelFormat::RGB8_Z24:
  case PixelFormat::Z24:
  {
    // Z24 and RGB8_Z24 are treated equal, so just return in this case
    if (new_format == PixelFormat::RGB8_Z24 || new_format == PixelFormat::Z24)
      return;

    if (new_format == PixelFormat::RGBA6_Z24)
    {
      g_renderer->ReinterpretPixelData(EFBReinterpretType::RGB8ToRGBA6);
      return;
    }
    else if (new_format == PixelFormat::RGB565_Z16)
    {
      g_renderer->ReinterpretPixelData(EFBReinterpretType::RGB8ToRGB565);
      return;
    }
  }
  break;

  case PixelFormat::RGBA6_Z24:
  {
    if (new_format == PixelFormat::RGB8_Z24 || new_format == PixelFormat::Z24)
    {
      g_renderer->ReinterpretPixelData(EFBReinterpretType::RGBA6ToRGB8);
      return;
    }
    else if (new_format == PixelFormat::RGB565_Z16)
    {
      g_renderer->ReinterpretPixelData(EFBReinterpretType::RGBA6ToRGB565);
      return;
    }
  }
  break;

  case PixelFormat::RGB565_Z16:
  {
    if (new_format == PixelFormat::RGB8_Z24 || new_format == PixelFormat::Z24)
    {
      g_renderer->ReinterpretPixelData(EFBReinterpretType::RGB565ToRGB8);
      return;
    }
    else if (new_format == PixelFormat::RGBA6_Z24)
    {
      g_renderer->ReinterpretPixelData(EFBReinterpretType::RGB565ToRGBA6);
      return;
    }
  }
  break;

  default:
    break;
  }

  ERROR_LOG_FMT(VIDEO, "Unhandled EFB format change: {} to {}", old_format, new_format);
}

void SetInterlacingMode(const BPCmd& bp)
{
  // TODO
  switch (bp.address)
  {
  case BPMEM_FIELDMODE:
  {
    // SDK always sets bpmem.lineptwidth.lineaspect via BPMEM_LINEPTWIDTH
    // just before this cmd
    DEBUG_LOG_FMT(VIDEO, "BPMEM_FIELDMODE texLOD:{} lineaspect:{}", bpmem.fieldmode.texLOD,
                  bpmem.lineptwidth.adjust_for_aspect_ratio);
  }
  break;
  case BPMEM_FIELDMASK:
  {
    // Determines if fields will be written to EFB (always computed)
    DEBUG_LOG_FMT(VIDEO, "BPMEM_FIELDMASK even:{} odd:{}", bpmem.fieldmask.even,
                  bpmem.fieldmask.odd);
  }
  break;
  default:
    ERROR_LOG_FMT(VIDEO, "SetInterlacingMode default");
    break;
  }
}
};  // namespace BPFunctions
