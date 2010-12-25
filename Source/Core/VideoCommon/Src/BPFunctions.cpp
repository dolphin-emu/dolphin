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
	- We're using an alpha channel although the game doesn't (1)
	- If the actual EFB format is PIXELFMT_RGBA6_Z24, we are using more bits per channel than the native HW (2)
	- When doing a z copy (EFB copy target format GX_TF_Z24X8 (and possibly others?)), the native HW assumes that the EFB format is RGB8 when clearing.
		Thus the RGBA6 values get overwritten with plain RGB8 data without any kind of conversion. (3)
	- When changing EFB formats, the EFB contents will NOT get converted to the new format;
		this currently isn't implemented in any HW accelerated plugin and might cause issues. (4)
	- Possible other oddities should be noted here as well

	To properly emulate the above points, we're doing the following:
	(1)
		- disable alpha channel writing of any kind of rendering if the actual EFB format doesn't use an alpha channel
		- NOTE: Always make sure that the EFB has been cleared to an alpha value of 0xFF in this case!
		- Same for color channels, these need to be cleared to 0x00 though.
	(2)
		- just scale down the RGBA8 color to RGBA6 and upscale it to RGBA8 again
	(3)
		- more tricky, doing some bit magic here to properly reinterpret the data
	(4) TODO
		- generally delay ClearScreen calls as long as possible (until any other EFB access)
		- when the pixel format changes:
			- call ClearScreen if it's still being delayed, reinterpret the color for the new format though
			- otherwise convert EFB contents to the new pixel format
*/
void ClearScreen(const BPCmd &bp, const EFBRectangle &rc)
{
	UPE_Copy PE_copy = bpmem.triggerEFBCopy;
	bool colorEnable = bpmem.blendmode.colorupdate;
	bool alphaEnable = bpmem.blendmode.alphaupdate;
	bool zEnable = bpmem.zmode.updateenable;

	// (1): Disable unused color channels
	switch (bpmem.zcontrol.pixel_format)
	{
		case PIXELFMT_RGBA6_Z24:
			if (colorEnable && PE_copy.tp_realFormat() == GX_TF_Z24X8) // (3): alpha update forced
				alphaEnable = true;
			break;

		case PIXELFMT_RGB8_Z24:
		case PIXELFMT_RGB565_Z16:
			alphaEnable = false;
			break;

		case PIXELFMT_Z24:
			alphaEnable = colorEnable = false;
			break;

		default:
			// TODO?
			break;
	}

	if (colorEnable || alphaEnable || zEnable)
	{
		u32 color = (bpmem.clearcolorAR << 16) | bpmem.clearcolorGB;
		u32 z = bpmem.clearZValue;
		if (bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24)
		{
			// TODO: Not sure whether there's more formats to check for here - maybe GX_TF_Z8 and GX_TF_Z16?
			if (PE_copy.tp_realFormat() == GX_TF_Z24X8) // (3): Reinterpret RGB8 color as RGBA6
			{
				// NOTE: color is passed in ARGB order, but EFB uses RGBA
				u32 srcr8 = (color & 0xFF0000) >> 16;
				u32 srcg8 = (color & 0xFF00) >> 8;
				u32 srcb8 =  color & 0xFF;
				u32 dstr6 = srcr8 >> 2;
				u32 dstg6 = ((srcr8 & 0x3) << 4) | (srcg8 >> 4);
				u32 dstb6 = ((srcg8 & 0xF) << 2) | (srcb8 >> 6);
				u32 dsta6 = srcb8 & 0x3F;
				u32 dstr8 = (dstr6 << 2) | (dstr6 >> 4);
				u32 dstg8 = (dstg6 << 2) | (dstg6 >> 4);
				u32 dstb8 = (dstb6 << 2) | (dstb6 >> 4);
				u32 dsta8 = (dsta6 << 2) | (dsta6 >> 4);
				color = (dsta8 << 24) | (dstr8 << 16) | (dstg8 << 8) | dstb8;
			}
			else // (2): convert RGBA8 color to RGBA6
			{
				color &= 0xFCFCFCFC;
				color |= (color >> 6) & 0x03030303;
			}
		}
		else if (bpmem.zcontrol.pixel_format == PIXELFMT_RGB565_Z16)
		{
			z >>= 8;
			u32 dstr5 = (color & 0xFF0000) >> 19;
			u32 dstg6 = (color & 0xFF00) >> 10;
			u32 dstb5 = (color & 0xFF) >> 3;
			u32 dstr8 = (dstr5 << 3) | (dstr5 >> 2);
			u32 dstg8 = (dstg6 << 2) | (dstg6 >> 4);
			u32 dstb8 = (dstb5 << 3) | (dstb5 >> 2);
			color = (dstr8 << 16) | (dstg8 << 8) | dstb8;
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
