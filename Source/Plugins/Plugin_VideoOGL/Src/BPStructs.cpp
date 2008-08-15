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

#include "Globals.h"

#include "VertexLoader.h"

#include "BPStructs.h"
#include "Render.h"
#include "OpcodeDecoding.h"
#include "TextureMngr.h"
#include "TextureDecoder.h"
#include "VertexShaderManager.h"
#include "PixelShaderManager.h"

// State translation lookup tables
static const GLenum glSrcFactors[8] =
{
    GL_ZERO,
    GL_ONE,
    GL_DST_COLOR,
    GL_ONE_MINUS_DST_COLOR,
    GL_SRC_ALPHA,
    GL_ONE_MINUS_SRC_ALPHA, 
    GL_DST_ALPHA,
    GL_ONE_MINUS_DST_ALPHA
};

static const GLenum glDestFactors[8] = {
    GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
    GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,  GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA };

static const GLenum glCmpFuncs[8] =  { GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS };

static const GLenum glLogicOpCodes[16] = {
    GL_CLEAR, GL_SET, GL_COPY, GL_COPY_INVERTED, GL_NOOP, GL_INVERT, GL_AND, GL_NAND,
    GL_OR, GL_NOR, GL_XOR, GL_EQUIV, GL_AND_REVERSE, GL_AND_INVERTED, GL_OR_REVERSE, GL_OR_INVERTED };

void BPInit()
{
    memset(&bpmem, 0, sizeof(bpmem));
    bpmem.bpMask = 0xFFFFFF;
}

