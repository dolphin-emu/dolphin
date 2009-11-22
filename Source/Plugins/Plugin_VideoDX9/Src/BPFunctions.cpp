// Copyright (C) 2003 Dolphin Project.

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

#include "BPFunctions.h"
#include "D3DBase.h"
#include "VideoConfig.h"
#include "Common.h"
#include "TextureCache.h"
#include "VertexManager.h"
#include "VertexShaderManager.h"
#include "Utils.h"
#include "debugger/debugger.h"
#include "TextureConverter.h"


bool textureChanged[8];

const bool renderFog = false;

using namespace D3D;

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
	D3DTEXF_LINEAR,
	D3DTEXF_LINEAR, //reserved
};

static const D3DTEXTUREADDRESS d3dClamps[4] =
{
	D3DTADDRESS_CLAMP,
	D3DTADDRESS_WRAP,
	D3DTADDRESS_MIRROR,
	D3DTADDRESS_WRAP //reserved
};

namespace BPFunctions
{

void FlushPipeline()
{
	VertexManager::Flush();
}

void SetGenerationMode(const BPCmd &bp)
{
	D3D::SetRenderState(D3DRS_CULLMODE, d3dCullModes[bpmem.genMode.cullmode]);

	if (bpmem.genMode.cullmode == 3)
	{
		D3D::SetRenderState(D3DRS_COLORWRITEENABLE, 0);
	}
	else
	{
		DWORD write = 0;
		if (bpmem.blendmode.alphaupdate) 
			write = D3DCOLORWRITEENABLE_ALPHA;
		if (bpmem.blendmode.colorupdate) 
			write |= D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE;
		
		D3D::SetRenderState(D3DRS_COLORWRITEENABLE, write);
	}
}

void SetScissor(const BPCmd &bp)
{
	Renderer::SetScissorRect();
}

void SetLineWidth(const BPCmd &bp)
{
	// We can't change line width in D3D unless we use ID3DXLine
	float psize = float(bpmem.lineptwidth.pointsize) * 6.0f;
	D3D::SetRenderState(D3DRS_POINTSIZE, *((DWORD*)&psize));
}

void SetDepthMode(const BPCmd &bp)
{
	if (bpmem.zmode.testenable)
	{
		D3D::SetRenderState(D3DRS_ZENABLE, TRUE);
		D3D::SetRenderState(D3DRS_ZWRITEENABLE, bpmem.zmode.updateenable);
		D3D::SetRenderState(D3DRS_ZFUNC, d3dCmpFuncs[bpmem.zmode.func]);		
	}
	else
	{
		D3D::SetRenderState(D3DRS_ZENABLE, FALSE);
		D3D::SetRenderState(D3DRS_ZWRITEENABLE, FALSE);  // ??		
	}			
}

void SetBlendMode(const BPCmd &bp)
{
	Renderer::SetBlendMode(false);
}
void SetDitherMode(const BPCmd &bp)
{
	D3D::SetRenderState(D3DRS_DITHERENABLE,bpmem.blendmode.dither);
}
void SetLogicOpMode(const BPCmd &bp)
{
	// Logic op blending. D3D can't do this but can fake some modes.
}

void SetColorMask(const BPCmd &bp)
{
	Renderer::SetColorMask();
}

void CopyEFB(const BPCmd &bp, const EFBRectangle &rc, const u32 &address, const bool &fromZBuffer, const bool &isIntensityFmt, const u32 &copyfmt, const int &scaleByHalf)
{
	if (!g_ActiveConfig.bEFBCopyDisable)
	{
		//uncomment this to see the efb to ram work in progress
		/*if (g_ActiveConfig.bCopyEFBToRAM)
		{
			// To RAM, not implemented yet
			TextureConverter::EncodeToRam(address, fromZBuffer, isIntensityFmt, copyfmt, scaleByHalf, rc);
		}
		else // To D3D Texture*/
		{
			TextureCache::CopyRenderTargetToTexture(address, fromZBuffer, isIntensityFmt, copyfmt, scaleByHalf, rc);			
		}

	}
}

void ClearScreen(const BPCmd &bp, const EFBRectangle &rc)
{
	// it seems that the GC is able to alpha blend on color-fill
	// we cant do that so if alpha is != 255 we skip it

	bool colorEnable = bpmem.blendmode.colorupdate;
	bool alphaEnable = (bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24 && bpmem.blendmode.alphaupdate);
	bool zEnable = bpmem.zmode.updateenable;

	if (colorEnable || alphaEnable || zEnable)
	{
		u32 color = (bpmem.clearcolorAR << 16) | bpmem.clearcolorGB;
		u32 z = bpmem.clearZValue;

		Renderer::ClearScreen(rc, colorEnable, alphaEnable, zEnable, color, z);
	}
}

void RestoreRenderState(const BPCmd &bp)
{
	//Renderer::SetRenderMode(Renderer::RM_Normal);
}

bool GetConfig(const int &type)
{
	switch (type)
	{
	case CONFIG_ISWII:
		return g_VideoInitialize.bWii;
	case CONFIG_DISABLEFOG:
		return false;
	case CONFIG_SHOWEFBREGIONS:
		return false;
	default:
		PanicAlert("GetConfig Error: Unknown Config Type!");
		return false;
	}
}

u8 *GetPointer(const u32 &address)
{
	return g_VideoInitialize.pGetMemoryPointer(address);
}

void SetSamplerState(const BPCmd &bp)
{
	const FourTexUnits &tex = bpmem.tex[(bp.address & 0xE0) == 0xA0];
	int stage = (bp.address & 3);//(addr>>4)&2;
	const TexMode0 &tm0 = tex.texMode0[stage];
	
	D3DTEXTUREFILTERTYPE min, mag, mip;
	if (g_ActiveConfig.bForceFiltering)
	{
		min = mag = mip = D3DTEXF_LINEAR;
	}
	else
	{
		min = (tm0.min_filter & 4) ? D3DTEXF_LINEAR : D3DTEXF_POINT;
		mag = tm0.mag_filter ? D3DTEXF_LINEAR : D3DTEXF_POINT;
		mip = d3dMipFilters[tm0.min_filter & 3];
	}
	if ((bp.address & 0xE0) == 0xA0)
		stage += 4;	
	
	if (mag == D3DTEXF_LINEAR && min == D3DTEXF_LINEAR &&
		g_ActiveConfig.iMaxAnisotropy > 1)
	{
		min = D3DTEXF_ANISOTROPIC;
	}
	D3D::SetSamplerState(stage, D3DSAMP_MINFILTER, min);
	D3D::SetSamplerState(stage, D3DSAMP_MAGFILTER, mag);
	D3D::SetSamplerState(stage, D3DSAMP_MIPFILTER, mip);
	
	D3D::SetSamplerState(stage, D3DSAMP_ADDRESSU, d3dClamps[tm0.wrap_s]);
	D3D::SetSamplerState(stage, D3DSAMP_ADDRESSV, d3dClamps[tm0.wrap_t]);
	//wip
	//dev->SetSamplerState(stage,D3DSAMP_MIPMAPLODBIAS,tm0.lod_bias/4.0f);
	//char temp[256];
	//sprintf(temp,"lod %f",tm0.lod_bias/4.0f);
	//g_VideoInitialize.pLog(temp);
}

void SetInterlacingMode(const BPCmd &bp)
{
	// TODO
}

};