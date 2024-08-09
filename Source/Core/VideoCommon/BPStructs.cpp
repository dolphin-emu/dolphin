// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/BPStructs.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/EnumMap.h"
#include "Common/Logging/Log.h"

#include "Core/CoreTiming.h"
#include "Core/DolphinAnalytics.h"
#include "Core/FifoPlayer/FifoPlayer.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/VideoInterface.h"
#include "Core/System.h"
#include "Core/NetPlayClient.h"

#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PerfQueryBase.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TMEM.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VideoEvents.h"
#include "VideoCommon/XFStateManager.h"

using namespace BPFunctions;

static constexpr Common::EnumMap<float, GammaCorrection::Invalid2_2> s_gammaLUT = {1.0f, 1.7f, 2.2f,
                                                                                   2.2f};

void BPInit()
{
  memset(reinterpret_cast<u8*>(&bpmem), 0, sizeof(bpmem));
  bpmem.bpMask = 0xFFFFFF;
}

static void BPWritten(PixelShaderManager& pixel_shader_manager, XFStateManager& xf_state_manager,
                      GeometryShaderManager& geometry_shader_manager, const BPCmd& bp,
                      int cycles_into_future)
{
  /*
  ----------------------------------------------------------------------------------------------------------------
  Purpose: Writes to the BP registers
  Called: At the end of every: OpcodeDecoding.cpp ExecuteDisplayList > Decode() > LoadBPReg
  How It Works: First the pipeline is flushed then update the bpmem with the new value.
          Some of the BP cases have to call certain functions while others just update the bpmem.
          some bp cases check the changes variable, because they might not have to be updated all
  the time
  NOTE: it seems not all bp cases like checking changes, so calling if (bp.changes == 0 ? false :
  true)
      had to be ditched and the games seem to work fine with out it.
  NOTE2: Yet Another GameCube Documentation calls them Bypass Raster State Registers but possibly
  completely wrong
  NOTE3: This controls the register groups: RAS1/2, SU, TF, TEV, C/Z, PEC
  TODO: Turn into function table. The (future) DisplayList (DL) jit can then call the functions
  directly,
      getting rid of dynamic dispatch. Unfortunately, few games use DLs properly - most\
      just stuff geometry in them and don't put state changes there
  ----------------------------------------------------------------------------------------------------------------
  */

  if (((s32*)&bpmem)[bp.address] == bp.newvalue)
  {
    if (!(bp.address == BPMEM_TRIGGER_EFB_COPY || bp.address == BPMEM_CLEARBBOX1 ||
          bp.address == BPMEM_CLEARBBOX2 || bp.address == BPMEM_SETDRAWDONE ||
          bp.address == BPMEM_PE_TOKEN_ID || bp.address == BPMEM_PE_TOKEN_INT_ID ||
          bp.address == BPMEM_LOADTLUT0 || bp.address == BPMEM_LOADTLUT1 ||
          bp.address == BPMEM_TEXINVALIDATE || bp.address == BPMEM_PRELOAD_MODE ||
          bp.address == BPMEM_CLEAR_PIXEL_PERF))
    {
      return;
    }
  }

  FlushPipeline();

  ((u32*)&bpmem)[bp.address] = bp.newvalue;

  switch (bp.address)
  {
  case BPMEM_GENMODE:  // Set the Generation Mode
    PRIM_LOG("genmode: texgen={}, col={}, multisampling={}, tev={}, cullmode={}, ind={}, zfeeze={}",
             bpmem.genMode.numtexgens, bpmem.genMode.numcolchans, bpmem.genMode.multisampling,
             bpmem.genMode.numtevstages + 1, bpmem.genMode.cullmode, bpmem.genMode.numindstages,
             bpmem.genMode.zfreeze);

    if (bp.changes)
      pixel_shader_manager.SetGenModeChanged();

    // Only call SetGenerationMode when cull mode changes.
    if (bp.changes & 0xC000)
      SetGenerationMode();
    return;
  case BPMEM_IND_MTXA:  // Index Matrix Changed
  case BPMEM_IND_MTXB:
  case BPMEM_IND_MTXC:
  case BPMEM_IND_MTXA + 3:
  case BPMEM_IND_MTXB + 3:
  case BPMEM_IND_MTXC + 3:
  case BPMEM_IND_MTXA + 6:
  case BPMEM_IND_MTXB + 6:
  case BPMEM_IND_MTXC + 6:
    if (bp.changes)
      pixel_shader_manager.SetIndMatrixChanged((bp.address - BPMEM_IND_MTXA) / 3);
    return;
  case BPMEM_RAS1_SS0:  // Index Texture Coordinate Scale 0
    if (bp.changes)
      pixel_shader_manager.SetIndTexScaleChanged(false);
    return;
  case BPMEM_RAS1_SS1:  // Index Texture Coordinate Scale 1
    if (bp.changes)
      pixel_shader_manager.SetIndTexScaleChanged(true);
    return;
  // ----------------
  // Scissor Control
  // ----------------
  case BPMEM_SCISSORTL:      // Scissor Rectable Top, Left
  case BPMEM_SCISSORBR:      // Scissor Rectable Bottom, Right
  case BPMEM_SCISSOROFFSET:  // Scissor Offset
    xf_state_manager.SetViewportChanged();
    geometry_shader_manager.SetViewportChanged();
    return;
  case BPMEM_LINEPTWIDTH:  // Line Width
    geometry_shader_manager.SetLinePtWidthChanged();
    return;
  case BPMEM_ZMODE:  // Depth Control
    PRIM_LOG("zmode: test={}, func={}, upd={}", bpmem.zmode.testenable, bpmem.zmode.func,
             bpmem.zmode.updateenable);
    SetDepthMode();
    pixel_shader_manager.SetZModeControl();
    return;
  case BPMEM_BLENDMODE:  // Blending Control
    if (bp.changes & 0xFFFF)
    {
      PRIM_LOG("blendmode: en={}, open={}, colupd={}, alphaupd={}, dst={}, src={}, sub={}, mode={}",
               bpmem.blendmode.blendenable, bpmem.blendmode.logicopenable,
               bpmem.blendmode.colorupdate, bpmem.blendmode.alphaupdate, bpmem.blendmode.dstfactor,
               bpmem.blendmode.srcfactor, bpmem.blendmode.subtract, bpmem.blendmode.logicmode);

      SetBlendMode();

      pixel_shader_manager.SetBlendModeChanged();
    }
    return;
  case BPMEM_CONSTANTALPHA:  // Set Destination Alpha
    PRIM_LOG("constalpha: alp={}, en={}", bpmem.dstalpha.alpha, bpmem.dstalpha.enable);
    if (bp.changes)
    {
      pixel_shader_manager.SetAlpha();
      pixel_shader_manager.SetDestAlphaChanged();
    }
    if (bp.changes & 0x100)
      SetBlendMode();
    return;

  // This is called when the game is done drawing the new frame (eg: like in DX: Begin(); Draw();
  // End();)
  // Triggers an interrupt on the PPC side so that the game knows when the GPU has finished drawing.
  // Tokens are similar.
  case BPMEM_SETDRAWDONE:
    switch (bp.newvalue & 0xFF)
    {
    case 0x02:
    {
      INCSTAT(g_stats.this_frame.num_draw_done);
      g_texture_cache->FlushEFBCopies();
      g_texture_cache->FlushStaleBinds();
      g_framebuffer_manager->InvalidatePeekCache(false);
      g_framebuffer_manager->RefreshPeekCache();
      auto& system = Core::System::GetInstance();
      if (!system.GetFifo().UseDeterministicGPUThread())
        system.GetPixelEngine().SetFinish(cycles_into_future);  // may generate interrupt
      DEBUG_LOG_FMT(VIDEO, "GXSetDrawDone SetPEFinish (value: {:#04X})", bp.newvalue & 0xFFFF);
      return;
    }

    default:
      WARN_LOG_FMT(VIDEO, "GXSetDrawDone ??? (value {:#04X})", bp.newvalue & 0xFFFF);
      return;
    }
    return;
  case BPMEM_PE_TOKEN_ID:  // Pixel Engine Token ID
  {
    INCSTAT(g_stats.this_frame.num_token);
    g_texture_cache->FlushEFBCopies();
    g_texture_cache->FlushStaleBinds();
    g_framebuffer_manager->InvalidatePeekCache(false);
    g_framebuffer_manager->RefreshPeekCache();
    auto& system = Core::System::GetInstance();
    if (!system.GetFifo().UseDeterministicGPUThread())
    {
      system.GetPixelEngine().SetToken(static_cast<u16>(bp.newvalue & 0xFFFF), false,
                                       cycles_into_future);
    }
    DEBUG_LOG_FMT(VIDEO, "SetPEToken {:#06X}", bp.newvalue & 0xFFFF);
    return;
  }
  case BPMEM_PE_TOKEN_INT_ID:  // Pixel Engine Interrupt Token ID
  {
    INCSTAT(g_stats.this_frame.num_token_int);
    g_texture_cache->FlushEFBCopies();
    g_texture_cache->FlushStaleBinds();
    g_framebuffer_manager->InvalidatePeekCache(false);
    g_framebuffer_manager->RefreshPeekCache();
    auto& system = Core::System::GetInstance();
    if (!system.GetFifo().UseDeterministicGPUThread())
    {
      system.GetPixelEngine().SetToken(static_cast<u16>(bp.newvalue & 0xFFFF), true,
                                       cycles_into_future);
    }
    DEBUG_LOG_FMT(VIDEO, "SetPEToken + INT {:#06X}", bp.newvalue & 0xFFFF);
    return;
  }

  // ------------------------
  // EFB copy command. This copies a rectangle from the EFB to either RAM in a texture format or to
  // XFB as YUYV.
  // It can also optionally clear the EFB while copying from it. To emulate this, we of course copy
  // first and clear afterwards.
  case BPMEM_TRIGGER_EFB_COPY:  // Copy EFB Region or Render to the XFB or Clear the screen.
  {
    // The bottom right is within the rectangle
    // The values in bpmem.copyTexSrcXY and bpmem.copyTexSrcWH are updated in case 0x49 and 0x4a in
    // this function

    u32 destAddr = bpmem.copyTexDest << 5;
    u32 destStride = bpmem.copyDestStride << 5;

    MathUtil::Rectangle<s32> srcRect;
    srcRect.left = bpmem.copyTexSrcXY.x;
    srcRect.top = bpmem.copyTexSrcXY.y;

    // Here Width+1 like Height, otherwise some textures are corrupted already since the native
    // resolution.
    srcRect.right = bpmem.copyTexSrcXY.x + bpmem.copyTexSrcWH.x + 1;
    srcRect.bottom = bpmem.copyTexSrcXY.y + bpmem.copyTexSrcWH.y + 1;

    const UPE_Copy PE_copy = bpmem.triggerEFBCopy;

    // Since the copy X and Y coordinates/sizes are 10-bit, the game can configure a copy region up
    // to 1024x1024. Hardware tests have found that the number of bytes written does not depend on
    // the configured stride, instead it is based on the size registers, writing beyond the length
    // of a single row. The data written for the pixels which lie outside the EFB bounds does not
    // wrap around instead returning different colors based on the pixel format of the EFB. This
    // suggests it's not based on coordinates, but instead on memory addresses. The effect of a
    // within-bounds size but out-of-bounds offset (e.g. offset 320,0, size 640,480) are the same.

    // As it would be difficult to emulate the exact behavior of out-of-bounds reads, instead of
    // writing the junk data, we don't write anything to RAM at all for over-sized copies, and clamp
    // to the EFB borders for over-offset copies. The arcade virtual console games (e.g. 1942) are
    // known for configuring these out-of-range copies.

    if (u32(srcRect.right) > EFB_WIDTH || u32(srcRect.bottom) > EFB_HEIGHT)
    {
      WARN_LOG_FMT(VIDEO, "Oversized EFB copy: {}x{} (offset {},{} stride {})", srcRect.GetWidth(),
                   srcRect.GetHeight(), srcRect.left, srcRect.top, destStride);

      if (u32(srcRect.left) >= EFB_WIDTH || u32(srcRect.top) >= EFB_HEIGHT)
      {
        // This is not a sane src rectangle, it doesn't touch any valid image data at all
        // Just ignore it
        // Apparently Mario Kart Wii in wifi mode can generate a deformed EFB copy of size 4x4
        // at offset (328,1020)
        if (PE_copy.copy_to_xfb == 1)
        {
          // Make sure we disable Bounding box to match the side effects of the non-failure path
          g_bounding_box->Disable(pixel_shader_manager);
        }

        return;
      }

      // Clamp the copy region to fit within EFB. So that we don't end up with a stretched image.
      srcRect.right = std::clamp<int>(srcRect.right, 0, EFB_WIDTH);
      srcRect.bottom = std::clamp<int>(srcRect.bottom, 0, EFB_HEIGHT);
    }

    const u32 copy_width = srcRect.GetWidth();
    const u32 copy_height = srcRect.GetHeight();

    // Check if we are to copy from the EFB or draw to the XFB
    if (PE_copy.copy_to_xfb == 0)
    {
      // bpmem.zcontrol.pixel_format to PixelFormat::Z24 is when the game wants to copy from ZBuffer
      // (Zbuffer uses 24-bit Format)
      bool is_depth_copy = bpmem.zcontrol.pixel_format == PixelFormat::Z24;
      g_texture_cache->CopyRenderTargetToTexture(
          destAddr, PE_copy.tp_realFormat(), copy_width, copy_height, destStride, is_depth_copy,
          srcRect, PE_copy.intensity_fmt && PE_copy.auto_conv, PE_copy.half_scale, 1.0f,
          s_gammaLUT[PE_copy.gamma], bpmem.triggerEFBCopy.clamp_top,
          bpmem.triggerEFBCopy.clamp_bottom, bpmem.copyfilter.GetCoefficients());
    }
    else
    {
      // We should be able to get away with deactivating the current bbox tracking
      // here. Not sure if there's a better spot to put this.
      // the number of lines copied is determined by the y scale * source efb height
      g_bounding_box->Disable(pixel_shader_manager);

      float yScale;
      if (PE_copy.scale_invert)
        yScale = 256.0f / static_cast<float>(bpmem.dispcopyyscale);
      else
        yScale = static_cast<float>(bpmem.dispcopyyscale) / 256.0f;

      float num_xfb_lines = 1.0f + bpmem.copyTexSrcWH.y * yScale;

      u32 height = static_cast<u32>(num_xfb_lines);

      DEBUG_LOG_FMT(VIDEO,
                    "RenderToXFB: destAddr: {:08x} | srcRect [{} {} {} {}] | fbWidth: {} | "
                    "fbStride: {} | fbHeight: {} | yScale: {}",
                    destAddr, srcRect.left, srcRect.top, srcRect.right, srcRect.bottom,
                    bpmem.copyTexSrcWH.x + 1, destStride, height, yScale);

      bool is_depth_copy = bpmem.zcontrol.pixel_format == PixelFormat::Z24;
      g_texture_cache->CopyRenderTargetToTexture(
          destAddr, EFBCopyFormat::XFB, copy_width, height, destStride, is_depth_copy, srcRect,
          false, false, yScale, s_gammaLUT[PE_copy.gamma], bpmem.triggerEFBCopy.clamp_top,
          bpmem.triggerEFBCopy.clamp_bottom, bpmem.copyfilter.GetCoefficients());

      // This is as closest as we have to an "end of the frame"
      // It works 99% of the time.
      // But sometimes games want to render an XFB larger than the EFB's 640x528 pixel resolution
      // (especially when using the 3xMSAA mode, which cuts EFB resolution to 640x264). So they
      // render multiple sub-frames and arrange the XFB copies in next to each-other in main memory
      // so they form a single completed XFB.
      // See https://dolphin-emu.org/blog/2017/11/19/hybridxfb/ for examples and more detail.
      AfterFrameEvent::Trigger(Core::System::GetInstance());

      // Note: Theoretically, in the future we could track the VI configuration and try to detect
      //       when an XFB is the last XFB copy of a frame. Not only would we get a clean "end of
      //       the frame", but we would also be able to use ImmediateXFB even for these games.
      //       Might also clean up some issues with games doing XFB copies they don't intend to
      //       display.

      auto& system = Core::System::GetInstance();
      if (g_ActiveConfig.bImmediateXFB && !NetPlay::IsRollingBack())
      {
        // below div two to convert from bytes to pixels - it expects width, not stride
        u64 ticks = system.GetCoreTiming().GetTicks();
        g_presenter->ImmediateSwap(destAddr, destStride / 2, destStride, height, ticks);
      }
      else
      {
        if (system.GetFifoPlayer().IsRunningWithFakeVideoInterfaceUpdates())
        {
          auto& vi = system.GetVideoInterface();
          vi.FakeVIUpdate(destAddr, srcRect.GetWidth(), destStride, height);
        }
      }
    }

    // Clear the rectangular region after copying it.
    if (PE_copy.clear)
    {
      ClearScreen(srcRect);
    }

    return;
  }
  case BPMEM_LOADTLUT0:  // This updates bpmem.tmem_config.tlut_src, no need to do anything here.
    return;
  case BPMEM_LOADTLUT1:  // Load a Texture Look Up Table
  {
    u32 tmem_addr = bpmem.tmem_config.tlut_dest.tmem_addr << 9;
    u32 tmem_transfer_count = bpmem.tmem_config.tlut_dest.tmem_line_count * TMEM_LINE_SIZE;
    u32 addr = bpmem.tmem_config.tlut_src << 5;

    // The GameCube ignores the upper bits of this address. Some games (WW, MKDD) set them.
    auto& system = Core::System::GetInstance();
    if (!system.IsWii())
      addr = addr & 0x01FFFFFF;

    // The copy below will always be in bounds as tmem is bigger than the maximum address a TLUT can
    // be loaded to.
    static constexpr u32 MAX_LOADABLE_TMEM_ADDR =
        (1 << bpmem.tmem_config.tlut_dest.tmem_addr.NumBits()) << 9;
    static constexpr u32 MAX_TMEM_LINE_COUNT =
        (1 << bpmem.tmem_config.tlut_dest.tmem_line_count.NumBits()) * TMEM_LINE_SIZE;
    static_assert(MAX_LOADABLE_TMEM_ADDR + MAX_TMEM_LINE_COUNT < TMEM_SIZE);

    auto& memory = system.GetMemory();
    memory.CopyFromEmu(s_tex_mem.data() + tmem_addr, addr, tmem_transfer_count);

    if (OpcodeDecoder::g_record_fifo_data)
      system.GetFifoRecorder().UseMemory(addr, tmem_transfer_count, MemoryUpdate::Type::TMEM);

    TMEM::InvalidateAll();

    return;
  }
  case BPMEM_FOGRANGE:  // Fog Settings Control
  case BPMEM_FOGRANGE + 1:
  case BPMEM_FOGRANGE + 2:
  case BPMEM_FOGRANGE + 3:
  case BPMEM_FOGRANGE + 4:
  case BPMEM_FOGRANGE + 5:
    if (bp.changes)
      pixel_shader_manager.SetFogRangeAdjustChanged();
    return;
  case BPMEM_FOGPARAM0:
  case BPMEM_FOGBMAGNITUDE:
  case BPMEM_FOGBEXPONENT:
  case BPMEM_FOGPARAM3:
    if (bp.changes)
      pixel_shader_manager.SetFogParamChanged();
    return;
  case BPMEM_FOGCOLOR:  // Fog Color
    if (bp.changes)
      pixel_shader_manager.SetFogColorChanged();
    return;
  case BPMEM_ALPHACOMPARE:  // Compare Alpha Values
    PRIM_LOG("alphacmp: ref0={}, ref1={}, comp0={}, comp1={}, logic={}", bpmem.alpha_test.ref0,
             bpmem.alpha_test.ref1, bpmem.alpha_test.comp0, bpmem.alpha_test.comp1,
             bpmem.alpha_test.logic);
    if (bp.changes & 0xFFFF)
      pixel_shader_manager.SetAlpha();
    if (bp.changes)
    {
      pixel_shader_manager.SetAlphaTestChanged();
      SetBlendMode();
    }
    return;
  case BPMEM_BIAS:  // BIAS
    PRIM_LOG("ztex bias={:#x}", bpmem.ztex1.bias);
    if (bp.changes)
      pixel_shader_manager.SetZTextureBias();
    return;
  case BPMEM_ZTEX2:  // Z Texture type
  {
    if (bp.changes & 3)
      pixel_shader_manager.SetZTextureTypeChanged();
    if (bp.changes & 12)
      pixel_shader_manager.SetZTextureOpChanged();
    PRIM_LOG("ztex op={}, type={}", bpmem.ztex2.op, bpmem.ztex2.type);
  }
    return;
  // ----------------------------------
  // Display Copy Filtering Control - GX_SetCopyFilter(u8 aa,u8 sample_pattern[12][2],u8 vf,u8
  // vfilter[7])
  // Fields: Destination, Frame2Field, Gamma, Source
  // ----------------------------------
  case BPMEM_DISPLAYCOPYFILTER:      // if (aa) { use sample_pattern } else { use 666666 }
  case BPMEM_DISPLAYCOPYFILTER + 1:  // if (aa) { use sample_pattern } else { use 666666 }
  case BPMEM_DISPLAYCOPYFILTER + 2:  // if (aa) { use sample_pattern } else { use 666666 }
  case BPMEM_DISPLAYCOPYFILTER + 3:  // if (aa) { use sample_pattern } else { use 666666 }
  case BPMEM_COPYFILTER0:            // if (vf) { use vfilter } else { use 595000 }
  case BPMEM_COPYFILTER1:            // if (vf) { use vfilter } else { use 000015 }
    return;
  // -----------------------------------
  // Interlacing Control
  // -----------------------------------
  case BPMEM_FIELDMASK:  // GX_SetFieldMask(u8 even_mask,u8 odd_mask)
  case BPMEM_FIELDMODE:  // GX_SetFieldMode(u8 field_mode,u8 half_aspect_ratio)
    // TODO
    return;
  // ----------------------------------------
  // Unimportant regs (Clock, Perf, ...)
  // ----------------------------------------
  case BPMEM_BUSCLOCK0:   // TB Bus Clock ?
  case BPMEM_BUSCLOCK1:   // TB Bus Clock ?
  case BPMEM_PERF0_TRI:   // Perf: Triangles
  case BPMEM_PERF0_QUAD:  // Perf: Quads
  case BPMEM_PERF1:       // Perf: Some Clock, Texels, TX, TC
    return;
  // ----------------
  // EFB Copy config
  // ----------------
  case BPMEM_EFB_TL:    // EFB Source Rect. Top, Left
  case BPMEM_EFB_WH:    // EFB Source Rect. Width, Height - 1
  case BPMEM_EFB_ADDR:  // EFB Target Address
    return;
  // --------------
  // Clear Config
  // --------------
  case BPMEM_CLEAR_AR:  // Alpha and Red Components
  case BPMEM_CLEAR_GB:  // Green and Blue Components
  case BPMEM_CLEAR_Z:   // Z Components (24-bit Zbuffer)
    return;
  // -------------------------
  // Bounding Box Control
  // -------------------------
  case BPMEM_CLEARBBOX1:
  case BPMEM_CLEARBBOX2:
  {
    const u8 offset = bp.address & 2;
    g_bounding_box->Enable(pixel_shader_manager);

    g_bounding_box->Set(offset, bp.newvalue & 0x3ff);
    g_bounding_box->Set(offset + 1, bp.newvalue >> 10);
  }
    return;
  case BPMEM_TEXINVALIDATE:
    TMEM::Invalidate(bp.newvalue);
    return;

  case BPMEM_ZCOMPARE:  // Set the Z-Compare and EFB pixel format
    OnPixelFormatChange();
    if (bp.changes & 7)
      SetBlendMode();  // dual source could be activated by changing to PIXELFMT_RGBA6_Z24
    pixel_shader_manager.SetZModeControl();
    return;

  case BPMEM_EFB_STRIDE:  // Display Copy Stride
  case BPMEM_COPYYSCALE:  // Display Copy Y Scale
    return;

  /* 24 RID
   * 21 BC3 - Ind. Tex Stage 3 NTexCoord
   * 18 BI3 - Ind. Tex Stage 3 NTexMap
   * 15 BC2 - Ind. Tex Stage 2 NTexCoord
   * 12 BI2 - Ind. Tex Stage 2 NTexMap
   * 9 BC1 - Ind. Tex Stage 1 NTexCoord
   * 6 BI1 - Ind. Tex Stage 1 NTexMap
   * 3 BC0 - Ind. Tex Stage 0 NTexCoord
   * 0 BI0 - Ind. Tex Stage 0 NTexMap */
  case BPMEM_IREF:
  {
    if (bp.changes)
      pixel_shader_manager.SetTevIndirectChanged();
    return;
  }

  case BPMEM_TEV_KSEL:      // Texture Environment Swap Mode Table 0
  case BPMEM_TEV_KSEL + 1:  // Texture Environment Swap Mode Table 1
  case BPMEM_TEV_KSEL + 2:  // Texture Environment Swap Mode Table 2
  case BPMEM_TEV_KSEL + 3:  // Texture Environment Swap Mode Table 3
  case BPMEM_TEV_KSEL + 4:  // Texture Environment Swap Mode Table 4
  case BPMEM_TEV_KSEL + 5:  // Texture Environment Swap Mode Table 5
  case BPMEM_TEV_KSEL + 6:  // Texture Environment Swap Mode Table 6
  case BPMEM_TEV_KSEL + 7:  // Texture Environment Swap Mode Table 7
    pixel_shader_manager.SetTevKSel(bp.address - BPMEM_TEV_KSEL, bp.newvalue);
    return;

  /* This Register can be used to limit to which bits of BP registers is
   * actually written to. The mask is only valid for the next BP write,
   * and will reset itself afterwards. It's handled as a special case in
   * LoadBPReg. */
  case BPMEM_BP_MASK:

  case BPMEM_IND_IMASK:  // Index Mask ?
  case BPMEM_REVBITS:    // Always set to 0x0F when GX_InitRevBits() is called.
    return;

  case BPMEM_CLEAR_PIXEL_PERF:
    // GXClearPixMetric writes 0xAAA here, Sunshine alternates this register between values 0x000
    // and 0xAAA
    if (PerfQueryBase::ShouldEmulate())
      g_perf_query->ResetQuery();
    return;

  case BPMEM_PRELOAD_ADDR:
  case BPMEM_PRELOAD_TMEMEVEN:
  case BPMEM_PRELOAD_TMEMODD:  // Used when PRELOAD_MODE is set
    return;

  case BPMEM_PRELOAD_MODE:  // Set to 0 when GX_TexModeSync() is called.
    // if this is different from 0, manual TMEM management is used (GX_PreloadEntireTexture).
    if (bp.newvalue != 0)
    {
      // TODO: Not quite sure if this is completely correct (likely not)
      // NOTE: libogc's implementation of GX_PreloadEntireTexture seems flawed, so it's not
      // necessarily a good reference for RE'ing this feature.

      BPS_TmemConfig& tmem_cfg = bpmem.tmem_config;
      u32 src_addr = tmem_cfg.preload_addr << 5;  // TODO: Should we add mask here on GC?
      u32 bytes_read = 0;
      u32 tmem_addr_even = tmem_cfg.preload_tmem_even * TMEM_LINE_SIZE;

      if (tmem_cfg.preload_tile_info.type != 3)
      {
        if (tmem_addr_even < TMEM_SIZE)
        {
          bytes_read = tmem_cfg.preload_tile_info.count * TMEM_LINE_SIZE;
          if (tmem_addr_even + bytes_read > TMEM_SIZE)
            bytes_read = TMEM_SIZE - tmem_addr_even;

          auto& system = Core::System::GetInstance();
          auto& memory = system.GetMemory();
          memory.CopyFromEmu(s_tex_mem.data() + tmem_addr_even, src_addr, bytes_read);
        }
      }
      else  // RGBA8 tiles (and CI14, but that might just be stupid libogc!)
      {
        auto& system = Core::System::GetInstance();
        auto& memory = system.GetMemory();

        // AR and GB tiles are stored in separate TMEM banks => can't use a single memcpy for
        // everything
        u32 tmem_addr_odd = tmem_cfg.preload_tmem_odd * TMEM_LINE_SIZE;

        for (u32 i = 0; i < tmem_cfg.preload_tile_info.count; ++i)
        {
          if (tmem_addr_even + TMEM_LINE_SIZE > TMEM_SIZE ||
              tmem_addr_odd + TMEM_LINE_SIZE > TMEM_SIZE)
          {
            break;
          }

          memory.CopyFromEmu(s_tex_mem.data() + tmem_addr_even, src_addr + bytes_read,
                             TMEM_LINE_SIZE);
          memory.CopyFromEmu(s_tex_mem.data() + tmem_addr_odd,
                             src_addr + bytes_read + TMEM_LINE_SIZE, TMEM_LINE_SIZE);
          tmem_addr_even += TMEM_LINE_SIZE;
          tmem_addr_odd += TMEM_LINE_SIZE;
          bytes_read += TMEM_LINE_SIZE * 2;
        }
      }

      if (OpcodeDecoder::g_record_fifo_data)
      {
        Core::System::GetInstance().GetFifoRecorder().UseMemory(src_addr, bytes_read,
                                                                MemoryUpdate::Type::TMEM);
      }

      TMEM::InvalidateAll();
    }
    return;

  // ---------------------------------------------------
  // Set the TEV Color
  // ---------------------------------------------------
  //
  // NOTE: Each of these registers actually maps to two variables internally.
  //       There's a bit that specifies which one is currently written to.
  //
  // NOTE: Some games write only to the RA register (or only to the BG register).
  //       We may not assume that the unwritten register holds a valid value, hence
  //       both component pairs need to be loaded individually.
  case BPMEM_TEV_COLOR_RA:
  case BPMEM_TEV_COLOR_RA + 2:
  case BPMEM_TEV_COLOR_RA + 4:
  case BPMEM_TEV_COLOR_RA + 6:
  {
    int num = (bp.address >> 1) & 0x3;
    if (bpmem.tevregs[num].ra.type == TevRegType::Constant)
    {
      pixel_shader_manager.SetTevKonstColor(num, 0, bpmem.tevregs[num].ra.red);
      pixel_shader_manager.SetTevKonstColor(num, 3, bpmem.tevregs[num].ra.alpha);
    }
    else
    {
      pixel_shader_manager.SetTevColor(num, 0, bpmem.tevregs[num].ra.red);
      pixel_shader_manager.SetTevColor(num, 3, bpmem.tevregs[num].ra.alpha);
    }
    return;
  }

  case BPMEM_TEV_COLOR_BG:
  case BPMEM_TEV_COLOR_BG + 2:
  case BPMEM_TEV_COLOR_BG + 4:
  case BPMEM_TEV_COLOR_BG + 6:
  {
    int num = (bp.address >> 1) & 0x3;
    if (bpmem.tevregs[num].bg.type == TevRegType::Constant)
    {
      pixel_shader_manager.SetTevKonstColor(num, 1, bpmem.tevregs[num].bg.green);
      pixel_shader_manager.SetTevKonstColor(num, 2, bpmem.tevregs[num].bg.blue);
    }
    else
    {
      pixel_shader_manager.SetTevColor(num, 1, bpmem.tevregs[num].bg.green);
      pixel_shader_manager.SetTevColor(num, 2, bpmem.tevregs[num].bg.blue);
    }
    return;
  }

  default:
    break;
  }

  switch (bp.address & 0xFC)  // Texture sampler filter
  {
  // -------------------------
  // Texture Environment Order
  // -------------------------
  case BPMEM_TREF:
  case BPMEM_TREF + 4:
    pixel_shader_manager.SetTevOrder(bp.address - BPMEM_TREF, bp.newvalue);
    return;
  // ----------------------
  // Set wrap size
  // ----------------------
  case BPMEM_SU_SSIZE:  // Matches BPMEM_SU_TSIZE too
  case BPMEM_SU_SSIZE + 4:
  case BPMEM_SU_SSIZE + 8:
  case BPMEM_SU_SSIZE + 12:
    if (bp.changes)
    {
      pixel_shader_manager.SetTexCoordChanged((bp.address - BPMEM_SU_SSIZE) >> 1);
      geometry_shader_manager.SetTexCoordChanged((bp.address - BPMEM_SU_SSIZE) >> 1);
    }
    return;
  }

  if ((bp.address & 0xc0) == 0x80)
  {
    auto tex_address = TexUnitAddress::FromBPAddress(bp.address);

    switch (tex_address.Reg)
    {
    // ------------------------
    // BPMEM_TX_SETMODE0 - (Texture lookup and filtering mode) LOD/BIAS Clamp, MaxAnsio, LODBIAS,
    // DiagLoad, Min Filter, Mag Filter, Wrap T, S
    // BPMEM_TX_SETMODE1 - (LOD Stuff) - Max LOD, Min LOD
    // ------------------------
    case TexUnitAddress::Register::SETMODE0:
    case TexUnitAddress::Register::SETMODE1:
      TMEM::ConfigurationChanged(tex_address, bp.newvalue);
      return;

    // --------------------------------------------
    // BPMEM_TX_SETIMAGE0 - Texture width, height, format
    // BPMEM_TX_SETIMAGE1 - even LOD address in TMEM - Image Type, Cache Height, Cache Width,
    //                      TMEM Offset
    // BPMEM_TX_SETIMAGE2 - odd LOD address in TMEM - Cache Height, Cache Width, TMEM Offset
    // BPMEM_TX_SETIMAGE3 - Address of Texture in main memory
    // --------------------------------------------
    case TexUnitAddress::Register::SETIMAGE0:
    case TexUnitAddress::Register::SETIMAGE1:
    case TexUnitAddress::Register::SETIMAGE2:
    case TexUnitAddress::Register::SETIMAGE3:
      TMEM::ConfigurationChanged(tex_address, bp.newvalue);
      return;

    // -------------------------------
    // Set a TLUT
    // BPMEM_TX_SETTLUT - Format, TMEM Offset (offset of TLUT from start of TMEM high bank > > 5)
    // -------------------------------
    case TexUnitAddress::Register::SETTLUT:
      TMEM::ConfigurationChanged(tex_address, bp.newvalue);
      return;
    case TexUnitAddress::Register::UNKNOWN:
      break;  // Not handled
    }
  }

  switch (bp.address & 0xF0)
  {
  // --------------
  // Indirect Tev
  // --------------
  case BPMEM_IND_CMD:
    pixel_shader_manager.SetTevIndirectChanged();
    return;
  // --------------------------------------------------
  // Set Color/Alpha of a Tev
  // BPMEM_TEV_COLOR_ENV - Dest, Shift, Clamp, Sub, Bias, Sel A, Sel B, Sel C, Sel D
  // BPMEM_TEV_ALPHA_ENV - Dest, Shift, Clamp, Sub, Bias, Sel A, Sel B, Sel C, Sel D, T Swap, R Swap
  // --------------------------------------------------
  case BPMEM_TEV_COLOR_ENV:  // Texture Environment 1
  case BPMEM_TEV_COLOR_ENV + 16:
    pixel_shader_manager.SetTevCombiner((bp.address - BPMEM_TEV_COLOR_ENV) >> 1,
                                        (bp.address - BPMEM_TEV_COLOR_ENV) & 1, bp.newvalue);
    return;
  default:
    break;
  }

  DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_UNKNOWN_BP_COMMAND);
  WARN_LOG_FMT(VIDEO, "Unknown BP opcode: address = {:#010x} value = {:#010x}", bp.address,
               bp.newvalue);
}

