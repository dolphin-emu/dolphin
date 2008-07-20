#include "D3DBase.h"

#include "Globals.h"
#include "Common.h"
#include "BPStructs.h"
#include "OpcodeDecoding.h"
#include "TextureCache.h"
#include "TextureDecoder.h"
#include "VertexHandler.h"
#include "PixelShader.h"
#include "Utils.h"

#include "main.h" //for the plugin interface

bool textureChanged[8];

// State translation lookup tables
const D3DBLEND d3dSrcFactors[8] =
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

const D3DBLEND d3dDestFactors[8] =
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

const D3DCULL d3dCullModes[4] = 
{
	D3DCULL_NONE,
	D3DCULL_CCW,
	D3DCULL_CW,
	D3DCULL_CCW
};

const D3DCMPFUNC d3dCmpFuncs[8] = 
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

const D3DTEXTUREFILTERTYPE d3dMipFilters[4] = 
{
	D3DTEXF_NONE,
	D3DTEXF_POINT,
	D3DTEXF_ANISOTROPIC,
	D3DTEXF_LINEAR, //reserved
};

const D3DTEXTUREADDRESS d3dClamps[4] =
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

using namespace D3D;
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
			CVertexHandler::Flush();
			((u32*)&bpmem)[addr] = newval;
			dev->SetRenderState(D3DRS_CULLMODE, d3dCullModes[bpmem.genMode.cullmode]); 
			if (bpmem.genMode.cullmode == 3)
				dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
			else
			{
				DWORD write = 0;
				if (bpmem.blendmode.alphaupdate) 
					write = D3DCOLORWRITEENABLE_ALPHA;
				if (bpmem.blendmode.colorupdate) 
					write |= D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE;
				dev->SetRenderState(D3DRS_COLORWRITEENABLE, write);
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
            CVertexHandler::Flush();
            ((u32*)&bpmem)[addr] = newval;
            // PixelShaderMngr::SetIndMatrixChanged((addr-BPMEM_IND_MTX)/3);
        }
        break;
    case BPMEM_RAS1_SS0:
    case BPMEM_RAS1_SS1:
        if (changes) {
            CVertexHandler::Flush();
            ((u32*)&bpmem)[addr] = newval;
            // PixelShaderMngr::SetIndTexScaleChanged();
        }
        break;

	case BPMEM_ZMODE:
		if (changes)
		{
			CVertexHandler::Flush();
			((u32*)&bpmem)[addr] = newval;
			if (bpmem.zmode.testenable)
			{
				dev->SetRenderState(D3DRS_ZENABLE, TRUE);
				dev->SetRenderState(D3DRS_ZWRITEENABLE, bpmem.zmode.updateenable);
				dev->SetRenderState(D3DRS_ZFUNC,d3dCmpFuncs[bpmem.zmode.func]);
			}
			else
			{
				// if the test is disabled write is disabled too
				dev->SetRenderState(D3DRS_ZENABLE,		FALSE);
				dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
			}			
		}
		break;

	case BPMEM_ALPHACOMPARE:
		if (changes)
		{
			CVertexHandler::Flush();
			((u32*)&bpmem)[addr] = newval;
			float f[4] =
			{
				bpmem.alphaFunc.ref0/255.0f,
				bpmem.alphaFunc.ref1/255.0f,
				0,0
			};
			dev->SetPixelShaderConstantF(PS_CONST_ALPHAREF,f,1);

			if (D3D::GetShaderVersion() == PSNONE)
			{
				dev->SetRenderState(D3DRS_ALPHATESTENABLE, (Compare)bpmem.alphaFunc.comp0 != COMPARE_ALWAYS);
				dev->SetRenderState(D3DRS_ALPHAREF, bpmem.alphaFunc.ref0*4);
				dev->SetRenderState(D3DRS_ALPHAFUNC, d3dCmpFuncs[bpmem.alphaFunc.comp0]);
			}
			// Normally, use texkill in pixel shader to emulate alpha test
		}
		break;
		
	case BPMEM_CONSTANTALPHA:
		if (changes)
		{
			CVertexHandler::Flush();
			((u32*)&bpmem)[addr] = newval;
			float f[4] = {
				bpmem.dstalpha.alpha/255.0f,0,0,0
			};
			dev->SetPixelShaderConstantF(PS_CONST_CONSTALPHA,f,1);
		}
		break;

	case BPMEM_LINEPTWIDTH:
		// We can't change line width in D3D. However, we can change point size. TODO
		//bpmem.lineptwidth.pointsize);
		//bpmem.lineptwidth.linesize);
		break;
	
	case 0x43:
        if (changes) {
            CVertexHandler::Flush();
            ((u32*)&bpmem)[addr] = newval;
        }
        break;
	
	case BPMEM_BLENDMODE:	
		if (changes & 0xFFFF)
		{
			CVertexHandler::Flush();
			((u32*)&bpmem)[addr] = newval;
			if (changes & 1) dev->SetRenderState(D3DRS_ALPHABLENDENABLE,bpmem.blendmode.blendenable);
			if (changes & 2) {} // Logic op blending. D3D can't do this but can fake some modes.
			if (changes & 4) {
				// Dithering is pointless. Will make things uglier and will be different from GC.
				// dev->SetRenderState(D3DRS_DITHERENABLE,bpmem.blendmode.dither);
			}
			D3DBLEND src = d3dSrcFactors[bpmem.blendmode.srcfactor];
			D3DBLEND dst = d3dDestFactors[bpmem.blendmode.dstfactor];

			if (changes & 0x700) {
				dev->SetRenderState(D3DRS_SRCBLEND, src);
			}
			if (changes & 0xE0) {
				if (!bpmem.blendmode.subtract)
					dev->SetRenderState(D3DRS_DESTBLEND, dst);
				else
					dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
			}
			if (changes & 0x800) {
				if (bpmem.blendmode.subtract) {
					dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
					dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
				} else {
					dev->SetRenderState(D3DRS_SRCBLEND, src);
					dev->SetRenderState(D3DRS_DESTBLEND, dst);
				}
				dev->SetRenderState(D3DRS_BLENDOP,bpmem.blendmode.subtract?D3DBLENDOP_SUBTRACT:D3DBLENDOP_ADD);
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
				dev->SetRenderState(D3DRS_COLORWRITEENABLE, write);
			}
		}
		break;

	case BPMEM_FOGPARAM0:
		{
			// u32 fogATemp = bpmem.fog.a<<12;
			// float fogA = *(float*)(&fogATemp);
            CVertexHandler::Flush();
            ((u32*)&bpmem)[addr] = newval;
		}
		break;

	case BPMEM_FOGBEXPONENT: 
	case BPMEM_FOGBMAGNITUDE:
		{
            CVertexHandler::Flush();
            ((u32*)&bpmem)[addr] = newval;
		}
		break;

	case BPMEM_FOGPARAM3:
		//fog settings
		{
			/// u32 fogCTemp = bpmem.fog.c_proj_fsel.cShifted12 << 12;
			// float fogC = *(float*)(&fogCTemp);
            CVertexHandler::Flush();
            ((u32*)&bpmem)[addr] = newval;
		}
		break;

	case BPMEM_FOGCOLOR:
		if (changes)
		{
			CVertexHandler::Flush();
			((u32*)&bpmem)[addr] = newval;
			dev->SetRenderState(D3DRS_FOGCOLOR,bpmem.fog.color);
		}
		break;

	case BPMEM_TEXINVALIDATE:
		//TexCache_Invalidate();
		break;

	case BPMEM_SCISSOROFFSET: //TODO: investigate
		{
			CVertexHandler::Flush();
			((u32*)&bpmem)[addr] = newval;
		}
		break;

	case BPMEM_SCISSORTL:
	case BPMEM_SCISSORBR:
		{
			CVertexHandler::Flush();
			((u32*)&bpmem)[addr] = newval;
			int xoff = bpmem.scissorOffset.x*2-342;
			int yoff = bpmem.scissorOffset.y*2-342;
			RECT rc;
			rc.left=bpmem.scissorTL.x + xoff - 342 -1;
			if (rc.left<0) rc.left=0;
			rc.top=bpmem.scissorTL.y + yoff - 342 -1;
			if (rc.top<0) rc.top=0;
			rc.right=bpmem.scissorBR.x + xoff - 342 +2;
			if (rc.right>640) rc.right=640;
            rc.bottom=bpmem.scissorBR.y + yoff - 342 +2;
			if (rc.bottom>480) rc.bottom=480;
			char temp[256];
			sprintf(temp,"ScissorRect: %i %i %i %i",rc.left,rc.top,rc.right,rc.bottom);
			g_VideoInitialize.pLog(temp, FALSE);
			dev->SetRenderState(D3DRS_SCISSORTESTENABLE,TRUE);
			Renderer::SetScissorBox(rc);
		}
		break;
	case BPMEM_ZTEX1:
        if (changes) {
            CVertexHandler::Flush();
            ((u32*)&bpmem)[addr] = newval;
            //PRIM_LOG("ztex bias=0x%x\n", bpmem.ztex1.bias);
            //PixelShaderMngr::SetZTextureBias(bpmem.ztex1.bias);
        }
		break;
	case BPMEM_ZTEX2:
        if (changes) {
            CVertexHandler::Flush();
            ((u32*)&bpmem)[addr] = newval;
#ifdef _DEBUG
            const char* pzop[] = {"DISABLE", "ADD", "REPLACE", "?"};
            const char* pztype[] = {"Z8", "Z16", "Z24", "?"};
            DebugLog("ztex op=%s, type=%s\n", pzop[bpmem.ztex2.op], pztype[bpmem.ztex2.type]);
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
            CVertexHandler::Flush();
            ((u32*)&bpmem)[addr] = newval;
            // PixelShaderMngr::SetTevKSelChanged(addr-0xf6);
        }
        break;

	default:
		switch(addr & 0xF8)  //texture sampler filter
		{
		case 0x80: // TEX MODE 0
		case 0xA0: 
			if (changes)
			{
				CVertexHandler::Flush();
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
				textureChanged[((addr&0xE0)==0xA0)*4+(addr&3)] = true;
				CVertexHandler::Flush();
			}
			break;
		case 0x8C://TEX IMAGE 1
		case 0xAC:
			if (changes)
			{
				textureChanged[((addr&0xE0)==0xA0)*4+(addr&3)] = true;
				CVertexHandler::Flush();
			}
			break;
		case 0x90://TEX IMAGE 2
		case 0xB0:
			if (changes)
			{
				textureChanged[((addr&0xE0)==0xA0)*4+(addr&3)] = true;
				CVertexHandler::Flush();
			}
			break;
		case 0x94://TEX IMAGE 3
		case 0xB4:
			if (changes)
			{
				textureChanged[((addr&0xE0)==0xA0)*4+(addr&3)] = true;
				CVertexHandler::Flush();
			}
			break;
		case 0x98://TEX TLUT
		case 0xB8:
			if (changes)
			{
				textureChanged[((addr&0xE0)==0xA0)*4+(addr&3)] = true;
				CVertexHandler::Flush();
			}
			break;
		case 0x9C://TEX UNKNOWN
		case 0xBC:
			break;
		default:
			switch(addr&0xF0) {
			case 0x30:
				{
					int tc = addr&0x1;
					int stage = (addr>>1)&0x7;
					TCoordInfo &tci = bpmem.texcoords[stage];
					//TCInfo &t = (tc?tci.s:tc.t);
					// cylindric wrapping here
					//dev->SetRenderState(D3DRS_WRAP0+stage, D3DWRAPCOORD_0);
				}
				break;
            case 0xC0:
            case 0xD0:
                if (changes)
                {
                    CVertexHandler::Flush();
                    ((u32*)&bpmem)[addr] = newval;
                    // PixelShaderMngr::SetTevCombinerChanged((addr&0x1f)/2);
                }
                break;

			case 0xE0:
				if (addr<0xe8)
				{
					if (addr&1)
					{
						CVertexHandler::Flush();
						((u32*)&bpmem)[addr] = newval;
						static int lastRGBA[2][4] = {
							{0xEEEEEEEE,0xEEEEEEEE,0xEEEEEEEE,0xEEEEEEEE},
							{0xEEEEEEEE,0xEEEEEEEE,0xEEEEEEEE,0xEEEEEEEE}
						};
						//Terrible hack
						//The reason is that there are two sets of registers 
						//overloaded here...
						int num=(addr>>1)&0x3;
						int type = bpmem.tevregs[num].high.type;
						int colorbase = type ? PS_CONST_KCOLORS : PS_CONST_COLORS;
						int r=bpmem.tevregs[num].low.a, a=bpmem.tevregs[num].low.b;
						int b=bpmem.tevregs[num].high.a, g=bpmem.tevregs[num].high.b;
						int rgba = ((a<<24) | (r << 16) | (g << 8) | b) & 0xFCFCFCFC; //let's not detect minimal changes
						if (rgba != lastRGBA[type][num])
						{
							CVertexHandler::Flush();
							lastRGBA[type][num] = rgba;
							float temp[4] = {
								r/255.0f,g/255.0f,b/255.0f,a/255.0f
							};
							D3D::dev->SetPixelShaderConstantF(colorbase+num,temp,1);
						}
					}
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
					CVertexHandler::Flush();
					((u32*)&bpmem)[addr] = newval;
				}
				break;
			}
			break;
			
		}
		break;
	}
}

