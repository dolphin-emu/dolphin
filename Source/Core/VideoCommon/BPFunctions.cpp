// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

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
  g_renderer->SetGenerationMode();
}

void SetScissor()
{
  /* NOTE: the minimum value here for the scissor rect and offset is -342.
   * GX internally adds on an offset of 342 to both the offset and scissor
   * coords to ensure that the register was always unsigned.
   *
   * The code that was here before tried to "undo" this offset, but
   * since we always take the difference, the +342 added to both
   * sides cancels out. */

  /* The scissor offset is always even, so to save space, the scissor offset
   * register is scaled down by 2. So, if somebody calls
   * GX_SetScissorBoxOffset(20, 20); the registers will be set to 10, 10. */
  const int xoff = bpmem.scissorOffset.x * 2;
  const int yoff = bpmem.scissorOffset.y * 2;

  EFBRectangle rc(bpmem.scissorTL.x - xoff, bpmem.scissorTL.y - yoff, bpmem.scissorBR.x - xoff + 1,
                  bpmem.scissorBR.y - yoff + 1);

  if (rc.left < 0)
    rc.left = 0;
  if (rc.top < 0)
    rc.top = 0;
  if (rc.right > EFB_WIDTH)
    rc.right = EFB_WIDTH;
  if (rc.bottom > EFB_HEIGHT)
    rc.bottom = EFB_HEIGHT;

  if (rc.left > rc.right)
    rc.right = rc.left;
  if (rc.top > rc.bottom)
    rc.bottom = rc.top;

  g_renderer->SetScissorRect(rc);
}

void SetDepthMode()
{
  g_renderer->SetDepthMode();
}

void SetBlendMode()
{
  g_renderer->SetBlendMode(false);
}

void SetLogicOpMode()
{
  g_renderer->SetLogicOpMode();
}

void SetColorMask()
{
  g_renderer->SetColorMask();
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
void ClearScreen(const EFBRectangle& rc)
{
  bool colorEnable = (bpmem.blendmode.colorupdate != 0);
  bool alphaEnable = (bpmem.blendmode.alphaupdate != 0);
  bool zEnable = (bpmem.zmode.updateenable != 0);
  auto pixel_format = bpmem.zcontrol.pixel_format;

  // (1): Disable unused color channels
  if (pixel_format == PEControl::RGB8_Z24 || pixel_format == PEControl::RGB565_Z16 ||
      pixel_format == PEControl::Z24)
  {
    alphaEnable = false;
  }

  if (colorEnable || alphaEnable || zEnable)
  {
    u32 color = (bpmem.clearcolorAR << 16) | bpmem.clearcolorGB;
    u32 z = bpmem.clearZValue;

    // (2) drop additional accuracy
    if (pixel_format == PEControl::RGBA6_Z24)
    {
      color = RGBA8ToRGBA6ToRGBA8(color);
    }
    else if (pixel_format == PEControl::RGB565_Z16)
    {
      color = RGBA8ToRGB565ToRGBA8(color);
      z = Z24ToZ16ToZ24(z);
    }
    g_renderer->ClearScreen(rc, colorEnable, alphaEnable, zEnable, color, z);
  }
}

void OnPixelFormatChange()
{
  int convtype = -1;

  // TODO : Check for Z compression format change
  // When using 16bit Z, the game may enable a special compression format which we need to handle
  // If we don't, Z values will be completely screwed up, currently only Star Wars:RS2 uses that.

  /*
   * When changing the EFB format, the pixel data won't get converted to the new format but stays
   * the same.
   * Since we are always using an RGBA8 buffer though, this causes issues in some games.
   * Thus, we reinterpret the old EFB data with the new format here.
   */
  if (!g_ActiveConfig.bEFBEmulateFormatChanges)
    return;

  auto old_format = g_renderer->GetPrevPixelFormat();
  auto new_format = bpmem.zcontrol.pixel_format;

  // no need to reinterpret pixel data in these cases
  if (new_format == old_format || old_format == PEControl::INVALID_FMT)
    goto skip;

  // Check for pixel format changes
  switch (old_format)
  {
  case PEControl::RGB8_Z24:
  case PEControl::Z24:
    // Z24 and RGB8_Z24 are treated equal, so just return in this case
    if (new_format == PEControl::RGB8_Z24 || new_format == PEControl::Z24)
      goto skip;

    if (new_format == PEControl::RGBA6_Z24)
      convtype = 0;
    else if (new_format == PEControl::RGB565_Z16)
      convtype = 1;
    break;

  case PEControl::RGBA6_Z24:
    if (new_format == PEControl::RGB8_Z24 || new_format == PEControl::Z24)
      convtype = 2;
    else if (new_format == PEControl::RGB565_Z16)
      convtype = 3;
    break;

  case PEControl::RGB565_Z16:
    if (new_format == PEControl::RGB8_Z24 || new_format == PEControl::Z24)
      convtype = 4;
    else if (new_format == PEControl::RGBA6_Z24)
      convtype = 5;
    break;

  default:
    break;
  }

  if (convtype == -1)
  {
    ERROR_LOG(VIDEO, "Unhandled EFB format change: %d to %d", static_cast<int>(old_format),
              static_cast<int>(new_format));
    goto skip;
  }

  g_renderer->ReinterpretPixelData(convtype);

skip:
  DEBUG_LOG(VIDEO, "pixelfmt: pixel=%d, zc=%d", static_cast<int>(new_format),
            static_cast<int>(bpmem.zcontrol.zformat));

  g_renderer->StorePixelFormat(new_format);
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
    const char* action[] = {"don't adjust", "adjust"};
    DEBUG_LOG(VIDEO, "BPMEM_FIELDMODE texLOD:%s lineaspect:%s", action[bpmem.fieldmode.texLOD],
              action[bpmem.lineptwidth.lineaspect]);
  }
  break;
  case BPMEM_FIELDMASK:
  {
    // Determines if fields will be written to EFB (always computed)
    const char* action[] = {"skip", "write"};
    DEBUG_LOG(VIDEO, "BPMEM_FIELDMASK even:%s odd:%s", action[bpmem.fieldmask.even],
              action[bpmem.fieldmask.odd]);
  }
  break;
  default:
    ERROR_LOG(VIDEO, "SetInterlacingMode default");
    break;
  }
}
};
