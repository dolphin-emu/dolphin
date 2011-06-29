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

#include "VirtualEFBCopy.h"

#include "EFBCopy.h"
#include "FramebufferManager.h"
#include "PixelShaderCache.h"
#include "PixelShaderManager.h"
#include "Render.h"
#include "TextureCache.h"
#include "VertexShaderManager.h"

namespace OGL
{

VirtualEFBCopy::~VirtualEFBCopy()
{
	glDeleteTextures(1, &m_texture.tex);
	m_texture.tex = 0;
}
	
// TODO: These matrices should be moved to a backend-independent place.
// They are used for any virtual EFB copy system that encodes to an RGBA
// texture.
static const float RGBA_MATRIX[4*4] = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1
};
static const float YUVA_MATRIX[4*4] = {
	 0.257f,  0.504f,  0.098f, 0.f,
	-0.148f, -0.291f,  0.439f, 0.f,
	 0.439f, -0.368f, -0.071f, 0.f,
	    0.f,     0.f,     0.f, 1.f
};

static const float RGB0_MATRIX[4*4] = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 0
};
static const float YUV0_MATRIX[4*4] = {
	 0.257f,  0.504f,  0.098f, 0.f,
	-0.148f, -0.291f,  0.439f, 0.f,
	 0.439f, -0.368f, -0.071f, 0.f,
	    0.f,     0.f,     0.f, 0.f
};

static const float ZERO_MATRIX[4*4] = {
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0
};

// FIXME: Should this be AAAR?
static const float RRRA_MATRIX[4*4] = {
	1, 0, 0, 0,
	1, 0, 0, 0,
	1, 0, 0, 0,
	0, 0, 0, 1
};
static const float YYYA_MATRIX[4*4] = {
	0.257f, 0.504f, 0.098f, 0.f,
	0.257f, 0.504f, 0.098f, 0.f,
	0.257f, 0.504f, 0.098f, 0.f,
	   0.f,    0.f,    0.f, 1.f
};

static const float RRR0_MATRIX[4*4] = {
	1, 0, 0, 0,
	1, 0, 0, 0,
	1, 0, 0, 0,
	0, 0, 0, 0
};
static const float YYY0_MATRIX[4*4] = {
	0.257f, 0.504f, 0.098f, 0.f,
	0.257f, 0.504f, 0.098f, 0.f,
	0.257f, 0.504f, 0.098f, 0.f,
	   0.f,    0.f,    0.f, 0.f
};

static const float AAAA_MATRIX[4*4] = {
	0, 0, 0, 1,
	0, 0, 0, 1,
	0, 0, 0, 1,
	0, 0, 0, 1
};

static const float RRRR_MATRIX[4*4] = {
	1, 0, 0, 0,
	1, 0, 0, 0,
	1, 0, 0, 0,
	1, 0, 0, 0
};
static const float YYYY_MATRIX[4*4] = {
	0.257f, 0.504f, 0.098f, 0.f,
	0.257f, 0.504f, 0.098f, 0.f,
	0.257f, 0.504f, 0.098f, 0.f,
	0.257f, 0.504f, 0.098f, 0.f
};

static const float GGGG_MATRIX[4*4] = {
	0, 1, 0, 0,
	0, 1, 0, 0,
	0, 1, 0, 0,
	0, 1, 0, 0
};
static const float UUUU_MATRIX[4*4] = {
	-0.148f, -0.291f, 0.439f, 0.f,
	-0.148f, -0.291f, 0.439f, 0.f,
	-0.148f, -0.291f, 0.439f, 0.f,
	-0.148f, -0.291f, 0.439f, 0.f
};

static const float BBBB_MATRIX[4*4] = {
	0, 0, 1, 0,
	0, 0, 1, 0,
	0, 0, 1, 0,
	0, 0, 1, 0
};
static const float VVVV_MATRIX[4*4] = {
	0.439f, -0.368f, -0.071f, 0.f,
	0.439f, -0.368f, -0.071f, 0.f,
	0.439f, -0.368f, -0.071f, 0.f,
	0.439f, -0.368f, -0.071f, 0.f
};

// FIXME: Should this be GGGR?
static const float RRRG_MATRIX[4*4] = {
	1, 0, 0, 0,
	1, 0, 0, 0,
	1, 0, 0, 0,
	0, 1, 0, 0
};
static const float YYYU_MATRIX[4*4] = {
	 0.257f,  0.504f, 0.098f, 0.f,
	 0.257f,  0.504f, 0.098f, 0.f,
	 0.257f,  0.504f, 0.098f, 0.f,
	-0.148f, -0.291f, 0.439f, 0.f
};

// FIXME: Should this be BBBG?
static const float GGGB_MATRIX[4*4] = {
	0, 1, 0, 0,
	0, 1, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0
};
static const float UUUV_MATRIX[4*4] = {
	-0.148f, -0.291f,  0.439f, 0.f,
	-0.148f, -0.291f,  0.439f, 0.f,
	-0.148f, -0.291f,  0.439f, 0.f,
	 0.439f, -0.368f, -0.071f, 0.f
};

