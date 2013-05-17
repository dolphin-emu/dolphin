// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "BPFunctions.h"
#include "Common.h"
#include "RenderBase.h"
#include "TextureCacheBase.h"
#include "VertexManagerBase.h"
#include "VertexShaderManager.h"
#include "VideoConfig.h"
#include "HW/Memmap.h"
#include "ConfigManager.h"

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

void SetGenerationMode()
{
	g_renderer->SetGenerationMode();
}

void SetScissor()
{
	const int xoff = bpmem.scissorOffset.x * 2 - 342;
	const int yoff = bpmem.scissorOffset.y * 2 - 342;

	EFBRectangle rc (bpmem.scissorTL.x - xoff - 342, bpmem.scissorTL.y - yoff - 342,
					bpmem.scissorBR.x - xoff - 341, bpmem.scissorBR.y - yoff - 341);

	if (rc.left < 0) rc.left = 0;
	if (rc.top < 0) rc.top = 0;
	if (rc.right > EFB_WIDTH) rc.right = EFB_WIDTH;
	if (rc.bottom > EFB_HEIGHT) rc.bottom = EFB_HEIGHT;

	if (rc.left > rc.right) rc.right = rc.left;
	if (rc.top > rc.bottom) rc.bottom = rc.top;

	TargetRectangle trc = g_renderer->ConvertEFBRectangle(rc);
	g_renderer->SetScissorRect(trc);

	UpdateViewportWithCorrection();
}

void SetLineWidth()
{
	g_renderer->SetLineWidth();
}

void SetDepthMode()
{
	g_renderer->SetDepthMode();
}

void SetBlendMode()
{
	g_renderer->SetBlendMode(false);
}
void SetDitherMode()
{
	g_renderer->SetDitherMode();
}
void SetLogicOpMode()
{
	g_renderer->SetLogicOpMode();
}

void SetColorMask()
{
	g_renderer->SetColorMask();
}

void CopyEFB(u32 dstAddr, unsigned int dstFormat, unsigned int srcFormat,
	const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf)
{
	// bpmem.zcontrol.pixel_format to PIXELFMT_Z24 is when the game wants to copy from ZBuffer (Zbuffer uses 24-bit Format)
	if (g_ActiveConfig.bEFBCopyEnable)
	{
		TextureCache::CopyRenderTargetToTexture(dstAddr, dstFormat, srcFormat,
			srcRect, isIntensity, scaleByHalf);
	}
}

/* Explanation of the magic behind ClearScreen:
	There's numerous possible formats for the pixel data in the EFB.
	However, in the HW accelerated backends we're always using RGBA8
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
void ClearScreen(const EFBRectangle &rc)
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

void OnPixelFormatChange()
{
	int convtype = -1;

	// TODO : Check for Z compression format change
	// When using 16bit Z, the game may enable a special compression format which we need to handle
	// If we don't, Z values will be completely screwed up, currently only Star Wars:RS2 uses that.

	/*
	 * When changing the EFB format, the pixel data won't get converted to the new format but stays the same.
	 * Since we are always using an RGBA8 buffer though, this causes issues in some games.
	 * Thus, we reinterpret the old EFB data with the new format here.
	 */
	if (!g_ActiveConfig.bEFBEmulateFormatChanges ||
		!g_ActiveConfig.backend_info.bSupportsFormatReinterpretation)
		return;

	u32 old_format = Renderer::GetPrevPixelFormat();
	u32 new_format = bpmem.zcontrol.pixel_format;

	// no need to reinterpret pixel data in these cases
	if (new_format == old_format || old_format == (unsigned int)-1)
		goto skip;

	// Check for pixel format changes
	switch (old_format)
	{
		case PIXELFMT_RGB8_Z24:
		case PIXELFMT_Z24:
			// Z24 and RGB8_Z24 are treated equal, so just return in this case
			if (new_format == PIXELFMT_RGB8_Z24 || new_format == PIXELFMT_Z24)
				goto skip;

			if (new_format == PIXELFMT_RGBA6_Z24)
				convtype = 0;
			else if (new_format == PIXELFMT_RGB565_Z16)
				convtype = 1;
			break;

		case PIXELFMT_RGBA6_Z24:
			if (new_format == PIXELFMT_RGB8_Z24 ||
				new_format == PIXELFMT_Z24)
				convtype = 2;
			else if (new_format == PIXELFMT_RGB565_Z16)
				convtype = 3;
			break;

		case PIXELFMT_RGB565_Z16:
			if (new_format == PIXELFMT_RGB8_Z24 ||
				new_format == PIXELFMT_Z24)
				convtype = 4;
			else if (new_format == PIXELFMT_RGBA6_Z24)
				convtype = 5;
			break;

		default:
			break;
	}

	if (convtype == -1)
	{
		ERROR_LOG(VIDEO, "Unhandled EFB format change: %d to %d\n", old_format, new_format);
		goto skip;
	}

	g_renderer->ReinterpretPixelData(convtype);

skip:
	DEBUG_LOG(VIDEO, "pixelfmt: pixel=%d, zc=%d", new_format, bpmem.zcontrol.zformat);

	Renderer::StorePixelFormat(new_format);
}

bool GetConfig(const int &type)
{
	switch (type)
	{
	case CONFIG_ISWII:
		return SConfig::GetInstance().m_LocalCoreStartupParameter.bWii;
	case CONFIG_DISABLEFOG:
		return g_ActiveConfig.bDisableFog;
	case CONFIG_SHOWEFBREGIONS:
		return g_ActiveConfig.bShowEFBCopyRegions;
	default:
		PanicAlert("GetConfig Error: Unknown Config Type!");
		return false;
	}
}

u8 *GetPointer(const u32 &address)
{
	return Memory::GetPointer(address);
}

// Never used. All backends call SetSamplerState in VertexManager::Flush
void SetTextureMode(const BPCmd &bp)
{
	g_renderer->SetSamplerState(bp.address & 3, (bp.address & 0xE0) == 0xA0);
}

void SetInterlacingMode(const BPCmd &bp)
{
	// TODO
	switch (bp.address)
	{
	case BPMEM_FIELDMODE:
		{
			// SDK always sets bpmem.lineptwidth.lineaspect via BPMEM_LINEPTWIDTH
			// just before this cmd
			const char *action[] = { "don't adjust", "adjust" };
			DEBUG_LOG(VIDEO, "BPMEM_FIELDMODE texLOD:%s lineaspect:%s",
				action[bpmem.fieldmode.texLOD],
				action[bpmem.lineptwidth.lineaspect]);
		}
		break;
	case BPMEM_FIELDMASK:
		{
			// Determines if fields will be written to EFB (always computed)
			const char *action[] = { "skip", "write" };
			DEBUG_LOG(VIDEO, "BPMEM_FIELDMASK even:%s odd:%s",
				action[bpmem.fieldmask.even], action[bpmem.fieldmask.odd]);
		}
		break;
	default:
		ERROR_LOG(VIDEO, "SetInterlacingMode default");
		break;
	}
}

};
