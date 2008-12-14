// Copyright (C) 2003-2008 Dolphin Project.

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

#include "TextureConverter.h"
#include "PixelShaderManager.h"
#include "VertexShaderManager.h"
#include "Globals.h"
#include "GLUtil.h"
#include "Render.h"

namespace TextureConverter
{

static GLuint s_frameBuffer = 0;
static GLuint s_srcTexture = 0;			// for decoding from RAM
static GLuint s_dstRenderBuffer = 0;	// for encoding to RAM

const int renderBufferWidth = 1024;
const int renderBufferHeight = 1024;

static FRAGMENTSHADER s_rgbToYuyvProgram;
static FRAGMENTSHADER s_yuyvToRgbProgram;

void CreateRgbToYuyvProgram()
{
	// output is BGRA because that is slightly faster than RGBA
  char *FProgram = (char *)
	"uniform samplerRECT samp0 : register(s0);\n"	
	"void main(\n"
	"  out float4 ocol0 : COLOR0,\n"
	"  in float2 uv0 : TEXCOORD0)\n"
	"{\n"		
	"  float2 uv1 = float2(uv0.x + 1.0f, uv0.y);\n"
	"  float3 c0 = texRECT(samp0, uv0).rgb;\n"
	"  float3 c1 = texRECT(samp0, uv1).rgb;\n"

	"  float y0 = (0.257f * c0.r) + (0.504f * c0.g) + (0.098f * c0.b) + 0.0625f;\n"
	"  float u0 =-(0.148f * c0.r) - (0.291f * c0.g) + (0.439f * c0.b) + 0.5f;\n"
	"  float y1 = (0.257f * c1.r) + (0.504f * c1.g) + (0.098f * c1.b) + 0.0625f;\n"
	"  float v1 = (0.439f * c1.r) - (0.368f * c1.g) - (0.071f * c1.b) + 0.5f;\n"	

	"  ocol0 = float4(y1, u0, y0, v1);\n"
	"}\n";

	if (!PixelShaderMngr::CompilePixelShader(s_rgbToYuyvProgram, FProgram)) {
        ERROR_LOG("Failed to create RGB to YUYV fragment program\n");
    }
}

void CreateYuyvToRgbProgram()
{
  char *FProgram = (char *)
	"uniform samplerRECT samp0 : register(s0);\n"	
	"void main(\n"
	"  out float4 ocol0 : COLOR0,\n"
	"  in float2 uv0 : TEXCOORD0)\n"
	"{\n"		
	"  float4 c0 = texRECT(samp0, uv0).rgba;\n"

	"  float f = step(0.5, frac(uv0.x));\n"
	"  float y = lerp(c0.b, c0.r, f);\n"
	"  float yComp = 1.164f * (y - 0.0625f);\n"
	"  float uComp = c0.g - 0.5f;\n"
	"  float vComp = c0.a - 0.5f;\n"

    "  ocol0 = float4(yComp + (1.596f * vComp),\n"
	"                 yComp - (0.813f * vComp) - (0.391f * uComp),\n"
	"                 yComp + (2.018f * uComp),\n"
	"                 1.0f);\n"
	"}\n";

	if (!PixelShaderMngr::CompilePixelShader(s_yuyvToRgbProgram, FProgram)) {
        ERROR_LOG("Failed to create YUYV to RGB fragment program\n");
    }
}

void Init()
{
	glGenFramebuffersEXT( 1, &s_frameBuffer);

	glGenRenderbuffersEXT(1, &s_dstRenderBuffer);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, s_dstRenderBuffer);
	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA, renderBufferWidth, renderBufferHeight);

	glGenTextures(1, &s_srcTexture);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, s_srcTexture);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	CreateRgbToYuyvProgram();
	CreateYuyvToRgbProgram();
}

void Shutdown()
{
	glDeleteTextures(1, &s_srcTexture);	

	glDeleteRenderbuffersEXT(1, &s_dstRenderBuffer);

	glDeleteFramebuffersEXT(1, &s_frameBuffer);	
}

