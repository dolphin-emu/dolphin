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
#include "Common.h"
#include "RenderBase.h"
#include "TextureCacheBase.h"
#include "VertexManagerBase.h"
#include "VertexShaderManager.h"
#include "VideoConfig.h"

bool textureChanged[8];
const bool renderFog = false;
namespace BPFunctions
{
// ----------------------------------------------
// State translation lookup tables
// Reference: Yet Another Gamecube Documentation
// ----------------------------------------------


void FlushPipeline()
{
	VertexManager::Flush();
}

void SetGenerationMode(const BPCmd &bp)
{
	g_renderer->SetGenerationMode();
}

void SetScissor(const BPCmd &bp)
{
	g_renderer->SetScissorRect();
}

void SetLineWidth(const BPCmd &bp)
{
	g_renderer->SetLineWidth();
}

void SetDepthMode(const BPCmd &bp)
{
	g_renderer->SetDepthMode();
}

void SetBlendMode(const BPCmd &bp)
{
	g_renderer->SetBlendMode(false);
}
void SetDitherMode(const BPCmd &bp)
{
	g_renderer->SetDitherMode();
}
void SetLogicOpMode(const BPCmd &bp)
{
	g_renderer->SetLogicOpMode();
}

void SetColorMask(const BPCmd &bp)
{
	g_renderer->SetColorMask();
}

void CopyEFB(const BPCmd &bp, const EFBRectangle &rc, const u32 &address, const bool &fromZBuffer, const bool &isIntensityFmt, const u32 &copyfmt, const int &scaleByHalf)
{
	// bpmem.zcontrol.pixel_format to PIXELFMT_Z24 is when the game wants to copy from ZBuffer (Zbuffer uses 24-bit Format)
	if (g_ActiveConfig.bEFBCopyEnable)
	{
		TextureCache::CopyRenderTargetToTexture(address, fromZBuffer, isIntensityFmt, copyfmt, !!scaleByHalf, rc);
	}
}

/* Explanation of the magic behind ClearScreen:
	There's numerous possible formats for the pixel data in the EFB.
	However, in the HW accelerated plugins we're always using RGBA8
	for the EFB format, which causes some problems:
	- We're using an alpha channel although the game doesn't
	- If the actual EFB format is RGBA6_Z24 or R5G6B5_Z16, we are using more bits per channel than the native HW

	To properly emulate the above points, we're doing the following:
	(1)
		- disable alpha channel writing of any kind of rendering if the actual EFB format doesn't use an alpha channel
		- NOTE: Always make sure that the EFB has been cleared to an alpha value of 0xFF in this case!
		- Same for color channels, these need to be cleared to 0x00 though.
	(2)
		- convert the RGBA8 color to RGBA6/RGB8/RGB565 and convert it to RGBA8 again
		- convert the Z24 depth value to Z16 and back to Z24
*/
void ClearScreen(const BPCmd &bp, const EFBRectangle &rc)
{
	bool colorEnable = bpmem.blendmode.colorupdate;
	bool alphaEnable = bpmem.blendmode.alphaupdate;
	bool zEnable = bpmem.zmode.updateenable;

	// (1): Disable unused color channels
	if (bpmem.zcontrol.pixel_format == PIXELFMT_RGB8_Z24 ||
		bpmem.zcontrol.pixel_format == PIXELFMT_RGB565_Z16 ||
		bpmem.zcontrol.pixel_format == PIXELFMT_Z24)
	{
		alphaEnable = false;
	}

	if (colorEnable || alphaEnable || zEnable)
	{
		u32 color = (bpmem.clearcolorAR << 16) | bpmem.clearcolorGB;
		u32 z = bpmem.clearZValue;

		// (2) drop additional accuracy
		if (bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24)
		{
			color = RGBA8ToRGBA6ToRGBA8(color);
		}
		else if (bpmem.zcontrol.pixel_format == PIXELFMT_RGB565_Z16)
		{
			color = RGBA8ToRGB565ToRGBA8(color);
			z = Z24ToZ16ToZ24(z);
		}
		g_renderer->ClearScreen(rc, colorEnable, alphaEnable, zEnable, color, z);
	}
}


void RestoreRenderState(const BPCmd &bp)
{
	g_renderer->RestoreAPIState();
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

void SetTextureMode(const BPCmd &bp)
{
	g_renderer->SetSamplerState(bp.address & 3, (bp.address & 0xE0) == 0xA0);
}

void SetInterlacingMode(const BPCmd &bp)
{
	// TODO
}

};
