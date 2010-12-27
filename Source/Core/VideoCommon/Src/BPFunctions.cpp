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
int prev_pix_format = -1;
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
	- We're using an alpha channel although the game doesn't (1)
	- If the actual EFB format is PIXELFMT_RGBA6_Z24, we are using more bits per channel than the native HW (2)
	- When doing a z copy EFB copy target format GX_TF_Z24X8 , the native HW don't worry about hthe color efb format because it only uses the depth part
	- When changing EFB formats the efb must be cleared (3) 
	- Possible other oddities should be noted here as well

	To properly emulate the above points, we're doing the following:
	(1)
		- disable alpha channel writing of any kind of rendering if the actual EFB format doesn't use an alpha channel
		- NOTE: Always make sure that the EFB has been cleared to an alpha value of 0xFF in this case!
		- in a dition to the previus make sure we always return 0xFF in alpha when reading the efb content if the efb format has no alpha
		- Same for color channels, these need to be cleared to 0x00 though.
	(2)
		- just scale down the RGBA8 color to RGBA6 and upscale it to RGBA8 again
	(3) - when the pixel format changes:
		- call ClearScreen using the correct alpha format for the previus pixel format
		
			
*/
void ClearScreen(const BPCmd &bp, const EFBRectangle &rc)
{
	UPE_Copy PE_copy = bpmem.triggerEFBCopy;
	bool colorEnable = bpmem.blendmode.colorupdate;
	bool alphaEnable = bpmem.blendmode.alphaupdate;
	bool zEnable = bpmem.zmode.updateenable;
	u32 color = (bpmem.clearcolorAR << 16) | bpmem.clearcolorGB;
	u32 z = bpmem.clearZValue;
	if(prev_pix_format == -1)
	{
		prev_pix_format = bpmem.zcontrol.pixel_format;
	}
	// (1): Disable unused color channels
	switch (bpmem.zcontrol.pixel_format)
	{
		case PIXELFMT_RGB8_Z24:
		case PIXELFMT_RGB565_Z16:
			alphaEnable = true;
			color |= (prev_pix_format == PIXELFMT_RGBA6_Z24)? 0x0 : 0xFF000000;//(3)
			break;

		case PIXELFMT_Z24:
			alphaEnable = colorEnable = false;
			break;
		default:			
			break;
	}

	if (colorEnable || alphaEnable || zEnable)
	{
		if (bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24)
		{
			color = RGBA8ToRGBA6ToRGBA8(color);
		}
		else if (bpmem.zcontrol.pixel_format == PIXELFMT_RGB565_Z16)
		{
			color = RGBA8ToRGB565ToRGB8(color);
		}
		g_renderer->ClearScreen(rc, colorEnable, alphaEnable, zEnable, color, z);
	}
	prev_pix_format = bpmem.zcontrol.pixel_format;
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