void EncodeToRam(GLuint srcTexture, const TRectangle& sourceRc,
				 u8* destAddr, int dstWidth, int dstHeight)
{
	Renderer::SetRenderMode(Renderer::RM_Normal);

	Renderer::ResetGLState();

	float dstFormatFactor = 0.5f;
	float dstFmtWidth = dstWidth * dstFormatFactor;

	// switch to texture converter frame buffer
	// attach render buffer as color destination
	Renderer::SetFramebuffer(s_frameBuffer);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, s_dstRenderBuffer);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, s_dstRenderBuffer);	
	GL_REPORT_ERRORD();
	
	// set source texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, srcTexture);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    TextureMngr::EnableTexRECT(0);
	for (int i = 1; i < 8; ++i)
		TextureMngr::DisableStage(i);	
	GL_REPORT_ERRORD();

	glViewport(0, 0, (GLsizei)dstFmtWidth, (GLsizei)dstHeight);

	glEnable(GL_FRAGMENT_PROGRAM_ARB);
    glBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, s_rgbToYuyvProgram.glprogid);	

	glBegin(GL_QUADS);
    glTexCoord2f((float)sourceRc.left, (float)sourceRc.top);     glVertex2f(-1,-1);
	glTexCoord2f((float)sourceRc.left, (float)sourceRc.bottom);  glVertex2f(-1,1);
    glTexCoord2f((float)sourceRc.right, (float)sourceRc.bottom); glVertex2f(1,1);
    glTexCoord2f((float)sourceRc.right, (float)sourceRc.top);    glVertex2f(1,-1);
    glEnd();
	GL_REPORT_ERRORD();

	// TODO: this is real slow. try using a pixel buffer object.
	glReadPixels(0, 0, dstFmtWidth, dstHeight, GL_BGRA, GL_UNSIGNED_BYTE, destAddr);
	GL_REPORT_ERRORD();

	Renderer::SetFramebuffer(0);
    Renderer::RestoreGLState();
    VertexShaderMngr::SetViewportChanged();
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    TextureMngr::DisableStage(0);

	Renderer::RestoreGLState();
    GL_REPORT_ERRORD();
}

void DecodeToTexture(u8* srcAddr, int srcWidth, int srcHeight, GLuint destTexture)
{
	Renderer::SetRenderMode(Renderer::RM_Normal);

	Renderer::ResetGLState();

	float srcFormatFactor = 0.5f;
	float srcFmtWidth = srcWidth * srcFormatFactor;

	// swich to texture converter frame buffer
	// attach destTexture as color destination
	Renderer::SetFramebuffer(s_frameBuffer);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, destTexture);	
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, destTexture, 0);

	// activate source texture
	// set srcAddr as data for source texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, s_srcTexture);

	// TODO: this is slow. try using a pixel buffer object.
    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8, srcFmtWidth, srcHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, srcAddr);	

    TextureMngr::EnableTexRECT(0);
    for (int i = 1; i < 8; ++i)
		TextureMngr::DisableStage(i);

	glViewport(0, 0, srcWidth, srcHeight);

	glEnable(GL_FRAGMENT_PROGRAM_ARB);
    glBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, s_yuyvToRgbProgram.glprogid);	

	GL_REPORT_ERRORD();

    glBegin(GL_QUADS);
	glTexCoord2f(srcFmtWidth, srcHeight); glVertex2f(1,-1);
	glTexCoord2f(srcFmtWidth, 0); glVertex2f(1,1);
	glTexCoord2f(0, 0); glVertex2f(-1,1);
	glTexCoord2f(0, srcHeight); glVertex2f(-1,-1);
    glEnd();	

	// reset state
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    TextureMngr::DisableStage(0);

	VertexShaderMngr::SetViewportChanged();

	Renderer::RestoreGLState();
    GL_REPORT_ERRORD();
}

}
