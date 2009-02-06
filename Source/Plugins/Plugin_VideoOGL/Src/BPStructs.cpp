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


// ---------------------------------------------------------------------------------------
// State translation lookup tables
// -------------

static const GLenum glCmpFuncs[8] = {
	GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS
};

static const GLenum glLogicOpCodes[16] = {
    GL_CLEAR, GL_SET, GL_COPY, GL_COPY_INVERTED, GL_NOOP, GL_INVERT, GL_AND, GL_NAND,
    GL_OR, GL_NOR, GL_XOR, GL_EQUIV, GL_AND_REVERSE, GL_AND_INVERTED, GL_OR_REVERSE, GL_OR_INVERTED
};

void BPInit()
{
    memset(&bpmem, 0, sizeof(bpmem));
    bpmem.bpMask = 0xFFFFFF;
}

// Called at the end of every: OpcodeDecoding.cpp ExecuteDisplayList > Decode() > LoadBPReg
// TODO - turn into function table. The (future) DL jit can then call the functions directly,
// getting rid of dynamic dispatch.
void BPWritten(int addr, int changes, int newval)
{
    //static int count = 0;
    //ERROR_LOG("(%d) %x: %x\n", count++, addr, newval);

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
            else if (glIsEnabled(GL_CULL_FACE) == GL_TRUE)
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
			Renderer::SetBlendMode(false);
        }
        break;

    case BPMEM_LINEPTWIDTH:
        {
			float fratio = xfregs.rawViewport[0] != 0 ? (float)Renderer::GetTargetWidth() / 640.0f : 1.0f;
			if (bpmem.lineptwidth.linesize > 0)
				glLineWidth((float)bpmem.lineptwidth.linesize * fratio / 6.0f); // scale by ratio of widths
			if (bpmem.lineptwidth.pointsize > 0)
				glPointSize((float)bpmem.lineptwidth.pointsize * fratio / 6.0f);
			break;
        }
    
    case 0x43:
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
            
            if (changes & 2) {
                if (Renderer::CanBlendLogicOp()) {
                    if (bpmem.blendmode.logicopenable) {
                        glEnable(GL_COLOR_LOGIC_OP);
						PanicAlert("Logic Op Blend : %i", bpmem.blendmode.logicmode);
                        glLogicOp(glLogicOpCodes[bpmem.blendmode.logicmode]);
                    }
                    else glDisable(GL_COLOR_LOGIC_OP);
                }
                //else {
                //    if (bpmem.blendmode.logicopenable) {
                //        switch(bpmem.blendmode.logicmode) {
                //            case 0: // clear dst to 0
                //                glEnable(GL_BLEND);
                //                glBlendFunc(GL_ZERO, GL_ZERO);
                //                break;
                //            case 1: // set dst to 1
                //                glEnable(GL_BLEND);
                //                glBlendFunc(GL_ONE, GL_ONE);
                //                break;
                //            case 2: // set dst to src
                //                glDisable(GL_BLEND);
                //                break;
                //            case 3: // set dst to ~src
                //                glEnable(GL_BLEND);
                //                glBlendFunc(GL_ONE_MINUS_SRC_COLOR, GL_ZERO); //?
                //                break;
                //            case 4: // set dst to dst
                //                glEnable(GL_BLEND);
                //                glBlendFunc(GL_ZERO, GL_ONE); //?
                //                break;
                //            case 5: // set dst to ~dst
                //                glEnable(GL_BLEND);
                //                glBlendFunc(GL_ZERO, GL_ONE_MINUS_DST_COLOR); //?
                //                break;
                //            case 6: // set dst to src&dst
                //            case 7: // set dst to ~(src&dst)
                //            case 8: // set dst to src|dst
                //            case 9: // set dst to ~(src|dst)
                //            case 10: // set dst to src xor dst
                //            case 11: // set dst to ~(src xor dst)
                //            case 12: // set dst to src&~dst
                //            case 13: // set dst to ~src&dst
                //            case 14: // set dst to src|~dst
                //            case 15: // set dst to ~src|dst
                //                ERROR_LOG("logicopenable %d not supported\n", bpmem.blendmode.logicmode);
                //                break;

                //        }
                //    }
                //}
            }
            /*if (changes & 4) {
                // pointless
                //if (bpmem.blendmode.dither) glEnable(GL_DITHER);
                //else glDisable(GL_DITHER);
            }
            */
			if (changes & 0xFE1)
				Renderer::SetBlendMode(false);

            if (changes & 0x18) 
                Renderer::SetColorMask();
        }
        break;

	case BPMEM_FOGRANGE:
		if (changes) {
			// TODO(XK): Fog range format
			//glFogi(GL_FOG_START, ...
			//glFogi(GL_FOG_END, ...
		}
		break;

    case BPMEM_FOGPARAM0:
    case BPMEM_FOGBEXPONENT: 
    case BPMEM_FOGBMAGNITUDE:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
        }
        break;

    case BPMEM_FOGPARAM3:
        //fog settings
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
           //printf("%f %f magnitude: %x\n", bpmem.fog.a.GetA(),bpmem.fog.c_proj_fsel.GetC(), bpmem.fog.b_magnitude);
            switch(bpmem.fog.c_proj_fsel.fsel)
            {
                case 0: // Off
                    glDisable(GL_FOG); // Should be what we do
                break;
                case 2: // Linear
                    glFogi(GL_FOG_MODE, GL_LINEAR);
                    glEnable(GL_FOG);
                break;
                case 4: // exp
                    glFogi(GL_FOG_MODE, GL_EXP);
                    glEnable(GL_FOG);
                break;
                case 5: // exp2
                    glFogi(GL_FOG_MODE, GL_EXP2);
                    glEnable(GL_FOG);
                break;
                case 6: // Backward exp
                case 7: // Backward exp2
                    // TODO: Figure out how to do these in GL
					//TEV_FSEL_BX, TEV_FSEL_BX2?
                default:
					DEBUG_LOG("Non-Emulated Fog selection %d\n", bpmem.fog.c_proj_fsel.fsel);
                break;
            }
        }
        break;

    case BPMEM_FOGCOLOR:
        if (changes)
        {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            float fogcolor[4] = { ((bpmem.fog.color>>16)&0xff)/255.0f, ((bpmem.fog.color>>8)&0xff)/255.0f, (bpmem.fog.color&0xff)/255.0f, (bpmem.fog.color>>24)/255.0f };
            //printf("r: %f g: %f b: %f a: %f %x %x %x %f\n",fogcolor[0],fogcolor[1], fogcolor[2], fogcolor[3], bpmem.fogRangeAdj, bpmem.unknown15[0],bpmem.unknown15[1],bpmem.unknown15[2]);
            glFogfv(GL_FOG_COLOR, fogcolor);
        }
        break;

    case BPMEM_TEXINVALIDATE:
        //TexCache_Invalidate();
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

        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            if (!Renderer::SetScissorRect()) {
                if (addr == BPMEM_SCISSORBR ) 
                    ERROR_LOG("bad scissor!\n");
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
    case 0x45: //GXSetDrawDone
        VertexManager::Flush();
        switch (newval & 0xFF)
        {
        case 0x02:
            g_VideoInitialize.pSetPEFinish(); // may generate interrupt
            DebugLog("GXSetDrawDone SetPEFinish (value: 0x%02X)", (newval & 0xFFFF));
            break;

        default:
            DebugLog("GXSetDrawDone ??? (value 0x%02X)", (newval & 0xFFFF));
            break;
        }
        break;

    case BPMEM_PE_TOKEN_ID:
        g_VideoInitialize.pSetPEToken(static_cast<u16>(newval & 0xFFFF), FALSE);
        DebugLog("SetPEToken 0x%04x", (newval & 0xFFFF));
        break;

    case BPMEM_PE_TOKEN_INT_ID:
        g_VideoInitialize.pSetPEToken(static_cast<u16>(newval & 0xFFFF), TRUE);
        DebugLog("SetPEToken + INT 0x%04x", (newval & 0xFFFF));
        break;

    case 0x67: // set gp metric?
        break;

    case 0x52:
        {
			DVSTARTSUBPROFILE("LoadBPReg:swap");
            VertexManager::Flush();

            ((u32*)&bpmem)[addr] = newval;
			//The bottom right is within the rectangle.
			TRectangle rc = {
                (int)(bpmem.copyTexSrcXY.x),
                (int)(bpmem.copyTexSrcXY.y),
                (int)((bpmem.copyTexSrcXY.x + bpmem.copyTexSrcWH.x + 1)),
                (int)((bpmem.copyTexSrcXY.y + bpmem.copyTexSrcWH.y + 1))
            };
			float MValueX = OpenGL_GetXmax();
			float MValueY = OpenGL_GetYmax();
			//Need another rc here to get it to scale.
			//Here the bottom right is the out of the rectangle.
            TRectangle multirc = {
                (int)(bpmem.copyTexSrcXY.x * MValueX),
                (int)(bpmem.copyTexSrcXY.y * MValueY),
                (int)((bpmem.copyTexSrcXY.x * MValueX + (bpmem.copyTexSrcWH.x + 1) * MValueX)),
                (int)((bpmem.copyTexSrcXY.y * MValueY + (bpmem.copyTexSrcWH.y + 1) * MValueY))
            };

			UPE_Copy PE_copy;
			PE_copy.Hex = bpmem.triggerEFBCopy;

            if (PE_copy.copy_to_xfb == 0) {
				// EFB to texture 
                // for some reason it sets bpmem.zcontrol.pixel_format to PIXELFMT_Z24 every time a zbuffer format is given as a dest to GXSetTexCopyDst
				if (g_Config.bEFBCopyDisable) {
					glViewport(rc.left,rc.bottom,rc.right,rc.top);
					glScissor(rc.left,rc.bottom,rc.right,rc.top);
				}
				else if (g_Config.bCopyEFBToRAM) {
					TextureConverter::EncodeToRam(bpmem.copyTexDest<<5, bpmem.zcontrol.pixel_format==PIXELFMT_Z24, PE_copy.intensity_fmt>0,
                    (PE_copy.target_pixel_format/2)+((PE_copy.target_pixel_format&1)*8), PE_copy.half_scale>0, rc);
				}
				else {
					TextureMngr::CopyRenderTargetToTexture(bpmem.copyTexDest<<5, bpmem.zcontrol.pixel_format==PIXELFMT_Z24, PE_copy.intensity_fmt>0,
                    (PE_copy.target_pixel_format/2)+((PE_copy.target_pixel_format&1)*8), PE_copy.half_scale>0, &rc);
				}
			}
            else {
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
					Renderer::Swap(multirc);
				}
				g_VideoInitialize.pCopiedToXFB();
            }

            // clearing
            if (PE_copy.clear) {
                // clear color
                Renderer::SetRenderMode(Renderer::RM_Normal);

                u32 nRestoreZBufferTarget = Renderer::GetZBufferTarget();
                
                glViewport(0, 0, Renderer::GetTargetWidth(), Renderer::GetTargetHeight());

                // Always set the scissor in case it was set by the game and has not been reset                
                glScissor(multirc.left, (Renderer::GetTargetHeight() - multirc.bottom), 
                    (multirc.right - multirc.left), (multirc.bottom - multirc.top)); 

                VertexShaderManager::SetViewportChanged();

                // Since clear operations use the source rectangle, we have to do
				// regular renders (glClear clears the entire buffer)
                if (bpmem.blendmode.colorupdate || bpmem.blendmode.alphaupdate || bpmem.zmode.updateenable)
				{                    
                    GLbitfield bits = 0;
                    if (bpmem.blendmode.colorupdate || bpmem.blendmode.alphaupdate) {
                        u32 clearColor = (bpmem.clearcolorAR << 16) | bpmem.clearcolorGB;
                        glClearColor(((clearColor>>16) & 0xff)*(1/255.0f),

							         ((clearColor>>8 ) & 0xff)*(1/255.0f),
                                     ((clearColor>>0 ) & 0xff)*(1/255.0f),
									 ((clearColor>>24) & 0xff)*(1/255.0f));
                        bits |= GL_COLOR_BUFFER_BIT;
                    }
                    if (bpmem.zmode.updateenable) {
                        glClearDepth((float)(bpmem.clearZValue&0xFFFFFF) / float(0xFFFFFF));
                        bits |= GL_DEPTH_BUFFER_BIT;
                    }
                    if (nRestoreZBufferTarget )
                        glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT); // don't clear ztarget here
                    glClear(bits);
                }

                if (bpmem.zmode.updateenable && nRestoreZBufferTarget) { // have to clear the target zbuffer
                    glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT);
                    GL_REPORT_ERRORD();
                    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
                    
                    // red should probably be the LSB
                    glClearColor(((bpmem.clearZValue>>0)&0xff)*(1/255.0f),
						         ((bpmem.clearZValue>>8)&0xff)*(1/255.0f),
                                 ((bpmem.clearZValue>>16)&0xff)*(1/255.0f), 0);
                    glClear(GL_COLOR_BUFFER_BIT);
                    Renderer::SetColorMask();
                    GL_REPORT_ERRORD();   
                }

                if (nRestoreZBufferTarget) {
                    // restore target
                    GLenum s_drawbuffers[2] = {GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT};
                    glDrawBuffers(2, s_drawbuffers);
                }
                
                Renderer::SetScissorRect(); // reset the scissor rect
            }
        }
        break;

    case 0x65: //GXLoadTlut
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
        switch(addr & 0xFC)  //texture sampler filter
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
            switch(addr&0xF0) {
            case 0x10: // tevindirect 0-15
                if (changes) {
                    VertexManager::Flush();
                    ((u32*)&bpmem)[addr] = newval;
                    PixelShaderManager::SetTevIndirectChanged(addr-0x10);
                }
                break;

            case 0x30:
                if (changes) {
                    VertexManager::Flush();
                    ((u32*)&bpmem)[addr] = newval;
                    PixelShaderManager::SetTexDimsChanged((addr>>1)&0x7);
                }
                break;

            case 0xC0:
            case 0xD0:
                if (changes)
                {
                    VertexManager::Flush();
                    ((u32*)&bpmem)[addr] = newval;
                    PixelShaderManager::SetTevCombinerChanged((addr&0x1f)/2);
                }
                break;

            case 0x20:
            case 0x80:
            case 0x90:
            case 0xA0:
            case 0xB0:
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

// Call browser: OpcodeDecoding.cpp ExecuteDisplayList > Decode() > LoadBPReg()
void LoadBPReg(u32 value0)
{
    DVSTARTPROFILE();
    //handle the mask register
    int opcode = value0 >> 24;
    int oldval = ((u32*)&bpmem)[opcode];
    int newval = (oldval & ~bpmem.bpMask) | (value0 & bpmem.bpMask);
    int changes = (oldval ^ newval) & 0xFFFFFF;
    //reset the mask register
    if (opcode != 0xFE)
        bpmem.bpMask = 0xFFFFFF;
    //notify the video handling so it can update render states
    BPWritten(opcode, changes, newval);
}

// Called when loading a saved state.
// Needs more testing though.
void BPReload()
{
	for (int i = 0; i < 254; i++)
	{
		switch (i) {
		
		case 0x41:
		case 0x45: //GXSetDrawDone
		case 0x52:
		case 0x65:
		case 0x67: // set gp metric?
		case BPMEM_PE_TOKEN_ID:
		case BPMEM_PE_TOKEN_INT_ID:
			// Cases in which we DON'T want to reload the BP
			continue;
		default:
			BPWritten(i, 0xFFFFFF, ((u32*)&bpmem)[i]);
		}
	}
}
