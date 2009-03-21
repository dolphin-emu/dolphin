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

// Fast image conversion using OpenGL shaders.
// This kind of stuff would be a LOT nicer with OpenCL.

#include "TextureConverter.h"
#include "TextureConversionShader.h"
#include "PixelShaderCache.h"
#include "VertexShaderManager.h"
#include "Globals.h"
#include "Config.h"
#include "ImageWrite.h"
#include "Render.h"

namespace TextureConverter
{

static GLuint s_texConvFrameBuffer = 0;
static GLuint s_srcTexture = 0;			// for decoding from RAM
static GLuint s_dstRenderBuffer = 0;	// for encoding to RAM

const int renderBufferWidth = 1024;
const int renderBufferHeight = 1024;

static FRAGMENTSHADER s_rgbToYuyvProgram;
static FRAGMENTSHADER s_yuyvToRgbProgram;

// todo - store shaders in a smarter way - there are not 64 different copy texture formats
const u32 NUM_ENCODING_PROGRAMS = 64;
static FRAGMENTSHADER s_encodingPrograms[NUM_ENCODING_PROGRAMS];

void CreateRgbToYuyvProgram()
{
	// output is BGRA because that is slightly faster than RGBA
	const char *FProgram =
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
	"  float v0 = (0.439f * c0.r) - (0.368f * c0.g) - (0.071f * c0.b) + 0.5f;\n"
	"  float y1 = (0.257f * c1.r) + (0.504f * c1.g) + (0.098f * c1.b) + 0.0625f;\n"
	"  float u1 =-(0.148f * c1.r) - (0.291f * c1.g) + (0.439f * c1.b) + 0.5f;\n"
	"  float v1 = (0.439f * c1.r) - (0.368f * c1.g) - (0.071f * c1.b) + 0.5f;\n"

	"  ocol0 = float4(y1, (u0 + u1) / 2, y0, (v0 + v1) / 2);\n"
	"}\n";

	if (!PixelShaderCache::CompilePixelShader(s_rgbToYuyvProgram, FProgram)) {
        ERROR_LOG(VIDEO, "Failed to create RGB to YUYV fragment program");
    }
}

void CreateYuyvToRgbProgram()
{
	const char *FProgram =
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

	if (!PixelShaderCache::CompilePixelShader(s_yuyvToRgbProgram, FProgram)) {
        ERROR_LOG(VIDEO, "Failed to create YUYV to RGB fragment program");
    }
}

FRAGMENTSHADER &GetOrCreateEncodingShader(u32 format)
{
	if (format > NUM_ENCODING_PROGRAMS)
	{
		PanicAlert("Unknown texture copy format: 0x%x\n", format);
		return s_encodingPrograms[0];
	}

	// todo - this does not handle the case that an application is using RGB555/4443
	// and switches EFB formats between a format that does and does not support alpha
	if (s_encodingPrograms[format].glprogid == 0)
	{
		const char* shader = TextureConversionShader::GenerateEncodingShader(format);

#if defined(_DEBUG) || defined(DEBUGFAST)
		if (g_Config.iLog & CONF_SAVESHADERS && shader) {
			static int counter = 0;
			char szTemp[MAX_PATH];
			sprintf(szTemp, "%s/enc_%04i.txt", FULL_DUMP_DIR, counter++);

			SaveData(szTemp, shader);
		}
#endif

		if (!PixelShaderCache::CompilePixelShader(s_encodingPrograms[format], shader)) {
			const char* error = cgGetLastListing(g_cgcontext);
			ERROR_LOG(VIDEO, "Failed to create encoding fragment program");
		}
    }

	return s_encodingPrograms[format];
}

void Init()
{
	glGenFramebuffersEXT(1, &s_texConvFrameBuffer);

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
	glDeleteFramebuffersEXT(1, &s_texConvFrameBuffer);
	s_srcTexture = 0;
	s_dstRenderBuffer = 0;
	s_texConvFrameBuffer = 0;
}

void EncodeToRamUsingShader(FRAGMENTSHADER& shader, GLuint srcTexture, const TRectangle& sourceRc,
				            u8* destAddr, int dstWidth, int dstHeight, bool linearFilter)
{
	Renderer::SetRenderMode(Renderer::RM_Normal);

	Renderer::ResetGLState();
	
	// switch to texture converter frame buffer
	// attach render buffer as color destination
	Renderer::SetFramebuffer(s_texConvFrameBuffer);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, s_dstRenderBuffer);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, s_dstRenderBuffer);	
	GL_REPORT_ERRORD();
	
	// set source texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, srcTexture);

	if (linearFilter)
	{
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}

    TextureMngr::EnableTexRECT(0);
	for (int i = 1; i < 8; ++i)
		TextureMngr::DisableStage(i);	
	GL_REPORT_ERRORD();

	glViewport(0, 0, (GLsizei)dstWidth, (GLsizei)dstHeight);

	glEnable(GL_FRAGMENT_PROGRAM_ARB);
	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, shader.glprogid);	

	// Draw...
	glBegin(GL_QUADS);
    glTexCoord2f((float)sourceRc.left, (float)sourceRc.top);     glVertex2f(-1,-1);
	glTexCoord2f((float)sourceRc.left, (float)sourceRc.bottom);  glVertex2f(-1,1);
    glTexCoord2f((float)sourceRc.right, (float)sourceRc.bottom); glVertex2f(1,1);
    glTexCoord2f((float)sourceRc.right, (float)sourceRc.top);    glVertex2f(1,-1);
    glEnd();
	GL_REPORT_ERRORD();

	// .. and then readback the results.
	// TODO: make this less slow.
	glReadPixels(0, 0, (GLsizei)dstWidth, (GLsizei)dstHeight, GL_BGRA, GL_UNSIGNED_BYTE, destAddr);
	GL_REPORT_ERRORD();

	Renderer::SetFramebuffer(0);
    Renderer::RestoreGLState();
    VertexShaderManager::SetViewportChanged();
	
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    TextureMngr::DisableStage(0);

	Renderer::RestoreGLState();
    GL_REPORT_ERRORD();
}

