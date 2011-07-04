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

static const char VIRTUAL_EFB_COPY_FS[] =
"// dolphin-emu GLSL virtual efb copy fragment shader\n"

// DEPTH should be 1 on, 0 off
"#ifndef DEPTH\n"
"#error DEPTH not defined.\n"
"#endif\n"

// SCALE should be 1 on, 0 off
"#ifndef SCALE\n"
"#error SCALE not defined.\n"
"#endif\n"

// Uniforms
// u_Matrix: Color matrix
"uniform mat4x4 u_Matrix;\n"
// u_Add: Color add
"uniform vec4 u_Add;\n"
// u_SourceRect: left, top, right, bottom of source rectangle in texture coordinates
"uniform vec4 u_SourceRect;\n"

// Samplers
"uniform sampler2DRect u_EFBSampler;\n"

"#if DEPTH\n"
"vec4 Fetch(vec2 coord)\n"
"{\n"
	// GLSL doesn't have strict floating-point precision requirements. This is a
	// careful way of translating Z24 to R8 G8 B8.
	// Ref: <http://www.horde3d.org/forums/viewtopic.php?f=1&t=569>
	// Ref: <http://code.google.com/p/dolphin-emu/source/detail?r=6217>
	"float depth = 255.99998474121094 * texture2DRect(u_EFBSampler, coord).r;\n"
	"vec4 result = depth * vec4(1,1,1,1);\n"

	"result.a = floor(result.a);\n" // bits 31..24

	"result.rgb -= result.a;\n"
	"result.rgb *= 256.0;\n"
	"result.r = floor(result.r);\n" // bits 23..16

	"result.gb -= result.r;\n"
	"result.gb *= 256.0;\n"
	"result.g = floor(result.g);\n" // bits 15..8

	"result.b -= result.g;\n"
	"result.b *= 256.0;\n"
	"result.b = floor(result.b);\n" // bits 7..0

	"result = vec4(result.arg / 255.0, 1.0);\n"
	"return result;\n"
"}\n"
"#else\n"
"vec4 Fetch(vec2 coord)\n"
"{\n"
	"return texture2DRect(u_EFBSampler, coord);\n"
"}\n"
"#endif\n" // #if DEPTH

"#if SCALE\n"
"vec4 ScaledFetch(vec2 coord)\n"
"{\n"
	// coord is in the center of a 2x2 square of texels. Sample from the center
	// of these texels and average them.
	"vec4 s0 = Fetch(coord + vec2(-0.5,-0.5));\n"
	"vec4 s1 = Fetch(coord + vec2( 0.5,-0.5));\n"
	"vec4 s2 = Fetch(coord + vec2(-0.5, 0.5));\n"
	"vec4 s3 = Fetch(coord + vec2( 0.5, 0.5));\n"
	// Box filter
	"return 0.25 * (s0 + s1 + s2 + s3);\n"
"}\n"
"#else\n"
"vec4 ScaledFetch(vec2 coord)\n"
"{\n"
	"return Fetch(coord);\n"
"}\n"
"#endif\n" // #if SCALE

// Main entry point
"void main()\n"
"{\n"
	"vec2 coord = mix(u_SourceRect.xy, u_SourceRect.zw, gl_TexCoord[0].xy);\n"
	"vec4 pixel = ScaledFetch(coord);\n"
	"gl_FragColor = pixel * u_Matrix + u_Add;\n"
"}\n"
;

VirtualCopyProgramManager::Program::Program()
	: program(0)
{ }

VirtualCopyProgramManager::Program::~Program()
{
	glDeleteProgram(program);
	program = 0;
}

bool VirtualCopyProgramManager::Program::Compile(bool scale, bool depth)
{
	const GLchar* fs[] = {
		"#version 120\n",
		"#extension GL_ARB_texture_rectangle : enable\n",
		depth ? "#define DEPTH 1\n" : "#define DEPTH 0\n",
		scale ? "#define SCALE 1\n" : "#define SCALE 0\n",
		VIRTUAL_EFB_COPY_FS
	};

	GLuint shader = OpenGL_CreateShader(GL_FRAGMENT_SHADER, sizeof(fs)/sizeof(const GLchar*), fs);
	if (!shader)
	{
		ERROR_LOG(VIDEO, "Failed to compile virtual copy shader");
		return false;
	}

	program = glCreateProgram();
	glAttachShader(program, shader);
	if (!OpenGL_LinkProgram(program))
	{
		glDeleteProgram(program);
		glDeleteShader(shader);
		program = 0;
		ERROR_LOG(VIDEO, "Failed to link virtual copy shader");
		return false;
	}

	// Shader is now embedded in the program
	glDeleteShader(shader);

	// Find uniform locations
	uMatrixLoc = glGetUniformLocation(program, "u_Matrix");
	uAddLoc = glGetUniformLocation(program, "u_Add");
	uSourceRectLoc = glGetUniformLocation(program, "u_SourceRect");
	uEFBSamplerLoc = glGetUniformLocation(program, "u_EFBSampler");

	(void)GL_REPORT_ERROR();

	return true;
}

const VirtualCopyProgramManager::Program&
VirtualCopyProgramManager::GetProgram(bool scale, bool depth)
{
	int key = MakeKey(scale, depth);
	if (!m_programs[key].program)
	{
		INFO_LOG(VIDEO, "Compiling virtual efb copy program scale %d, depth %d",
			scale ? 1 : 0, depth ? 1 : 0);

		m_programs[key].Compile(scale, depth);
	}

	return m_programs[key];
}

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
	VirtualCopyProgramManager& programMan = ((TextureCache*)g_textureCache)->GetVirtProgramManager();
	const VirtualCopyProgramManager::Program& program = programMan.GetProgram(scale, srcFormat == PIXELFMT_Z24);
	if (!program.program)
		return;

	g_renderer->ResetAPIState();

	// Bind destination texture to framebuffer
	GLuint fbo = ((TextureCache*)g_textureCache)->GetVirtCopyFramebuf();
	FramebufferManager::SetFramebuffer(fbo);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_texture.tex, 0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	GL_REPORT_FBO_ERROR();

	// Bind fragment program and uniforms
	glUseProgram(program.program);

	glUniformMatrix4fv(program.uMatrixLoc, 1, GL_FALSE, colorMatrix);
	glUniform4fv(program.uAddLoc, 1, colorAdd);

	TargetRectangle targetRect = g_renderer->ConvertEFBRectangle(srcRect);
	GLfloat uSourceRect[4] = {
		GLfloat(targetRect.left),
		GLfloat(targetRect.top),
		GLfloat(targetRect.right),
		GLfloat(targetRect.bottom)
	};
	glUniform4fv(program.uSourceRectLoc, 1, uSourceRect);

	glUniform1i(program.uEFBSamplerLoc, 0); // Set u_EFBSampler to sampler 0

	// Bind texSrc to sampler 0
	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_RECTANGLE_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texSrc);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glViewport(0, 0, virtualW, virtualH);

	// Encode!
	OpenGL_DrawEncoderQuad();

	// Clean-up

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	glDisable(GL_TEXTURE_RECTANGLE_ARB);

	glUseProgram(0);

	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, 0, 0);

	g_renderer->RestoreAPIState();
	
	FramebufferManager::SetFramebuffer(0);
	VertexShaderManager::SetViewportChanged();
}

}