void BPWritten(int addr, int changes, int newval)
{
    DVSTARTPROFILE();

    //static int count = 0;
    //ERROR_LOG("(%d) %x: %x\n", count++, addr, newval);

    switch(addr)
    {
    case BPMEM_GENMODE:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            PRIM_LOG("genmode: texgen=%d, col=%d, ms_en=%d, tev=%d, culmode=%d, ind=%d, zfeeze=%d\n", bpmem.genMode.numtexgens, bpmem.genMode.numcolchans,
                bpmem.genMode.ms_en, bpmem.genMode.numtevstages+1, bpmem.genMode.cullmode, bpmem.genMode.numindstages, bpmem.genMode.zfreeze);

            // none, ccw, cw, ccw
            if (bpmem.genMode.cullmode>0) {
                glEnable(GL_CULL_FACE);
                glFrontFace(bpmem.genMode.cullmode==2?GL_CCW:GL_CW);
            }
            else glDisable(GL_CULL_FACE);

            PixelShaderMngr::SetGenModeChanged();
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
            PixelShaderMngr::SetIndMatrixChanged((addr-BPMEM_IND_MTX)/3);
        }
        break;
    case BPMEM_RAS1_SS0:
    case BPMEM_RAS1_SS1:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            PixelShaderMngr::SetIndTexScaleChanged();
        }
        break;
    case BPMEM_ZMODE:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            PRIM_LOG("zmode: test=%d, func=%d, upd=%d\n", bpmem.zmode.testenable, bpmem.zmode.func, bpmem.zmode.updateenable);
            
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

            if( !bpmem.zmode.updateenable )
                Renderer::SetRenderMode(Renderer::RM_Normal);
        }
        break;

    case BPMEM_ALPHACOMPARE:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            PRIM_LOG("alphacmp: ref0=%d, ref1=%d, comp0=%d, comp1=%d, logic=%d\n", bpmem.alphaFunc.ref0, bpmem.alphaFunc.ref1,
                bpmem.alphaFunc.comp0, bpmem.alphaFunc.comp1, bpmem.alphaFunc.logic);
            PixelShaderMngr::SetAlpha(bpmem.alphaFunc);
        }
        break;
        
    case BPMEM_CONSTANTALPHA:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            PRIM_LOG("constalpha: alp=%d, en=%d\n", bpmem.dstalpha.alpha, bpmem.dstalpha.enable);
            PixelShaderMngr::SetDestAlpha(bpmem.dstalpha);
        }
        break;

    case BPMEM_LINEPTWIDTH:
        {
        float fratio = VertexShaderMngr::rawViewport[0] != 0 ? (float)Renderer::GetTargetWidth()/640.0f : 1.0f;
        if( bpmem.lineptwidth.linesize > 0 ) {
            glLineWidth((float)bpmem.lineptwidth.linesize*fratio/6.0f); // scale by ratio of widths
        }
        if( bpmem.lineptwidth.pointsize > 0 )
            glPointSize((float)bpmem.lineptwidth.pointsize*fratio/6.0f);
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

            if (changes & 1) {
                if (bpmem.blendmode.blendenable) {
                    glEnable(GL_BLEND);
                    
                }
                else glDisable(GL_BLEND);
            }
            if( changes & 2 ) {
                if( Renderer::CanBlendLogicOp() ) {
                    if( bpmem.blendmode.logicopenable ) {
                        glEnable(GL_COLOR_LOGIC_OP);
						PanicAlert("Logic Op Blend : %i", bpmem.blendmode.logicmode);
                        glLogicOp(glLogicOpCodes[bpmem.blendmode.logicmode]);
                    }
                    else glDisable(GL_COLOR_LOGIC_OP);
                }
                //else {
                //    if( bpmem.blendmode.logicopenable ) {
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
            if (changes & 4) {
                // pointless
                //if (bpmem.blendmode.dither) glEnable(GL_DITHER);
                //else glDisable(GL_DITHER);
            }
            if( changes & 0xFE0) {
                if( !bpmem.blendmode.subtract )
                    glBlendFunc(glSrcFactors[bpmem.blendmode.srcfactor], glDestFactors[bpmem.blendmode.dstfactor]);
            }
            if (changes & 0x800) {
                glBlendEquation(bpmem.blendmode.subtract?GL_FUNC_REVERSE_SUBTRACT:GL_FUNC_ADD);
                if( bpmem.blendmode.subtract )
                    glBlendFunc(GL_ONE, GL_ONE);
                else
                    glBlendFunc(glSrcFactors[bpmem.blendmode.srcfactor], glDestFactors[bpmem.blendmode.dstfactor]);
            }

            if (changes & 0x18) 
                SetColorMask();
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
        }
        break;

    case BPMEM_FOGCOLOR:
        if (changes)
        {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            float fogcolor[4] = { ((bpmem.fog.color>>16)&0xff)/255.0f, ((bpmem.fog.color>>8)&0xff)/255.0f, (bpmem.fog.color&0xff)/255.0f, (bpmem.fog.color>>24)/255.0f };
        }
        break;

    case BPMEM_TEXINVALIDATE:
        //TexCache_Invalidate();
        break;

    case BPMEM_SCISSOROFFSET:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            SetScissorRect();
        }
        break;

    case BPMEM_SCISSORTL:
    case BPMEM_SCISSORBR:

        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            if( !SetScissorRect() ) {
                if( addr == BPMEM_SCISSORBR ) 
                    ERROR_LOG("bad scissor!\n");
            }
        }
        break;
    case BPMEM_ZTEX1:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            PRIM_LOG("ztex bias=0x%x\n", bpmem.ztex1.bias);
            PixelShaderMngr::SetZTetureBias(bpmem.ztex1.bias);
        }
        break;
    case BPMEM_ZTEX2:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
#ifdef _DEBUG
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
            PixelShaderMngr::SetTevKSelChanged(addr-0xf6);
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
                PixelShaderMngr::SetTevOrderChanged(addr-0x28);
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
                PixelShaderMngr::SetColorChanged(bpmem.tevregs[num].high.type, num);
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
                    PixelShaderMngr::SetTevIndirectChanged(addr-0x10);
                }
                break;

            case 0x30:
                if( changes ) {
                    VertexManager::Flush();
                    ((u32*)&bpmem)[addr] = newval;
                    PixelShaderMngr::SetTexDimsChanged((addr>>1)&0x7);
                }
                break;

            case 0xC0:
            case 0xD0:
                if (changes)
                {
                    VertexManager::Flush();
                    ((u32*)&bpmem)[addr] = newval;
                    PixelShaderMngr::SetTevCombinerChanged((addr&0x1f)/2);
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

void SetColorMask()
{
    if (bpmem.blendmode.alphaupdate && bpmem.blendmode.colorupdate)
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    else if (bpmem.blendmode.alphaupdate)
        glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_TRUE);
    else if (bpmem.blendmode.colorupdate) 
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_FALSE);
}

