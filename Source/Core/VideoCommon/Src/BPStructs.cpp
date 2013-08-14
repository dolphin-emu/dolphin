// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>

#include "VideoConfig.h"
#include "Statistics.h"
#include "RenderBase.h"
#include "VideoCommon.h"
#include "PixelShaderManager.h"
#include "PixelEngine.h"
#include "BPFunctions.h"
#include "BPStructs.h"
#include "TextureDecoder.h"
#include "OpcodeDecoding.h"
#include "VertexLoader.h"
#include "VertexShaderManager.h"
#include "Thread.h"
#include "HW/Memmap.h"
#include "PerfQueryBase.h"

using namespace BPFunctions;

static u32 mapTexAddress;
static bool mapTexFound;
static int numWrites;

extern volatile bool g_bSkipCurrentFrame;

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

	mapTexAddress = 0;
	numWrites = 0;
	mapTexFound = false;
}

void RenderToXFB(const BPCmd &bp, const EFBRectangle &rc, float yScale, float xfbLines, u32 xfbAddr, const u32 dstWidth, const u32 dstHeight, float gamma)
{
	Renderer::RenderToXFB(xfbAddr, dstWidth, dstHeight, rc, gamma);
}
void BPWritten(const BPCmd& bp)
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
	NOTE2: Yet Another Gamecube Documentation calls them Bypass Raster State Registers but possibly completely wrong
	NOTE3: This controls the register groups: RAS1/2, SU, TF, TEV, C/Z, PEC
	TODO: Turn into function table. The (future) DisplayList (DL) jit can then call the functions directly,
		  getting rid of dynamic dispatch. Unfortunately, few games use DLs properly - most\
		  just stuff geometry in them and don't put state changes there
	----------------------------------------------------------------------------------------------------------------
	*/
	
	// check for invalid state, else unneeded configuration are built
	g_video_backend->CheckInvalidState();

	// Debugging only, this lets you skip a bp update
	//static int times = 0;
	//static bool enable = false;

	//switch (bp.address)
	//{
	//case BPMEM_CONSTANTALPHA:
	//	{
	//	if (times-- == 0 && enable)
	//		return;
	//	else
	//		break;
	//	}
	//default: break;
	//}

	// FIXME: Hangs load-state, but should fix graphic-heavy games state loading
	//std::lock_guard<std::mutex> lk(s_bpCritical);

	// BEGIN ZTP SPEEDUP HACK CHANGES
	// This hunk of code disables the usual pipeline flush for certain BP Writes
	// that occur while the minimap is being drawn in Zelda: Twilight Princess.
	// This significantly increases speed while in hyrule field. In depth discussion
	// on how this Hack came to be can be found at: http://forums.dolphin-emu.com/thread-10638.html
	// -fircrestsk8
	if (g_ActiveConfig.bZTPSpeedHack)
	{
		if (!mapTexFound)
		{
			if (bp.address != BPMEM_TEV_COLOR_ENV && bp.address != BPMEM_TEV_ALPHA_ENV)
			{
				numWrites = 0;
			}
			else if (++numWrites >= 100)	// seem that if 100 consecutive BP writes are called to either of these addresses in ZTP, 
			{								// then it is safe to assume the map texture address is currently loaded into the BP memory
				mapTexAddress = bpmem.tex[0].texImage3[0].hex << 5;
				mapTexFound = true;
				WARN_LOG(VIDEO, "\nZTP map texture found at address %08x\n", mapTexAddress);
			}
			FlushPipeline();
		}
		else if ( (bpmem.tex[0].texImage3[0].hex << 5) != mapTexAddress ||
					bpmem.tevorders[0].getEnable(0) == 0 ||
					bp.address == BPMEM_TREF)
		{
			FlushPipeline();
		}
	}  // END ZTP SPEEDUP HACK
	else 
	{
		if (((s32*)&bpmem)[bp.address] == bp.newvalue)
		{
			if (!(bp.address == BPMEM_TRIGGER_EFB_COPY
					|| bp.address == BPMEM_CLEARBBOX1
					|| bp.address == BPMEM_CLEARBBOX2
					|| bp.address == BPMEM_SETDRAWDONE
					|| bp.address == BPMEM_PE_TOKEN_ID
					|| bp.address == BPMEM_PE_TOKEN_INT_ID
					|| bp.address == BPMEM_LOADTLUT0
					|| bp.address == BPMEM_LOADTLUT1
					|| bp.address == BPMEM_TEXINVALIDATE
					|| bp.address == BPMEM_PRELOAD_MODE
					|| bp.address == BPMEM_CLEAR_PIXEL_PERF))
			{
				return;
			}
		}

		FlushPipeline();
	}

	((u32*)&bpmem)[bp.address] = bp.newvalue;
	
	switch (bp.address)
	{
	case BPMEM_GENMODE: // Set the Generation Mode
		{
			PRIM_LOG("genmode: texgen=%d, col=%d, multisampling=%d, tev=%d, cullmode=%d, ind=%d, zfeeze=%d",
			bpmem.genMode.numtexgens, bpmem.genMode.numcolchans,
			bpmem.genMode.multisampling, bpmem.genMode.numtevstages+1, bpmem.genMode.cullmode,
			bpmem.genMode.numindstages, bpmem.genMode.zfreeze);

			// Only call SetGenerationMode when cull mode changes.
			if (bp.changes & 0xC000)
				SetGenerationMode();
			break;
		}
	case BPMEM_IND_MTXA: // Index Matrix Changed
	case BPMEM_IND_MTXB:
	case BPMEM_IND_MTXC:
	case BPMEM_IND_MTXA+3:
	case BPMEM_IND_MTXB+3:
	case BPMEM_IND_MTXC+3:
	case BPMEM_IND_MTXA+6:
	case BPMEM_IND_MTXB+6:
	case BPMEM_IND_MTXC+6:
		PixelShaderManager::SetIndMatrixChanged((bp.address - BPMEM_IND_MTXA) / 3);
		break;
	case BPMEM_RAS1_SS0: // Index Texture Coordinate Scale 0
		PixelShaderManager::SetIndTexScaleChanged(0x03);
	case BPMEM_RAS1_SS1: // Index Texture Coordinate Scale 1
		PixelShaderManager::SetIndTexScaleChanged(0x0c);
		break;
	// ----------------
	// Scissor Control
	// ----------------
	case BPMEM_SCISSORTL: // Scissor Rectable Top, Left
	case BPMEM_SCISSORBR: // Scissor Rectable Bottom, Right
	case BPMEM_SCISSOROFFSET: // Scissor Offset
		SetScissor();
		break;
	case BPMEM_LINEPTWIDTH: // Line Width
		SetLineWidth();
		break;
	case BPMEM_ZMODE: // Depth Control
		PRIM_LOG("zmode: test=%d, func=%d, upd=%d", bpmem.zmode.testenable, bpmem.zmode.func,
		bpmem.zmode.updateenable);
		SetDepthMode();
		break;
	case BPMEM_BLENDMODE: // Blending Control
		{
			if (bp.changes & 0xFFFF)
			{
				PRIM_LOG("blendmode: en=%d, open=%d, colupd=%d, alphaupd=%d, dst=%d, src=%d, sub=%d, mode=%d", 
					bpmem.blendmode.blendenable, bpmem.blendmode.logicopenable, bpmem.blendmode.colorupdate, bpmem.blendmode.alphaupdate,
					bpmem.blendmode.dstfactor, bpmem.blendmode.srcfactor, bpmem.blendmode.subtract, bpmem.blendmode.logicmode);

				// Set LogicOp Blending Mode
				if (bp.changes & 2)
					SetLogicOpMode();

				// Set Dithering Mode
				if (bp.changes & 4)
					SetDitherMode();

				// Set Blending Mode
				if (bp.changes & 0xFF1)
					SetBlendMode();

				// Set Color Mask
				if (bp.changes & 0x18)
					SetColorMask();
			}
			break;
		}
	case BPMEM_CONSTANTALPHA: // Set Destination Alpha
		{
			PRIM_LOG("constalpha: alp=%d, en=%d", bpmem.dstalpha.alpha, bpmem.dstalpha.enable);
			PixelShaderManager::SetDestAlpha(bpmem.dstalpha);
			if(bp.changes & 0x100)
				SetBlendMode();
			break;
		}
	// This is called when the game is done drawing the new frame (eg: like in DX: Begin(); Draw(); End();)
	// Triggers an interrupt on the PPC side so that the game knows when the GPU has finished drawing.
	// Tokens are similar.
	case BPMEM_SETDRAWDONE:
		switch (bp.newvalue & 0xFF)
		{
		case 0x02:
			PixelEngine::SetFinish(); // may generate interrupt
			DEBUG_LOG(VIDEO, "GXSetDrawDone SetPEFinish (value: 0x%02X)", (bp.newvalue & 0xFFFF));
			break;

		default:
			WARN_LOG(VIDEO, "GXSetDrawDone ??? (value 0x%02X)", (bp.newvalue & 0xFFFF));
			break;
		}
		break;
	case BPMEM_PE_TOKEN_ID: // Pixel Engine Token ID
		PixelEngine::SetToken(static_cast<u16>(bp.newvalue & 0xFFFF), false);
		DEBUG_LOG(VIDEO, "SetPEToken 0x%04x", (bp.newvalue & 0xFFFF));
		break;
	case BPMEM_PE_TOKEN_INT_ID: // Pixel Engine Interrupt Token ID
		PixelEngine::SetToken(static_cast<u16>(bp.newvalue & 0xFFFF), true);
		DEBUG_LOG(VIDEO, "SetPEToken + INT 0x%04x", (bp.newvalue & 0xFFFF));
		break;
	// ------------------------
	// EFB copy command. This copies a rectangle from the EFB to either RAM in a texture format or to XFB as YUYV.
	// It can also optionally clear the EFB while copying from it. To emulate this, we of course copy first and clear afterwards.
	case BPMEM_TRIGGER_EFB_COPY: // Copy EFB Region or Render to the XFB or Clear the screen.
		{
			// The bottom right is within the rectangle
			// The values in bpmem.copyTexSrcXY and bpmem.copyTexSrcWH are updated in case 0x49 and 0x4a in this function

			EFBRectangle rc;
			rc.left = (int)bpmem.copyTexSrcXY.x;
			rc.top = (int)bpmem.copyTexSrcXY.y;
			
			// Here Width+1 like Height, otherwise some textures are corrupted already since the native resolution.
			rc.right = (int)(bpmem.copyTexSrcXY.x + bpmem.copyTexSrcWH.x + 1);
			rc.bottom = (int)(bpmem.copyTexSrcXY.y + bpmem.copyTexSrcWH.y + 1);

			UPE_Copy PE_copy = bpmem.triggerEFBCopy;

			// Check if we are to copy from the EFB or draw to the XFB
			if (PE_copy.copy_to_xfb == 0)
			{
				if (GetConfig(CONFIG_SHOWEFBREGIONS))
					stats.efb_regions.push_back(rc);

				CopyEFB(bpmem.copyTexDest << 5, PE_copy.tp_realFormat(),
					bpmem.zcontrol.pixel_format, rc, PE_copy.intensity_fmt,
					PE_copy.half_scale);
			}
			else
			{
				// We should be able to get away with deactivating the current bbox tracking
				// here. Not sure if there's a better spot to put this.
				// the number of lines copied is determined by the y scale * source efb height
			
				PixelEngine::bbox_active = false;

				float yScale;
				if (PE_copy.scale_invert)
					yScale = 256.0f / (float)bpmem.dispcopyyscale;
				else
					yScale = (float)bpmem.dispcopyyscale / 256.0f;

				float xfbLines = ((bpmem.copyTexSrcWH.y + 1.0f) * yScale);
				if ((u32)xfbLines > MAX_XFB_HEIGHT)
				{
					INFO_LOG(VIDEO, "Tried to scale EFB to too many XFB lines (%f)", xfbLines);
					xfbLines = MAX_XFB_HEIGHT;
				}

				RenderToXFB(bp, rc, yScale, xfbLines, 
									 bpmem.copyTexDest << 5, 
									 bpmem.copyMipMapStrideChannels << 4,
									 (u32)xfbLines,
									 s_gammaLUT[PE_copy.gamma]);
			}

			// Clear the rectangular region after copying it.
			if (PE_copy.clear)
			{
				ClearScreen(rc);
			}

			break;
		}
	case BPMEM_LOADTLUT0: // This one updates bpmem.tlutXferSrc, no need to do anything here.
		break;
	case BPMEM_LOADTLUT1: // Load a Texture Look Up Table
		{
			u32 tlutTMemAddr = (bp.newvalue & 0x3FF) << 9;
			u32 tlutXferCount = (bp.newvalue & 0x1FFC00) >> 5;

			u8 *ptr = 0;

			// TODO - figure out a cleaner way.
			if (GetConfig(CONFIG_ISWII))
				ptr = GetPointer(bpmem.tmem_config.tlut_src << 5);
			else
				ptr = GetPointer((bpmem.tmem_config.tlut_src & 0xFFFFF) << 5);

			if (ptr)
				memcpy_gc(texMem + tlutTMemAddr, ptr, tlutXferCount);
			else
				PanicAlert("Invalid palette pointer %08x %08x %08x", bpmem.tmem_config.tlut_src, bpmem.tmem_config.tlut_src << 5, (bpmem.tmem_config.tlut_src & 0xFFFFF)<< 5);

			break;
		}
	case BPMEM_FOGRANGE: // Fog Settings Control
	case BPMEM_FOGRANGE+1:
	case BPMEM_FOGRANGE+2:
	case BPMEM_FOGRANGE+3:
	case BPMEM_FOGRANGE+4:
	case BPMEM_FOGRANGE+5:
		if (!GetConfig(CONFIG_DISABLEFOG))
			PixelShaderManager::SetFogRangeAdjustChanged();
		break;
	case BPMEM_FOGPARAM0:
	case BPMEM_FOGBMAGNITUDE:
	case BPMEM_FOGBEXPONENT:
	case BPMEM_FOGPARAM3:
		if (!GetConfig(CONFIG_DISABLEFOG))
			PixelShaderManager::SetFogParamChanged();
		break;
	case BPMEM_FOGCOLOR: // Fog Color
		if (!GetConfig(CONFIG_DISABLEFOG))
			PixelShaderManager::SetFogColorChanged();
		break;
	case BPMEM_ALPHACOMPARE: // Compare Alpha Values
		PRIM_LOG("alphacmp: ref0=%d, ref1=%d, comp0=%d, comp1=%d, logic=%d", bpmem.alpha_test.ref0,
				bpmem.alpha_test.ref1, bpmem.alpha_test.comp0, bpmem.alpha_test.comp1, bpmem.alpha_test.logic);
		PixelShaderManager::SetAlpha(bpmem.alpha_test);
		g_renderer->SetColorMask();
		break;
	case BPMEM_BIAS: // BIAS
		PRIM_LOG("ztex bias=0x%x", bpmem.ztex1.bias);
		PixelShaderManager::SetZTextureBias(bpmem.ztex1.bias);
		break;
	case BPMEM_ZTEX2: // Z Texture type
		{
			if (bp.changes & 3)
				PixelShaderManager::SetZTextureTypeChanged();
			#if defined(_DEBUG) || defined(DEBUGFAST) 
			const char* pzop[] = {"DISABLE", "ADD", "REPLACE", "?"};
			const char* pztype[] = {"Z8", "Z16", "Z24", "?"};
			PRIM_LOG("ztex op=%s, type=%s", pzop[bpmem.ztex2.op], pztype[bpmem.ztex2.type]);
			#endif
			break;
		}
	// ----------------------------------
	// Display Copy Filtering Control - GX_SetCopyFilter(u8 aa,u8 sample_pattern[12][2],u8 vf,u8 vfilter[7])
	// Fields: Destination, Frame2Field, Gamma, Source
	// ----------------------------------
	case BPMEM_DISPLAYCOPYFILER:   // if (aa) { use sample_pattern } else { use 666666 }
	case BPMEM_DISPLAYCOPYFILER+1: // if (aa) { use sample_pattern } else { use 666666 }
	case BPMEM_DISPLAYCOPYFILER+2: // if (aa) { use sample_pattern } else { use 666666 }
	case BPMEM_DISPLAYCOPYFILER+3: // if (aa) { use sample_pattern } else { use 666666 }
	case BPMEM_COPYFILTER0:        // if (vf) { use vfilter } else { use 595000 }
	case BPMEM_COPYFILTER1:        // if (vf) { use vfilter } else { use 000015 }
		break;
	// -----------------------------------
	// Interlacing Control
	// -----------------------------------
	case BPMEM_FIELDMASK: // GX_SetFieldMask(u8 even_mask,u8 odd_mask)
	case BPMEM_FIELDMODE: // GX_SetFieldMode(u8 field_mode,u8 half_aspect_ratio)
		SetInterlacingMode(bp);
		break;
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
		break;
	// --------------
	// Clear Config
	// --------------
	case BPMEM_CLEAR_AR: // Alpha and Red Components
	case BPMEM_CLEAR_GB: // Green and Blue Components
	case BPMEM_CLEAR_Z:  // Z Components (24-bit Zbuffer)
		break;
	// -------------------------
	// Bounding Box Control
	// -------------------------
	case BPMEM_CLEARBBOX1:
	case BPMEM_CLEARBBOX2:
		{
			if(g_ActiveConfig.bUseBBox)
			{
				// Don't compute bounding box if this frame is being skipped!
				// Wrong but valid values are better than bogus values...
				if(g_bSkipCurrentFrame)
					break;

				if (bp.address == BPMEM_CLEARBBOX1)
				{
					int right = bp.newvalue >> 10;
					int left = bp.newvalue & 0x3ff;
			
					// We should only set these if bbox is calculated properly.
					PixelEngine::bbox[0] = left;
					PixelEngine::bbox[1] = right;
					PixelEngine::bbox_active = true;
				}
				else
				{
					int bottom = bp.newvalue >> 10;
					int top = bp.newvalue & 0x3ff;

					// We should only set these if bbox is calculated properly.
					PixelEngine::bbox[2] = top;
					PixelEngine::bbox[3] = bottom;
					PixelEngine::bbox_active = true;
				}
			}
		}
		break;
	case BPMEM_TEXINVALIDATE:
		// TODO: Needs some restructuring in TextureCacheBase.
		break;

	case BPMEM_ZCOMPARE:      // Set the Z-Compare and EFB pixel format
		OnPixelFormatChange();
		if(bp.changes & 7)
		{
			SetBlendMode(); // dual source could be activated by changing to PIXELFMT_RGBA6_Z24
			g_renderer->SetColorMask(); // alpha writing needs to be disabled if the new pixel format doesn't have an alpha channel
		}
		break;

	case BPMEM_MIPMAP_STRIDE: // MipMap Stride Channel
	case BPMEM_COPYYSCALE:    // Display Copy Y Scale
	case BPMEM_IREF:          /* 24 RID
								 21 BC3 - Ind. Tex Stage 3 NTexCoord
								 18 BI3 - Ind. Tex Stage 3 NTexMap
								 15 BC2 - Ind. Tex Stage 2 NTexCoord
								 12 BI2 - Ind. Tex Stage 2 NTexMap
								 9 BC1 - Ind. Tex Stage 1 NTexCoord
								 6 BI1 - Ind. Tex Stage 1 NTexMap
								 3 BC0 - Ind. Tex Stage 0 NTexCoord
								 0 BI0 - Ind. Tex Stage 0 NTexMap*/
	case BPMEM_TEV_KSEL:  // Texture Environment Swap Mode Table 0
	case BPMEM_TEV_KSEL+1:// Texture Environment Swap Mode Table 1
	case BPMEM_TEV_KSEL+2:// Texture Environment Swap Mode Table 2
	case BPMEM_TEV_KSEL+3:// Texture Environment Swap Mode Table 3
	case BPMEM_TEV_KSEL+4:// Texture Environment Swap Mode Table 4
	case BPMEM_TEV_KSEL+5:// Texture Environment Swap Mode Table 5
	case BPMEM_TEV_KSEL+6:// Texture Environment Swap Mode Table 6
	case BPMEM_TEV_KSEL+7:// Texture Environment Swap Mode Table 7
	case BPMEM_BP_MASK:   // This Register can be used to limit to which bits of BP registers is actually written to. the mask is
						  // only valid for the next BP command, and will reset itself.
	case BPMEM_IND_IMASK: // Index Mask ?
	case BPMEM_REVBITS: // Always set to 0x0F when GX_InitRevBits() is called.
		break;
 
	case BPMEM_CLEAR_PIXEL_PERF:
		// GXClearPixMetric writes 0xAAA here, Sunshine alternates this register between values 0x000 and 0xAAA
		g_perf_query->ResetQuery();
		break;

	case BPMEM_PRELOAD_ADDR:
	case BPMEM_PRELOAD_TMEMEVEN:
	case BPMEM_PRELOAD_TMEMODD: // Used when PRELOAD_MODE is set
		break;

	case BPMEM_PRELOAD_MODE: // Set to 0 when GX_TexModeSync() is called.
		// if this is different from 0, manual TMEM management is used (GX_PreloadEntireTexture).
		if (bp.newvalue != 0)
		{
			// TODO: Not quite sure if this is completely correct (likely not)
			// NOTE: libogc's implementation of GX_PreloadEntireTexture seems flawed, so it's not necessarily a good reference for RE'ing this feature.

			BPS_TmemConfig& tmem_cfg = bpmem.tmem_config;
			u8* src_ptr = Memory::GetPointer(tmem_cfg.preload_addr << 5); // TODO: Should we add mask here on GC?
			u32 size = tmem_cfg.preload_tile_info.count * TMEM_LINE_SIZE;
			u32 tmem_addr_even = tmem_cfg.preload_tmem_even * TMEM_LINE_SIZE;

			if (tmem_cfg.preload_tile_info.type != 3)
			{
				if (tmem_addr_even + size > TMEM_SIZE)
					size = TMEM_SIZE - tmem_addr_even;

				memcpy(texMem + tmem_addr_even, src_ptr, size);
			}
			else // RGBA8 tiles (and CI14, but that might just be stupid libogc!)
			{
				// AR and GB tiles are stored in separate TMEM banks => can't use a single memcpy for everything
				u32 tmem_addr_odd = tmem_cfg.preload_tmem_odd * TMEM_LINE_SIZE;

				for (u32 i = 0; i < tmem_cfg.preload_tile_info.count; ++i)
				{
					if (tmem_addr_even + TMEM_LINE_SIZE > TMEM_SIZE ||
						tmem_addr_odd  + TMEM_LINE_SIZE > TMEM_SIZE)
						break;

					memcpy(texMem + tmem_addr_even, src_ptr, TMEM_LINE_SIZE);
					memcpy(texMem + tmem_addr_odd, src_ptr + TMEM_LINE_SIZE, TMEM_LINE_SIZE);
					tmem_addr_even += TMEM_LINE_SIZE;
					tmem_addr_odd += TMEM_LINE_SIZE;
					src_ptr += TMEM_LINE_SIZE * 2;
				}
			}
		}
		break;

		// ------------------------------------------------
		// On Default, we try to look for other things
		// before we give up and say its an unknown opcode
		// ------------------------------------------------
	default:
		switch (bp.address & 0xFC)  // Texture sampler filter
		{
		// -------------------------
		// Texture Environment Order
		// -------------------------
		case BPMEM_TREF:
		case BPMEM_TREF+4:
			break;
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
			PixelShaderManager::SetTexCoordChanged((bp.address - BPMEM_SU_SSIZE) >> 1);
			break;
		// ------------------------
		// BPMEM_TX_SETMODE0 - (Texture lookup and filtering mode) LOD/BIAS Clamp, MaxAnsio, LODBIAS, DiagLoad, Min Filter, Mag Filter, Wrap T, S
		// BPMEM_TX_SETMODE1 - (LOD Stuff) - Max LOD, Min LOD
		// ------------------------
		case BPMEM_TX_SETMODE0: // (0x90 for linear)
		case BPMEM_TX_SETMODE0_4:
			break;

		case BPMEM_TX_SETMODE1:
		case BPMEM_TX_SETMODE1_4:
			break;
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
			break;
		// -------------------------------
		// Set a TLUT
		// BPMEM_TX_SETTLUT - Format, TMEM Offset (offset of TLUT from start of TMEM high bank > > 5)
		// -------------------------------
		case BPMEM_TX_SETTLUT:
		case BPMEM_TX_SETLUT_4:
			break;

		// ---------------------------------------------------
		// Set the TEV Color
		// ---------------------------------------------------
		case BPMEM_TEV_REGISTER_L:   // Reg 1
		case BPMEM_TEV_REGISTER_H:
		case BPMEM_TEV_REGISTER_L+2: // Reg 2
		case BPMEM_TEV_REGISTER_H+2:
		case BPMEM_TEV_REGISTER_L+4: // Reg 3
		case BPMEM_TEV_REGISTER_H+4:
		case BPMEM_TEV_REGISTER_L+6: // Reg 4
		case BPMEM_TEV_REGISTER_H+6:
			// some games only send the _L part, so always update
			// there actually are 2 register behind each of these
			// addresses, selected by the type bit.
			{
				// don't compare with changes!
				int num = (bp.address >> 1) & 0x3;
				if ((bp.address & 1) == 0)
					PixelShaderManager::SetColorChanged(bpmem.tevregs[num].low.type, num, false);
				else
					PixelShaderManager::SetColorChanged(bpmem.tevregs[num].high.type, num, true);
			}
			break;

		// ------------------------------------------------
		// On Default, we try to look for other things
		// before we give up and say its an unknown opcode
		// again ...
		// ------------------------------------------------
		default:
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
				break;
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
				break;
			default:
				WARN_LOG(VIDEO, "Unknown BP opcode: address = 0x%08x value = 0x%08x", bp.address, bp.newvalue);
				break;
			}
	}
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
	{
		BPCmd bp = {BPMEM_FIELDMASK, 0xFFFFFF, static_cast<int>(((u32*)&bpmem)[BPMEM_FIELDMASK])};
		SetInterlacingMode(bp);
	}
	{
		BPCmd bp = {BPMEM_FIELDMODE, 0xFFFFFF, static_cast<int>(((u32*)&bpmem)[BPMEM_FIELDMODE])};
		SetInterlacingMode(bp);
	}
}
