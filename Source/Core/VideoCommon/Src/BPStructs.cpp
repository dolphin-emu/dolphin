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
#include "BPFunctions.h"
#include "BPStructs.h"
#include "TextureDecoder.h"
#include "OpcodeDecoding.h"
#include "VertexLoader.h"
#include "VertexShaderManager.h"

using namespace BPFunctions;

void BPInit()
{
    memset(&bpmem, 0, sizeof(bpmem));
    bpmem.bpMask = 0xFFFFFF;
}

// ----------------------------------------------------------------------------------------------------------
// Write to the Bypass Memory
/* ------------------
   Called:
		At the end of every: OpcodeDecoding.cpp ExecuteDisplayList > Decode() > LoadBPReg
   TODO:
		Turn into function table. The (future) DisplayList (DL) jit can then call the functions directly,
		getting rid of dynamic dispatch. Unfortunately, few games use DLs properly - most\
		just stuff geometry in them and don't put state changes there. */
// ----------------------------------------------------------------------------------------------------------
void BPWritten(const Bypass& bp)
{

	// --------------------------------------------------------------------------------------------------------
	// First the pipeline is flushed then update the bpmem with the new value.
	// Some of the BP cases have to call certain functions while others just update the bpmem.
	// some bp cases check the changes variable, because they might not have to be updated all the time
	// NOTE: it seems not all bp cases like checking changes, so calling if (bp.changes == 0 ? false : true)
	//       had to be ditched and the games seem to work fine with out it.
	// --------------------------------------------------------------------------------------------------------

	FlushPipeline();
    ((u32*)&bpmem)[bp.address] = bp.newvalue;
	
	switch (bp.address)
	{
	case BPMEM_GENMODE: // Set the Generation Mode
        PRIM_LOG("genmode: texgen=%d, col=%d, ms_en=%d, tev=%d, culmode=%d, ind=%d, zfeeze=%d",
		bpmem.genMode.numtexgens, bpmem.genMode.numcolchans,
        bpmem.genMode.ms_en, bpmem.genMode.numtevstages+1, bpmem.genMode.cullmode,
		bpmem.genMode.numindstages, bpmem.genMode.zfreeze);
		SetGenerationMode(bp);
		break;
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
	case BPMEM_RAS1_SS1: // Index Texture Coordinate Scale 1
		PixelShaderManager::SetIndTexScaleChanged();
		break;
	case BPMEM_SCISSORTL: // Scissor Rectable Top, Left
	case BPMEM_SCISSORBR: // Scissor Rectable Bottom, Right
	case BPMEM_SCISSOROFFSET: // Scissor Offset
		SetScissor(bp);
		break;
	case BPMEM_LINEPTWIDTH: // Line Width
		SetLineWidth(bp);
		break;
	case BPMEM_ZMODE: // Depth Control
		PRIM_LOG("zmode: test=%d, func=%d, upd=%d", bpmem.zmode.testenable, bpmem.zmode.func,
		bpmem.zmode.updateenable);
		SetDepthMode(bp);
		break;
	case BPMEM_BLENDMODE: // Blending Control
		if (bp.changes & 0xFFFF)
		{
			PRIM_LOG("blendmode: en=%d, open=%d, colupd=%d, alphaupd=%d, dst=%d, src=%d, sub=%d, mode=%d", 
                bpmem.blendmode.blendenable, bpmem.blendmode.logicopenable, bpmem.blendmode.colorupdate, bpmem.blendmode.alphaupdate,
                bpmem.blendmode.dstfactor, bpmem.blendmode.srcfactor, bpmem.blendmode.subtract, bpmem.blendmode.logicmode);
			// Set LogicOp Blending Mode
            if (bp.changes & 2)
			{
				SETSTAT(stats.logicOpMode, bpmem.blendmode.logicopenable != 0 ? bpmem.blendmode.logicmode : stats.logicOpMode);
				SetLogicOpMode(bp);
			}
			// Set Dithering Mode
            if (bp.changes & 4)
			{
				SETSTAT(stats.dither, bpmem.blendmode.dither);
				SetDitherMode(bp);
			}
			// Set Blending Mode
			if (bp.changes & 0xFE1)
			{
				SETSTAT(stats.srcFactor, bpmem.blendmode.srcfactor);
				SETSTAT(stats.dstFactor, bpmem.blendmode.dstfactor);
				SetBlendMode(bp);
			}
			// Set Color Mask
            if (bp.changes & 0x18)
			{
				SETSTAT(stats.alphaUpdate, bpmem.blendmode.alphaupdate);
				SETSTAT(stats.colorUpdate, bpmem.blendmode.colorupdate);
				SetColorMask(bp);
			}
		}
		break;
	case BPMEM_CONSTANTALPHA: // Set Destination Alpha
		PRIM_LOG("constalpha: alp=%d, en=%d", bpmem.dstalpha.alpha, bpmem.dstalpha.enable);
		SETSTAT(stats.dstAlphaEnable, bpmem.dstalpha.enable);
		SETSTAT_UINT(stats.dstAlpha, bpmem.dstalpha.alpha);
		PixelShaderManager::SetDestAlpha(bpmem.dstalpha);
		break;
	case BPMEM_SETDRAWDONE: // This is called when the game is done drawing (eg: like in DX: Begin(); Draw(); End();)
		switch (bp.newvalue & 0xFF)
        {
        case 0x02:
            g_VideoInitialize.pSetPEFinish(); // may generate interrupt
            DEBUG_LOG(VIDEO, "GXSetDrawDone SetPEFinish (value: 0x%02X)", (bp.newvalue & 0xFFFF));
            break;

        default:
            DEBUG_LOG(VIDEO, "GXSetDrawDone ??? (value 0x%02X)", (bp.newvalue & 0xFFFF));
            break;
        }
        break;
	case BPMEM_PE_TOKEN_ID: // Pixel Engine Token ID
        g_VideoInitialize.pSetPEToken(static_cast<u16>(bp.newvalue & 0xFFFF), FALSE);
        DEBUG_LOG(VIDEO, "SetPEToken 0x%04x", (bp.newvalue & 0xFFFF));
        break;
    case BPMEM_PE_TOKEN_INT_ID: // Pixel Engine Interrupt Token ID
        g_VideoInitialize.pSetPEToken(static_cast<u16>(bp.newvalue & 0xFFFF), TRUE);
        DEBUG_LOG(VIDEO, "SetPEToken + INT 0x%04x", (bp.newvalue & 0xFFFF));
        break;
	// ------------------------
	// EFB copy command. This copies a rectangle from the EFB to either RAM in a texture format or to XFB as YUYV.
	// It can also optionally clear the EFB while copying from it. To emulate this, we of course copy first and clear afterwards.
    case BPMEM_TRIGGER_EFB_COPY: // Copy EFB Region or Render to the XFB or Clear the screen.
	{
		DVSTARTSUBPROFILE("LoadBPReg:swap");
		// The bottom right is within the rectangle
		// The values in bpmem.copyTexSrcXY and bpmem.copyTexSrcWH are updated in case 0x49 and 0x4a in this function
		TRectangle rc = {
			(int)(bpmem.copyTexSrcXY.x),
			(int)(bpmem.copyTexSrcXY.y),
			(int)((bpmem.copyTexSrcXY.x + bpmem.copyTexSrcWH.x + 1)),
			(int)((bpmem.copyTexSrcXY.y + bpmem.copyTexSrcWH.y + 1))
		};

		float MValueX = GetRendererTargetScaleX();
		float MValueY = GetRendererTargetScaleY();

		// Need another rc here to get it to scale.
		// Here the bottom right is the out of the rectangle.
		TRectangle multirc = {
			(int)(bpmem.copyTexSrcXY.x * MValueX),
			(int)(bpmem.copyTexSrcXY.y * MValueY),
			(int)((bpmem.copyTexSrcXY.x * MValueX + (bpmem.copyTexSrcWH.x + 1) * MValueX)),
			(int)((bpmem.copyTexSrcXY.y * MValueY + (bpmem.copyTexSrcWH.y + 1) * MValueY))
		};
		UPE_Copy PE_copy;
		PE_copy.Hex = bpmem.triggerEFBCopy;
		// ----------------------------------------------------------
		// Check to where we should copy the image data from the EFB.
		// ----------------------------------------------------------
		if (PE_copy.copy_to_xfb == 0)
		{
			if (GetConfig(CONFIG_SHOWEFBREGIONS)) 
				stats.efb_regions.push_back(rc);

			CopyEFB(bp, rc, bpmem.copyTexDest << 5, 
						bpmem.zcontrol.pixel_format == PIXELFMT_Z24, 
						PE_copy.intensity_fmt > 0, 
						((PE_copy.target_pixel_format / 2) + ((PE_copy.target_pixel_format & 1) * 8)), 
						PE_copy.half_scale > 0);
		}
		else
		{
			// the number of lines copied is determined by the y scale * source efb height
			const float yScale = bpmem.dispcopyyscale / 256.0f;
			const float xfbLines = ((bpmem.copyTexSrcWH.y + 1.0f) * yScale);
			RenderToXFB(bp, multirc, yScale, xfbLines, 
								 Memory_GetPtr(bpmem.copyTexDest << 5), 
								 bpmem.copyMipMapStrideChannels << 4, 
								 (u32)ceil(xfbLines));
		}

		// ----------------------------------------------------------------------------------
		// Clear the picture after it's done and submitted, to prepare for the next picture
		// ----------------------------------------------------------------------------------
		if (PE_copy.clear)
			ClearScreen(bp, multirc);

		RestoreRenderState(bp);
	        
		break;
	}
	case BPMEM_LOADTLUT0: // Load the Texture Look Up Table
	case BPMEM_LOADTLUT1:
        {
            DVSTARTSUBPROFILE("LoadBPReg:GXLoadTlut");

            u32 tlutTMemAddr = (bp.newvalue & 0x3FF) << 9;
            u32 tlutXferCount = (bp.newvalue & 0x1FFC00) >> 5; 

			u8 *ptr = 0;

            // TODO - figure out a cleaner way.
			if (GetConfig(CONFIG_ISWII))
				ptr = GetPointer(bpmem.tlutXferSrc << 5);
			else
				ptr = GetPointer((bpmem.tlutXferSrc & 0xFFFFF) << 5);

			if (ptr)
				memcpy_gc(texMem + tlutTMemAddr, ptr, tlutXferCount);
			else
				PanicAlert("Invalid palette pointer %08x %08x %08x", bpmem.tlutXferSrc, bpmem.tlutXferSrc << 5, (bpmem.tlutXferSrc & 0xFFFFF)<< 5);

            // TODO(ector) : kill all textures that use this palette
            // Not sure if it's a good idea, though. For now, we hash texture palettes
        }
        break;
	case BPMEM_FOGRANGE: // Fog Settings Control
	case BPMEM_FOGPARAM0:
	case BPMEM_FOGBMAGNITUDE:
	case BPMEM_FOGBEXPONENT:
	case BPMEM_FOGPARAM3:
		if(!GetConfig(CONFIG_DISABLEFOG))
			PixelShaderManager::SetFogParamChanged();
		break;
	case BPMEM_FOGCOLOR: // Fog Color
		PixelShaderManager::SetFogColorChanged();
		break;
	case BPMEM_ALPHACOMPARE:
		PRIM_LOG("alphacmp: ref0=%d, ref1=%d, comp0=%d, comp1=%d, logic=%d", bpmem.alphaFunc.ref0,
				bpmem.alphaFunc.ref1, bpmem.alphaFunc.comp0, bpmem.alphaFunc.comp1, bpmem.alphaFunc.logic);
        PixelShaderManager::SetAlpha(bpmem.alphaFunc);
		break;
	case BPMEM_ZTEX1: // BIAS
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

	case BPMEM_DISPLAYCOPYFILER: // Display Filtering Control
	case BPMEM_DISPLAYCOPYFILER+1:
	case BPMEM_DISPLAYCOPYFILER+2:
	case BPMEM_DISPLAYCOPYFILER+3:
	case BPMEM_COPYFILTER0:
	case BPMEM_COPYFILTER1:
		break;
	case BPMEM_SU_COUNTER: // Counters
	case BPMEM_RAS_COUNTER:
		break;
	case BPMEM_FIELDMASK: // Field Control
	case BPMEM_FIELDMODE:
		break;
	case BPMEM_CLOCK0: // Debugging info
	case BPMEM_CLOCK1:
	case BPMEM_SETGPMETRIC: 
		break;
	case BPMEM_EFB_TL:   // EFB Source Rect. Top, Left
	case BPMEM_EFB_BR:   // EFB Source Rect. Bottom, Right (w, h - 1)
	case BPMEM_EFB_ADDR: // EFB Target Address
		break;
	case BPMEM_CLEAR_AR: // Set Clear Alpha and Red Components
	case BPMEM_CLEAR_GB: // Green and Blue
	case BPMEM_CLEAR_Z:  // Clear Z, 24-bit Z Value
		break;
	case BPMEM_CLEARBBOX1: // let's hope not many games use bboxes..
	case BPMEM_CLEARBBOX2: // TODO(ector): add something that watches bboxes
		break;
	case BPMEM_IREF:
	case BPMEM_PE_CONTROL:    // Pixel Engine Control (GXSetZCompLoc, GXPixModeSync)
	case BPMEM_TEXINVALIDATE: // Used, if game has manual control the Texture Cache, which we don't allow
	case BPMEM_MIPMAP_STRIDE: // MipMap Stride Channel
	case BPMEM_COPYYSCALE:    // Display Copy Y Scale
		break;
	case BPMEM_TEV_KSEL:  // Texture Environment Swap Mode Table 0
	case BPMEM_TEV_KSEL+1:// Texture Environment Swap Mode Table 1
	case BPMEM_TEV_KSEL+2:// Texture Environment Swap Mode Table 2
	case BPMEM_TEV_KSEL+3:// Texture Environment Swap Mode Table 3
	case BPMEM_TEV_KSEL+4:// Texture Environment Swap Mode Table 4
	case BPMEM_TEV_KSEL+5:// Texture Environment Swap Mode Table 5
	case BPMEM_TEV_KSEL+6:// Texture Environment Swap Mode Table 6
	case BPMEM_TEV_KSEL+7:// Texture Environment Swap Mode Table 7
		break;
	case BPMEM_BP_MASK: // Masks
	case BPMEM_IND_IMASK:
		break;
	case BPMEM_UNKNOWN: // This is always set to 0xF at boot of any game, so this sounds like a useless reg

		// ------------------------------------------------
		// On Default, we try to look for other things
		// before we give up and say its an unknown opcode
		// ------------------------------------------------
	default:
		switch (bp.address & 0xFC)  // Texture sampler filter
		{
		case BPMEM_TREF: // Texture Environment Order
		case BPMEM_TREF+1:
		case BPMEM_TREF+2:
		case BPMEM_TREF+3:
		case BPMEM_TREF+4:
		case BPMEM_TREF+5:
		case BPMEM_TREF+6:
		case BPMEM_TREF+7:
			break;
		// -------------------------------------
		// This could be Level Of Detail control for Wrap 
		case BPMEM_SU_SSIZE: // Texture Wrap Size U?
		case BPMEM_SU_TSIZE: // Texture Wrap Size V?
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
			break;
		case BPMEM_TX_SETMODE0: // 0x90 for Linear, Index 0
		case BPMEM_TX_SETMODE0+1:
		case BPMEM_TX_SETMODE0+2:
		case BPMEM_TX_SETMODE0+3: // Index 4
		case BPMEM_TX_SETMODE1:
		case BPMEM_TX_SETMODE1+1:
		case BPMEM_TX_SETMODE1+2:
		case BPMEM_TX_SETMODE1+3:
		case BPMEM_TX_SETMODE0_4:
		case BPMEM_TX_SETMODE0_4+1:
		case BPMEM_TX_SETMODE0_4+2:
		case BPMEM_TX_SETMODE0_4+3:
		case BPMEM_TX_SETMODE1_4:
		case BPMEM_TX_SETMODE1_4+1:
		case BPMEM_TX_SETMODE1_4+2:
		case BPMEM_TX_SETMODE1_4+3:
			SetSamplerState(bp);
			break;
		case BPMEM_TX_SETIMAGE0:
		case BPMEM_TX_SETIMAGE0+1:
		case BPMEM_TX_SETIMAGE0+2:
		case BPMEM_TX_SETIMAGE0+3:
		case BPMEM_TX_SETIMAGE0_4:
		case BPMEM_TX_SETIMAGE0_4+1:
		case BPMEM_TX_SETIMAGE0_4+2:
		case BPMEM_TX_SETIMAGE0_4+3:
		case BPMEM_TX_SETIMAGE1:
		case BPMEM_TX_SETIMAGE1+1:
		case BPMEM_TX_SETIMAGE1+2:
		case BPMEM_TX_SETIMAGE1+3:
		case BPMEM_TX_SETIMAGE1_4:
		case BPMEM_TX_SETIMAGE1_4+1:
		case BPMEM_TX_SETIMAGE1_4+2:
		case BPMEM_TX_SETIMAGE1_4+3:
		case BPMEM_TX_SETIMAGE2:
		case BPMEM_TX_SETIMAGE2+1:
		case BPMEM_TX_SETIMAGE2+2:
		case BPMEM_TX_SETIMAGE2+3:
		case BPMEM_TX_SETIMAGE2_4:
		case BPMEM_TX_SETIMAGE2_4+1:
		case BPMEM_TX_SETIMAGE2_4+2:
		case BPMEM_TX_SETIMAGE2_4+3:
		case BPMEM_TX_SETIMAGE3: // Texture Pointer
		case BPMEM_TX_SETIMAGE3+1:
		case BPMEM_TX_SETIMAGE3+2:
		case BPMEM_TX_SETIMAGE3+3:
		case BPMEM_TX_SETIMAGE3_4:
		case BPMEM_TX_SETIMAGE3_4+1:
		case BPMEM_TX_SETIMAGE3_4+2:
		case BPMEM_TX_SETIMAGE3_4+3:
			break;
		case BPMEM_TX_SETTLUT:
		case BPMEM_TX_SETTLUT+1:
		case BPMEM_TX_SETTLUT+2:
		case BPMEM_TX_SETTLUT+3:
		case BPMEM_TX_SETLUT_4:
		case BPMEM_TX_SETLUT_4+1:
		case BPMEM_TX_SETLUT_4+2:
		case BPMEM_TX_SETLUT_4+3:
			break;
		case BPMEM_TEV_REGISTER_L:   // Reg 1
		case BPMEM_TEV_REGISTER_H:
		case BPMEM_TEV_REGISTER_L+2: // Reg 2
		case BPMEM_TEV_REGISTER_H+2:
		case BPMEM_TEV_REGISTER_L+4: // Reg 3
		case BPMEM_TEV_REGISTER_H+4:
		case BPMEM_TEV_REGISTER_L+6: // Reg 4
		case BPMEM_TEV_REGISTER_H+6:
			if (bp.address & 1) 
			{ 
				// don't compare with changes!
                int num = (bp.address >> 1 ) & 0x3;
                PixelShaderManager::SetColorChanged(bpmem.tevregs[num].high.type, num);
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
			case BPMEM_TEV_COLOR_ENV:    // Texture Environment 1
			case BPMEM_TEV_ALPHA_ENV:
			case BPMEM_TEV_COLOR_ENV+2:  // Texture Environment 2
			case BPMEM_TEV_ALPHA_ENV+2:
			case BPMEM_TEV_COLOR_ENV+4:  // Texture Environment 3
			case BPMEM_TEV_ALPHA_ENV+4:
			case BPMEM_TEV_COLOR_ENV+8:  // Texture Environment 4
			case BPMEM_TEV_ALPHA_ENV+8:
			case BPMEM_TEV_COLOR_ENV+10: // Texture Environment 5
			case BPMEM_TEV_ALPHA_ENV+10:
			case BPMEM_TEV_COLOR_ENV+12: // Texture Environment 6
			case BPMEM_TEV_ALPHA_ENV+12:
			case BPMEM_TEV_COLOR_ENV+14: // Texture Environment 7
			case BPMEM_TEV_ALPHA_ENV+14:
			case BPMEM_TEV_COLOR_ENV+16: // Texture Environment 8
			case BPMEM_TEV_ALPHA_ENV+16:
			case BPMEM_TEV_COLOR_ENV+18: // Texture Environment 9
			case BPMEM_TEV_ALPHA_ENV+18:
			case BPMEM_TEV_COLOR_ENV+20: // Texture Environment 10
			case BPMEM_TEV_ALPHA_ENV+20:
			case BPMEM_TEV_COLOR_ENV+22: // Texture Environment 11
			case BPMEM_TEV_ALPHA_ENV+22:
			case BPMEM_TEV_COLOR_ENV+24: // Texture Environment 12
			case BPMEM_TEV_ALPHA_ENV+24:
			case BPMEM_TEV_COLOR_ENV+26: // Texture Environment 13
			case BPMEM_TEV_ALPHA_ENV+26:
			case BPMEM_TEV_COLOR_ENV+28: // Texture Environment 14
			case BPMEM_TEV_ALPHA_ENV+28:
			case BPMEM_TEV_COLOR_ENV+30: // Texture Environment 15
			case BPMEM_TEV_ALPHA_ENV+30:
			case BPMEM_TEV_COLOR_ENV+32: // Texture Environment 16
			case BPMEM_TEV_ALPHA_ENV+32:
				break;
			default:
				WARN_LOG(VIDEO, "Unknown Bypass opcode: address = %08x value = %08x", bp.address, bp.newvalue); 
			}
			
		}

	}
}