bool SetScissorRect()
{
    int xoff = bpmem.scissorOffset.x*2-342;
    int yoff = bpmem.scissorOffset.y*2-342;
    //printf("xoff: %d yoff: %d\n", xoff, yoff);
    RECT rc;
    rc.left = bpmem.scissorTL.x + xoff - 342;
    rc.left *= MValue;
    if (rc.left < 0) rc.left = 0;
    rc.top = bpmem.scissorTL.y + yoff - 342;
    rc.top *= MValue;
    if (rc.top < 0) rc.top = 0;
    
    rc.right = bpmem.scissorBR.x + xoff - 342 +1;
    rc.right *= MValue;
    if (rc.right > 640 * MValue) rc.right = 640 * MValue;
    rc.bottom = bpmem.scissorBR.y + yoff - 342 +1;
    rc.bottom *= MValue;
    if (rc.bottom > 480 * MValue) rc.bottom = 480 * MValue;

    //printf("scissor: lt=(%d,%d),rb=(%d,%d),off=(%d,%d)\n", rc.left, rc.top, rc.right, rc.bottom, xoff, yoff);

    if( rc.right>=rc.left && rc.bottom>=rc.top ) {
        glScissor(rc.left, Renderer::GetTargetHeight()-(rc.bottom), (rc.right-rc.left), (rc.bottom-rc.top));
        return true;
    }

    return false;
}

