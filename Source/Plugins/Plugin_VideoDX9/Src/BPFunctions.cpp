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

#include "BPFunctions.h"

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

namespace BPFunctions
{

void FlushPipeline()
{
	VertexManager::Flush();
}

void SetGenerationMode()
{
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

void SetScissor(const BreakPoint &bp)
{
	
}
void SetLineWidth()
{
	// We can't change line width in D3D unless we use ID3DXLine
	float psize = float(bpmem.lineptwidth.pointsize) * 6.0f;
	Renderer::SetRenderState(D3DRS_POINTSIZE, *((DWORD*)&psize));
}
void SetDepthMode()
{
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
void SetBlendMode()
{
	
}
void SetDitherMode()
{
    
}
void SetLogicOpMode()
{
	
}
void SetColorMask()
{
    
}
float GetRendererTargetScaleX()
{
	Renderer::GetXScale();
}
float GetRendererTargetScaleY()
{
	Renderer::GetYScale();
}
void CopyEFB(const TRectangle &rc, const u32 &address, const bool &fromZBuffer, const bool &isIntensityFmt, const u32 &copyfmt, const bool &scaleByHalf)
{
	TextureCache::CopyEFBToRenderTarget(bpmem.copyTexDest<<5, &rc);
}

void RenderToXFB(const TRectangle &multirc, const float &yScale, const float &xfbLines, u8* pXFB, const u32 &dstWidth, const u32 &dstHeight)
{
    Renderer::SwapBuffers();
	PRIM_LOG("Renderer::SwapBuffers()");
	g_VideoInitialize.pCopiedToXFB();
}
void ClearScreen(const TRectangle &multirc)
{
	// it seems that the GC is able to alpha blend on color-fill
	// we cant do that so if alpha is != 255 we skip it

	VertexShaderManager::SetViewportChanged();

	// Since clear operations use the source rectangle, we have to do
	// regular renders
	DWORD clearflags = 0;
	D3DCOLOR col = 0;
	float clearZ = 0;
	if (bpmem.blendmode.colorupdate || bpmem.blendmode.alphaupdate)
	{                    
		if (bpmem.blendmode.colorupdate || bpmem.blendmode.alphaupdate)
			col = (bpmem.clearcolorAR << 16) | bpmem.clearcolorGB;
		// clearflags |= D3DCLEAR_TARGET;   set to break animal crossing :p
	}

	// clear z-buffer
	if (bpmem.zmode.updateenable)
	{
		clearZ = (float)(bpmem.clearZValue & 0xFFFFFF) / float(0xFFFFFF);
		if (clearZ > 1.0f) clearZ = 1.0f;
		if (clearZ < 0.0f) clearZ = 0.0f;
		clearflags |= D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL;
	}				
	D3D::dev->Clear(0, NULL, clearflags, col, clearZ, 0);
}

void RestoreRenderState()
{
}

void SetScissorRectangle()
{
	Renderer::SetScissorRect();
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
	g_VideoInitialize.pGetMemoryPointer(address);
}
};