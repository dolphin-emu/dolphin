// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>

#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"

#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/BPStructs.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/PerfQueryBase.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

using namespace BPFunctions;

static const float s_gammaLUT[] =
{
	1.0f,
	1.7f,
	2.2f,
	1.0f
};

void BPInit()
{
	memset(&bpmem, 0, sizeof(bpmem));
	bpmem.bpMask = 0xFFFFFF;
}

static void BPWritten(const BPCmd& bp)
{
	/*
	----------------------------------------------------------------------------------------------------------------
	Purpose: Writes to the BP registers
	Called: At the end of every: OpcodeDecoding.cpp ExecuteDisplayList > Decode() > LoadBPReg
	How It Works: First the pipeline is flushed then update the bpmem with the new value.
				  Some of the BP cases have to call certain functions while others just update the bpmem.
				  some bp cases check the changes variable, because they might not have to be updated all the time
	NOTE: it seems not all bp cases like checking changes, so calling if (bp.changes == 0 ? false : true)
		  had to be ditched and the games seem to work fine with out it.
	NOTE2: Yet Another GameCube Documentation calls them Bypass Raster State Registers but possibly completely wrong
	NOTE3: This controls the register groups: RAS1/2, SU, TF, TEV, C/Z, PEC
	TODO: Turn into function table. The (future) DisplayList (DL) jit can then call the functions directly,
		  getting rid of dynamic dispatch. Unfortunately, few games use DLs properly - most\
		  just stuff geometry in them and don't put state changes there
	----------------------------------------------------------------------------------------------------------------
	*/

	// check for invalid state, else unneeded configuration are built
	g_video_backend->CheckInvalidState();

	if (((s32*)&bpmem)[bp.address] == bp.newvalue)
	{
		if (!(bp.address == BPMEM_TRIGGER_EFB_COPY ||
		      bp.address == BPMEM_CLEARBBOX1 ||
		      bp.address == BPMEM_CLEARBBOX2 ||
		      bp.address == BPMEM_SETDRAWDONE ||
		      bp.address == BPMEM_PE_TOKEN_ID ||
		      bp.address == BPMEM_PE_TOKEN_INT_ID ||
		      bp.address == BPMEM_LOADTLUT0 ||
		      bp.address == BPMEM_LOADTLUT1 ||
		      bp.address == BPMEM_TEXINVALIDATE ||
		      bp.address == BPMEM_PRELOAD_MODE ||
		      bp.address == BPMEM_CLEAR_PIXEL_PERF))
		{
			return;
		}
	}

	FlushPipeline();

	((u32*)&bpmem)[bp.address] = bp.newvalue;

	switch (bp.address)
	{
	case BPMEM_GENMODE: // Set the Generation Mode
		PRIM_LOG("genmode: texgen=%d, col=%d, multisampling=%d, tev=%d, cullmode=%d, ind=%d, zfeeze=%d",
		         (u32)bpmem.genMode.numtexgens, (u32)bpmem.genMode.numcolchans,
		         (u32)bpmem.genMode.multisampling, (u32)bpmem.genMode.numtevstages+1, (u32)bpmem.genMode.cullmode,
		         (u32)bpmem.genMode.numindstages, (u32)bpmem.genMode.zfreeze);

		// Only call SetGenerationMode when cull mode changes.
		if (bp.changes & 0xC000)
			SetGenerationMode();
		return;
	case BPMEM_IND_MTXA: // Index Matrix Changed
	case BPMEM_IND_MTXB:
	case BPMEM_IND_MTXC:
	case BPMEM_IND_MTXA+3:
	case BPMEM_IND_MTXB+3:
	case BPMEM_IND_MTXC+3:
	case BPMEM_IND_MTXA+6:
	case BPMEM_IND_MTXB+6:
	case BPMEM_IND_MTXC+6:
		if (bp.changes)
			PixelShaderManager::SetIndMatrixChanged((bp.address - BPMEM_IND_MTXA) / 3);
		return;
	case BPMEM_RAS1_SS0: // Index Texture Coordinate Scale 0
		if (bp.changes)
			PixelShaderManager::SetIndTexScaleChanged(false);
		return;
	case BPMEM_RAS1_SS1: // Index Texture Coordinate Scale 1
		if (bp.changes)
			PixelShaderManager::SetIndTexScaleChanged(true);
		return;
	// ----------------
	// Scissor Control
	// ----------------
	case BPMEM_SCISSORTL: // Scissor Rectable Top, Left
	case BPMEM_SCISSORBR: // Scissor Rectable Bottom, Right
	case BPMEM_SCISSOROFFSET: // Scissor Offset
		SetScissor();
		VertexShaderManager::SetViewportChanged();
		return;
	case BPMEM_LINEPTWIDTH: // Line Width
		SetLineWidth();
		return;
	case BPMEM_ZMODE: // Depth Control
		PRIM_LOG("zmode: test=%d, func=%d, upd=%d", (int)bpmem.zmode.testenable,
		         (int)bpmem.zmode.func, (int)bpmem.zmode.updateenable);
		SetDepthMode();
		return;
	case BPMEM_BLENDMODE: // Blending Control
		if (bp.changes & 0xFFFF)
		{
			PRIM_LOG("blendmode: en=%d, open=%d, colupd=%d, alphaupd=%d, dst=%d, src=%d, sub=%d, mode=%d",
			         (int)bpmem.blendmode.blendenable, (int)bpmem.blendmode.logicopenable, (int)bpmem.blendmode.colorupdate,
			         (int)bpmem.blendmode.alphaupdate, (int)bpmem.blendmode.dstfactor, (int)bpmem.blendmode.srcfactor,
			         (int)bpmem.blendmode.subtract, (int)bpmem.blendmode.logicmode);

			// Set LogicOp Blending Mode
			if (bp.changes & 0xF002) // logicopenable | logicmode
				SetLogicOpMode();

			// Set Dithering Mode
			if (bp.changes & 4) // dither
				SetDitherMode();

			// Set Blending Mode
			if (bp.changes & 0xFF1) // blendenable | alphaupdate | dstfactor | srcfactor | subtract
				SetBlendMode();

			// Set Color Mask
			if (bp.changes & 0x18) // colorupdate | alphaupdate
				SetColorMask();
		}
		return;
	case BPMEM_CONSTANTALPHA: // Set Destination Alpha
		PRIM_LOG("constalpha: alp=%d, en=%d", bpmem.dstalpha.alpha, bpmem.dstalpha.enable);
		if (bp.changes & 0xFF)
			PixelShaderManager::SetDestAlpha();
		if (bp.changes & 0x100)
			SetBlendMode();
		return;

	// This is called when the game is done drawing the new frame (eg: like in DX: Begin(); Draw(); End();)
	// Triggers an interrupt on the PPC side so that the game knows when the GPU has finished drawing.
	// Tokens are similar.
	case BPMEM_SETDRAWDONE:
		switch (bp.newvalue & 0xFF)
		{
		case 0x02:
			if (!g_use_deterministic_gpu_thread)
				PixelEngine::SetFinish(); // may generate interrupt
			DEBUG_LOG(VIDEO, "GXSetDrawDone SetPEFinish (value: 0x%02X)", (bp.newvalue & 0xFFFF));
			return;

		default:
			WARN_LOG(VIDEO, "GXSetDrawDone ??? (value 0x%02X)", (bp.newvalue & 0xFFFF));
			return;
		}
		return;
	case BPMEM_PE_TOKEN_ID: // Pixel Engine Token ID
		if (!g_use_deterministic_gpu_thread)
			PixelEngine::SetToken(static_cast<u16>(bp.newvalue & 0xFFFF), false);
		DEBUG_LOG(VIDEO, "SetPEToken 0x%04x", (bp.newvalue & 0xFFFF));
		return;
	case BPMEM_PE_TOKEN_INT_ID: // Pixel Engine Interrupt Token ID
		if (!g_use_deterministic_gpu_thread)
			PixelEngine::SetToken(static_cast<u16>(bp.newvalue & 0xFFFF), true);
		DEBUG_LOG(VIDEO, "SetPEToken + INT 0x%04x", (bp.newvalue & 0xFFFF));
		return;

	// ------------------------
	// EFB copy command. This copies a rectangle from the EFB to either RAM in a texture format or to XFB as YUYV.
	// It can also optionally clear the EFB while copying from it. To emulate this, we of course copy first and clear afterwards.
	case BPMEM_TRIGGER_EFB_COPY: // Copy EFB Region or Render to the XFB or Clear the screen.
		{
			// The bottom right is within the rectangle
			// The values in bpmem.copyTexSrcXY and bpmem.copyTexSrcWH are updated in case 0x49 and 0x4a in this function

			u32 destAddr = bpmem.copyTexDest << 5;

			EFBRectangle srcRect;
			srcRect.left = (int)bpmem.copyTexSrcXY.x;
			srcRect.top = (int)bpmem.copyTexSrcXY.y;

			// Here Width+1 like Height, otherwise some textures are corrupted already since the native resolution.
			// TODO: What's the behavior of out of bound access?
			srcRect.right = (int)(bpmem.copyTexSrcXY.x + bpmem.copyTexSrcWH.x + 1);
			srcRect.bottom = (int)(bpmem.copyTexSrcXY.y + bpmem.copyTexSrcWH.y + 1);

			UPE_Copy PE_copy = bpmem.triggerEFBCopy;

			// Check if we are to copy from the EFB or draw to the XFB
			if (PE_copy.copy_to_xfb == 0)
			{
				if (g_ActiveConfig.bShowEFBCopyRegions)
					stats.efb_regions.push_back(srcRect);

				CopyEFB(destAddr, srcRect,
					PE_copy.tp_realFormat(), bpmem.zcontrol.pixel_format,
					!!PE_copy.intensity_fmt, !!PE_copy.half_scale);
			}
			else
			{
				// We should be able to get away with deactivating the current bbox tracking
				// here. Not sure if there's a better spot to put this.
				// the number of lines copied is determined by the y scale * source efb height

				BoundingBox::active = false;

				float yScale;
				if (PE_copy.scale_invert)
					yScale = 256.0f / (float)bpmem.dispcopyyscale;
				else
					yScale = (float)bpmem.dispcopyyscale / 256.0f;

				float num_xfb_lines = ((bpmem.copyTexSrcWH.y + 1.0f) * yScale);

				u32 height = static_cast<u32>(num_xfb_lines);
				if (height > MAX_XFB_HEIGHT)
				{
					INFO_LOG(VIDEO, "Tried to scale EFB to too many XFB lines: %d (%f)",
							 height, num_xfb_lines);
					height = MAX_XFB_HEIGHT;
				}

				u32 width = bpmem.copyMipMapStrideChannels << 4;

				Renderer::RenderToXFB(destAddr, srcRect, width, height, s_gammaLUT[PE_copy.gamma]);
			}

			// Clear the rectangular region after copying it.
			if (PE_copy.clear)
			{
				ClearScreen(srcRect);
			}

			return;
		}
	case BPMEM_LOADTLUT0: // This one updates bpmem.tlutXferSrc, no need to do anything here.
		return;
	case BPMEM_LOADTLUT1: // Load a Texture Look Up Table
		{
			u32 tlutTMemAddr = (bp.newvalue & 0x3FF) << 9;
			u32 tlutXferCount = (bp.newvalue & 0x1FFC00) >> 5;
			u32 addr = bpmem.tmem_config.tlut_src << 5;

			// The GameCube ignores the upper bits of this address. Some games (WW, MKDD) set them.
			if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
				addr = addr & 0x01FFFFFF;

			Memory::CopyFromEmu(texMem + tlutTMemAddr, addr, tlutXferCount);

			return;
		}
	case BPMEM_FOGRANGE: // Fog Settings Control
	case BPMEM_FOGRANGE+1:
	case BPMEM_FOGRANGE+2:
	case BPMEM_FOGRANGE+3:
	case BPMEM_FOGRANGE+4:
	case BPMEM_FOGRANGE+5:
		if (bp.changes)
			PixelShaderManager::SetFogRangeAdjustChanged();
		return;
	case BPMEM_FOGPARAM0:
	case BPMEM_FOGBMAGNITUDE:
	case BPMEM_FOGBEXPONENT:
	case BPMEM_FOGPARAM3:
		if (bp.changes)
			PixelShaderManager::SetFogParamChanged();
		return;
	case BPMEM_FOGCOLOR: // Fog Color
		if (bp.changes)
			PixelShaderManager::SetFogColorChanged();
		return;
	case BPMEM_ALPHACOMPARE: // Compare Alpha Values
		PRIM_LOG("alphacmp: ref0=%d, ref1=%d, comp0=%d, comp1=%d, logic=%d",
		         (int)bpmem.alpha_test.ref0, (int)bpmem.alpha_test.ref1,
		         (int)bpmem.alpha_test.comp0, (int)bpmem.alpha_test.comp1,
		         (int)bpmem.alpha_test.logic);
		if (bp.changes & 0xFFFF)
			PixelShaderManager::SetAlpha();
		if (bp.changes)
			g_renderer->SetColorMask();
		return;
	case BPMEM_BIAS: // BIAS
		PRIM_LOG("ztex bias=0x%x", bpmem.ztex1.bias);
		if (bp.changes)
			PixelShaderManager::SetZTextureBias();
		return;
	case BPMEM_ZTEX2: // Z Texture type
		{
			if (bp.changes & 3)
				PixelShaderManager::SetZTextureTypeChanged();
			#if defined(_DEBUG) || defined(DEBUGFAST)
			const char* pzop[] = {"DISABLE", "ADD", "REPLACE", "?"};
			const char* pztype[] = {"Z8", "Z16", "Z24", "?"};
			PRIM_LOG("ztex op=%s, type=%s", pzop[bpmem.ztex2.op], pztype[bpmem.ztex2.type]);
			#endif
		}
		return;
	// ----------------------------------
	// Display Copy Filtering Control - GX_SetCopyFilter(u8 aa,u8 sample_pattern[12][2],u8 vf,u8 vfilter[7])
	// Fields: Destination, Frame2Field, Gamma, Source
	// ----------------------------------
	case BPMEM_DISPLAYCOPYFILTER:   // if (aa) { use sample_pattern } else { use 666666 }
	case BPMEM_DISPLAYCOPYFILTER+1: // if (aa) { use sample_pattern } else { use 666666 }
	case BPMEM_DISPLAYCOPYFILTER+2: // if (aa) { use sample_pattern } else { use 666666 }
	case BPMEM_DISPLAYCOPYFILTER+3: // if (aa) { use sample_pattern } else { use 666666 }
	case BPMEM_COPYFILTER0:	        // if (vf) { use vfilter } else { use 595000 }
	case BPMEM_COPYFILTER1:	        // if (vf) { use vfilter } else { use 000015 }
		return;
	// -----------------------------------
	// Interlacing Control
	// -----------------------------------
	case BPMEM_FIELDMASK: // GX_SetFieldMask(u8 even_mask,u8 odd_mask)
	case BPMEM_FIELDMODE: // GX_SetFieldMode(u8 field_mode,u8 half_aspect_ratio)
		// TODO
		return;
	// ----------------------------------------
	// Unimportant regs (Clock, Perf, ...)
	// ----------------------------------------
	case BPMEM_BUSCLOCK0:      // TB Bus Clock ?
	case BPMEM_BUSCLOCK1:      // TB Bus Clock ?
	case BPMEM_PERF0_TRI:      // Perf: Triangles
	case BPMEM_PERF0_QUAD:     // Perf: Quads
	case BPMEM_PERF1:          // Perf: Some Clock, Texels, TX, TC
		break;
	// ----------------
	// EFB Copy config
	// ----------------
	case BPMEM_EFB_TL:   // EFB Source Rect. Top, Left
	case BPMEM_EFB_BR:   // EFB Source Rect. Bottom, Right (w, h - 1)
	case BPMEM_EFB_ADDR: // EFB Target Address
		return;
	// --------------
	// Clear Config
	// --------------
	case BPMEM_CLEAR_AR: // Alpha and Red Components
	case BPMEM_CLEAR_GB: // Green and Blue Components
	case BPMEM_CLEAR_Z:  // Z Components (24-bit Zbuffer)
		return;
	// -------------------------
	// Bounding Box Control
	// -------------------------
	case BPMEM_CLEARBBOX1:
	case BPMEM_CLEARBBOX2:
		// Don't compute bounding box if this frame is being skipped!
		// Wrong but valid values are better than bogus values...
		if (!g_bSkipCurrentFrame)
		{
			u8 offset = bp.address & 2;

			BoundingBox::coords[offset]     = bp.newvalue & 0x3ff;
			BoundingBox::coords[offset + 1] = bp.newvalue >> 10;
			BoundingBox::active = true;

			g_renderer->BBoxWrite(offset, bp.newvalue & 0x3ff);
			g_renderer->BBoxWrite(offset + 1, bp.newvalue >> 10);
		}
		return;
	case BPMEM_TEXINVALIDATE:
		// TODO: Needs some restructuring in TextureCacheBase.
		return;

	case BPMEM_ZCOMPARE:      // Set the Z-Compare and EFB pixel format
		OnPixelFormatChange();
		if (bp.changes & 7)
		{
			SetBlendMode(); // dual source could be activated by changing to PIXELFMT_RGBA6_Z24
			g_renderer->SetColorMask(); // alpha writing needs to be disabled if the new pixel format doesn't have an alpha channel
		}
		return;

	case BPMEM_MIPMAP_STRIDE: // MipMap Stride Channel
	case BPMEM_COPYYSCALE:    // Display Copy Y Scale

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

	case BPMEM_TEV_KSEL:   // Texture Environment Swap Mode Table 0
	case BPMEM_TEV_KSEL+1: // Texture Environment Swap Mode Table 1
	case BPMEM_TEV_KSEL+2: // Texture Environment Swap Mode Table 2
	case BPMEM_TEV_KSEL+3: // Texture Environment Swap Mode Table 3
	case BPMEM_TEV_KSEL+4: // Texture Environment Swap Mode Table 4
	case BPMEM_TEV_KSEL+5: // Texture Environment Swap Mode Table 5
	case BPMEM_TEV_KSEL+6: // Texture Environment Swap Mode Table 6
	case BPMEM_TEV_KSEL+7: // Texture Environment Swap Mode Table 7

	/* This Register can be used to limit to which bits of BP registers is
	 * actually written to. The mask is only valid for the next BP write,
	 * and will reset itself afterwards. It's handled as a special case in
	 * LoadBPReg. */
	case BPMEM_BP_MASK:

	case BPMEM_IND_IMASK: // Index Mask ?
	case BPMEM_REVBITS: // Always set to 0x0F when GX_InitRevBits() is called.
		return;

	case BPMEM_CLEAR_PIXEL_PERF:
		// GXClearPixMetric writes 0xAAA here, Sunshine alternates this register between values 0x000 and 0xAAA
		if (PerfQueryBase::ShouldEmulate())
			g_perf_query->ResetQuery();
		return;

	case BPMEM_PRELOAD_ADDR:
	case BPMEM_PRELOAD_TMEMEVEN:
	case BPMEM_PRELOAD_TMEMODD: // Used when PRELOAD_MODE is set
		return;

	case BPMEM_PRELOAD_MODE: // Set to 0 when GX_TexModeSync() is called.
		// if this is different from 0, manual TMEM management is used (GX_PreloadEntireTexture).
		if (bp.newvalue != 0)
		{
			// TODO: Not quite sure if this is completely correct (likely not)
			// NOTE: libogc's implementation of GX_PreloadEntireTexture seems flawed, so it's not necessarily a good reference for RE'ing this feature.

			BPS_TmemConfig& tmem_cfg = bpmem.tmem_config;
			u32 src_addr = tmem_cfg.preload_addr << 5; // TODO: Should we add mask here on GC?
			u32 size = tmem_cfg.preload_tile_info.count * TMEM_LINE_SIZE;
			u32 tmem_addr_even = tmem_cfg.preload_tmem_even * TMEM_LINE_SIZE;

			if (tmem_cfg.preload_tile_info.type != 3)
			{
				if (tmem_addr_even + size > TMEM_SIZE)
					size = TMEM_SIZE - tmem_addr_even;

				Memory::CopyFromEmu(texMem + tmem_addr_even, src_addr, size);
			}
			else // RGBA8 tiles (and CI14, but that might just be stupid libogc!)
			{
				u8* src_ptr = Memory::GetPointer(src_addr);

				// AR and GB tiles are stored in separate TMEM banks => can't use a single memcpy for everything
				u32 tmem_addr_odd = tmem_cfg.preload_tmem_odd * TMEM_LINE_SIZE;

				for (u32 i = 0; i < tmem_cfg.preload_tile_info.count; ++i)
				{
					if (tmem_addr_even + TMEM_LINE_SIZE > TMEM_SIZE ||
					    tmem_addr_odd  + TMEM_LINE_SIZE > TMEM_SIZE)
						return;

					// TODO: This isn't very optimised, does a whole lot of small memcpys
					memcpy(texMem + tmem_addr_even, src_ptr, TMEM_LINE_SIZE);
					memcpy(texMem + tmem_addr_odd, src_ptr + TMEM_LINE_SIZE, TMEM_LINE_SIZE);
					tmem_addr_even += TMEM_LINE_SIZE;
					tmem_addr_odd += TMEM_LINE_SIZE;
					src_ptr += TMEM_LINE_SIZE * 2;
				}
			}
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
		if (bpmem.tevregs[num].type_ra)
		{
			PixelShaderManager::SetTevKonstColor(num, 0, (s32)bpmem.tevregs[num].red);
			PixelShaderManager::SetTevKonstColor(num, 3, (s32)bpmem.tevregs[num].alpha);
		}
		else
		{
			PixelShaderManager::SetTevColor(num, 0, (s32)bpmem.tevregs[num].red);
			PixelShaderManager::SetTevColor(num, 3, (s32)bpmem.tevregs[num].alpha);
		}
		return;
	}

	case BPMEM_TEV_COLOR_BG:
	case BPMEM_TEV_COLOR_BG + 2:
	case BPMEM_TEV_COLOR_BG + 4:
	case BPMEM_TEV_COLOR_BG + 6:
	{
		int num = (bp.address >> 1) & 0x3;
		if (bpmem.tevregs[num].type_bg)
		{
			PixelShaderManager::SetTevKonstColor(num, 1, (s32)bpmem.tevregs[num].green);
			PixelShaderManager::SetTevKonstColor(num, 2, (s32)bpmem.tevregs[num].blue);
		}
		else
		{
			PixelShaderManager::SetTevColor(num, 1, (s32)bpmem.tevregs[num].green);
			PixelShaderManager::SetTevColor(num, 2, (s32)bpmem.tevregs[num].blue);
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
	case BPMEM_TREF+4:
		return;
	// ----------------------
	// Set wrap size
	// ----------------------
	case BPMEM_SU_SSIZE:
	case BPMEM_SU_TSIZE:
	case BPMEM_SU_SSIZE+2:
	case BPMEM_SU_TSIZE+2:
	case BPMEM_SU_SSIZE+4:
	case BPMEM_SU_TSIZE+4:
	case BPMEM_SU_SSIZE+6:
	case BPMEM_SU_TSIZE+6:
	case BPMEM_SU_SSIZE+8:
	case BPMEM_SU_TSIZE+8:
	case BPMEM_SU_SSIZE+10:
	case BPMEM_SU_TSIZE+10:
	case BPMEM_SU_SSIZE+12:
	case BPMEM_SU_TSIZE+12:
	case BPMEM_SU_SSIZE+14:
	case BPMEM_SU_TSIZE+14:
		if (bp.changes)
			PixelShaderManager::SetTexCoordChanged((bp.address - BPMEM_SU_SSIZE) >> 1);
		return;
	// ------------------------
	// BPMEM_TX_SETMODE0 - (Texture lookup and filtering mode) LOD/BIAS Clamp, MaxAnsio, LODBIAS, DiagLoad, Min Filter, Mag Filter, Wrap T, S
	// BPMEM_TX_SETMODE1 - (LOD Stuff) - Max LOD, Min LOD
	// ------------------------
	case BPMEM_TX_SETMODE0: // (0x90 for linear)
	case BPMEM_TX_SETMODE0_4:
		return;

	case BPMEM_TX_SETMODE1:
	case BPMEM_TX_SETMODE1_4:
		return;
	// --------------------------------------------
	// BPMEM_TX_SETIMAGE0 - Texture width, height, format
	// BPMEM_TX_SETIMAGE1 - even LOD address in TMEM - Image Type, Cache Height, Cache Width, TMEM Offset
	// BPMEM_TX_SETIMAGE2 - odd LOD address in TMEM - Cache Height, Cache Width, TMEM Offset
	// BPMEM_TX_SETIMAGE3 - Address of Texture in main memory
	// --------------------------------------------
	case BPMEM_TX_SETIMAGE0:
	case BPMEM_TX_SETIMAGE0_4:
	case BPMEM_TX_SETIMAGE1:
	case BPMEM_TX_SETIMAGE1_4:
	case BPMEM_TX_SETIMAGE2:
	case BPMEM_TX_SETIMAGE2_4:
	case BPMEM_TX_SETIMAGE3:
	case BPMEM_TX_SETIMAGE3_4:
		return;
	// -------------------------------
	// Set a TLUT
	// BPMEM_TX_SETTLUT - Format, TMEM Offset (offset of TLUT from start of TMEM high bank > > 5)
	// -------------------------------
	case BPMEM_TX_SETTLUT:
	case BPMEM_TX_SETTLUT_4:
		return;

	default:
		break;
	}

	switch (bp.address & 0xF0)
	{
	// --------------
	// Indirect Tev
	// --------------
	case BPMEM_IND_CMD:
	case BPMEM_IND_CMD+1:
	case BPMEM_IND_CMD+2:
	case BPMEM_IND_CMD+3:
	case BPMEM_IND_CMD+4:
	case BPMEM_IND_CMD+5:
	case BPMEM_IND_CMD+6:
	case BPMEM_IND_CMD+7:
	case BPMEM_IND_CMD+8:
	case BPMEM_IND_CMD+9:
	case BPMEM_IND_CMD+10:
	case BPMEM_IND_CMD+11:
	case BPMEM_IND_CMD+12:
	case BPMEM_IND_CMD+13:
	case BPMEM_IND_CMD+14:
	case BPMEM_IND_CMD+15:
		return;
	// --------------------------------------------------
	// Set Color/Alpha of a Tev
	// BPMEM_TEV_COLOR_ENV - Dest, Shift, Clamp, Sub, Bias, Sel A, Sel B, Sel C, Sel D
	// BPMEM_TEV_ALPHA_ENV - Dest, Shift, Clamp, Sub, Bias, Sel A, Sel B, Sel C, Sel D, T Swap, R Swap
	// --------------------------------------------------
	case BPMEM_TEV_COLOR_ENV:    // Texture Environment 1
	case BPMEM_TEV_ALPHA_ENV:
	case BPMEM_TEV_COLOR_ENV+2:  // Texture Environment 2
	case BPMEM_TEV_ALPHA_ENV+2:
	case BPMEM_TEV_COLOR_ENV+4:  // Texture Environment 3
	case BPMEM_TEV_ALPHA_ENV+4:
	case BPMEM_TEV_COLOR_ENV+6:  // Texture Environment 4
	case BPMEM_TEV_ALPHA_ENV+6:
	case BPMEM_TEV_COLOR_ENV+8:  // Texture Environment 5
	case BPMEM_TEV_ALPHA_ENV+8:
	case BPMEM_TEV_COLOR_ENV+10: // Texture Environment 6
	case BPMEM_TEV_ALPHA_ENV+10:
	case BPMEM_TEV_COLOR_ENV+12: // Texture Environment 7
	case BPMEM_TEV_ALPHA_ENV+12:
	case BPMEM_TEV_COLOR_ENV+14: // Texture Environment 8
	case BPMEM_TEV_ALPHA_ENV+14:
	case BPMEM_TEV_COLOR_ENV+16: // Texture Environment 9
	case BPMEM_TEV_ALPHA_ENV+16:
	case BPMEM_TEV_COLOR_ENV+18: // Texture Environment 10
	case BPMEM_TEV_ALPHA_ENV+18:
	case BPMEM_TEV_COLOR_ENV+20: // Texture Environment 11
	case BPMEM_TEV_ALPHA_ENV+20:
	case BPMEM_TEV_COLOR_ENV+22: // Texture Environment 12
	case BPMEM_TEV_ALPHA_ENV+22:
	case BPMEM_TEV_COLOR_ENV+24: // Texture Environment 13
	case BPMEM_TEV_ALPHA_ENV+24:
	case BPMEM_TEV_COLOR_ENV+26: // Texture Environment 14
	case BPMEM_TEV_ALPHA_ENV+26:
	case BPMEM_TEV_COLOR_ENV+28: // Texture Environment 15
	case BPMEM_TEV_ALPHA_ENV+28:
	case BPMEM_TEV_COLOR_ENV+30: // Texture Environment 16
	case BPMEM_TEV_ALPHA_ENV+30:
		return;
	default:
		break;
	}

	WARN_LOG(VIDEO, "Unknown BP opcode: address = 0x%08x value = 0x%08x", bp.address, bp.newvalue);
}

// Call browser: OpcodeDecoding.cpp ExecuteDisplayList > Decode() > LoadBPReg()
void LoadBPReg(u32 value0)
{
	int regNum = value0 >> 24;
	int oldval = ((u32*)&bpmem)[regNum];
	int newval = (oldval & ~bpmem.bpMask) | (value0 & bpmem.bpMask);
	int changes = (oldval ^ newval) & 0xFFFFFF;

	BPCmd bp = {regNum, changes, newval};

	// Reset the mask register if we're not trying to set it ourselves.
	if (regNum != BPMEM_BP_MASK)
		bpmem.bpMask = 0xFFFFFF;

	BPWritten(bp);
}

void LoadBPRegPreprocess(u32 value0)
{
	int regNum = value0 >> 24;
	// masking could hypothetically be a problem
	u32 newval = value0 & 0xffffff;
	switch (regNum)
	{
	case BPMEM_SETDRAWDONE:
		if ((newval & 0xff) == 0x02)
			PixelEngine::SetFinish();
		break;
	case BPMEM_PE_TOKEN_ID:
		PixelEngine::SetToken(newval & 0xffff, false);
		break;
	case BPMEM_PE_TOKEN_INT_ID: // Pixel Engine Interrupt Token ID
		PixelEngine::SetToken(newval & 0xffff, true);
		break;
	}
}

void GetBPRegInfo(const u8* data, std::string* name, std::string* desc)
{
	const char* no_yes[2] = { "No", "Yes" };

	u32 cmddata = Common::swap32(*(u32*)data) & 0xFFFFFF;
	switch (data[0])
	{
	 // Macro to set the register name and make sure it was written correctly via compile time assertion
	#define SetRegName(reg) \
		*name = #reg; \
		(void)(reg);

	case BPMEM_GENMODE: // 0x00
		SetRegName(BPMEM_GENMODE);
		// TODO: Description
		break;

	case BPMEM_DISPLAYCOPYFILTER: // 0x01
		// TODO: This is actually the sample pattern used for copies from an antialiased EFB
		SetRegName(BPMEM_DISPLAYCOPYFILTER);
		// TODO: Description
		break;

	case 0x02: // 0x02
	case 0x03: // 0x03
	case 0x04: // 0x04
		// TODO: same as BPMEM_DISPLAYCOPYFILTER
		break;

	case BPMEM_IND_MTXA: // 0x06
	case BPMEM_IND_MTXA+3:
	case BPMEM_IND_MTXA+6:
		SetRegName(BPMEM_IND_MTXA);
		// TODO: Description
		break;

	case BPMEM_IND_MTXB: // 0x07
	case BPMEM_IND_MTXB+3:
	case BPMEM_IND_MTXB+6:
		SetRegName(BPMEM_IND_MTXB);
		// TODO: Descriptio
		break;

	case BPMEM_IND_MTXC: // 0x08
	case BPMEM_IND_MTXC+3:
	case BPMEM_IND_MTXC+6:
		SetRegName(BPMEM_IND_MTXC);
		// TODO: Description
		break;

	case BPMEM_IND_IMASK: // 0x0F
		SetRegName(BPMEM_IND_IMASK);
		// TODO: Description
		break;

	case BPMEM_IND_CMD: // 0x10
	case BPMEM_IND_CMD+1:
	case BPMEM_IND_CMD+2:
	case BPMEM_IND_CMD+3:
	case BPMEM_IND_CMD+4:
	case BPMEM_IND_CMD+5:
	case BPMEM_IND_CMD+6:
	case BPMEM_IND_CMD+7:
	case BPMEM_IND_CMD+8:
	case BPMEM_IND_CMD+9:
	case BPMEM_IND_CMD+10:
	case BPMEM_IND_CMD+11:
	case BPMEM_IND_CMD+12:
	case BPMEM_IND_CMD+13:
	case BPMEM_IND_CMD+14:
	case BPMEM_IND_CMD+15:
		SetRegName(BPMEM_IND_CMD);
		// TODO: Description
		break;

	case BPMEM_SCISSORTL: // 0x20
		SetRegName(BPMEM_SCISSORTL);
		// TODO: Description
		break;

	case BPMEM_SCISSORBR: // 0x21
		SetRegName(BPMEM_SCISSORBR);
		// TODO: Description
		break;

	case BPMEM_LINEPTWIDTH: // 0x22
		SetRegName(BPMEM_LINEPTWIDTH);
		// TODO: Description
		break;

	case BPMEM_PERF0_TRI: // 0x23
		SetRegName(BPMEM_PERF0_TRI);
		// TODO: Description
		break;

	case BPMEM_PERF0_QUAD: // 0x24
		SetRegName(BPMEM_PERF0_QUAD);
		// TODO: Description
		break;

	case BPMEM_RAS1_SS0: // 0x25
		SetRegName(BPMEM_RAS1_SS0);
		// TODO: Description
		break;

	case BPMEM_RAS1_SS1: // 0x26
		SetRegName(BPMEM_RAS1_SS1);
		// TODO: Description
		break;

	case BPMEM_IREF: // 0x27
		SetRegName(BPMEM_IREF);
		// TODO: Description
		break;

	case BPMEM_TREF: // 0x28
	case BPMEM_TREF+1:
	case BPMEM_TREF+2:
	case BPMEM_TREF+3:
	case BPMEM_TREF+4:
	case BPMEM_TREF+5:
	case BPMEM_TREF+6:
	case BPMEM_TREF+7:
		SetRegName(BPMEM_TREF);
		// TODO: Description
		break;

	case BPMEM_SU_SSIZE: // 0x30
	case BPMEM_SU_SSIZE+2:
	case BPMEM_SU_SSIZE+4:
	case BPMEM_SU_SSIZE+6:
	case BPMEM_SU_SSIZE+8:
	case BPMEM_SU_SSIZE+10:
	case BPMEM_SU_SSIZE+12:
	case BPMEM_SU_SSIZE+14:
		SetRegName(BPMEM_SU_SSIZE);
		// TODO: Description
		break;

	case BPMEM_SU_TSIZE: // 0x31
	case BPMEM_SU_TSIZE+2:
	case BPMEM_SU_TSIZE+4:
	case BPMEM_SU_TSIZE+6:
	case BPMEM_SU_TSIZE+8:
	case BPMEM_SU_TSIZE+10:
	case BPMEM_SU_TSIZE+12:
	case BPMEM_SU_TSIZE+14:
		SetRegName(BPMEM_SU_TSIZE);
		// TODO: Description
		break;

	case BPMEM_ZMODE: // 0x40
		SetRegName(BPMEM_ZMODE);
		// TODO: Description
		break;

	case BPMEM_BLENDMODE: // 0x41
		{
			SetRegName(BPMEM_BLENDMODE);
			BlendMode mode; mode.hex = cmddata;
			const char* dstfactors[] = { "0", "1", "src_color", "1-src_color", "src_alpha", "1-src_alpha", "dst_alpha", "1-dst_alpha" };
			const char* srcfactors[] = { "0", "1", "dst_color", "1-dst_color", "src_alpha", "1-src_alpha", "dst_alpha", "1-dst_alpha" };
			const char* logicmodes[] = { "0", "s & d", "s & ~d", "s", "~s & d", "d", "s ^ d", "s | d", "~(s | d)", "~(s ^ d)", "~d", "s | ~d", "~s", "~s | d", "~(s & d)", "1" };
			*desc = StringFromFormat("Enable: %s\n"
			                         "Logic ops: %s\n"
			                         "Dither: %s\n"
			                         "Color write: %s\n"
			                         "Alpha write: %s\n"
			                         "Dest factor: %s\n"
			                         "Source factor: %s\n"
			                         "Subtract: %s\n"
			                         "Logic mode: %s\n",
			                         no_yes[mode.blendenable], no_yes[mode.logicopenable], no_yes[mode.dither],
			                         no_yes[mode.colorupdate], no_yes[mode.alphaupdate], dstfactors[mode.dstfactor],
			                         srcfactors[mode.srcfactor], no_yes[mode.subtract], logicmodes[mode.logicmode]);
		}
		break;

	case BPMEM_CONSTANTALPHA: // 0x42
		SetRegName(BPMEM_CONSTANTALPHA);
		// TODO: Description
		break;

	case BPMEM_ZCOMPARE: // 0x43
		{
			SetRegName(BPMEM_ZCOMPARE);
			PEControl config; config.hex = cmddata;
			const char* pixel_formats[] = { "RGB8_Z24", "RGBA6_Z24", "RGB565_Z16", "Z24", "Y8", "U8", "V8", "YUV420" };
			const char* zformats[] = { "linear", "compressed (near)", "compressed (mid)", "compressed (far)", "inv linear", "compressed (inv near)", "compressed (inv mid)", "compressed (inv far)" };
			*desc = StringFromFormat("EFB pixel format: %s\n"
			                         "Depth format: %s\n"
			                         "Early depth test: %s\n",
			                         pixel_formats[config.pixel_format], zformats[config.zformat], no_yes[config.early_ztest]);
		}
		break;

	case BPMEM_FIELDMASK: // 0x44
		SetRegName(BPMEM_FIELDMASK);
		// TODO: Description
		break;

	case BPMEM_SETDRAWDONE: // 0x45
		SetRegName(BPMEM_SETDRAWDONE);
		// TODO: Description
		break;

	case BPMEM_BUSCLOCK0: // 0x46
		SetRegName(BPMEM_BUSCLOCK0);
		// TODO: Description
		break;

	case BPMEM_PE_TOKEN_ID: // 0x47
		SetRegName(BPMEM_PE_TOKEN_ID);
		// TODO: Description
		break;

	case BPMEM_PE_TOKEN_INT_ID: // 0x48
		SetRegName(BPMEM_PE_TOKEN_INT_ID);
		// TODO: Description
		break;

	case BPMEM_EFB_TL: // 0x49
		{
			SetRegName(BPMEM_EFB_TL);
			X10Y10 left_top; left_top.hex = cmddata;
			*desc = StringFromFormat("Left: %d\nTop: %d", left_top.x, left_top.y);
		}
		break;

	case BPMEM_EFB_BR: // 0x4A
		{
			// TODO: Misleading name, should be BPMEM_EFB_WH instead
			SetRegName(BPMEM_EFB_BR);
			X10Y10 width_height; width_height.hex = cmddata;
			*desc = StringFromFormat("Width: %d\nHeight: %d", width_height.x+1, width_height.y+1);
		}
		break;

	case BPMEM_EFB_ADDR: // 0x4B
		SetRegName(BPMEM_EFB_ADDR);
		*desc = StringFromFormat("Target address (32 byte aligned): 0x%06X", cmddata << 5);
		break;

	case BPMEM_MIPMAP_STRIDE: // 0x4D
		SetRegName(BPMEM_MIPMAP_STRIDE);
		// TODO: Description
		break;

	case BPMEM_COPYYSCALE: // 0x4E
		SetRegName(BPMEM_COPYYSCALE);
		*desc = StringFromFormat("Scaling factor (XFB copy only): 0x%X (%f or inverted %f)", cmddata, (float)cmddata/256.f, 256.f/(float)cmddata);
		break;

	case BPMEM_CLEAR_AR: // 0x4F
		SetRegName(BPMEM_CLEAR_AR);
		*desc = StringFromFormat("Alpha: 0x%02X\nRed: 0x%02X", (cmddata&0xFF00)>>8, cmddata&0xFF);
		break;

	case BPMEM_CLEAR_GB: // 0x50
		SetRegName(BPMEM_CLEAR_GB);
		*desc = StringFromFormat("Green: 0x%02X\nBlue: 0x%02X", (cmddata&0xFF00)>>8, cmddata&0xFF);
		break;

	case BPMEM_CLEAR_Z: // 0x51
		SetRegName(BPMEM_CLEAR_Z);
		*desc = StringFromFormat("Z value: 0x%06X", cmddata);
		break;

	case BPMEM_TRIGGER_EFB_COPY: // 0x52
		{
			SetRegName(BPMEM_TRIGGER_EFB_COPY);
			UPE_Copy copy; copy.Hex = cmddata;
			*desc = StringFromFormat("Clamping: %s\n"
			                         "Converting from RGB to YUV: %s\n"
			                         "Target pixel format: 0x%X\n"
			                         "Gamma correction: %s\n"
			                         "Mipmap filter: %s\n"
			                         "Vertical scaling: %s\n"
			                         "Clear: %s\n"
			                         "Frame to field: 0x%01X\n"
			                         "Copy to XFB: %s\n"
			                         "Intensity format: %s\n"
			                         "Automatic color conversion: %s",
			                         (copy.clamp0 && copy.clamp1) ? "Top and Bottom" : (copy.clamp0) ? "Top only" : (copy.clamp1) ? "Bottom only" : "None",
			                         no_yes[copy.yuv],
			                         copy.tp_realFormat(),
			                         (copy.gamma==0)?"1.0":(copy.gamma==1)?"1.7":(copy.gamma==2)?"2.2":"Invalid value 0x3?",
			                         no_yes[copy.half_scale],
			                         no_yes[copy.scale_invert],
			                         no_yes[copy.clear],
			                         (u32)copy.frame_to_field,
			                         no_yes[copy.copy_to_xfb],
			                         no_yes[copy.intensity_fmt],
			                         no_yes[copy.auto_conv]);
		}
		break;

	case BPMEM_COPYFILTER0: // 0x53
		SetRegName(BPMEM_COPYFILTER0);
		// TODO: Description
		break;

	case BPMEM_COPYFILTER1: // 0x54
		SetRegName(BPMEM_COPYFILTER1);
		// TODO: Description
		break;

	case BPMEM_CLEARBBOX1: // 0x55
		SetRegName(BPMEM_CLEARBBOX1);
		// TODO: Description
		break;

	case BPMEM_CLEARBBOX2: // 0x56
		SetRegName(BPMEM_CLEARBBOX2);
		// TODO: Description
		break;

	case BPMEM_CLEAR_PIXEL_PERF: // 0x57
		SetRegName(BPMEM_CLEAR_PIXEL_PERF);
		// TODO: Description
		break;

	case BPMEM_REVBITS: // 0x58
		SetRegName(BPMEM_REVBITS);
		// TODO: Description
		break;

	case BPMEM_SCISSOROFFSET: // 0x59
		SetRegName(BPMEM_SCISSOROFFSET);
		// TODO: Description
		break;

	case BPMEM_PRELOAD_ADDR: // 0x60
		SetRegName(BPMEM_PRELOAD_ADDR);
		// TODO: Description
		break;

	case BPMEM_PRELOAD_TMEMEVEN: // 0x61
		SetRegName(BPMEM_PRELOAD_TMEMEVEN);
		// TODO: Description
		break;

	case BPMEM_PRELOAD_TMEMODD: // 0x62
		SetRegName(BPMEM_PRELOAD_TMEMODD);
		// TODO: Description
		break;

	case BPMEM_PRELOAD_MODE: // 0x63
		SetRegName(BPMEM_PRELOAD_MODE);
		// TODO: Description
		break;

	case BPMEM_LOADTLUT0: // 0x64
		SetRegName(BPMEM_LOADTLUT0);
		// TODO: Description
		break;

	case BPMEM_LOADTLUT1: // 0x65
		SetRegName(BPMEM_LOADTLUT1);
		// TODO: Description
		break;

	case BPMEM_TEXINVALIDATE: // 0x66
		SetRegName(BPMEM_TEXINVALIDATE);
		// TODO: Description
		break;

	case BPMEM_PERF1: // 0x67
		SetRegName(BPMEM_PERF1);
		// TODO: Description
		break;

	case BPMEM_FIELDMODE: // 0x68
		SetRegName(BPMEM_FIELDMODE);
		// TODO: Description
		break;

	case BPMEM_BUSCLOCK1: // 0x69
		SetRegName(BPMEM_BUSCLOCK1);
		// TODO: Description
		break;

	case BPMEM_TX_SETMODE0: // 0x80
	case BPMEM_TX_SETMODE0+1:
	case BPMEM_TX_SETMODE0+2:
	case BPMEM_TX_SETMODE0+3:
		SetRegName(BPMEM_TX_SETMODE0);
		// TODO: Description
		break;

	case BPMEM_TX_SETMODE1: // 0x84
	case BPMEM_TX_SETMODE1+1:
	case BPMEM_TX_SETMODE1+2:
	case BPMEM_TX_SETMODE1+3:
		SetRegName(BPMEM_TX_SETMODE1);
		// TODO: Description
		break;

	case BPMEM_TX_SETIMAGE0: // 0x88
	case BPMEM_TX_SETIMAGE0+1:
	case BPMEM_TX_SETIMAGE0+2:
	case BPMEM_TX_SETIMAGE0+3:
	case BPMEM_TX_SETIMAGE0_4: // 0xA8
	case BPMEM_TX_SETIMAGE0_4+1:
	case BPMEM_TX_SETIMAGE0_4+2:
	case BPMEM_TX_SETIMAGE0_4+3:
		SetRegName(BPMEM_TX_SETIMAGE0);
		// TODO: Description
		break;

	case BPMEM_TX_SETIMAGE1: // 0x8C
	case BPMEM_TX_SETIMAGE1+1:
	case BPMEM_TX_SETIMAGE1+2:
	case BPMEM_TX_SETIMAGE1+3:
	case BPMEM_TX_SETIMAGE1_4: // 0xAC
	case BPMEM_TX_SETIMAGE1_4+1:
	case BPMEM_TX_SETIMAGE1_4+2:
	case BPMEM_TX_SETIMAGE1_4+3:
		SetRegName(BPMEM_TX_SETIMAGE1);
		// TODO: Description
		break;

	case BPMEM_TX_SETIMAGE2: // 0x90
	case BPMEM_TX_SETIMAGE2+1:
	case BPMEM_TX_SETIMAGE2+2:
	case BPMEM_TX_SETIMAGE2+3:
	case BPMEM_TX_SETIMAGE2_4: // 0xB0
	case BPMEM_TX_SETIMAGE2_4+1:
	case BPMEM_TX_SETIMAGE2_4+2:
	case BPMEM_TX_SETIMAGE2_4+3:
		SetRegName(BPMEM_TX_SETIMAGE2);
		// TODO: Description
		break;

	case BPMEM_TX_SETIMAGE3: // 0x94
	case BPMEM_TX_SETIMAGE3+1:
	case BPMEM_TX_SETIMAGE3+2:
	case BPMEM_TX_SETIMAGE3+3:
	case BPMEM_TX_SETIMAGE3_4: // 0xB4
	case BPMEM_TX_SETIMAGE3_4+1:
	case BPMEM_TX_SETIMAGE3_4+2:
	case BPMEM_TX_SETIMAGE3_4+3:
		{
			SetRegName(BPMEM_TX_SETIMAGE3);
			TexImage3 teximg; teximg.hex = cmddata;
			*desc = StringFromFormat("Source address (32 byte aligned): 0x%06X", teximg.image_base << 5);
		}
		break;

	case BPMEM_TX_SETTLUT: // 0x98
	case BPMEM_TX_SETTLUT+1:
	case BPMEM_TX_SETTLUT+2:
	case BPMEM_TX_SETTLUT+3:
	case BPMEM_TX_SETTLUT_4: // 0xB8
	case BPMEM_TX_SETTLUT_4+1:
	case BPMEM_TX_SETTLUT_4+2:
	case BPMEM_TX_SETTLUT_4+3:
		SetRegName(BPMEM_TX_SETTLUT);
		// TODO: Description
		break;

	case BPMEM_TEV_COLOR_ENV: // 0xC0
	case BPMEM_TEV_COLOR_ENV+2:
	case BPMEM_TEV_COLOR_ENV+4:
	case BPMEM_TEV_COLOR_ENV+8:
	case BPMEM_TEV_COLOR_ENV+10:
	case BPMEM_TEV_COLOR_ENV+12:
	case BPMEM_TEV_COLOR_ENV+14:
	case BPMEM_TEV_COLOR_ENV+16:
	case BPMEM_TEV_COLOR_ENV+18:
	case BPMEM_TEV_COLOR_ENV+20:
	case BPMEM_TEV_COLOR_ENV+22:
	case BPMEM_TEV_COLOR_ENV+24:
	case BPMEM_TEV_COLOR_ENV+26:
	case BPMEM_TEV_COLOR_ENV+28:
	case BPMEM_TEV_COLOR_ENV+30:
		{
			SetRegName(BPMEM_TEV_COLOR_ENV);
			TevStageCombiner::ColorCombiner cc; cc.hex = cmddata;
			const char* tevin[] =
			{
				"prev.rgb", "prev.aaa",
				"c0.rgb", "c0.aaa",
				"c1.rgb", "c1.aaa",
				"c2.rgb", "c2.aaa",
				"tex.rgb", "tex.aaa",
				"ras.rgb", "ras.aaa",
				"ONE", "HALF", "konst.rgb", "ZERO",
			};
			const char* tevbias[] = { "0", "+0.5", "-0.5", "compare" };
			const char* tevop[] = { "add", "sub" };
			const char* tevscale[] = { "1", "2", "4", "0.5" };
			const char* tevout[] = { "prev.rgb", "c0.rgb", "c1.rgb", "c2.rgb" };
			*desc = StringFromFormat("Tev stage: %d\n"
			                         "a: %s\n"
			                         "b: %s\n"
			                         "c: %s\n"
			                         "d: %s\n"
			                         "Bias: %s\n"
			                         "Op: %s\n"
			                         "Clamp: %s\n"
			                         "Scale factor: %s\n"
			                         "Dest: %s\n",
			                         (data[0] - BPMEM_TEV_COLOR_ENV)/2, tevin[cc.a], tevin[cc.b], tevin[cc.c], tevin[cc.d],
			                         tevbias[cc.bias], tevop[cc.op], no_yes[cc.clamp], tevscale[cc.shift], tevout[cc.dest]);
			break;
		}

	case BPMEM_TEV_ALPHA_ENV: // 0xC1
	case BPMEM_TEV_ALPHA_ENV+2:
	case BPMEM_TEV_ALPHA_ENV+4:
	case BPMEM_TEV_ALPHA_ENV+6:
	case BPMEM_TEV_ALPHA_ENV+8:
	case BPMEM_TEV_ALPHA_ENV+10:
	case BPMEM_TEV_ALPHA_ENV+12:
	case BPMEM_TEV_ALPHA_ENV+14:
	case BPMEM_TEV_ALPHA_ENV+16:
	case BPMEM_TEV_ALPHA_ENV+18:
	case BPMEM_TEV_ALPHA_ENV+20:
	case BPMEM_TEV_ALPHA_ENV+22:
	case BPMEM_TEV_ALPHA_ENV+24:
	case BPMEM_TEV_ALPHA_ENV+26:
	case BPMEM_TEV_ALPHA_ENV+28:
	case BPMEM_TEV_ALPHA_ENV+30:
		{
			SetRegName(BPMEM_TEV_ALPHA_ENV);
			TevStageCombiner::AlphaCombiner ac; ac.hex = cmddata;
			const char* tevin[] =
			{
				"prev", "c0", "c1", "c2",
				"tex", "ras", "konst", "ZERO",
			};
			const char* tevbias[] = { "0", "+0.5", "-0.5", "compare" };
			const char* tevop[] = { "add", "sub" };
			const char* tevscale[] = { "1", "2", "4", "0.5" };
			const char* tevout[] = { "prev", "c0", "c1", "c2" };
			*desc = StringFromFormat("Tev stage: %d\n"
			                         "a: %s\n"
			                         "b: %s\n"
			                         "c: %s\n"
			                         "d: %s\n"
			                         "Bias: %s\n"
			                         "Op: %s\n"
			                         "Clamp: %s\n"
			                         "Scale factor: %s\n"
			                         "Dest: %s\n"
			                         "Ras sel: %d\n"
			                         "Tex sel: %d\n",
			                         (data[0] - BPMEM_TEV_ALPHA_ENV)/2, tevin[ac.a], tevin[ac.b], tevin[ac.c], tevin[ac.d],
			                         tevbias[ac.bias], tevop[ac.op], no_yes[ac.clamp], tevscale[ac.shift], tevout[ac.dest],
			                         ac.rswap, ac.tswap);
			break;
		}

	case BPMEM_TEV_COLOR_RA: // 0xE0
	case BPMEM_TEV_COLOR_RA + 2: // 0xE2
	case BPMEM_TEV_COLOR_RA + 4: // 0xE4
	case BPMEM_TEV_COLOR_RA + 6: // 0xE6
		SetRegName(BPMEM_TEV_COLOR_RA);
		// TODO: Description
		break;

	case BPMEM_TEV_COLOR_BG: // 0xE1
	case BPMEM_TEV_COLOR_BG + 2: // 0xE3
	case BPMEM_TEV_COLOR_BG + 4: // 0xE5
	case BPMEM_TEV_COLOR_BG + 6: // 0xE7
		SetRegName(BPMEM_TEV_COLOR_BG);
		// TODO: Description
		break;

	case BPMEM_FOGRANGE: // 0xE8
	case BPMEM_FOGRANGE+1:
	case BPMEM_FOGRANGE+2:
	case BPMEM_FOGRANGE+3:
	case BPMEM_FOGRANGE+4:
	case BPMEM_FOGRANGE+5:
		SetRegName(BPMEM_FOGRANGE);
		// TODO: Description
		break;

	case BPMEM_FOGPARAM0: // 0xEE
		SetRegName(BPMEM_FOGPARAM0);
		// TODO: Description
		break;

	case BPMEM_FOGBMAGNITUDE: // 0xEF
		SetRegName(BPMEM_FOGBMAGNITUDE);
		// TODO: Description
		break;

	case BPMEM_FOGBEXPONENT: // 0xF0
		SetRegName(BPMEM_FOGBEXPONENT);
		// TODO: Description
		break;

	case BPMEM_FOGPARAM3: // 0xF1
		SetRegName(BPMEM_FOGPARAM3);
		// TODO: Description
		break;

	case BPMEM_FOGCOLOR: // 0xF2
		SetRegName(BPMEM_FOGCOLOR);
		// TODO: Description
		break;

	case BPMEM_ALPHACOMPARE: // 0xF3
		{
			SetRegName(BPMEM_ALPHACOMPARE);
			AlphaTest test; test.hex = cmddata;
			const char* functions[] = { "NEVER", "LESS", "EQUAL", "LEQUAL", "GREATER", "NEQUAL", "GEQUAL", "ALWAYS" };
			const char* logic[] = { "AND", "OR", "XOR", "XNOR" };
			*desc = StringFromFormat("Test 1: %s (ref: %#02x)\n"
			                         "Test 2: %s (ref: %#02x)\n"
			                         "Logic: %s\n",
			                         functions[test.comp0], (int)test.ref0, functions[test.comp1], (int)test.ref1, logic[test.logic]);
			break;
		}

	case BPMEM_BIAS: // 0xF4
		SetRegName(BPMEM_BIAS);
		// TODO: Description
		break;

	case BPMEM_ZTEX2: // 0xF5
		SetRegName(BPMEM_ZTEX2);
		// TODO: Description
		break;

	case BPMEM_TEV_KSEL: // 0xF6
	case BPMEM_TEV_KSEL+1:
	case BPMEM_TEV_KSEL+2:
	case BPMEM_TEV_KSEL+3:
	case BPMEM_TEV_KSEL+4:
	case BPMEM_TEV_KSEL+5:
	case BPMEM_TEV_KSEL+6:
	case BPMEM_TEV_KSEL+7:
		SetRegName(BPMEM_TEV_KSEL);
		// TODO: Description
		break;

#undef SetRegName
	}
}

// Called when loading a saved state.
void BPReload()
{
	// restore anything that goes straight to the renderer.
	// let's not risk actually replaying any writes.
	// note that PixelShaderManager is already covered since it has its own DoState.
	SetGenerationMode();
	SetScissor();
	SetLineWidth();
	SetDepthMode();
	SetLogicOpMode();
	SetDitherMode();
	SetBlendMode();
	SetColorMask();
	OnPixelFormatChange();
}