static const float ZERO_ADD[4] = { 0, 0, 0, 0 };
static const float A1_ADD[4] = { 0, 0, 0, 1 };
static const float ALL_ONE_ADD[4] = { 1, 1, 1, 1 };
static const float YUV0_ADD[4] = { 16.f/255.f, 128.f/255.f, 128.f/255.f, 0.f };
static const float YUV1_ADD[4] = { 16.f/255.f, 128.f/255.f, 128.f/255.f, 1.f };

static const float YYY1_ADD[4] = { 16.f/255.f, 16.f/255.f, 16.f/255.f, 1.f };
static const float YYYU_ADD[4] = { 16.f/255.f, 16.f/255.f, 16.f/255.f, 128.f/255.f };
static const float UUUV_ADD[4] = { 128.f/255.f, 128.f/255.f, 128.f/255.f, 128.f/255.f };

static const float YYYY_ADD[4] = { 16.f/255.f, 16.f/255.f, 16.f/255.f, 16.f/255.f };
static const float UUUU_ADD[4] = { 128.f/255.f, 128.f/255.f, 128.f/255.f, 128.f/255.f };
static const float VVVV_ADD[4] = { 128.f/255.f, 128.f/255.f, 128.f/255.f, 128.f/255.f };

void VirtualEFBCopy::Update(u32 dstAddr, unsigned int dstFormat, unsigned int srcFormat,
	const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf)
{
	GLuint srcTex = (srcFormat == PIXELFMT_Z24)
		? FramebufferManager::ResolveAndGetDepthTarget(srcRect)
		: FramebufferManager::ResolveAndGetRenderTarget(srcRect);

	// Clamp srcRect to 640x528. BPS: The Strike tries to encode an 800x600
	// texture, which is invalid.
	EFBRectangle correctSrc = srcRect;
	correctSrc.ClampUL(0, 0, EFB_WIDTH, EFB_HEIGHT);

	unsigned int newRealW = correctSrc.GetWidth() / (scaleByHalf ? 2 : 1);
	unsigned int newRealH = correctSrc.GetHeight() / (scaleByHalf ? 2 : 1);

	TargetRectangle targetRect = g_renderer->ConvertEFBRectangle(correctSrc);
	unsigned int newVirtualW = targetRect.GetWidth() / (scaleByHalf ? 2 : 1);
	unsigned int newVirtualH = targetRect.GetHeight() / (scaleByHalf ? 2 : 1);

	EnsureVirtualTexture(newVirtualW, newVirtualH);

	const float* colorMatrix;
	const float* colorAdd;

	bool disableAlpha = (srcFormat != PIXELFMT_RGBA6_Z24);

	switch (dstFormat)
	{
	case EFB_COPY_R4:
	case EFB_COPY_R8_1:
	case EFB_COPY_R8:
		if (isIntensity) {
			colorMatrix = YYYY_MATRIX;
			colorAdd = YYYY_ADD;
		} else {
			colorMatrix = RRRR_MATRIX;
			colorAdd = ZERO_ADD;
		}
		break;
	case EFB_COPY_RA4:
	case EFB_COPY_RA8:
		if (isIntensity) {
			colorMatrix = disableAlpha ? YYY0_MATRIX : YYYA_MATRIX;
			colorAdd = disableAlpha ? YYY1_ADD : YUV0_ADD;
		} else {
			colorMatrix = disableAlpha ? RRR0_MATRIX : RRRA_MATRIX;
			colorAdd = disableAlpha ? A1_ADD : ZERO_ADD;
		}
		break;
	case EFB_COPY_RGB565:
		if (isIntensity) {
			colorMatrix = YUV0_MATRIX;
			colorAdd = YUV1_ADD;
		} else {
			colorMatrix = RGB0_MATRIX;
			colorAdd = A1_ADD;
		}
		break;
	case EFB_COPY_RGB5A3:
	case EFB_COPY_RGBA8:
		if (isIntensity) {
			colorMatrix = disableAlpha ? YUV0_MATRIX : YUVA_MATRIX;
			colorAdd = disableAlpha ? YUV1_ADD : YUV0_ADD;
		} else {
			colorMatrix = disableAlpha ? RGB0_MATRIX : RGBA_MATRIX;
			colorAdd = disableAlpha ? A1_ADD : ZERO_ADD;
		}
		break;
	case EFB_COPY_A8:
		colorMatrix = disableAlpha ? ZERO_MATRIX : AAAA_MATRIX;
		colorAdd = disableAlpha ? ALL_ONE_ADD : ZERO_ADD;
		break;
	case EFB_COPY_G8:
		if (isIntensity) {
			colorMatrix = UUUU_MATRIX;
			colorAdd = UUUU_ADD;
		} else {
			colorMatrix = GGGG_MATRIX;
			colorAdd = ZERO_ADD;
		}
		break;
	case EFB_COPY_B8:
		if (isIntensity) {
			colorMatrix = VVVV_MATRIX;
			colorAdd = VVVV_ADD;
		} else {
			colorMatrix = BBBB_MATRIX;
			colorAdd = ZERO_ADD;
		}
		break;
	case EFB_COPY_RG8:
		if (isIntensity) {
			colorMatrix = YYYU_MATRIX;
			colorAdd = YYYU_ADD;
		} else {
			colorMatrix = RRRG_MATRIX;
			colorAdd = ZERO_ADD;
		}
		break;
	case EFB_COPY_GB8:
		if (isIntensity) {
			colorMatrix = UUUV_MATRIX;
			colorAdd = UUUV_ADD;
		} else {
			colorMatrix = GGGB_MATRIX;
			colorAdd = ZERO_ADD;
		}
		break;
	default:
		ERROR_LOG(VIDEO, "Couldn't fake this EFB copy format 0x%X", dstFormat);
		return;
	}

	VirtualizeShade(srcTex, srcFormat, isIntensity, scaleByHalf,
		correctSrc, newVirtualW, newVirtualH,
		colorMatrix, colorAdd);

	m_realW = newRealW;
	m_realH = newRealH;
	m_dstFormat = dstFormat;
	m_dirty = true;
}

