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
#include "Globals.h"
#include "Profiler.h"
#include "Config.h"
#include "VertexManager.h"
#include "Render.h"
#include "TextureMngr.h"
#include "TextureConverter.h"
#include "VertexShaderManager.h"
#include "XFB.h"
#include "main.h"

namespace BPFunctions
{
// ----------------------------------------------
// State translation lookup tables
// Reference: Yet Another Gamecube Documentation
// ----------------------------------------------

static const GLenum glCmpFuncs[8] = {
	GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS
};

static const GLenum glLogicOpCodes[16] = {
    GL_CLEAR, GL_AND, GL_AND_REVERSE, GL_COPY, GL_AND_INVERTED, GL_NOOP, GL_XOR, 
	GL_OR, GL_NOR, GL_EQUIV, GL_INVERT, GL_OR_REVERSE, GL_COPY_INVERTED, GL_OR_INVERTED, GL_NAND, GL_SET
};

void FlushPipeline()
{
	VertexManager::Flush();
}
void SetGenerationMode(const BPCmd &bp)
{
    // none, ccw, cw, ccw
    if (bpmem.genMode.cullmode > 0) 
	{
        glEnable(GL_CULL_FACE);
        glFrontFace(bpmem.genMode.cullmode == 2 ? GL_CCW : GL_CW);
    }
    else
		glDisable(GL_CULL_FACE);
}


void SetScissor(const BPCmd &bp)
{
	if (!Renderer::SetScissorRect())
		if (bp.address == BPMEM_SCISSORBR)
			ERROR_LOG(VIDEO, "bad scissor!");
}
void SetLineWidth(const BPCmd &bp)
{
	float fratio = xfregs.rawViewport[0] != 0 ? ((float)Renderer::GetTargetWidth() / EFB_WIDTH) : 1.0f;
	if (bpmem.lineptwidth.linesize > 0)
		glLineWidth((float)bpmem.lineptwidth.linesize * fratio / 6.0f); // scale by ratio of widths
	if (bpmem.lineptwidth.pointsize > 0)
		glPointSize((float)bpmem.lineptwidth.pointsize * fratio / 6.0f);
}
void SetDepthMode(const BPCmd &bp)
{
	if (bpmem.zmode.testenable) 
	{
		glEnable(GL_DEPTH_TEST);
		glDepthMask(bpmem.zmode.updateenable ? GL_TRUE : GL_FALSE);
		glDepthFunc(glCmpFuncs[bpmem.zmode.func]);
	}
	else 
	{
		// if the test is disabled write is disabled too
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
	}
}
void SetBlendMode(const BPCmd &bp)
{
	Renderer::SetBlendMode(false);
}
void SetDitherMode(const BPCmd &bp)
{
    if (bpmem.blendmode.dither) 
		glEnable(GL_DITHER);
    else 
		glDisable(GL_DITHER);
}
void SetLogicOpMode(const BPCmd &bp)
{
	if (bpmem.blendmode.logicopenable) 
	{
		glEnable(GL_COLOR_LOGIC_OP);
		glLogicOp(glLogicOpCodes[bpmem.blendmode.logicmode]);
	}
	else 
		glDisable(GL_COLOR_LOGIC_OP);
}

void SetColorMask(const BPCmd &bp)
{
    Renderer::SetColorMask();
}

void CopyEFB(const BPCmd &bp, const EFBRectangle &rc, const u32 &address, const bool &fromZBuffer, const bool &isIntensityFmt, const u32 &copyfmt, const int &scaleByHalf)
{
	// bpmem.zcontrol.pixel_format to PIXELFMT_Z24 is when the game wants to copy from ZBuffer (Zbuffer uses 24-bit Format)
	if (!g_Config.bEFBCopyDisable)
	{
		if (g_Config.bCopyEFBToRAM) // To RAM
			TextureConverter::EncodeToRam(address, fromZBuffer, isIntensityFmt, copyfmt, scaleByHalf, rc);
		else // To OGL Texture
			TextureMngr::CopyRenderTargetToTexture(address, fromZBuffer, isIntensityFmt, copyfmt, scaleByHalf, rc);
	}
}

void RenderToXFB(const BPCmd &bp, const EFBRectangle &rc, const float &yScale, const float &xfbLines, u32 xfbAddr, const u32 &dstWidth, const u32 &dstHeight)
{
	Renderer::RenderToXFB(xfbAddr, dstWidth, dstHeight, rc);
}

void ClearScreen(const BPCmd &bp, const EFBRectangle &rc)
{
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
		return g_Config.bDisableFog;
	case CONFIG_SHOWEFBREGIONS:
		return g_Config.bShowEFBCopyRegions;
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
	// TODO
}

void SetInterlacingMode(const BPCmd &bp)
{
	// TODO
}

};