// Call browser: OpcodeDecoding.cpp RunCallback::OnBP()
void LoadBPReg(u8 reg, u32 value, int cycles_into_future)
{
  auto& system = Core::System::GetInstance();

  int oldval = ((u32*)&bpmem)[reg];
  int newval = (oldval & ~bpmem.bpMask) | (value & bpmem.bpMask);
  int changes = (oldval ^ newval) & 0xFFFFFF;

  BPCmd bp = {reg, changes, newval};

  // Reset the mask register if we're not trying to set it ourselves.
  if (reg != BPMEM_BP_MASK)
    bpmem.bpMask = 0xFFFFFF;

  BPWritten(system.GetPixelShaderManager(), system.GetXFStateManager(),
            system.GetGeometryShaderManager(), bp, cycles_into_future);
}

void LoadBPRegPreprocess(u8 reg, u32 value, int cycles_into_future)
{
  auto& system = Core::System::GetInstance();

  // masking via BPMEM_BP_MASK could hypothetically be a problem
  u32 newval = value & 0xffffff;
  switch (reg)
  {
  case BPMEM_SETDRAWDONE:
    if ((newval & 0xff) == 0x02)
      system.GetPixelEngine().SetFinish(cycles_into_future);
    break;
  case BPMEM_PE_TOKEN_ID:
    system.GetPixelEngine().SetToken(newval & 0xffff, false, cycles_into_future);
    break;
  case BPMEM_PE_TOKEN_INT_ID:  // Pixel Engine Interrupt Token ID
    system.GetPixelEngine().SetToken(newval & 0xffff, true, cycles_into_future);
    break;
  }
}