inline bool IsPaletted(u32 format) {
	return format == GX_TF_C4 || format == GX_TF_C8 || format == GX_TF_C14X2;
}

GLuint VirtualEFBCopy::Virtualize(u32 ramAddr, u32 width, u32 height, u32 levels,
	u32 format, u32 tlutAddr, u32 tlutFormat, bool force)
{
	// FIXME: Check if encoded dstFormat and texture format are compatible,
	// reinterpret or fall back to RAM if necessary
	
	// Only make these checks if there is a RAM copy to fall back on.
	if (!force && (width != m_realW || height != m_realH || levels != 1))
	{
		INFO_LOG(VIDEO, "Virtual EFB copy was incompatible; falling back to RAM");
		return 0;
	}
	
	DEBUG_LOG(VIDEO, "Interpreting dstFormat %s as tex format %s",
		EFB_COPY_DST_FORMAT_NAMES[m_dstFormat], TEX_FORMAT_NAMES[format]);

	return m_texture.tex;
}

void VirtualEFBCopy::EnsureVirtualTexture(GLsizei width, GLsizei height)
{
	bool recreate = !m_texture.tex ||
		width != m_texture.width ||
		height != m_texture.height;

	if (!m_texture.tex)
		glGenTextures(1, &m_texture.tex);

	if (recreate)
	{
		glBindTexture(GL_TEXTURE_2D, m_texture.tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);

		m_texture.width = width;
		m_texture.height = height;
	}
}

void VirtualEFBCopy::VirtualizeShade(GLuint texSrc, unsigned int srcFormat,
	bool yuva, bool scale,
	const EFBRectangle& srcRect,
	unsigned int virtualW, unsigned int virtualH,
	const float* colorMatrix, const float* colorAdd)
{
	float colMat[28] = { 0 };
	float* colMatAdd = &colMat[16];
	float* colMatMask = &colMat[20];

	colMatMask[0] = colMatMask[1] = colMatMask[2] = colMatMask[3] = 255.f;
	colMatMask[4] = colMatMask[5] = colMatMask[6] = colMatMask[7] = 1.f / 255.f;

	memcpy(colMat, colorMatrix, 4*4*sizeof(float));
	memcpy(colMatAdd, colorAdd, 4*sizeof(float));

	g_renderer->ResetAPIState();
	
	GLuint virtCopyFramebuf = ((TextureCache*)g_textureCache)->GetVirtCopyFramebuf();
	FramebufferManager::SetFramebuffer(virtCopyFramebuf);
	glBindTexture(GL_TEXTURE_2D, m_texture.tex);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_texture.tex, 0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);

	GL_REPORT_FBO_ERROR();

	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_RECTANGLE_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texSrc);

	if (scale)
	{
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}

	glViewport(0, 0, virtualW, virtualH);

	PixelShaderCache::SetCurrentShader((srcFormat == PIXELFMT_Z24)
		? PixelShaderCache::GetDepthMatrixProgram()
		: PixelShaderCache::GetColorMatrixProgram());
	PixelShaderManager::SetColorMatrix(colMat);
	GL_REPORT_ERRORD();

	TargetRectangle targetRect = g_renderer->ConvertEFBRectangle(srcRect);

	glBegin(GL_QUADS);
	glTexCoord2f((GLfloat)targetRect.left,  (GLfloat)targetRect.bottom); glVertex2f(-1,  1);
	glTexCoord2f((GLfloat)targetRect.left,  (GLfloat)targetRect.top  ); glVertex2f(-1, -1);
	glTexCoord2f((GLfloat)targetRect.right, (GLfloat)targetRect.top  ); glVertex2f( 1, -1);
	glTexCoord2f((GLfloat)targetRect.right, (GLfloat)targetRect.bottom); glVertex2f( 1,  1);
	glEnd();
	
	g_renderer->RestoreAPIState();

	FramebufferManager::SetFramebuffer(0);
    VertexShaderManager::SetViewportChanged();
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	glDisable(GL_TEXTURE_RECTANGLE_ARB);

    GL_REPORT_ERRORD();
}

}
