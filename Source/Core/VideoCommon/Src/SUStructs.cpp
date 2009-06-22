// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <cmath>

#include "Profiler.h"
#include "Statistics.h"
#include "VideoCommon.h"
#include "PixelShaderManager.h"
#include "SUFunctions.h"
#include "SUStructs.h"
#include "TextureDecoder.h"
#include "OpcodeDecoding.h"
#include "VertexLoader.h"
#include "VertexShaderManager.h"

using namespace SUFunctions;

void SUInit()
{
    memset(&sumem, 0, sizeof(sumem));
    sumem.suMask = 0xFFFFFF;
}

// ----------------------------------------------------------------------------------------------------------
// Write the bypass command to the Setup/Rasterization Memory
/* ------------------
   Called:
		At the end of every: OpcodeDecoding.cpp ExecuteDisplayList > Decode() > LoadSUReg
   TODO:
		Turn into function table. The (future) DisplayList (DL) jit can then call the functions directly,
		getting rid of dynamic dispatch. Unfortunately, few games use DLs properly - most\
		just stuff geometry in them and don't put state changes there. */
// ----------------------------------------------------------------------------------------------------------
void SUWritten(const BPCommand& bp)
{
	// --------------------------------------------------------------------------------------------------------
	// First the pipeline is flushed then update the sumem with the new value.
	// Some of the BP cases have to call certain functions while others just update the sumem.
	// some bp cases check the changes variable, because they might not have to be updated all the time
	// NOTE: it seems not all su cases like checking changes, so calling if (bp.changes == 0 ? false : true)
	//       had to be ditched and the games seem to work fine with out it.
	// --------------------------------------------------------------------------------------------------------

	// Debugging only, this lets you skip a bp update
	//static int times = 0;
	//static bool enable = false;

	//switch (bp.address)
	//{
	//case SUMEM_CONSTANTALPHA:
	//	{
	//	if (times-- == 0 && enable)
	//		return;
	//	else
	//		break;
	//	}
	//default: break;
	//}

	FlushPipeline();
    ((u32*)&sumem)[bp.address] = bp.newvalue;
	
	switch (bp.address)
	{
	case SUMEM_GENMODE: // Set the Generation Mode
		{
			PRIM_LOG("genmode: texgen=%d, col=%d, ms_en=%d, tev=%d, culmode=%d, ind=%d, zfeeze=%d",
			sumem.genMode.numtexgens, sumem.genMode.numcolchans,
			sumem.genMode.ms_en, sumem.genMode.numtevstages+1, sumem.genMode.cullmode,
			sumem.genMode.numindstages, sumem.genMode.zfreeze);
			SetGenerationMode(bp);
			break;
		}
	case SUMEM_IND_MTXA: // Index Matrix Changed
	case SUMEM_IND_MTXB:
	case SUMEM_IND_MTXC:
	case SUMEM_IND_MTXA+3:
	case SUMEM_IND_MTXB+3:
	case SUMEM_IND_MTXC+3:
	case SUMEM_IND_MTXA+6:
	case SUMEM_IND_MTXB+6:
	case SUMEM_IND_MTXC+6:
		PixelShaderManager::SetIndMatrixChanged((bp.address - SUMEM_IND_MTXA) / 3);
		break;
	case SUMEM_RAS1_SS0: // Index Texture Coordinate Scale 0
        PixelShaderManager::SetIndTexScaleChanged(0x03);
	case SUMEM_RAS1_SS1: // Index Texture Coordinate Scale 1
        PixelShaderManager::SetIndTexScaleChanged(0x0c);
		break;
	case SUMEM_SCISSORTL: // Scissor Rectable Top, Left
	case SUMEM_SCISSORBR: // Scissor Rectable Bottom, Right
	case SUMEM_SCISSOROFFSET: // Scissor Offset
		SetScissor(bp);
		break;
	case SUMEM_LINEPTWIDTH: // Line Width
		SetLineWidth(bp);
		break;
	case SUMEM_ZMODE: // Depth Control
		PRIM_LOG("zmode: test=%d, func=%d, upd=%d", sumem.zmode.testenable, sumem.zmode.func,
		sumem.zmode.updateenable);
		SetDepthMode(bp);
		break;
	case SUMEM_BLENDMODE: // Blending Control
		{
			if (bp.changes & 0xFFFF)
			{
				PRIM_LOG("blendmode: en=%d, open=%d, colupd=%d, alphaupd=%d, dst=%d, src=%d, sub=%d, mode=%d", 
					sumem.blendmode.blendenable, sumem.blendmode.logicopenable, sumem.blendmode.colorupdate, sumem.blendmode.alphaupdate,
					sumem.blendmode.dstfactor, sumem.blendmode.srcfactor, sumem.blendmode.subtract, sumem.blendmode.logicmode);
				// Set LogicOp Blending Mode
				if (bp.changes & 2)
				{
					SETSTAT(stats.logicOpMode, sumem.blendmode.logicopenable != 0 ? sumem.blendmode.logicmode : stats.logicOpMode);
					SetLogicOpMode(bp);
				}
				// Set Dithering Mode
				if (bp.changes & 4)
				{
					SETSTAT(stats.dither, sumem.blendmode.dither);
					SetDitherMode(bp);
				}
				// Set Blending Mode
				if (bp.changes & 0xFE1)
				{
					SETSTAT(stats.srcFactor, sumem.blendmode.srcfactor);
					SETSTAT(stats.dstFactor, sumem.blendmode.dstfactor);
					SetBlendMode(bp);
				}
				// Set Color Mask
				if (bp.changes & 0x18)
				{
					SETSTAT(stats.alphaUpdate, sumem.blendmode.alphaupdate);
					SETSTAT(stats.colorUpdate, sumem.blendmode.colorupdate);
					SetColorMask(bp);
				}
			}
			break;
		}
	case SUMEM_CONSTANTALPHA: // Set Destination Alpha
		{
			PRIM_LOG("constalpha: alp=%d, en=%d", sumem.dstalpha.alpha, sumem.dstalpha.enable);
			SETSTAT(stats.dstAlphaEnable, sumem.dstalpha.enable);
			SETSTAT_UINT(stats.dstAlpha, sumem.dstalpha.alpha);
			PixelShaderManager::SetDestAlpha(sumem.dstalpha);
			break;
		}
	// This is called when the game is done drawing the new frame (eg: like in DX: Begin(); Draw(); End();)
	case SUMEM_SETDRAWDONE: 
		switch (bp.newvalue & 0xFF)
        {
        case 0x02:
            g_VideoInitialize.pSetPEFinish(); // may generate interrupt
            DEBUG_LOG(VIDEO, "GXSetDrawDone SetPEFinish (value: 0x%02X)", (bp.newvalue & 0xFFFF));
            break;

        default:
            WARN_LOG(VIDEO, "GXSetDrawDone ??? (value 0x%02X)", (bp.newvalue & 0xFFFF));
            break;
        }
        break;
	case SUMEM_PE_TOKEN_ID: // Pixel Engine Token ID
        g_VideoInitialize.pSetPEToken(static_cast<u16>(bp.newvalue & 0xFFFF), FALSE);
        DEBUG_LOG(VIDEO, "SetPEToken 0x%04x", (bp.newvalue & 0xFFFF));
        break;
    case SUMEM_PE_TOKEN_INT_ID: // Pixel Engine Interrupt Token ID
        g_VideoInitialize.pSetPEToken(static_cast<u16>(bp.newvalue & 0xFFFF), TRUE);
        DEBUG_LOG(VIDEO, "SetPEToken + INT 0x%04x", (bp.newvalue & 0xFFFF));
        break;
	// ------------------------
	// EFB copy command. This copies a rectangle from the EFB to either RAM in a texture format or to XFB as YUYV.
	// It can also optionally clear the EFB while copying from it. To emulate this, we of course copy first and clear afterwards.
    case SUMEM_TRIGGER_EFB_COPY: // Copy EFB Region or Render to the XFB or Clear the screen.
		{
			DVSTARTSUBPROFILE("LoadSUReg:swap");
			// The bottom right is within the rectangle
			// The values in sumem.copyTexSrcXY and sumem.copyTexSrcWH are updated in case 0x49 and 0x4a in this function
			TRectangle rc = {
				(int)(sumem.copyTexSrcXY.x),
				(int)(sumem.copyTexSrcXY.y),
				(int)((sumem.copyTexSrcXY.x + sumem.copyTexSrcWH.x + 1)),
				(int)((sumem.copyTexSrcXY.y + sumem.copyTexSrcWH.y + 1))
			};

			float MValueX = GetRendererTargetScaleX();
			float MValueY = GetRendererTargetScaleY();

			// Need another rc here to get it to scale.
			// Here the bottom right is the out of the rectangle.
			TRectangle multirc = {
				(int)(sumem.copyTexSrcXY.x * MValueX),
				(int)(sumem.copyTexSrcXY.y * MValueY),
				(int)((sumem.copyTexSrcXY.x * MValueX + (sumem.copyTexSrcWH.x + 1) * MValueX)),
				(int)((sumem.copyTexSrcXY.y * MValueY + (sumem.copyTexSrcWH.y + 1) * MValueY))
			};
			UPE_Copy PE_copy;
			PE_copy.Hex = sumem.triggerEFBCopy;

			// Check if we are to copy from the EFB or draw to the XFB
			if (PE_copy.copy_to_xfb == 0)
			{
				if (GetConfig(CONFIG_SHOWEFBREGIONS)) 
					stats.efb_regions.push_back(rc);

				CopyEFB(bp, rc, sumem.copyTexDest << 5, 
							sumem.zcontrol.pixel_format == PIXELFMT_Z24, 
							PE_copy.intensity_fmt > 0, 
							((PE_copy.target_pixel_format / 2) + ((PE_copy.target_pixel_format & 1) * 8)), 
							PE_copy.half_scale > 0);
			}
			else
			{
				// the number of lines copied is determined by the y scale * source efb height
				const float yScale = sumem.dispcopyyscale / 256.0f;
				const float xfbLines = ((sumem.copyTexSrcWH.y + 1.0f) * yScale);
				RenderToXFB(bp, multirc, yScale, xfbLines, 
									 Memory_GetPtr(sumem.copyTexDest << 5), 
									 sumem.copyMipMapStrideChannels << 4, 
									 (u32)ceil(xfbLines));
			}

			// Clear the picture after it's done and submitted, to prepare for the next picture
			if (PE_copy.clear)
				ClearScreen(bp, multirc);

			RestoreRenderState(bp);
		        
			break;
		}
	case SUMEM_LOADTLUT0: // Load a Texture Look Up Table
	case SUMEM_LOADTLUT1:
        {
            DVSTARTSUBPROFILE("LoadSUReg:GXLoadTlut");

            u32 tlutTMemAddr = (bp.newvalue & 0x3FF) << 9;
            u32 tlutXferCount = (bp.newvalue & 0x1FFC00) >> 5; 

			u8 *ptr = 0;

            // TODO - figure out a cleaner way.
			if (GetConfig(CONFIG_ISWII))
				ptr = GetPointer(sumem.tlutXferSrc << 5);
			else
				ptr = GetPointer((sumem.tlutXferSrc & 0xFFFFF) << 5);

			if (ptr)
				memcpy_gc(texMem + tlutTMemAddr, ptr, tlutXferCount);
			else
				PanicAlert("Invalid palette pointer %08x %08x %08x", sumem.tlutXferSrc, sumem.tlutXferSrc << 5, (sumem.tlutXferSrc & 0xFFFFF)<< 5);

            // TODO(ector) : kill all textures that use this palette
            // Not sure if it's a good idea, though. For now, we hash texture palettes
			break;
        }
	case SUMEM_FOGRANGE: // Fog Settings Control
	case SUMEM_FOGPARAM0:
	case SUMEM_FOGBMAGNITUDE:
	case SUMEM_FOGBEXPONENT:
	case SUMEM_FOGPARAM3:
		if(!GetConfig(CONFIG_DISABLEFOG))
			PixelShaderManager::SetFogParamChanged();
		break;
	case SUMEM_FOGCOLOR: // Fog Color
		PixelShaderManager::SetFogColorChanged();
		break;
	case SUMEM_ALPHACOMPARE: // Compare Alpha Values
		PRIM_LOG("alphacmp: ref0=%d, ref1=%d, comp0=%d, comp1=%d, logic=%d", sumem.alphaFunc.ref0,
				sumem.alphaFunc.ref1, sumem.alphaFunc.comp0, sumem.alphaFunc.comp1, sumem.alphaFunc.logic);
        PixelShaderManager::SetAlpha(sumem.alphaFunc);
		break;
	case SUMEM_BIAS: // BIAS
		PRIM_LOG("ztex bias=0x%x", sumem.ztex1.bias);
        PixelShaderManager::SetZTextureBias(sumem.ztex1.bias);
		break;
	case SUMEM_ZTEX2: // Z Texture type
		{
			if (bp.changes & 3) 
				PixelShaderManager::SetZTextureTypeChanged();
			#if defined(_DEBUG) || defined(DEBUGFAST) 
			const char* pzop[] = {"DISABLE", "ADD", "REPLACE", "?"};
			const char* pztype[] = {"Z8", "Z16", "Z24", "?"};
			PRIM_LOG("ztex op=%s, type=%s", pzop[sumem.ztex2.op], pztype[sumem.ztex2.type]);
			#endif
			break;
		}
	// ----------------------------------
	// Display Copy Filtering Control
	// Fields: Destination, Frame2Field, Gamma, Source
	// TODO: We might have to implement the gamma one, some games might need this, if they are too dark to see.
	// ----------------------------------
	case SUMEM_DISPLAYCOPYFILER: 
	case SUMEM_DISPLAYCOPYFILER+1:
	case SUMEM_DISPLAYCOPYFILER+2:
	case SUMEM_DISPLAYCOPYFILER+3:
	case SUMEM_COPYFILTER0: //GXSetCopyFilter
	case SUMEM_COPYFILTER1: 
		break;
	case SUMEM_FIELDMASK: // Interlacing Control
	case SUMEM_FIELDMODE:
		SetInterlacingMode(bp);
		break;
	// ---------------------------------------------------
	// Debugging/Profiling info, we don't care about them
	// ---------------------------------------------------
	case SUMEM_CLOCK0: // Some Clock
	case SUMEM_CLOCK1: // Some Clock
	case SUMEM_SU_COUNTER: // Pixel or Poly Count
	case SUMEM_RAS_COUNTER: // Sound Count of something in the Texture Units
	case SUMEM_SETGPMETRIC: // Set the Graphic Processor Metric
		break;
	// ----------------
	// EFB Copy config
	// ----------------
	case SUMEM_EFB_TL:   // EFB Source Rect. Top, Left
	case SUMEM_EFB_BR:   // EFB Source Rect. Bottom, Right (w, h - 1)
	case SUMEM_EFB_ADDR: // EFB Target Address
		break;
	// --------------
	// Clear Config
	// --------------
	case SUMEM_CLEAR_AR: // Alpha and Red Components
	case SUMEM_CLEAR_GB: // Green and Blue Components
	case SUMEM_CLEAR_Z:  // Z Components (24-bit Zbuffer)
		break;
	// -------------------------
	// Culling Occulsion, we don't support this
	// let's hope not many games use bboxes..
	// TODO(ector): add something that watches bboxes
	// -------------------------
	case SUMEM_CLEARBBOX1:
	case SUMEM_CLEARBBOX2:
		break;
	case SUMEM_ZCOMPARE:      // Set the Z-Compare
	case SUMEM_TEXINVALIDATE: // Used, if game has manual control the Texture Cache, which we don't allow
	case SUMEM_MIPMAP_STRIDE: // MipMap Stride Channel
	case SUMEM_COPYYSCALE:    // Display Copy Y Scale
	case SUMEM_IREF:          /* 24 RID
								 21 BC3 - Ind. Tex Stage 3 NTexCoord
								 18 BI3 - Ind. Tex Stage 3 NTexMap
								 15 BC2 - Ind. Tex Stage 2 NTexCoord
								 12 BI2 - Ind. Tex Stage 2 NTexMap
								 9 BC1 - Ind. Tex Stage 1 NTexCoord
								 6 BI1 - Ind. Tex Stage 1 NTexMap
								 3 BC0 - Ind. Tex Stage 0 NTexCoord
								 0 BI0 - Ind. Tex Stage 0 NTexMap */
		break;
	case SUMEM_TEV_KSEL:  // Texture Environment Swap Mode Table 0
	case SUMEM_TEV_KSEL+1:// Texture Environment Swap Mode Table 1
	case SUMEM_TEV_KSEL+2:// Texture Environment Swap Mode Table 2
	case SUMEM_TEV_KSEL+3:// Texture Environment Swap Mode Table 3
	case SUMEM_TEV_KSEL+4:// Texture Environment Swap Mode Table 4
	case SUMEM_TEV_KSEL+5:// Texture Environment Swap Mode Table 5
	case SUMEM_TEV_KSEL+6:// Texture Environment Swap Mode Table 6
	case SUMEM_TEV_KSEL+7:// Texture Environment Swap Mode Table 7
		break;
	case SUMEM_BP_MASK:   // This Register can be used to limit to which bits of BP registers is actually written to. the mask is
		                  // only valid for the next BP command, and will reset itself.
	case SUMEM_IND_IMASK: // Index Mask ?
		break;
	case SUMEM_UNKNOWN: // This is always set to 0xF at boot of any game, so this sounds like a useless reg
		if (bp.newvalue != 0x0F)
			PanicAlert("Unknown is not 0xF! val = 0x%08x", bp.newvalue);
		break;

	// Cases added due to: http://code.google.com/p/dolphin-emu/issues/detail?id=360#c90
	// Are these related to BBox?
	case SUMEM_UNKNOWN1:
	case SUMEM_UNKNOWN2:
	case SUMEM_UNKNOWN3:
	case SUMEM_UNKNOWN4:
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
		case SUMEM_TREF:
		case SUMEM_TREF+1:
		case SUMEM_TREF+2:
		case SUMEM_TREF+3:
		case SUMEM_TREF+4:
		case SUMEM_TREF+5:
		case SUMEM_TREF+6:
		case SUMEM_TREF+7:
			break;
		// ----------------------
		// Set wrap size
	    // ----------------------
		case SUMEM_SU_SSIZE:
		case SUMEM_SU_TSIZE:
		case SUMEM_SU_SSIZE+2:
		case SUMEM_SU_TSIZE+2:
		case SUMEM_SU_SSIZE+4:
		case SUMEM_SU_TSIZE+4:
		case SUMEM_SU_SSIZE+6:
		case SUMEM_SU_TSIZE+6:
		case SUMEM_SU_SSIZE+8:
		case SUMEM_SU_TSIZE+8:
		case SUMEM_SU_SSIZE+10:
		case SUMEM_SU_TSIZE+10:
		case SUMEM_SU_SSIZE+12:
		case SUMEM_SU_TSIZE+12:
		case SUMEM_SU_SSIZE+14:
		case SUMEM_SU_TSIZE+14:
            PixelShaderManager::SetTexCoordChanged((bp.address - SUMEM_SU_SSIZE) >> 1);
			break;
		// ------------------------
		// SUMEM_TX_SETMODE0 - (Texture lookup and filtering mode) LOD/BIAS Clamp, MaxAnsio, LODBIAS, DiagLoad, Min Filter, Mag Filter, Wrap T, S
		// SUMEM_TX_SETMODE1 - (LOD Stuff) - Max LOD, Min LOD
	    // ------------------------
		case SUMEM_TX_SETMODE0: // (0x90 for linear)
		case SUMEM_TX_SETMODE0+1:
		case SUMEM_TX_SETMODE0+2:
		case SUMEM_TX_SETMODE0+3:
		case SUMEM_TX_SETMODE1:
		case SUMEM_TX_SETMODE1+1:
		case SUMEM_TX_SETMODE1+2:
		case SUMEM_TX_SETMODE1+3:
		case SUMEM_TX_SETMODE0_4:
		case SUMEM_TX_SETMODE0_4+1:
		case SUMEM_TX_SETMODE0_4+2:
		case SUMEM_TX_SETMODE0_4+3:
		case SUMEM_TX_SETMODE1_4:
		case SUMEM_TX_SETMODE1_4+1:
		case SUMEM_TX_SETMODE1_4+2:
		case SUMEM_TX_SETMODE1_4+3:
			SetSamplerState(bp);
			break;
		// --------------------------------------------
		// SUMEM_TX_SETIMAGE0 - Texture width, height, format
		// SUMEM_TX_SETIMAGE1 - even LOD address in TMEM - Image Type, Cache Height, Cache Width, TMEM Offset
		// SUMEM_TX_SETIMAGE2 - odd LOD address in TMEM - Cache Height, Cache Width, TMEM Offset
		// SUMEM_TX_SETIMAGE3 - Address of Texture in main memory
	    // --------------------------------------------
		case SUMEM_TX_SETIMAGE0:
		case SUMEM_TX_SETIMAGE0+1:
		case SUMEM_TX_SETIMAGE0+2:
		case SUMEM_TX_SETIMAGE0+3:
		case SUMEM_TX_SETIMAGE0_4:
		case SUMEM_TX_SETIMAGE0_4+1:
		case SUMEM_TX_SETIMAGE0_4+2:
		case SUMEM_TX_SETIMAGE0_4+3:
		case SUMEM_TX_SETIMAGE1:
		case SUMEM_TX_SETIMAGE1+1:
		case SUMEM_TX_SETIMAGE1+2:
		case SUMEM_TX_SETIMAGE1+3:
		case SUMEM_TX_SETIMAGE1_4:
		case SUMEM_TX_SETIMAGE1_4+1:
		case SUMEM_TX_SETIMAGE1_4+2:
		case SUMEM_TX_SETIMAGE1_4+3:
		case SUMEM_TX_SETIMAGE2:
		case SUMEM_TX_SETIMAGE2+1:
		case SUMEM_TX_SETIMAGE2+2:
		case SUMEM_TX_SETIMAGE2+3:
		case SUMEM_TX_SETIMAGE2_4:
		case SUMEM_TX_SETIMAGE2_4+1:
		case SUMEM_TX_SETIMAGE2_4+2:
		case SUMEM_TX_SETIMAGE2_4+3:
		case SUMEM_TX_SETIMAGE3:
		case SUMEM_TX_SETIMAGE3+1:
		case SUMEM_TX_SETIMAGE3+2:
		case SUMEM_TX_SETIMAGE3+3:
		case SUMEM_TX_SETIMAGE3_4:
		case SUMEM_TX_SETIMAGE3_4+1:
		case SUMEM_TX_SETIMAGE3_4+2:
		case SUMEM_TX_SETIMAGE3_4+3:
			break;
		// -------------------------------
		// Set a TLUT
		// SUMEM_TX_SETTLUT - Format, TMEM Offset (offset of TLUT from start of TMEM high bank > > 5)
	    // -------------------------------
		case SUMEM_TX_SETTLUT:
		case SUMEM_TX_SETTLUT+1:
		case SUMEM_TX_SETTLUT+2:
		case SUMEM_TX_SETTLUT+3:
		case SUMEM_TX_SETLUT_4:
		case SUMEM_TX_SETLUT_4+1:
		case SUMEM_TX_SETLUT_4+2:
		case SUMEM_TX_SETLUT_4+3:
			break;
		// ---------------------------------------------------
		// Set the TEV Color
	    // ---------------------------------------------------
		case SUMEM_TEV_REGISTER_L:   // Reg 1
		case SUMEM_TEV_REGISTER_H:
		case SUMEM_TEV_REGISTER_L+2: // Reg 2
		case SUMEM_TEV_REGISTER_H+2:
		case SUMEM_TEV_REGISTER_L+4: // Reg 3
		case SUMEM_TEV_REGISTER_H+4:
		case SUMEM_TEV_REGISTER_L+6: // Reg 4
		case SUMEM_TEV_REGISTER_H+6:
			{
				if (bp.address & 1) 
				{ 
					// don't compare with changes!
					int num = (bp.address >> 1 ) & 0x3;
					PixelShaderManager::SetColorChanged(sumem.tevregs[num].high.type, num);
				}
				break;
			}
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
			case SUMEM_IND_CMD:
			case SUMEM_IND_CMD+1:
			case SUMEM_IND_CMD+2:
			case SUMEM_IND_CMD+3:
			case SUMEM_IND_CMD+4:
			case SUMEM_IND_CMD+5:
			case SUMEM_IND_CMD+6:
			case SUMEM_IND_CMD+7:
			case SUMEM_IND_CMD+8:
			case SUMEM_IND_CMD+9:
			case SUMEM_IND_CMD+10:
			case SUMEM_IND_CMD+11:
			case SUMEM_IND_CMD+12:
			case SUMEM_IND_CMD+13:
			case SUMEM_IND_CMD+14:
			case SUMEM_IND_CMD+15:
				break;
			// --------------------------------------------------
			// Set Color/Alpha of a Tev
			// SUMEM_TEV_COLOR_ENV - Dest, Shift, Clamp, Sub, Bias, Sel A, Sel B, Sel C, Sel D
			// SUMEM_TEV_ALPHA_ENV - Dest, Shift, Clamp, Sub, Bias, Sel A, Sel B, Sel C, Sel D, T Swap, R Swap
			// --------------------------------------------------
			case SUMEM_TEV_COLOR_ENV:    // Texture Environment 1
			case SUMEM_TEV_ALPHA_ENV:
			case SUMEM_TEV_COLOR_ENV+2:  // Texture Environment 2
			case SUMEM_TEV_ALPHA_ENV+2:
			case SUMEM_TEV_COLOR_ENV+4:  // Texture Environment 3
			case SUMEM_TEV_ALPHA_ENV+4:
			case SUMEM_TEV_COLOR_ENV+8:  // Texture Environment 4
			case SUMEM_TEV_ALPHA_ENV+8:
			case SUMEM_TEV_COLOR_ENV+10: // Texture Environment 5
			case SUMEM_TEV_ALPHA_ENV+10:
			case SUMEM_TEV_COLOR_ENV+12: // Texture Environment 6
			case SUMEM_TEV_ALPHA_ENV+12:
			case SUMEM_TEV_COLOR_ENV+14: // Texture Environment 7
			case SUMEM_TEV_ALPHA_ENV+14:
			case SUMEM_TEV_COLOR_ENV+16: // Texture Environment 8
			case SUMEM_TEV_ALPHA_ENV+16:
			case SUMEM_TEV_COLOR_ENV+18: // Texture Environment 9
			case SUMEM_TEV_ALPHA_ENV+18:
			case SUMEM_TEV_COLOR_ENV+20: // Texture Environment 10
			case SUMEM_TEV_ALPHA_ENV+20:
			case SUMEM_TEV_COLOR_ENV+22: // Texture Environment 11
			case SUMEM_TEV_ALPHA_ENV+22:
			case SUMEM_TEV_COLOR_ENV+24: // Texture Environment 12
			case SUMEM_TEV_ALPHA_ENV+24:
			case SUMEM_TEV_COLOR_ENV+26: // Texture Environment 13
			case SUMEM_TEV_ALPHA_ENV+26:
			case SUMEM_TEV_COLOR_ENV+28: // Texture Environment 14
			case SUMEM_TEV_ALPHA_ENV+28:
			case SUMEM_TEV_COLOR_ENV+30: // Texture Environment 15
			case SUMEM_TEV_ALPHA_ENV+30:
			case SUMEM_TEV_COLOR_ENV+32: // Texture Environment 16
			case SUMEM_TEV_ALPHA_ENV+32:
				break;
			default:
				WARN_LOG(VIDEO, "Unknown Bypass opcode: address = 0x%08x value = 0x%08x", bp.address, bp.newvalue);
				break;
			}
			
		}

	}
}

