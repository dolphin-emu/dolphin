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

namespace BPFunctions
{

void FlushPipeline()
{
	VertexManager::Flush();
}

void SetGenerationMode(const BPCmd &bp)
{
	Renderer::SetGenerationMode();
}

void SetScissor(const BPCmd &bp)
{
	Renderer::SetScissorRect();
}

void SetLineWidth(const BPCmd &bp)
{
	Renderer::SetLineWidth();
}

void SetDepthMode(const BPCmd &bp)
{
	Renderer::SetDepthMode();			
}

void SetBlendMode(const BPCmd &bp)
{
	Renderer::SetBlendMode(false);
}
void SetDitherMode(const BPCmd &bp)
{
	Renderer::SetDitherMode();
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
		if (g_ActiveConfig.bCopyEFBToRAM)
		{
			//ToRam
			TextureConverter::EncodeToRam(address, fromZBuffer, isIntensityFmt, copyfmt, scaleByHalf, rc);
		}
		else // To D3D Texture
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
	Renderer::RestoreAPIState();
}

bool GetConfig(const int &type)
{
	switch (type)
	{
	case CONFIG_ISWII:
		return g_VideoInitialize.bWii;
	case CONFIG_DISABLEFOG:
		return g_ActiveConfig.bDisableFog;
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
	int stage = (bp.address & 3);//(addr>>4)&2;
	Renderer::SetSamplerState(stage,(bp.address & 0xE0) == 0xA0);
}

void SetInterlacingMode(const BPCmd &bp)
{
	// TODO
}

};