void EncodeToRam(u32 address, bool bFromZBuffer, bool bIsIntensityFmt, u32 copyfmt, bool bScaleByHalf, const TRectangle& source)
{
	u32 format = copyfmt;

	if (bFromZBuffer)
	{
		format |= _GX_TF_ZTF;
		if (copyfmt == 11)
			format = GX_TF_Z16;
		else if (format < GX_TF_Z8 || format > GX_TF_Z24X8)
			format |= _GX_TF_CTF;
	}
	else
	{
		if (copyfmt > GX_TF_RGBA8 || (copyfmt < GX_TF_RGB565 && !bIsIntensityFmt))
			format |= _GX_TF_CTF;
	}

	FRAGMENTSHADER& texconv_shader = GetOrCreateEncodingShader(format);
	if (texconv_shader.glprogid == 0)
		return;

	u8 *dest_ptr = Memory_GetPtr(address);

	u32 source_texture = bFromZBuffer ? Renderer::ResolveAndGetFakeZTarget(source) : Renderer::ResolveAndGetRenderTarget(source);
	int width = source.right - source.left;
	int height = source.bottom - source.top;

	int size_in_bytes = TexDecoder_GetTextureSizeInBytes(width, height, format);

	// Invalidate any existing texture covering this memory range.
	// TODO - don't delete the texture if it already exists, just replace the contents.
	TextureMngr::InvalidateRange(address, size_in_bytes);

	if (bScaleByHalf)
	{
		// Hm. Shouldn't this only scale destination, not source?
		// The bloom in Beyond Good & Evil is a good test case - due to this problem,
		// it goes very wrong. Compare by switching back and forth between Copy textures to RAM and GL Texture.
		// This also affects the shadows in Burnout 2 badly.
		width /= 2;
		height /= 2;
	}
	
	u16 blkW = TextureConversionShader::GetBlockWidthInTexels(format) - 1;
	u16 blkH = TextureConversionShader::GetBlockHeightInTexels(format) - 1;	
	u16 samples = TextureConversionShader::GetEncodedSampleCount(format);	

	// only copy on cache line boundaries
	// extra pixels are copied but not displayed in the resulting texture
	s32 expandedWidth = (width + blkW) & (~blkW);
	s32 expandedHeight = (height + blkH) & (~blkH);		

	u32 top = Renderer::GetTargetHeight() - (source.top + expandedHeight);

	TextureConversionShader::SetShaderParameters(expandedWidth, expandedHeight, source.left, top, bScaleByHalf?2.0f:1.0f, format);

	TRectangle scaledSource;
	scaledSource.top = 0;
	scaledSource.bottom = expandedHeight;
	scaledSource.left = 0;
	scaledSource.right = expandedWidth / samples;	

	EncodeToRamUsingShader(texconv_shader, source_texture, scaledSource, dest_ptr, expandedWidth / samples, expandedHeight, bScaleByHalf);

	if (bFromZBuffer)
        Renderer::SetZBufferRender(); // notify for future settings
}

void EncodeToRamYUYV(GLuint srcTexture, const TRectangle& sourceRc,
				     u8* destAddr, int dstWidth, int dstHeight)
{
	EncodeToRamUsingShader(s_rgbToYuyvProgram, srcTexture, sourceRc, destAddr, dstWidth / 2, dstHeight, false);
}


// Should be scale free.
void DecodeToTexture(u8* srcAddr, int srcWidth, int srcHeight, GLuint destTexture)
{
	Renderer::SetRenderMode(Renderer::RM_Normal);
	Renderer::ResetGLState();

	float srcFormatFactor = 0.5f;
	float srcFmtWidth = srcWidth * srcFormatFactor;

	// swich to texture converter frame buffer
	// attach destTexture as color destination
	Renderer::SetFramebuffer(s_texConvFrameBuffer);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, destTexture);	
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, destTexture, 0);

	// activate source texture
	// set srcAddr as data for source texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, s_srcTexture);

	// TODO: make this less slow.  (How?)
    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8, (GLsizei)srcFmtWidth, (GLsizei)srcHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, srcAddr);	

    TextureMngr::EnableTexRECT(0);
    for (int i = 1; i < 8; ++i)
		TextureMngr::DisableStage(i);

	glViewport(0, 0, srcWidth, srcHeight);

	glEnable(GL_FRAGMENT_PROGRAM_ARB);
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, s_yuyvToRgbProgram.glprogid);	

	GL_REPORT_ERRORD();

    glBegin(GL_QUADS);
	glTexCoord2f(srcFmtWidth, (float)srcHeight); glVertex2f(1,-1);
	glTexCoord2f(srcFmtWidth, 0); glVertex2f(1,1);
	glTexCoord2f(0, 0); glVertex2f(-1,1);
	glTexCoord2f(0, (float)srcHeight); glVertex2f(-1,-1);
    glEnd();	

	// reset state
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    TextureMngr::DisableStage(0);

	VertexShaderManager::SetViewportChanged();

	Renderer::RestoreGLState();
    GL_REPORT_ERRORD();
}

}  // namespace