// __________________________________________________________________________________________________
// LoadBPReg
//
void LoadBPReg(u32 value0)
{
    DVSTARTPROFILE();

	//handle the mask register
	int opcode = value0 >> 24;
	int oldval = ((u32*)&bpmem)[opcode];
	int newval = (((u32*)&bpmem)[opcode] & ~bpmem.bpMask) | (value0 & bpmem.bpMask);
	int changes = (oldval ^ newval) & 0xFFFFFF;
	//reset the mask register
	if(opcode != 0xFE)
		bpmem.bpMask = 0xFFFFFF;

	switch (opcode)
	{
	case 0x45: //GXSetDrawDone
		CVertexHandler::Flush();
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
			CVertexHandler::Flush();

			((u32*)&bpmem)[opcode] = newval;
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
				DebugLog("Renderer::SwapBuffers()");
				g_VideoInitialize.pCopiedToXFB();
			}

			// clearing
			if (PE_copy.clear) // bpmem.triggerEFBCopy & EFBCOPY_CLEAR)
			{
				// it seems that the GC is able to alpha blend on color-fill
				// we cant do that so if alpha is != 255 we skip it

				// clear color
				u32 clearColor = (bpmem.clearcolorAR<<16)|bpmem.clearcolorGB;
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

	case 0x65: //GXLoadTlut
		{
			CVertexHandler::Flush();
			((u32*)&bpmem)[opcode] = newval;

			u32 tlutTMemAddr = (value0&0x3FF)<<9;
			u32 tlutXferCount = (value0&0x1FFC00)>>5; 
			//do the transfer!!			
			memcpy(texMem + tlutTMemAddr, g_VideoInitialize.pGetMemoryPointer((bpmem.tlutXferSrc&0xFFFFF)<<5), tlutXferCount);
			// TODO(ector) : kill all textures that use this palette
			// Not sure if it's a good idea, though. For now, we hash texture palettes
		}
		break;
	}

	//notify the video handling so it can update render states
	BPWritten(opcode, changes, newval);
	((u32*)&bpmem)[opcode] = newval;
}

