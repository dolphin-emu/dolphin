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

#include "D3DBase.h"

#include "Config.h"
#include "Common.h"
#include "Profiler.h"
#include "BPStructs.h"
#include "OpcodeDecoding.h"
#include "TextureCache.h"
#include "TextureDecoder.h"
#include "VertexManager.h"
#include "PixelShaderGen.h"
#include "PixelShaderManager.h"
#include "Utils.h"

#include "main.h" //for the plugin interface

bool textureChanged[8];

const bool renderFog = false;

using namespace D3D;

// State translation lookup tables
static const D3DBLEND d3dSrcFactors[8] =
{
	D3DBLEND_ZERO,
	D3DBLEND_ONE,
	D3DBLEND_DESTCOLOR,
	D3DBLEND_INVDESTCOLOR,
	D3DBLEND_SRCALPHA,
	D3DBLEND_INVSRCALPHA, 
	D3DBLEND_DESTALPHA,
	D3DBLEND_INVDESTALPHA
};

static const D3DBLEND d3dDestFactors[8] =
{
	D3DBLEND_ZERO,
	D3DBLEND_ONE,
	D3DBLEND_SRCCOLOR,
	D3DBLEND_INVSRCCOLOR,
	D3DBLEND_SRCALPHA,
	D3DBLEND_INVSRCALPHA, 
	D3DBLEND_DESTALPHA,
	D3DBLEND_INVDESTALPHA
};

static const D3DCULL d3dCullModes[4] = 
{
	D3DCULL_NONE,
	D3DCULL_CCW,
	D3DCULL_CW,
	D3DCULL_CCW
};

static const D3DCMPFUNC d3dCmpFuncs[8] = 
{
	D3DCMP_NEVER,
	D3DCMP_LESS,
	D3DCMP_EQUAL,
	D3DCMP_LESSEQUAL,
	D3DCMP_GREATER,
	D3DCMP_NOTEQUAL,
	D3DCMP_GREATEREQUAL,
	D3DCMP_ALWAYS
};

static const D3DTEXTUREFILTERTYPE d3dMipFilters[4] = 
{
	D3DTEXF_NONE,
	D3DTEXF_POINT,
	D3DTEXF_ANISOTROPIC,
	D3DTEXF_LINEAR, //reserved
};

static const D3DTEXTUREADDRESS d3dClamps[4] =
{
	D3DTADDRESS_CLAMP,
	D3DTADDRESS_WRAP,
	D3DTADDRESS_MIRROR,
	D3DTADDRESS_WRAP //reserved
};

void BPInit()
{
	memset(&bpmem, 0, sizeof(bpmem));
	bpmem.bpMask = 0xFFFFFF;
}

