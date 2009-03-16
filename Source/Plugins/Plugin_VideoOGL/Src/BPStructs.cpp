// Copyright (C) 2003-2008 Dolphin Project.

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

// ---------------------------------------------------------------------------------------
// includes
// -------------
#include "Globals.h"
#include "Profiler.h"
#include "Config.h"
#include "Statistics.h"

#include "VertexLoader.h"
#include "VertexManager.h"

#include "BPStructs.h"
#include "Render.h"
#include "OpcodeDecoding.h"
#include "TextureMngr.h"
#include "TextureDecoder.h"
#include "TextureConverter.h"
#include "VertexShaderManager.h"
#include "PixelShaderManager.h"
#include "XFB.h"
#include "main.h"

// ---------------------------------------------------------------------------------------
// State translation lookup tables
// -------------

static const GLenum glCmpFuncs[8] = {
	GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS
};

static const GLenum glLogicOpCodes[16] = {
    GL_CLEAR, GL_AND, GL_AND_REVERSE, GL_COPY, GL_AND_INVERTED, GL_NOOP, GL_XOR, 
	GL_OR, GL_NOR, GL_EQUIV, GL_INVERT, GL_OR_REVERSE, GL_COPY_INVERTED, GL_OR_INVERTED, GL_NAND, GL_SET
};

void BPInit()
{
    memset(&bpmem, 0, sizeof(bpmem));
    bpmem.bpMask = 0xFFFFFF;
}