std::pair<std::string, std::string> GetBPRegInfo(u8 cmd, u32 cmddata)
{
// Macro to set the register name and make sure it was written correctly via compile time assertion
#define RegName(reg) ((void)(reg), #reg)
#define DescriptionlessReg(reg) std::make_pair(RegName(reg), "");

  switch (cmd)
  {
  case BPMEM_GENMODE:  // 0x00
    return std::make_pair(RegName(BPMEM_GENMODE), fmt::to_string(GenMode{.hex = cmddata}));

  case BPMEM_DISPLAYCOPYFILTER:  // 0x01
  case BPMEM_DISPLAYCOPYFILTER + 1:
  case BPMEM_DISPLAYCOPYFILTER + 2:
  case BPMEM_DISPLAYCOPYFILTER + 3:
    // TODO: This is actually the sample pattern used for copies from an antialiased EFB
    return DescriptionlessReg(BPMEM_DISPLAYCOPYFILTER);
    // TODO: Description

  case BPMEM_IND_MTXA:  // 0x06
  case BPMEM_IND_MTXA + 3:
  case BPMEM_IND_MTXA + 6:
    return std::make_pair(fmt::format("BPMEM_IND_MTXA Matrix {}", (cmd - BPMEM_IND_MTXA) / 3),
                          fmt::format("Matrix {} column A\n{}", (cmd - BPMEM_IND_MTXA) / 3,
                                      IND_MTXA{.hex = cmddata}));

  case BPMEM_IND_MTXB:  // 0x07
  case BPMEM_IND_MTXB + 3:
  case BPMEM_IND_MTXB + 6:
    return std::make_pair(fmt::format("BPMEM_IND_MTXB Matrix {}", (cmd - BPMEM_IND_MTXB) / 3),
                          fmt::format("Matrix {} column B\n{}", (cmd - BPMEM_IND_MTXB) / 3,
                                      IND_MTXB{.hex = cmddata}));

  case BPMEM_IND_MTXC:  // 0x08
  case BPMEM_IND_MTXC + 3:
  case BPMEM_IND_MTXC + 6:
    return std::make_pair(fmt::format("BPMEM_IND_MTXC Matrix {}", (cmd - BPMEM_IND_MTXC) / 3),
                          fmt::format("Matrix {} column C\n{}", (cmd - BPMEM_IND_MTXC) / 3,
                                      IND_MTXC{.hex = cmddata}));

  case BPMEM_IND_IMASK:  // 0x0F
    return DescriptionlessReg(BPMEM_IND_IMASK);
    // TODO: Description

  case BPMEM_IND_CMD:  // 0x10
  case BPMEM_IND_CMD + 1:
  case BPMEM_IND_CMD + 2:
  case BPMEM_IND_CMD + 3:
  case BPMEM_IND_CMD + 4:
  case BPMEM_IND_CMD + 5:
  case BPMEM_IND_CMD + 6:
  case BPMEM_IND_CMD + 7:
  case BPMEM_IND_CMD + 8:
  case BPMEM_IND_CMD + 9:
  case BPMEM_IND_CMD + 10:
  case BPMEM_IND_CMD + 11:
  case BPMEM_IND_CMD + 12:
  case BPMEM_IND_CMD + 13:
  case BPMEM_IND_CMD + 14:
  case BPMEM_IND_CMD + 15:
    return std::make_pair(fmt::format("BPMEM_IND_CMD number {}", cmd - BPMEM_IND_CMD),
                          fmt::to_string(TevStageIndirect{.fullhex = cmddata}));

  case BPMEM_SCISSORTL:  // 0x20
    return std::make_pair(RegName(BPMEM_SCISSORTL), fmt::to_string(ScissorPos{.hex = cmddata}));

  case BPMEM_SCISSORBR:  // 0x21
    return std::make_pair(RegName(BPMEM_SCISSORBR), fmt::to_string(ScissorPos{.hex = cmddata}));

  case BPMEM_LINEPTWIDTH:  // 0x22
    return std::make_pair(RegName(BPMEM_LINEPTWIDTH), fmt::to_string(LPSize{.hex = cmddata}));

  case BPMEM_PERF0_TRI:  // 0x23
    return DescriptionlessReg(BPMEM_PERF0_TRI);
    // TODO: Description

  case BPMEM_PERF0_QUAD:  // 0x24
    return DescriptionlessReg(BPMEM_PERF0_QUAD);
    // TODO: Description

  case BPMEM_RAS1_SS0:  // 0x25
    return std::make_pair(RegName(BPMEM_RAS1_SS0),
                          fmt::to_string(std::make_pair(cmd, TEXSCALE{.hex = cmddata})));

  case BPMEM_RAS1_SS1:  // 0x26
    return std::make_pair(RegName(BPMEM_RAS1_SS1),
                          fmt::to_string(std::make_pair(cmd, TEXSCALE{.hex = cmddata})));

  case BPMEM_IREF:  // 0x27
    return std::make_pair(RegName(BPMEM_IREF), fmt::to_string(RAS1_IREF{.hex = cmddata}));

  case BPMEM_TREF:  // 0x28
  case BPMEM_TREF + 1:
  case BPMEM_TREF + 2:
  case BPMEM_TREF + 3:
  case BPMEM_TREF + 4:
  case BPMEM_TREF + 5:
  case BPMEM_TREF + 6:
  case BPMEM_TREF + 7:
    return std::make_pair(fmt::format("BPMEM_TREF number {}", cmd - BPMEM_TREF),
                          fmt::to_string(std::make_pair(cmd, TwoTevStageOrders{.hex = cmddata})));

  case BPMEM_SU_SSIZE:  // 0x30
  case BPMEM_SU_SSIZE + 2:
  case BPMEM_SU_SSIZE + 4:
  case BPMEM_SU_SSIZE + 6:
  case BPMEM_SU_SSIZE + 8:
  case BPMEM_SU_SSIZE + 10:
  case BPMEM_SU_SSIZE + 12:
  case BPMEM_SU_SSIZE + 14:
    return std::make_pair(fmt::format("BPMEM_SU_SSIZE number {}", (cmd - BPMEM_SU_SSIZE) / 2),
                          fmt::to_string(std::make_pair(true, TCInfo{.hex = cmddata})));

  case BPMEM_SU_TSIZE:  // 0x31
  case BPMEM_SU_TSIZE + 2:
  case BPMEM_SU_TSIZE + 4:
  case BPMEM_SU_TSIZE + 6:
  case BPMEM_SU_TSIZE + 8:
  case BPMEM_SU_TSIZE + 10:
  case BPMEM_SU_TSIZE + 12:
  case BPMEM_SU_TSIZE + 14:
    return std::make_pair(fmt::format("BPMEM_SU_TSIZE number {}", (cmd - BPMEM_SU_TSIZE) / 2),
                          fmt::to_string(std::make_pair(false, TCInfo{.hex = cmddata})));

  case BPMEM_ZMODE:  // 0x40
    return std::make_pair(RegName(BPMEM_ZMODE), fmt::format("Z mode: {}", ZMode{.hex = cmddata}));

  case BPMEM_BLENDMODE:  // 0x41
    return std::make_pair(RegName(BPMEM_BLENDMODE), fmt::to_string(BlendMode{.hex = cmddata}));

  case BPMEM_CONSTANTALPHA:  // 0x42
    return std::make_pair(RegName(BPMEM_CONSTANTALPHA),
                          fmt::to_string(ConstantAlpha{.hex = cmddata}));

  case BPMEM_ZCOMPARE:  // 0x43
    return std::make_pair(RegName(BPMEM_ZCOMPARE), fmt::to_string(PEControl{.hex = cmddata}));

  case BPMEM_FIELDMASK:  // 0x44
    return std::make_pair(RegName(BPMEM_FIELDMASK), fmt::to_string(FieldMask{.hex = cmddata}));

  case BPMEM_SETDRAWDONE:  // 0x45
    return DescriptionlessReg(BPMEM_SETDRAWDONE);
    // TODO: Description

  case BPMEM_BUSCLOCK0:  // 0x46
    return DescriptionlessReg(BPMEM_BUSCLOCK0);
    // TODO: Description

  case BPMEM_PE_TOKEN_ID:  // 0x47
    return DescriptionlessReg(BPMEM_PE_TOKEN_ID);
    // TODO: Description

  case BPMEM_PE_TOKEN_INT_ID:  // 0x48
    return DescriptionlessReg(BPMEM_PE_TOKEN_INT_ID);
    // TODO: Description

  case BPMEM_EFB_TL:  // 0x49
  {
    const X10Y10 left_top{.hex = cmddata};
    return std::make_pair(RegName(BPMEM_EFB_TL),
                          fmt::format("EFB Left: {}\nEFB Top: {}", left_top.x, left_top.y));
  }

  case BPMEM_EFB_WH:  // 0x4A
  {
    const X10Y10 width_height{.hex = cmddata};
    return std::make_pair(
        RegName(BPMEM_EFB_WH),
        fmt::format("EFB Width: {}\nEFB Height: {}", width_height.x + 1, width_height.y + 1));
  }

  case BPMEM_EFB_ADDR:  // 0x4B
    return std::make_pair(
        RegName(BPMEM_EFB_ADDR),
        fmt::format("EFB Target address (32 byte aligned): 0x{:06X}", cmddata << 5));

  case BPMEM_EFB_STRIDE:  // 0x4D
    return std::make_pair(
        RegName(BPMEM_EFB_STRIDE),
        fmt::format("EFB destination stride (32 byte aligned): 0x{:06X}", cmddata << 5));

  case BPMEM_COPYYSCALE:  // 0x4E
    return std::make_pair(
        RegName(BPMEM_COPYYSCALE),
        fmt::format("Y scaling factor (XFB copy only): 0x{:X} ({}, reciprocal {})", cmddata,
                    static_cast<float>(cmddata) / 256.f, 256.f / static_cast<float>(cmddata)));

  case BPMEM_CLEAR_AR:  // 0x4F
    return std::make_pair(RegName(BPMEM_CLEAR_AR),
                          fmt::format("Clear color alpha: 0x{:02X}\nClear color red: 0x{:02X}",
                                      (cmddata & 0xFF00) >> 8, cmddata & 0xFF));

  case BPMEM_CLEAR_GB:  // 0x50
    return std::make_pair(RegName(BPMEM_CLEAR_GB),
                          fmt::format("Clear color green: 0x{:02X}\nClear color blue: 0x{:02X}",
                                      (cmddata & 0xFF00) >> 8, cmddata & 0xFF));

  case BPMEM_CLEAR_Z:  // 0x51
    return std::make_pair(RegName(BPMEM_CLEAR_Z), fmt::format("Clear Z value: 0x{:06X}", cmddata));

  case BPMEM_TRIGGER_EFB_COPY:  // 0x52
    return std::make_pair(RegName(BPMEM_TRIGGER_EFB_COPY),
                          fmt::to_string(UPE_Copy{.Hex = cmddata}));

  case BPMEM_COPYFILTER0:  // 0x53
  {
    const u32 w0 = (cmddata & 0x00003f);
    const u32 w1 = (cmddata & 0x000fc0) >> 6;
    const u32 w2 = (cmddata & 0x03f000) >> 12;
    const u32 w3 = (cmddata & 0xfc0000) >> 18;
    return std::make_pair(RegName(BPMEM_COPYFILTER0),
                          fmt::format("w0: {}\nw1: {}\nw2: {}\nw3: {}", w0, w1, w2, w3));
  }

  case BPMEM_COPYFILTER1:  // 0x54
  {
    const u32 w4 = (cmddata & 0x00003f);
    const u32 w5 = (cmddata & 0x000fc0) >> 6;
    const u32 w6 = (cmddata & 0x03f000) >> 12;
    // There is no w7
    return std::make_pair(RegName(BPMEM_COPYFILTER1),
                          fmt::format("w4: {}\nw5: {}\nw6: {}", w4, w5, w6));
  }

  case BPMEM_CLEARBBOX1:  // 0x55
    return std::make_pair(RegName(BPMEM_CLEARBBOX1),
                          fmt::format("Bounding Box index 0: {}\nBounding Box index 1: {}",
                                      cmddata & 0x3ff, (cmddata >> 10) & 0x3ff));

  case BPMEM_CLEARBBOX2:  // 0x56
    return std::make_pair(RegName(BPMEM_CLEARBBOX2),
                          fmt::format("Bounding Box index 2: {}\nBounding Box index 3: {}",
                                      cmddata & 0x3ff, (cmddata >> 10) & 0x3ff));

  case BPMEM_CLEAR_PIXEL_PERF:  // 0x57
    return DescriptionlessReg(BPMEM_CLEAR_PIXEL_PERF);
    // TODO: Description

  case BPMEM_REVBITS:  // 0x58
    return DescriptionlessReg(BPMEM_REVBITS);
    // TODO: Description

  case BPMEM_SCISSOROFFSET:  // 0x59
    return std::make_pair(RegName(BPMEM_SCISSOROFFSET),
                          fmt::to_string(ScissorOffset{.hex = cmddata}));

  case BPMEM_PRELOAD_ADDR:  // 0x60
    return std::make_pair(
        RegName(BPMEM_PRELOAD_ADDR),
        fmt::format("Tmem preload address (32 byte aligned, in main memory): 0x{:06x}",
                    cmddata << 5));

  case BPMEM_PRELOAD_TMEMEVEN:  // 0x61
    return std::make_pair(RegName(BPMEM_PRELOAD_TMEMEVEN),
                          fmt::format("Tmem preload even line: 0x{:04x} (byte 0x{:05x})", cmddata,
                                      cmddata * TMEM_LINE_SIZE));

  case BPMEM_PRELOAD_TMEMODD:  // 0x62
    return std::make_pair(RegName(BPMEM_PRELOAD_TMEMODD),
                          fmt::format("Tmem preload odd line: 0x{:04x} (byte 0x{:05x})", cmddata,
                                      cmddata * TMEM_LINE_SIZE));

  case BPMEM_PRELOAD_MODE:  // 0x63
    return std::make_pair(RegName(BPMEM_PRELOAD_MODE),
                          fmt::to_string(BPU_PreloadTileInfo{.hex = cmddata}));

  case BPMEM_LOADTLUT0:  // 0x64
    return std::make_pair(
        RegName(BPMEM_LOADTLUT0),
        fmt::format("TLUT load address (32 byte aligned, in main memory): 0x{:06x}", cmddata << 5));

  case BPMEM_LOADTLUT1:  // 0x65
    return std::make_pair(RegName(BPMEM_LOADTLUT1),
                          fmt::to_string(BPU_LoadTlutInfo{.hex = cmddata}));

  case BPMEM_TEXINVALIDATE:  // 0x66
    return DescriptionlessReg(BPMEM_TEXINVALIDATE);
    // TODO: Description

  case BPMEM_PERF1:  // 0x67
    return DescriptionlessReg(BPMEM_PERF1);
    // TODO: Description

  case BPMEM_FIELDMODE:  // 0x68
    return std::make_pair(RegName(BPMEM_FIELDMODE), fmt::to_string(FieldMode{.hex = cmddata}));

  case BPMEM_BUSCLOCK1:  // 0x69
    return DescriptionlessReg(BPMEM_BUSCLOCK1);
    // TODO: Description

  case BPMEM_TX_SETMODE0:  // 0x80
  case BPMEM_TX_SETMODE0 + 1:
  case BPMEM_TX_SETMODE0 + 2:
  case BPMEM_TX_SETMODE0 + 3:
    return std::make_pair(fmt::format("BPMEM_TX_SETMODE0 Texture Unit {}", cmd - BPMEM_TX_SETMODE0),
                          fmt::to_string(TexMode0{.hex = cmddata}));

  case BPMEM_TX_SETMODE1:  // 0x84
  case BPMEM_TX_SETMODE1 + 1:
  case BPMEM_TX_SETMODE1 + 2:
  case BPMEM_TX_SETMODE1 + 3:
    return std::make_pair(fmt::format("BPMEM_TX_SETMODE1 Texture Unit {}", cmd - BPMEM_TX_SETMODE1),
                          fmt::to_string(TexMode1{.hex = cmddata}));

  case BPMEM_TX_SETIMAGE0:  // 0x88
  case BPMEM_TX_SETIMAGE0 + 1:
  case BPMEM_TX_SETIMAGE0 + 2:
  case BPMEM_TX_SETIMAGE0 + 3:
    return std::make_pair(
        fmt::format("BPMEM_TX_SETIMAGE0 Texture Unit {}", cmd - BPMEM_TX_SETIMAGE0),
        fmt::to_string(TexImage0{.hex = cmddata}));

  case BPMEM_TX_SETIMAGE1:  // 0x8C
  case BPMEM_TX_SETIMAGE1 + 1:
  case BPMEM_TX_SETIMAGE1 + 2:
  case BPMEM_TX_SETIMAGE1 + 3:
    return std::make_pair(
        fmt::format("BPMEM_TX_SETIMAGE1 Texture Unit {}", cmd - BPMEM_TX_SETIMAGE1),
        fmt::to_string(TexImage1{.hex = cmddata}));

  case BPMEM_TX_SETIMAGE2:  // 0x90
  case BPMEM_TX_SETIMAGE2 + 1:
  case BPMEM_TX_SETIMAGE2 + 2:
  case BPMEM_TX_SETIMAGE2 + 3:
    return std::make_pair(
        fmt::format("BPMEM_TX_SETIMAGE2 Texture Unit {}", cmd - BPMEM_TX_SETIMAGE2),
        fmt::to_string(TexImage2{.hex = cmddata}));

  case BPMEM_TX_SETIMAGE3:  // 0x94
  case BPMEM_TX_SETIMAGE3 + 1:
  case BPMEM_TX_SETIMAGE3 + 2:
  case BPMEM_TX_SETIMAGE3 + 3:
    return std::make_pair(
        fmt::format("BPMEM_TX_SETIMAGE3 Texture Unit {}", cmd - BPMEM_TX_SETIMAGE3),
        fmt::to_string(TexImage3{.hex = cmddata}));

  case BPMEM_TX_SETTLUT:  // 0x98
  case BPMEM_TX_SETTLUT + 1:
  case BPMEM_TX_SETTLUT + 2:
  case BPMEM_TX_SETTLUT + 3:
    return std::make_pair(fmt::format("BPMEM_TX_SETTLUT Texture Unit {}", cmd - BPMEM_TX_SETTLUT),
                          fmt::to_string(TexTLUT{.hex = cmddata}));

  case BPMEM_TX_SETMODE0_4:  // 0xA0
  case BPMEM_TX_SETMODE0_4 + 1:
  case BPMEM_TX_SETMODE0_4 + 2:
  case BPMEM_TX_SETMODE0_4 + 3:
    return std::make_pair(
        fmt::format("BPMEM_TX_SETMODE0_4 Texture Unit {}", cmd - BPMEM_TX_SETMODE0_4 + 4),
        fmt::to_string(TexMode0{.hex = cmddata}));

  case BPMEM_TX_SETMODE1_4:  // 0xA4
  case BPMEM_TX_SETMODE1_4 + 1:
  case BPMEM_TX_SETMODE1_4 + 2:
  case BPMEM_TX_SETMODE1_4 + 3:
    return std::make_pair(
        fmt::format("BPMEM_TX_SETMODE1_4 Texture Unit {}", cmd - BPMEM_TX_SETMODE1_4 + 4),
        fmt::to_string(TexMode1{.hex = cmddata}));

  case BPMEM_TX_SETIMAGE0_4:  // 0xA8
  case BPMEM_TX_SETIMAGE0_4 + 1:
  case BPMEM_TX_SETIMAGE0_4 + 2:
  case BPMEM_TX_SETIMAGE0_4 + 3:
    return std::make_pair(
        fmt::format("BPMEM_TX_SETIMAGE0_4 Texture Unit {}", cmd - BPMEM_TX_SETIMAGE0_4 + 4),
        fmt::to_string(TexImage0{.hex = cmddata}));

  case BPMEM_TX_SETIMAGE1_4:  // 0xAC
  case BPMEM_TX_SETIMAGE1_4 + 1:
  case BPMEM_TX_SETIMAGE1_4 + 2:
  case BPMEM_TX_SETIMAGE1_4 + 3:
    return std::make_pair(
        fmt::format("BPMEM_TX_SETIMAGE1_4 Texture Unit {}", cmd - BPMEM_TX_SETIMAGE1_4 + 4),
        fmt::to_string(TexImage1{.hex = cmddata}));

  case BPMEM_TX_SETIMAGE2_4:  // 0xB0
  case BPMEM_TX_SETIMAGE2_4 + 1:
  case BPMEM_TX_SETIMAGE2_4 + 2:
  case BPMEM_TX_SETIMAGE2_4 + 3:
    return std::make_pair(
        fmt::format("BPMEM_TX_SETIMAGE2_4 Texture Unit {}", cmd - BPMEM_TX_SETIMAGE2_4 + 4),
        fmt::to_string(TexImage2{.hex = cmddata}));

  case BPMEM_TX_SETIMAGE3_4:  // 0xB4
  case BPMEM_TX_SETIMAGE3_4 + 1:
  case BPMEM_TX_SETIMAGE3_4 + 2:
  case BPMEM_TX_SETIMAGE3_4 + 3:
    return std::make_pair(
        fmt::format("BPMEM_TX_SETIMAGE3_4 Texture Unit {}", cmd - BPMEM_TX_SETIMAGE3_4 + 4),
        fmt::to_string(TexImage3{.hex = cmddata}));

  case BPMEM_TX_SETTLUT_4:  // 0xB8
  case BPMEM_TX_SETTLUT_4 + 1:
  case BPMEM_TX_SETTLUT_4 + 2:
  case BPMEM_TX_SETTLUT_4 + 3:
    return std::make_pair(
        fmt::format("BPMEM_TX_SETTLUT_4 Texture Unit {}", cmd - BPMEM_TX_SETTLUT_4 + 4),
        fmt::to_string(TexTLUT{.hex = cmddata}));

  case BPMEM_TEV_COLOR_ENV:  // 0xC0
  case BPMEM_TEV_COLOR_ENV + 2:
  case BPMEM_TEV_COLOR_ENV + 4:
  case BPMEM_TEV_COLOR_ENV + 6:
  case BPMEM_TEV_COLOR_ENV + 8:
  case BPMEM_TEV_COLOR_ENV + 10:
  case BPMEM_TEV_COLOR_ENV + 12:
  case BPMEM_TEV_COLOR_ENV + 14:
  case BPMEM_TEV_COLOR_ENV + 16:
  case BPMEM_TEV_COLOR_ENV + 18:
  case BPMEM_TEV_COLOR_ENV + 20:
  case BPMEM_TEV_COLOR_ENV + 22:
  case BPMEM_TEV_COLOR_ENV + 24:
  case BPMEM_TEV_COLOR_ENV + 26:
  case BPMEM_TEV_COLOR_ENV + 28:
  case BPMEM_TEV_COLOR_ENV + 30:
    return std::make_pair(
        fmt::format("BPMEM_TEV_COLOR_ENV Tev stage {}", (cmd - BPMEM_TEV_COLOR_ENV) / 2),
        fmt::to_string(TevStageCombiner::ColorCombiner{.hex = cmddata}));

  case BPMEM_TEV_ALPHA_ENV:  // 0xC1
  case BPMEM_TEV_ALPHA_ENV + 2:
  case BPMEM_TEV_ALPHA_ENV + 4:
  case BPMEM_TEV_ALPHA_ENV + 6:
  case BPMEM_TEV_ALPHA_ENV + 8:
  case BPMEM_TEV_ALPHA_ENV + 10:
  case BPMEM_TEV_ALPHA_ENV + 12:
  case BPMEM_TEV_ALPHA_ENV + 14:
  case BPMEM_TEV_ALPHA_ENV + 16:
  case BPMEM_TEV_ALPHA_ENV + 18:
  case BPMEM_TEV_ALPHA_ENV + 20:
  case BPMEM_TEV_ALPHA_ENV + 22:
  case BPMEM_TEV_ALPHA_ENV + 24:
  case BPMEM_TEV_ALPHA_ENV + 26:
  case BPMEM_TEV_ALPHA_ENV + 28:
  case BPMEM_TEV_ALPHA_ENV + 30:
    return std::make_pair(
        fmt::format("BPMEM_TEV_ALPHA_ENV Tev stage {}", (cmd - BPMEM_TEV_ALPHA_ENV) / 2),
        fmt::to_string(TevStageCombiner::AlphaCombiner{.hex = cmddata}));

  case BPMEM_TEV_COLOR_RA:      // 0xE0
  case BPMEM_TEV_COLOR_RA + 2:  // 0xE2
  case BPMEM_TEV_COLOR_RA + 4:  // 0xE4
  case BPMEM_TEV_COLOR_RA + 6:  // 0xE6
    return std::make_pair(
        fmt::format("BPMEM_TEV_COLOR_RA Tev register {}", (cmd - BPMEM_TEV_COLOR_RA) / 2),
        fmt::to_string(TevReg::RA{.hex = cmddata}));

  case BPMEM_TEV_COLOR_BG:      // 0xE1
  case BPMEM_TEV_COLOR_BG + 2:  // 0xE3
  case BPMEM_TEV_COLOR_BG + 4:  // 0xE5
  case BPMEM_TEV_COLOR_BG + 6:  // 0xE7
    return std::make_pair(
        fmt::format("BPMEM_TEV_COLOR_BG Tev register {}", (cmd - BPMEM_TEV_COLOR_BG) / 2),
        fmt::to_string(TevReg::BG{.hex = cmddata}));

  case BPMEM_FOGRANGE:  // 0xE8
    return std::make_pair("BPMEM_FOGRANGE Base",
                          fmt::to_string(FogRangeParams::RangeBase{.hex = cmddata}));

  case BPMEM_FOGRANGE + 1:
  case BPMEM_FOGRANGE + 2:
  case BPMEM_FOGRANGE + 3:
  case BPMEM_FOGRANGE + 4:
  case BPMEM_FOGRANGE + 5:
    return std::make_pair(fmt::format("BPMEM_FOGRANGE K element {}", cmd - BPMEM_FOGRANGE),
                          fmt::to_string(FogRangeKElement{.HEX = cmddata}));

  case BPMEM_FOGPARAM0:  // 0xEE
    return std::make_pair(RegName(BPMEM_FOGPARAM0), fmt::to_string(FogParam0{.hex = cmddata}));

  case BPMEM_FOGBMAGNITUDE:  // 0xEF
    return std::make_pair(RegName(BPMEM_FOGBMAGNITUDE), fmt::format("B magnitude: {}", cmddata));

  case BPMEM_FOGBEXPONENT:  // 0xF0
    return std::make_pair(RegName(BPMEM_FOGBEXPONENT),
                          fmt::format("B shift: 1>>{} (1/{})", cmddata, 1 << cmddata));

  case BPMEM_FOGPARAM3:  // 0xF1
    return std::make_pair(RegName(BPMEM_FOGPARAM3), fmt::to_string(FogParam3{.hex = cmddata}));

  case BPMEM_FOGCOLOR:  // 0xF2
    return std::make_pair(RegName(BPMEM_FOGCOLOR),
                          fmt::to_string(FogParams::FogColor{.hex = cmddata}));

  case BPMEM_ALPHACOMPARE:  // 0xF3
    return std::make_pair(RegName(BPMEM_ALPHACOMPARE), fmt::to_string(AlphaTest{.hex = cmddata}));

  case BPMEM_BIAS:  // 0xF4
    return std::make_pair(RegName(BPMEM_BIAS), fmt::to_string(ZTex1{.hex = cmddata}));

  case BPMEM_ZTEX2:  // 0xF5
    return std::make_pair(RegName(BPMEM_ZTEX2), fmt::to_string(ZTex2{.hex = cmddata}));

  case BPMEM_TEV_KSEL:  // 0xF6
  case BPMEM_TEV_KSEL + 1:
  case BPMEM_TEV_KSEL + 2:
  case BPMEM_TEV_KSEL + 3:
  case BPMEM_TEV_KSEL + 4:
  case BPMEM_TEV_KSEL + 5:
  case BPMEM_TEV_KSEL + 6:
  case BPMEM_TEV_KSEL + 7:
    return std::make_pair(fmt::format("BPMEM_TEV_KSEL number {}", cmd - BPMEM_TEV_KSEL),
                          fmt::to_string(std::make_pair(cmd, TevKSel{.hex = cmddata})));

  case BPMEM_BP_MASK:  // 0xFE
    return std::make_pair(RegName(BPMEM_BP_MASK),
                          fmt::format("The next BP command will only update these bits; others "
                                      "will retain their prior values: {:06x}",
                                      cmddata));

  default:
    return std::make_pair(fmt::format("Unknown BP Reg: {:02x}={:06x}", cmd, cmddata), "");

#undef DescriptionlessReg
#undef RegName
  }
}

// Called when loading a saved state.
void BPReload()
{
  // restore anything that goes straight to the renderer.
  // let's not risk actually replaying any writes.
  // note that PixelShaderManager is already covered since it has its own DoState.
  SetGenerationMode();
  SetScissorAndViewport();
  SetDepthMode();
  SetBlendMode();
  OnPixelFormatChange();
}