void LoadBPReg(u32 value0)
{
    DVSTARTPROFILE();

    //handle the mask register
    int opcode = value0 >> 24;
    int oldval = ((u32*)&bpmem)[opcode];
    int newval = (((u32*)&bpmem)[opcode] & ~bpmem.bpMask) | (value0 & bpmem.bpMask);
    int changes = (oldval ^ newval) & 0xFFFFFF;
    //reset the mask register
    if (opcode != 0xFE)
        bpmem.bpMask = 0xFFFFFF;

    switch (opcode)
    {
    case 0x45: //GXSetDrawDone
        VertexManager::Flush();
        switch (value0 & 0xFF)
        {
        case 0x02:
            g_VideoInitialize.pSetPEFinish(); // may generate interrupt
            DebugLog("GXSetDrawDone SetPEFinish (value: 0x%02X)", (value0 & 0xFFFF));
            break;

        default:
            DebugLog("GXSetDrawDone ??? (value 0x%02X)", (value0 & 0xFFFF));
            break;
        }
        break;

    case BPMEM_PE_TOKEN_ID:
        g_VideoInitialize.pSetPEToken(static_cast<WORD>(value0 & 0xFFFF), FALSE);
        DebugLog("SetPEToken 0x%04x", (value0 & 0xFFFF));
        break;

    case BPMEM_PE_TOKEN_INT_ID:
        g_VideoInitialize.pSetPEToken(static_cast<WORD>(value0 & 0xFFFF), TRUE);
        DebugLog("SetPEToken + INT 0x%04x", (value0 & 0xFFFF));
        break;

    case 0x67: // set gp metric?
        break;

    case 0x52:
        {
            DVProfileFunc _pf("LoadBPReg:swap");
            VertexManager::Flush();

            ((u32*)&bpmem)[opcode] = newval;
			TRectangle rc = {
                (int)(bpmem.copyTexSrcXY.x),
                (int)(bpmem.copyTexSrcXY.y),
                (int)((bpmem.copyTexSrcXY.x+bpmem.copyTexSrcWH.x)),
                (int)((bpmem.copyTexSrcXY.y+bpmem.copyTexSrcWH.y))
            };

            UPE_Copy PE_copy;
            PE_copy.Hex = bpmem.triggerEFBCopy;

            if (PE_copy.copy_to_xfb == 0) {
                // EFB to texture 
                // for some reason it sets bpmem.zcontrol.pixel_format to PIXELFMT_Z24 every time a zbuffer format is given as a dest to GXSetTexCopyDst
                TextureMngr::CopyRenderTargetToTexture(bpmem.copyTexDest<<5, bpmem.zcontrol.pixel_format==PIXELFMT_Z24, PE_copy.intensity_fmt>0,
                    (PE_copy.target_pixel_format/2)+((PE_copy.target_pixel_format&1)*8), PE_copy.half_scale>0, &rc);
            }
            else {
                // EFB to XFB
                Renderer::Swap(rc);
                g_VideoInitialize.pCopiedToXFB();
            }

            // clearing
            if (PE_copy.clear) {
                // clear color
                Renderer::SetRenderMode(Renderer::RM_Normal);

                u32 nRestoreZBufferTarget = Renderer::GetZBufferTarget();
                
                glViewport(0, 0, Renderer::GetTargetWidth(), Renderer::GetTargetHeight());
                // if copied to texture, set the dimensions to the source copy dims, otherwise, clear the entire buffer
                if( PE_copy.copy_to_xfb == 0 )
                    glScissor(rc.left, (Renderer::GetTargetHeight()-rc.bottom), (rc.right-rc.left), (rc.bottom-rc.top));   
                VertexShaderMngr::SetViewportChanged();

                // since clear operations use the source rectangle, have to do regular renders (glClear clears the entire buffer)
                if( bpmem.blendmode.colorupdate || bpmem.blendmode.alphaupdate || bpmem.zmode.updateenable ) {
                    
                    GLbitfield bits = 0;
                    if( bpmem.blendmode.colorupdate || bpmem.blendmode.alphaupdate ) {
                        u32 clearColor = (bpmem.clearcolorAR<<16)|bpmem.clearcolorGB;
                        glClearColor(((clearColor>>16)&0xff)*(1/255.0f),((clearColor>>8)&0xff)*(1/255.0f),
                            ((clearColor>>0)&0xff)*(1/255.0f),((clearColor>>24)&0xff)*(1/255.0f));
                        bits |= GL_COLOR_BUFFER_BIT;
                    }

                    if( bpmem.zmode.updateenable ) {
                        glClearDepth((float)(bpmem.clearZValue&0xFFFFFF) / float(0xFFFFFF));
                        bits |= GL_DEPTH_BUFFER_BIT;
                    }

                    if( nRestoreZBufferTarget )
                        glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT); // don't clear ztarget here
                    
                    glClear(bits);
                }

                if (bpmem.zmode.updateenable && nRestoreZBufferTarget ) { // have to clear the target zbuffer

                    glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT);
                    GL_REPORT_ERRORD();

                    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
                    
                    // red should probably be the LSB
                    glClearColor(((bpmem.clearZValue>>0)&0xff)*(1/255.0f),((bpmem.clearZValue>>8)&0xff)*(1/255.0f),
                        ((bpmem.clearZValue>>16)&0xff)*(1/255.0f), 0);
                    glClear(GL_COLOR_BUFFER_BIT);
                    SetColorMask();
                    GL_REPORT_ERRORD();   
                }

                if( nRestoreZBufferTarget ) {
                    // restore target
                    GLenum s_drawbuffers[2] = {GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT};
                    glDrawBuffers(2, s_drawbuffers);
                }
                
                if( PE_copy.copy_to_xfb == 0 )
                    SetScissorRect(); // reset the scissor rect
            }
        }
        break;

    case 0x65: //GXLoadTlut
        {
            DVProfileFunc _pf("LoadBPReg:GXLoadTlut");
            VertexManager::Flush();
            ((u32*)&bpmem)[opcode] = newval;

            u32 tlutTMemAddr = (value0&0x3FF)<<9;
            u32 tlutXferCount = (value0&0x1FFC00)>>5; 
            //do the transfer!!			
            memcpy_gc(texMem + tlutTMemAddr, g_VideoInitialize.pGetMemoryPointer((bpmem.tlutXferSrc&0xFFFFF)<<5), tlutXferCount);
            // TODO(ector) : kill all textures that use this palette
            // Not sure if it's a good idea, though. For now, we hash texture palettes
        }
        break;
    }

    //notify the video handling so it can update render states
    BPWritten(opcode, changes, newval);
    //((u32*)&bpmem)[opcode] = newval;
}

void BPReload()
{
    for (int i=0; i<254; i++)
        BPWritten(i, 0xFFFFFF, ((u32*)&bpmem)[i]);
}


size_t BPSaveLoadState(char *ptr, BOOL save)
{
    BEGINSAVELOAD;
    SAVELOAD(&bpmem,sizeof(BPMemory));
    if (!save)
        BPReload();
    //char temp[256];
    //sprintf(temp,"MOJS %08x",(bpmem.clearcolorAR<<16)|(bpmem.clearcolorGB));
    //g_VideoInitialize.pLog(temp, FALSE);
    ENDSAVELOAD;
}