//////////////////////////////////////////////////////////////////////////////////////
// Write to bpmem
/* ------------------
   Called:
		At the end of every: OpcodeDecoding.cpp ExecuteDisplayList > Decode() > LoadBPReg
   TODO:
		Turn into function table. The (future) DL jit can then call the functions directly,
		getting rid of dynamic dispatch. Unfortunately, few games use DLs properly - most\
		just stuff geometry in them and don't put state changes there.
// ------------------ */
void BPWritten(int addr, int changes, int newval)
{
    switch (addr)
    {
    case BPMEM_GENMODE:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            PRIM_LOG("genmode: texgen=%d, col=%d, ms_en=%d, tev=%d, culmode=%d, ind=%d, zfeeze=%d\n",
				bpmem.genMode.numtexgens, bpmem.genMode.numcolchans,
                bpmem.genMode.ms_en, bpmem.genMode.numtevstages+1, bpmem.genMode.cullmode,
				bpmem.genMode.numindstages, bpmem.genMode.zfreeze);

            // none, ccw, cw, ccw
            if (bpmem.genMode.cullmode > 0) {
                glEnable(GL_CULL_FACE);
                glFrontFace(bpmem.genMode.cullmode == 2 ? GL_CCW : GL_CW);
            }
            else
                glDisable(GL_CULL_FACE);
            PixelShaderManager::SetGenModeChanged();
        }
        break;

    case BPMEM_IND_MTX+0:
    case BPMEM_IND_MTX+1:
    case BPMEM_IND_MTX+2:
    case BPMEM_IND_MTX+3:
    case BPMEM_IND_MTX+4:
    case BPMEM_IND_MTX+5:
    case BPMEM_IND_MTX+6:
    case BPMEM_IND_MTX+7:
    case BPMEM_IND_MTX+8:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            PixelShaderManager::SetIndMatrixChanged((addr-BPMEM_IND_MTX)/3);
        }
        break;
    case BPMEM_RAS1_SS0:
    case BPMEM_RAS1_SS1:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            PixelShaderManager::SetIndTexScaleChanged();
        }
        break;
    case BPMEM_ZMODE:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            PRIM_LOG("zmode: test=%d, func=%d, upd=%d\n", bpmem.zmode.testenable, bpmem.zmode.func,
				bpmem.zmode.updateenable);
            
            if (bpmem.zmode.testenable) {
                glEnable(GL_DEPTH_TEST);
                glDepthMask(bpmem.zmode.updateenable?GL_TRUE:GL_FALSE);
                glDepthFunc(glCmpFuncs[bpmem.zmode.func]);
            }
            else {
                // if the test is disabled write is disabled too
                glDisable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
            }

            if (!bpmem.zmode.updateenable)
                Renderer::SetRenderMode(Renderer::RM_Normal);
        }
        break;

    case BPMEM_ALPHACOMPARE:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            PRIM_LOG("alphacmp: ref0=%d, ref1=%d, comp0=%d, comp1=%d, logic=%d\n", bpmem.alphaFunc.ref0,
				bpmem.alphaFunc.ref1, bpmem.alphaFunc.comp0, bpmem.alphaFunc.comp1, bpmem.alphaFunc.logic);
            PixelShaderManager::SetAlpha(bpmem.alphaFunc);
        }
        break;
        
    case BPMEM_CONSTANTALPHA:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            PRIM_LOG("constalpha: alp=%d, en=%d\n", bpmem.dstalpha.alpha, bpmem.dstalpha.enable);
			SETSTAT(stats.dstAlphaEnable, bpmem.dstalpha.enable);
			SETSTAT_UINT(stats.dstAlpha, bpmem.dstalpha.alpha);
            PixelShaderManager::SetDestAlpha(bpmem.dstalpha);
        }
        break;

    case BPMEM_LINEPTWIDTH:
        {
			float fratio = xfregs.rawViewport[0] != 0 ? ((float)Renderer::GetTargetWidth() / EFB_WIDTH) : 1.0f;
			if (bpmem.lineptwidth.linesize > 0)
				glLineWidth((float)bpmem.lineptwidth.linesize * fratio / 6.0f); // scale by ratio of widths
			if (bpmem.lineptwidth.pointsize > 0)
				glPointSize((float)bpmem.lineptwidth.pointsize * fratio / 6.0f);
			break;
        }

    case BPMEM_PE_CONTROL:  // GXSetZCompLoc, GXPixModeSync
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
        }
        break;

    case BPMEM_BLENDMODE:	
        if (changes & 0xFFFF) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;

            PRIM_LOG("blendmode: en=%d, open=%d, colupd=%d, alphaupd=%d, dst=%d, src=%d, sub=%d, mode=%d\n", 
                bpmem.blendmode.blendenable, bpmem.blendmode.logicopenable, bpmem.blendmode.colorupdate, bpmem.blendmode.alphaupdate,
                bpmem.blendmode.dstfactor, bpmem.blendmode.srcfactor, bpmem.blendmode.subtract, bpmem.blendmode.logicmode);

			/*
			Logic Operation Blend Modes
			--------------------
            0: GL_CLEAR
            1: GL_AND
            2: GL_AND_REVERSE
            3: GL_COPY [Super Smash. Bro. Melee, NES Zelda I, NES Zelda II]
            4: GL_AND_INVERTED
            5: GL_NOOP
            6: GL_XOR
            7: GL_OR [Zelda: TP]
            8: GL_NOR
            9: GL_EQUIV
            10: GL_INVERT
            11: GL_OR_REVERSE
            12: GL_COPY_INVERTED
			13: GL_OR_INVERTED
            14: GL_NAND
            15: GL_SET
			*/

			// LogicOp Blending
            if (changes & 2) {  
				SETSTAT(stats.logicOpMode, bpmem.blendmode.logicopenable != 0 ? bpmem.blendmode.logicmode : stats.logicOpMode);
				if (bpmem.blendmode.logicopenable) 
				{
					glEnable(GL_COLOR_LOGIC_OP);
					// PanicAlert("Logic Op Blend : %i", bpmem.blendmode.logicmode);
					glLogicOp(glLogicOpCodes[bpmem.blendmode.logicmode]);
				}
				else 
					glDisable(GL_COLOR_LOGIC_OP);
            }

			// Dithering
            if (changes & 4) {
				SETSTAT(stats.dither, bpmem.blendmode.dither);
                if (bpmem.blendmode.dither) glEnable(GL_DITHER);
                else glDisable(GL_DITHER);
            }

			// Blending
			if (changes & 0xFE1)
			{
				SETSTAT(stats.srcFactor, bpmem.blendmode.srcfactor);
			    SETSTAT(stats.dstFactor, bpmem.blendmode.dstfactor);
				Renderer::SetBlendMode(false);
			}

			// Color Mask
            if (changes & 0x18)
			{
				SETSTAT(stats.alphaUpdate, bpmem.blendmode.alphaupdate);
				SETSTAT(stats.colorUpdate, bpmem.blendmode.colorupdate);
                Renderer::SetColorMask();
			}
        }
        break;

	case BPMEM_FOGRANGE:
    case BPMEM_FOGPARAM0:
    case BPMEM_FOGBEXPONENT: 
    case BPMEM_FOGBMAGNITUDE:
    case BPMEM_FOGPARAM3:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            PixelShaderManager::SetFogParamChanged();
        }
        break;

    case BPMEM_FOGCOLOR:
        if (changes)
        {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            PixelShaderManager::SetFogColorChanged();
        }
        break;

    case BPMEM_TEXINVALIDATE:
        break;

    case BPMEM_SCISSOROFFSET:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            Renderer::SetScissorRect();
        }
        break;

    case BPMEM_SCISSORTL:
    case BPMEM_SCISSORBR:

        if (changes)
		{
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            if (!Renderer::SetScissorRect())
			{
				if (addr == BPMEM_SCISSORBR) {
                    ERROR_LOG(VIDEO, "bad scissor!\n");
				}
            }
        }
        break;
    case BPMEM_ZTEX1:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            PRIM_LOG("ztex bias=0x%x\n", bpmem.ztex1.bias);
            PixelShaderManager::SetZTextureBias(bpmem.ztex1.bias);
        }
        break;
    case BPMEM_ZTEX2:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
			if (changes & 3) {
				PixelShaderManager::SetZTextureTypeChanged();
			}