// __________________________________________________________________________________________________
// BPWritten
//
void BPWritten(int addr, int changes, int newval)
{
    switch(addr)
	{
	case BPMEM_GENMODE:
		if (changes)
		{
			VertexManager::Flush();
			((u32*)&bpmem)[addr] = newval;
			
			// dev->SetRenderState(D3DRS_CULLMODE, d3dCullModes[bpmem.genMode.cullmode]);
			Renderer::SetRenderState(D3DRS_CULLMODE, d3dCullModes[bpmem.genMode.cullmode]);

			if (bpmem.genMode.cullmode == 3)
			{
				// dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
				Renderer::SetRenderState(D3DRS_COLORWRITEENABLE, 0);
			}
			else
			{
				DWORD write = 0;
				if (bpmem.blendmode.alphaupdate) 
					write = D3DCOLORWRITEENABLE_ALPHA;
				if (bpmem.blendmode.colorupdate) 
					write |= D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE;
				
				// dev->SetRenderState(D3DRS_COLORWRITEENABLE, write);
				Renderer::SetRenderState(D3DRS_COLORWRITEENABLE, write);
			}
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
		if (changes)
		{
			VertexManager::Flush();
			((u32*)&bpmem)[addr] = newval;
			if (bpmem.zmode.testenable)
			{
				// dev->SetRenderState(D3DRS_ZENABLE, TRUE);
				// dev->SetRenderState(D3DRS_ZWRITEENABLE, bpmem.zmode.updateenable);
				// dev->SetRenderState(D3DRS_ZFUNC,d3dCmpFuncs[bpmem.zmode.func]);

				Renderer::SetRenderState(D3DRS_ZENABLE, TRUE);
				Renderer::SetRenderState(D3DRS_ZWRITEENABLE, bpmem.zmode.updateenable);
				Renderer::SetRenderState(D3DRS_ZFUNC, d3dCmpFuncs[bpmem.zmode.func]);
			}
			else
			{
				// if the test is disabled write is disabled too
				// dev->SetRenderState(D3DRS_ZENABLE,		FALSE);
				// dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

				Renderer::SetRenderState(D3DRS_ZENABLE, FALSE);
				Renderer::SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
			}			

            //if (!bpmem.zmode.updateenable)
            //    Renderer::SetRenderMode(Renderer::RM_Normal);
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
            PixelShaderManager::SetDestAlpha(bpmem.dstalpha);
        }
        break;

	case BPMEM_LINEPTWIDTH:
		{
			// We can't change line width in D3D unless we use ID3DXLine
			//bpmem.lineptwidth.linesize);
			float psize = float(bpmem.lineptwidth.pointsize) * 6.0f;

			Renderer::SetRenderState(D3DRS_POINTSIZE, *((DWORD*)&psize));
		}
		break;
	
	case 0x43:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
        }
        break;
	
	case BPMEM_BLENDMODE:	
		if (changes & 0xFFFF)
		{
			VertexManager::Flush();
			((u32*)&bpmem)[addr] = newval;
			if (changes & 1)
			{
				// dev->SetRenderState(D3DRS_ALPHABLENDENABLE,bpmem.blendmode.blendenable);
				Renderer::SetRenderState(D3DRS_ALPHABLENDENABLE, bpmem.blendmode.blendenable);
			}
			if (changes & 2) {} // Logic op blending. D3D can't do this but can fake some modes.
			if (changes & 4) {
				// Dithering is pointless. Will make things uglier and will be different from GC.
				// dev->SetRenderState(D3DRS_DITHERENABLE,bpmem.blendmode.dither);
			}
			D3DBLEND src = d3dSrcFactors[bpmem.blendmode.srcfactor];
			D3DBLEND dst = d3dDestFactors[bpmem.blendmode.dstfactor];

			if (changes & 0x700)
			{
				// dev->SetRenderState(D3DRS_SRCBLEND, src);
				Renderer::SetRenderState(D3DRS_SRCBLEND, src);
			}
			if (changes & 0xE0) {
				if (!bpmem.blendmode.subtract)
				{
					// dev->SetRenderState(D3DRS_DESTBLEND, dst);
					Renderer::SetRenderState(D3DRS_DESTBLEND, dst);
				}
				else
				{
					// dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
					Renderer::SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
				}
			}
			if (changes & 0x800) {
				if (bpmem.blendmode.subtract)
				{
					// dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
					// dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
					Renderer::SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
					Renderer::SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
				}
				else
				{
					// dev->SetRenderState(D3DRS_SRCBLEND, src);
					// dev->SetRenderState(D3DRS_DESTBLEND, dst);
					
					Renderer::SetRenderState(D3DRS_SRCBLEND, src);
					Renderer::SetRenderState(D3DRS_DESTBLEND, dst);
				}
				
				// dev->SetRenderState(D3DRS_BLENDOP,bpmem.blendmode.subtract?D3DBLENDOP_SUBTRACT:D3DBLENDOP_ADD);
				Renderer::SetRenderState(D3DRS_BLENDOP, bpmem.blendmode.subtract ? D3DBLENDOP_SUBTRACT : D3DBLENDOP_ADD);
			}
			//if (bpmem.blendmode.logicopenable) // && bpmem.blendmode.logicmode == 4)
			//	MessageBox(0,"LOGIC",0,0);

			if (changes & 0x18) 
			{
				// Color Mask
				DWORD write = 0;
				if (bpmem.blendmode.alphaupdate) 
					write = D3DCOLORWRITEENABLE_ALPHA;
				if (bpmem.blendmode.colorupdate) 
					write |= D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE;
				
				// dev->SetRenderState(D3DRS_COLORWRITEENABLE, write);
				Renderer::SetRenderState(D3DRS_COLORWRITEENABLE, write);
			}
		}
		break;

	case BPMEM_FOGRANGE:
		if (changes) {
			// TODO(XK): Fog range format
			//Renderer::SetRenderState(D3DRS_FOGSTART, ...
			//Renderer::SetRenderState(D3DRS_FOGEND, ...
		}
		break;

	case BPMEM_FOGPARAM0:
		{
			// u32 fogATemp = bpmem.fog.a<<12;
			// float fogA = *(float*)(&fogATemp);
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
		}
		break;

	case BPMEM_FOGBEXPONENT: 
	case BPMEM_FOGBMAGNITUDE:
		{
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
		}
		break;

	case BPMEM_FOGPARAM3:
		//fog settings
		if (changes) {
			static bool bFog = false;
			VertexManager::Flush();
			((u32*)&bpmem)[addr] = newval;
			
			if (!renderFog)
				break;

			/// u32 fogCTemp = bpmem.fog.c_proj_fsel.cShifted12 << 12;
			// float fogC = *(float*)(&fogCTemp);

			
			//printf("%f %f magnitude: %x\n", bpmem.fog.a.GetA(),bpmem.fog.c_proj_fsel.GetC(), bpmem.fog.b_magnitude);
			switch(bpmem.fog.c_proj_fsel.fsel)
			{
			case 0: // Off
				if (bFog) {
					Renderer::SetRenderState(D3DRS_FOGENABLE, false);
					bFog = false;
				}	
				break;
			case 2: // Linear
				Renderer::SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
				break;
			case 4: // exp
				Renderer::SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_EXP);
				break;
			case 5: // exp2
				Renderer::SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_EXP2);
				break;
			case 6: // Backward exp
			case 7: // Backward exp2
				PanicAlert("Backward Exponential Fog Detected");
				// TODO: Figure out how to do these in any rendering API
				//TEV_FSEL_BX, TEV_FSEL_BX2?
			default:
				PanicAlert("Non-Emulated Fog selection %d\n", bpmem.fog.c_proj_fsel.fsel);
				break;
			}

			if (bpmem.fog.c_proj_fsel.fsel > 0 && !bFog) {
				Renderer::SetRenderState(D3DRS_FOGENABLE, true);
				bFog = true;
			}

		}
		break;

	case BPMEM_FOGCOLOR:
		 if (changes) {
			VertexManager::Flush();
			((u32*)&bpmem)[addr] = newval;

			if (!renderFog)
				break;

			// dev->SetRenderState(D3DRS_FOGCOLOR,bpmem.fog.color);
			int fogcolor[3] = { bpmem.fog.color.r, bpmem.fog.color.g, bpmem.fog.color.b };
			//D3DCOLOR_RGBA(fogcolor[0], fogcolor[1], fogcolor[2], 0)
            Renderer::SetRenderState(D3DRS_FOGCOLOR, bpmem.fog.color.hex);
		}
		break;

	case BPMEM_TEXINVALIDATE:
		//TexCache_Invalidate();
		break;

	case BPMEM_SCISSOROFFSET: //TODO: investigate
		{
			VertexManager::Flush();
			((u32*)&bpmem)[addr] = newval;
			Renderer::SetScissorRect();
		}
		break;

	case BPMEM_ZTEX1:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
            //PRIM_LOG("ztex bias=0x%x\n", bpmem.ztex1.bias);
            //PixelShaderMngr::SetZTextureBias(bpmem.ztex1.bias);
        }
		break;
	case BPMEM_ZTEX2:
        if (changes) {
            VertexManager::Flush();
            ((u32*)&bpmem)[addr] = newval;
#ifdef _DEBUG
            const char* pzop[] = {"DISABLE", "ADD", "REPLACE", "?"};
            const char* pztype[] = {"Z8", "Z16", "Z24", "?"};
            DEBUG_LOG(VIDEO, "ztex op=%s, type=%s\n", pzop[bpmem.ztex2.op], pztype[bpmem.ztex2.type]);
#endif
        }
		break;

	case 0x45: //GXSetDrawDone
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

    case 0x67: // set gp metric?
        break;

	case BPMEM_TRIGGER_EFB_COPY:
		{
			VertexManager::Flush();

			((u32*)&bpmem)[addr] = newval;
			RECT rc = {
				(LONG)(bpmem.copyTexSrcXY.x*Renderer::GetXScale()),
				(LONG)(bpmem.copyTexSrcXY.y*Renderer::GetYScale()),
				(LONG)((bpmem.copyTexSrcXY.x+bpmem.copyTexSrcWH.x)*Renderer::GetXScale()),
				(LONG)((bpmem.copyTexSrcXY.y+bpmem.copyTexSrcWH.y)*Renderer::GetYScale())
			};

			UPE_Copy PE_copy;
			PE_copy.Hex = bpmem.triggerEFBCopy;

			// clamp0
			// clamp1
			// target_pixel_format
			// gamma
			// scale_something
			// clear
			// frame_to_field
			// copy_to_xfb

			// ???: start Mem to/from EFB transfer
/*			bool bMip = false; // ignored
			if (bpmem.triggerEFBCopy & EFBCOPY_GENERATEMIPS)
				bMip = true;*/

			if (PE_copy.copy_to_xfb == 0) // bpmem.triggerEFBCopy & EFBCOPY_EFBTOTEXTURE)
			{
				// EFB to texture 
                // for some reason it sets bpmem.zcontrol.pixel_format to PIXELFMT_Z24 every time a zbuffer format is given as a dest to GXSetTexCopyDst
				TextureCache::CopyEFBToRenderTarget(bpmem.copyTexDest<<5, &rc);
			}
			else
			{
				// EFB to XFB
				// MessageBox(0, "WASDF", 0, 0);
				Renderer::SwapBuffers();
				PRIM_LOG("Renderer::SwapBuffers()");
				g_VideoInitialize.pCopiedToXFB();
			}

			// clearing
			if (PE_copy.clear) // bpmem.triggerEFBCopy & EFBCOPY_CLEAR)
			{
				// it seems that the GC is able to alpha blend on color-fill
				// we cant do that so if alpha is != 255 we skip it

				// clear color
				u32 clearColor = (bpmem.clearcolorAR << 16) | bpmem.clearcolorGB;
				if (bpmem.blendmode.colorupdate)
				{
					D3DRECT drc;
					drc.x1 = rc.left;
					drc.x2 = rc.right;
					drc.y1 = rc.top;
					drc.y2 = rc.bottom;
					//D3D::dev->Clear(1, &drc, D3DCLEAR_STENCIL|D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,clearColor,1.0f,0);
					//if ((clearColor>>24) == 255)
						D3D::dev->ColorFill(D3D::GetBackBufferSurface(), &rc, clearColor);
				}
				else
				{
					// TODO:
					// bpmem.blendmode.alphaupdate
					// bpmem.blendmode.colorupdate
					// i dunno how to implement a clear on alpha only or color only
				}

				// clear z-buffer
				if (bpmem.zmode.updateenable)
				{
					float clearZ = (float)bpmem.clearZValue / float(0xFFFFFF);
					if (clearZ > 1.0f) clearZ = 1.0f;
					if (clearZ < 0.0f) clearZ = 0.0f;

					D3D::dev->Clear(0, 0, D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, 0, clearZ, 0);
				}				
			}
		}
		break;

	case BPMEM_LOADTLUT: //GXLoadTlut
		{
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

	default:
		switch (addr & 0xF8)  //texture sampler filter
		{
		case 0x80: // TEX MODE 0
		case 0xA0: 
			if (changes)
			{
				VertexManager::Flush();
				((u32*)&bpmem)[addr] = newval;
				FourTexUnits &tex = bpmem.tex[(addr&0xE0)==0xA0];
				int stage = (addr&3);//(addr>>4)&2;
				TexMode0 &tm0 = tex.texMode0[stage];
				
				D3DTEXTUREFILTERTYPE min, mag, mip;
				if (g_Config.bForceFiltering)
				{
					min = mag = mip = D3DTEXF_LINEAR;
				}
				else
				{
					min = (tm0.min_filter&4) ? D3DTEXF_LINEAR : D3DTEXF_POINT;
					mag = tm0.mag_filter ? D3DTEXF_LINEAR : D3DTEXF_POINT;
					mip = d3dMipFilters[tm0.min_filter&3];
				}
				if ((addr & 0xE0) == 0xA0)
					stage += 4;	
				
				if (g_Config.bForceMaxAniso)
				{
					mag = D3DTEXF_ANISOTROPIC;
					mip = D3DTEXF_ANISOTROPIC;
					min = D3DTEXF_ANISOTROPIC;
				}
				dev->SetSamplerState(stage, D3DSAMP_MINFILTER, min);
				dev->SetSamplerState(stage, D3DSAMP_MAGFILTER, mag);
				dev->SetSamplerState(stage, D3DSAMP_MIPFILTER, mip);
				
				dev->SetSamplerState(stage, D3DSAMP_MAXANISOTROPY,16);
				dev->SetSamplerState(stage, D3DSAMP_ADDRESSU,d3dClamps[tm0.wrap_s]);
				dev->SetSamplerState(stage, D3DSAMP_ADDRESSV,d3dClamps[tm0.wrap_t]);
				//wip
				//dev->SetSamplerState(stage,D3DSAMP_MIPMAPLODBIAS,tm0.lod_bias/4.0f);
				//char temp[256];
				//sprintf(temp,"lod %f",tm0.lod_bias/4.0f);
				//g_VideoInitialize.pLog(temp);
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
            ((u32*)&bpmem)[addr] = newval;
            if (addr&1) { // don't compare with changes!
                VertexManager::Flush();
                int num = (addr >> 1) & 0x3;
                PixelShaderManager::SetColorChanged(bpmem.tevregs[num].high.type, num);
            }
            break;

		default:
            switch (addr&0xF0)
			{
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
                    PixelShaderManager::SetTevCombinerChanged((addr & 0x1f)/2);
                }
                break;

            case 0x20:
            case 0x80:
            case 0x90:
            case 0xA0:
            case 0xB0:
            default:
				break;
			}
		}
		break;
	}
}