void BPReload()
{
	for (int i=0; i<254; i++)
		BPWritten(i, 0xFFFFFF, ((u32*)&bpmem)[i]);
}


size_t BPSaveLoadState(char *ptr, BOOL save)
{
	/*
	BEGINSAVELOAD;
	SAVELOAD(&bpmem,sizeof(BPMemory));
	if (!save)
		BPReload();
	char temp[256];
	sprintf(temp,"MOJS %08x",(bpmem.clearcolorAR<<16)|(bpmem.clearcolorGB));
	g_VideoInitialize.pLog(temp, FALSE);
	ENDSAVELOAD;*/
	return 0;
}

void ActivateTextures()
{
	for (int i = 0; i < 8; i++)
	{
		//TODO(ector): this should be a speedup
		//if (textureChanged[i] || ASK TEXCACHE ABOUT SENTINEL VALUE)
		{
			FourTexUnits &tex = bpmem.tex[i>>2];
			TextureCache::Load(i,
				(tex.texImage3[i&3].image_base) << 5,
				tex.texImage0[i&3].width+1,
				tex.texImage0[i&3].height+1,
				tex.texImage0[i&3].format,
				tex.texTlut[i&3].tmem_offset<<9,
				tex.texTlut[i&3].tlut_format);	
		}
		textureChanged[i] = false;
	}
}