#if defined(_DEBUG) || defined(DEBUGFAST) 
            const char* pzop[] = {"DISABLE", "ADD", "REPLACE", "?"};
            const char* pztype[] = {"Z8", "Z16", "Z24", "?"};
            PRIM_LOG("ztex op=%s, type=%s\n", pzop[bpmem.ztex2.op], pztype[bpmem.ztex2.type]);
#endif
        }
        break;

    case 0xf6: // ksel0
    case 0xf7: // ksel1
    case 0xf8: // ksel2
    case 0xf9: // ksel3
    case 0xfa: // ksel4
    case 0xfb: // ksel5
    case 0xfc: // ksel6
    case 0xfd: // ksel7
        if (changes)
        {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            PixelShaderManager::SetTevKSelChanged(addr-0xf6);
        }
        break;

    case BPMEM_SETDRAWDONE:
        VertexManager::Flush();
        switch (newval & 0xFF)
        {
        case 0x02:
            g_VideoInitialize.pSetPEFinish(); // may generate interrupt
            DEBUG_LOG(VIDEO, "GXSetDrawDone SetPEFinish (value: 0x%02X)", (newval & 0xFFFF));
            break;

        default:
            DEBUG_LOG(VIDEO, "GXSetDrawDone ??? (value 0x%02X)", (newval & 0xFFFF));
            break;
        }
        break;

    case BPMEM_PE_TOKEN_ID:
        g_VideoInitialize.pSetPEToken(static_cast<u16>(newval & 0xFFFF), FALSE);
        DEBUG_LOG(VIDEO, "SetPEToken 0x%04x", (newval & 0xFFFF));
        break;

    case BPMEM_PE_TOKEN_INT_ID:
        g_VideoInitialize.pSetPEToken(static_cast<u16>(newval & 0xFFFF), TRUE);
        DEBUG_LOG(VIDEO, "SetPEToken + INT 0x%04x", (newval & 0xFFFF));
        break;

    case BPMEM_SETGPMETRIC: // Set gp metric?
        break;

	// ------------------------
	// EFB copy command. This copies a rectangle from the EFB to either RAM in a texture format or to XFB as YUYV.
	// It can also optionally clear the EFB while copying from it. To emulate this, we of course copy first and clear afterwards.
    case BPMEM_TRIGGER_EFB_COPY:
        {
			DVSTARTSUBPROFILE("LoadBPReg:swap");
            VertexManager::Flush();

            ((u32*)&bpmem)[addr] = newval;
			// The bottom right is within the rectangle
			// The values in bpmem.copyTexSrcXY and bpmem.copyTexSrcWH are updated in case 0x49 and 0x4a in this function
			TRectangle rc = {
                (int)(bpmem.copyTexSrcXY.x),
                (int)(bpmem.copyTexSrcXY.y),
                (int)((bpmem.copyTexSrcXY.x + bpmem.copyTexSrcWH.x + 1)),
                (int)((bpmem.copyTexSrcXY.y + bpmem.copyTexSrcWH.y + 1))
            };
			float MValueX = Renderer::GetTargetScaleX();
			float MValueY = Renderer::GetTargetScaleY();
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

			// --------------------------------------------------------
            // Check to where we should copy the image data from the EFB.
			// --------------------------
            if (PE_copy.copy_to_xfb == 0)
			{
				if (g_Config.bShowEFBCopyRegions) 
					stats.efb_regions.push_back(rc);

				// EFB to texture 
                // for some reason it sets bpmem.zcontrol.pixel_format to PIXELFMT_Z24 every time a zbuffer format is given as a dest to GXSetTexCopyDst
				if (!g_Config.bEFBCopyDisable)
				{
					if (g_Config.bCopyEFBToRAM)
					{
						TextureConverter::EncodeToRam(bpmem.copyTexDest << 5,
							bpmem.zcontrol.pixel_format == PIXELFMT_Z24,
							PE_copy.intensity_fmt > 0,
							(PE_copy.target_pixel_format / 2) + ((PE_copy.target_pixel_format & 1) * 8),  // ??
							PE_copy.half_scale > 0, rc);
					}
					else
					{
						TextureMngr::CopyRenderTargetToTexture(bpmem.copyTexDest << 5,
							bpmem.zcontrol.pixel_format == PIXELFMT_Z24,
							PE_copy.intensity_fmt > 0,
							(PE_copy.target_pixel_format / 2) + ((PE_copy.target_pixel_format & 1) * 8),  // ??
							PE_copy.half_scale > 0, rc);
					}
				}
			}
            else
			{
                // EFB to XFB
				if (g_Config.bUseXFB)
				{
					// the number of lines copied is determined by the y scale * source efb height
					float yScale = bpmem.dispcopyyscale / 256.0f;
					float xfbLines = bpmem.copyTexSrcWH.y + 1.0f * yScale;
					u8* pXFB = Memory_GetPtr(bpmem.copyTexDest << 5);
					u32 dstWidth = (bpmem.copyMipMapStrideChannels << 4);
					u32 dstHeight = (u32)xfbLines;
					XFB_Write(pXFB, multirc, dstWidth, dstHeight);
					// FIXME: we draw XFB from here in DC mode.
					// Bad hack since we can have multiple EFB to XFB copy before a draw.
					// Plus we should use width and height from VI regs (see VI->Update()).
					// Dixit donkopunchstania for the info.
					//DebugLog("(EFB to XFB->XFB_Draw): ptr: %08x | %ix%i", (u32)pXFB, dstWidth, dstHeight);
					if (g_VideoInitialize.bUseDualCore)
						XFB_Draw(pXFB, dstWidth, dstHeight, 0);
				}
				else
				{
					// Hm, we need to compensate for the fact that the copy may be bigger than what is displayed.
					// Seen in Spartan Warrior. Not sure how to deal with it yet.
					Renderer::Swap(multirc);
				}
				g_VideoInitialize.pCopiedToXFB();
            }

			// --------------------------------------------------------
            // Clear the picture after it's done and submitted, to prepare for the next picture
			// --------------------------
            if (PE_copy.clear)
			{
                // Clear color
                Renderer::SetRenderMode(Renderer::RM_Normal);
				// Clear Z-Buffer target
                bool bRestoreZBufferTarget = Renderer::UseFakeZTarget();
                
				// Update the view port for clearing the picture
				glViewport(0, 0, Renderer::GetTargetWidth(), Renderer::GetTargetHeight());

                // Always set the scissor in case it was set by the game and has not been reset
				glScissor(multirc.left, (Renderer::GetTargetHeight() - multirc.bottom), 
					(multirc.right - multirc.left), (multirc.bottom - multirc.top));
				// ---------------------------

                VertexShaderManager::SetViewportChanged();

                // Since clear operations use the source rectangle, we have to do
				// regular renders (glClear clears the entire buffer)
                if (bpmem.blendmode.colorupdate || bpmem.blendmode.alphaupdate || bpmem.zmode.updateenable)
				{                    
                    GLbitfield bits = 0;
                    if (bpmem.blendmode.colorupdate || bpmem.blendmode.alphaupdate)
					{
                        u32 clearColor = (bpmem.clearcolorAR << 16) | bpmem.clearcolorGB;
						glClearColor(((clearColor>>16) & 0xff)*(1/255.0f),
									 ((clearColor>>8 ) & 0xff)*(1/255.0f),
									 ((clearColor>>0 ) & 0xff)*(1/255.0f),
									 ((clearColor>>24) & 0xff)*(1/255.0f));
                        bits |= GL_COLOR_BUFFER_BIT;
                    }
                    if (bpmem.zmode.updateenable)
					{
                        glClearDepth((float)(bpmem.clearZValue & 0xFFFFFF) / float(0xFFFFFF));
                        bits |= GL_DEPTH_BUFFER_BIT;
                    }
                    if (bRestoreZBufferTarget)
                        glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);  // don't clear ztarget here
                    glClear(bits);
                }

				// Have to clear the target zbuffer
                if (bpmem.zmode.updateenable && bRestoreZBufferTarget)
				{
                    glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT);
                    GL_REPORT_ERRORD();
                    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                    
                    // red should probably be the LSB
                    glClearColor(((bpmem.clearZValue>>0)&0xff)*(1/255.0f),
						         ((bpmem.clearZValue>>8)&0xff)*(1/255.0f),
                                 ((bpmem.clearZValue>>16)&0xff)*(1/255.0f), 0);
                    glClear(GL_COLOR_BUFFER_BIT);
                    Renderer::SetColorMask();
                    GL_REPORT_ERRORD();   
                }

                if (bRestoreZBufferTarget)
				{
                    // restore target
                    GLenum s_drawbuffers[2] = {GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT};
                    glDrawBuffers(2, s_drawbuffers);
                }
            }
			Renderer::RestoreGLState();
        }
        break;
	// ==================================

    case BPMEM_LOADTLUT:
        {
            DVSTARTSUBPROFILE("LoadBPReg:GXLoadTlut");
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;

            u32 tlutTMemAddr = (newval & 0x3FF) << 9;
            u32 tlutXferCount = (newval & 0x1FFC00) >> 5; 

			u8 *ptr = 0;

            // TODO - figure out a cleaner way.
			if (g_VideoInitialize.bWii)
				ptr = g_VideoInitialize.pGetMemoryPointer(bpmem.tlutXferSrc << 5);
			else
				ptr = g_VideoInitialize.pGetMemoryPointer((bpmem.tlutXferSrc & 0xFFFFF) << 5);

			if (ptr)
				memcpy_gc(texMem + tlutTMemAddr, ptr, tlutXferCount);
			else
				PanicAlert("Invalid palette pointer %08x %08x %08x", bpmem.tlutXferSrc, bpmem.tlutXferSrc << 5, (bpmem.tlutXferSrc & 0xFFFFF)<< 5);

            // TODO(ector) : kill all textures that use this palette
            // Not sure if it's a good idea, though. For now, we hash texture palettes
        }
        break;
    

    default:
        switch (addr & 0xFC)  //texture sampler filter
        {
        case 0x28: // tevorder 0-3
        case 0x2C: // tevorder 4-7
            if (changes)
            {
                VertexManager::Flush();
                ((u32*)&bpmem)[addr] = newval;
                PixelShaderManager::SetTevOrderChanged(addr - 0x28);
            }
            break;
        case 0x80: // TEX MODE 0
        case 0xA0: 
            if (changes)
            {
                VertexManager::Flush();
                ((u32*)&bpmem)[addr] = newval;
            }
            break;
        case 0x84://TEX MODE 1
        case 0xA4:
            break;
        case 0x88://TEX IMAGE 0
        case 0xA8:
            if (changes)
            {
                //textureChanged[((addr&0xE0)==0xA0)*4+(addr&3)] = true;
                VertexManager::Flush();
                ((u32*)&bpmem)[addr] = newval;
            }
            break;
        case 0x8C://TEX IMAGE 1
        case 0xAC:
            if (changes)
            {
                //textureChanged[((addr&0xE0)==0xA0)*4+(addr&3)] = true;
                VertexManager::Flush();
                ((u32*)&bpmem)[addr] = newval;
            }
            break;
        case 0x90://TEX IMAGE 2
        case 0xB0:
            if (changes)
            {
                //textureChanged[((addr&0xE0)==0xA0)*4+(addr&3)] = true;
                VertexManager::Flush();
                ((u32*)&bpmem)[addr] = newval;
            }
            break;
        case 0x94://TEX IMAGE 3
        case 0xB4:
            if (changes)
            {
                //textureChanged[((addr&0xE0)==0xA0)*4+(addr&3)] = true;
                VertexManager::Flush();
                ((u32*)&bpmem)[addr] = newval;
            }
            break;
        case 0x98://TEX TLUT
        case 0xB8:
            if (changes)
            {
                //textureChanged[((addr&0xE0)==0xA0)*4+(addr&3)] = true;
                VertexManager::Flush();
                ((u32*)&bpmem)[addr] = newval;
            }
            break;
        case 0x9C://TEX UNKNOWN
        case 0xBC:
            //ERROR_LOG("texunknown%x = %x\n", addr, newval);
            ((u32*)&bpmem)[addr] = newval;
            break;
        case 0xE0:
        case 0xE4:
            if (addr&1) { // don't compare with changes!
                VertexManager::Flush();
                ((u32*)&bpmem)[addr] = newval;
                int num = (addr>>1)&0x3;
                PixelShaderManager::SetColorChanged(bpmem.tevregs[num].high.type, num);
            }
            else
                ((u32*)&bpmem)[addr] = newval;
            break;
            
        default:
            switch (addr&0xF0) {
            case 0x10: // tevindirect 0-15
                if (changes) {
                    VertexManager::Flush();
                    ((u32*)&bpmem)[addr] = newval;
                    PixelShaderManager::SetTevIndirectChanged(addr - 0x10);
                }
                break;

            case 0x30:
                if (changes) {
                    VertexManager::Flush();
                    ((u32*)&bpmem)[addr] = newval;
                    PixelShaderManager::SetTexDimsChanged((addr >> 1) & 0x7);
                }
                break;

            case 0xC0:
            case 0xD0:
                if (changes)
                {
                    VertexManager::Flush();
                    ((u32*)&bpmem)[addr] = newval;
                    PixelShaderManager::SetTevCombinerChanged((addr & 0x1f) / 2);
                }
                break;

            case 0x20:
            case 0x80:
            case 0x90:
            case 0xA0:
            case 0xB0:

			// Just update the bpmem struct, don't do anything else
            default:
                if (changes)
                {
                    VertexManager::Flush();
                    ((u32*)&bpmem)[addr] = newval;
                    /*switch(addr) {
                        case 0x01:
                        case 0x02:
                        case 0x03:
                        case 0x04: break; // copy filter values
                        case 0x0f: break; // mask
                        case 0x27: break; // tev ind order
                        case 0x44: break; // field mask
                        case 0x45: break; // draw done
                        case 0x46: break; // clock
                        case 0x49:
                        case 0x4a: break; // copy tex src
                        case 0x4b: break; // copy tex dest
                        case 0x4d: break; // copyMipMapStrideChannels
                        case 0x4e: break; // disp copy scale
                        case 0x4f: break; // clear color
                        case 0x50: break; // clear color
                        case 0x51: break; // casez
                        case 0x52: break; // trigger efb copy
                        case 0x53:
                        case 0x54: break; // more copy filters
                        case 0x55:
                        case 0x56: break; // bounding box
                        case 0x64:
                        case 0x65: break; // tlut src dest
                        case 0xe8: break; // fog range
                        case 0xe9:
                        case 0xea:
                        case 0xeb:
                        case 0xec:
                        case 0xed: break; // fog
                        case 0xfe: break; // mask
                        default:
                            // 0x58 = 0xf
                            // 0x69 = 0x49e
                            ERROR_LOG("bp%.2x = %x\n", addr, newval);
                    }*/
                }
                break;
            }
            break;
            
        }
        break;
    }
}

