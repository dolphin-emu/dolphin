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
void SetGenerationMode(const Bypass &bp)
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


void SetScissor(const Bypass &bp)
{
	if (!Renderer::SetScissorRect())
		if (bp.address == BPMEM_SCISSORBR)
			ERROR_LOG(VIDEO, "bad scissor!");
}
void SetLineWidth(const Bypass &bp)
{
	float fratio = xfregs.rawViewport[0] != 0 ? ((float)Renderer::GetTargetWidth() / EFB_WIDTH) : 1.0f;
	if (bpmem.lineptwidth.linesize > 0)
		glLineWidth((float)bpmem.lineptwidth.linesize * fratio / 6.0f); // scale by ratio of widths
	if (bpmem.lineptwidth.pointsize > 0)
		glPointSize((float)bpmem.lineptwidth.pointsize * fratio / 6.0f);
}
void SetDepthMode(const Bypass &bp)
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
void SetBlendMode(const Bypass &bp)
{
	Renderer::SetBlendMode(false);
}
void SetDitherMode(const Bypass &bp)
{
    if (bpmem.blendmode.dither) 
		glEnable(GL_DITHER);
    else 
		glDisable(GL_DITHER);
}
void SetLogicOpMode(const Bypass &bp)
{
	if (bpmem.blendmode.logicopenable) 
	{
		glEnable(GL_COLOR_LOGIC_OP);
		glLogicOp(glLogicOpCodes[bpmem.blendmode.logicmode]);
	}
	else 
		glDisable(GL_COLOR_LOGIC_OP);
}
void SetColorMask(const Bypass &bp)
{
    Renderer::SetColorMask();
}
float GetRendererTargetScaleX()
{
	return Renderer::GetTargetScaleX();
}
float GetRendererTargetScaleY()
{
	return Renderer::GetTargetScaleY();
}
void CopyEFB(const Bypass &bp, const TRectangle &rc, const u32 &address, const bool &fromZBuffer, const bool &isIntensityFmt, const u32 &copyfmt, const bool &scaleByHalf)
{
	// bpmem.zcontrol.pixel_format to PIXELFMT_Z24 is when the game wants to copy from ZBuffer (Zbuffer uses 24-bit Format)
	if (!g_Config.bEFBCopyDisable)
		if (g_Config.bCopyEFBToRAM) // To RAM
			TextureConverter::EncodeToRam(address, fromZBuffer, isIntensityFmt, copyfmt, scaleByHalf, rc);
		else // To OGL Texture
			TextureMngr::CopyRenderTargetToTexture(address, fromZBuffer, isIntensityFmt, copyfmt, scaleByHalf, rc);
}

void RenderToXFB(const Bypass &bp, const TRectangle &multirc, const float &yScale, const float &xfbLines, u8* pXFB, const u32 &dstWidth, const u32 &dstHeight)
{
	Renderer::RenderToXFB(pXFB, multirc, dstWidth, dstHeight);
}
void ClearScreen(const Bypass &bp, const TRectangle &multirc)
{
	// Update the view port for clearing the picture
	glViewport(0, 0, Renderer::GetTargetWidth(), Renderer::GetTargetHeight());

    // Always set the scissor in case it was set by the game and has not been reset
	glScissor(multirc.left, (Renderer::GetTargetHeight() - multirc.bottom), 
		(multirc.right - multirc.left), (multirc.bottom - multirc.top));
	// ---------------------------

    VertexShaderManager::SetViewportChanged();

    // Since clear operations use the source rectangle, we have to do
	// regular renders (glClear clears the entire buffer)
    if (bpmem.blendmode.colorupdate || bpmem.blendmode.alphaupdate || bpmem.zmode.updateenable)
	{                    
        GLbitfield bits = 0;
        if (bpmem.blendmode.colorupdate || bpmem.blendmode.alphaupdate)
		{
            u32 clearColor = (bpmem.clearcolorAR << 16) | bpmem.clearcolorGB;
			glClearColor(((clearColor>>16) & 0xff)*(1/255.0f),
						 ((clearColor>>8 ) & 0xff)*(1/255.0f),
						 ((clearColor>>0 ) & 0xff)*(1/255.0f),
						 ((clearColor>>24) & 0xff)*(1/255.0f));
            bits |= GL_COLOR_BUFFER_BIT;
        }
        if (bpmem.zmode.updateenable)
		{
            glClearDepth((float)(bpmem.clearZValue & 0xFFFFFF) / float(0xFFFFFF));
            bits |= GL_DEPTH_BUFFER_BIT;
        }
        glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
        glClear(bits);
    }
}

void RestoreRenderState(const Bypass &bp)
{
	Renderer::RestoreGLState();
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
void SetSamplerState(const Bypass &bp)
{
	// TODO
}
void SetInterlacingMode(const Bypass &bp)
{
	// TODO
}
};
