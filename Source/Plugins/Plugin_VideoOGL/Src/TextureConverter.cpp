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

// Fast image conversion using OpenGL shaders.
// This kind of stuff would be a LOT nicer with OpenCL.

#include <math.h>

#include "EFBCopy.h"
#include "FileUtil.h"
#include "FramebufferManager.h"
#include "Globals.h"
#include "Hash.h"
#include "HW/Memmap.h"
#include "ImageWrite.h"
#include "PixelShaderCache.h"
#include "Render.h"
#include "TextureConverter.h"
#include "TextureConversionShader.h"
#include "TextureCache.h"
#include "VertexShaderManager.h"
#include "VideoConfig.h"

namespace OGL
{

namespace TextureConverter
{

static GLuint s_texConvFrameBuffer = 0;
static GLuint s_srcTexture = 0;			// for decoding from RAM
static GLuint s_srcTextureWidth = 0;
static GLuint s_srcTextureHeight = 0;
static GLuint s_dstRenderBuffer = 0;	// for encoding to RAM

const int renderBufferWidth = 1024;
const int renderBufferHeight = 1024;

static FRAGMENTSHADER s_rgbToYuyvProgram;
static FRAGMENTSHADER s_yuyvToRgbProgram;

inline u32 MakeEncoderKey(u32 dstFormat, bool isDepth, bool isIntensity) {
	return (dstFormat << 2) | (isDepth ? (1<<1) : 0) | (isIntensity ? (1<<0) : 0);
}

// Not all slots are taken - but who cares.
const u32 NUM_ENCODING_PROGRAMS = 16*2*2;
static FRAGMENTSHADER s_encodingPrograms[NUM_ENCODING_PROGRAMS];

void CreateRgbToYuyvProgram()
{
	// Output is BGRA because that is slightly faster than RGBA.
	const char *FProgram =
	"uniform samplerRECT samp0 : register(s0);\n"
	"void main(\n"
	"  out float4 ocol0 : COLOR0,\n"
	"  in float2 uv0 : TEXCOORD0)\n"
	"{\n"		
	"  float2 uv1 = float2(uv0.x + 1.0f, uv0.y);\n"
	"  float3 c0 = texRECT(samp0, uv0).rgb;\n"
	"  float3 c1 = texRECT(samp0, uv1).rgb;\n"
	"  float3 y_const = float3(0.257f,0.504f,0.098f);\n"
	"  float3 u_const = float3(-0.148f,-0.291f,0.439f);\n"
	"  float3 v_const = float3(0.439f,-0.368f,-0.071f);\n"
	"  float4 const3 = float4(0.0625f,0.5f,0.0625f,0.5f);\n"
	"  float3 c01 = (c0 + c1) * 0.5f;\n"  
	"  ocol0 = float4(dot(c1,y_const),dot(c01,u_const),dot(c0,y_const),dot(c01, v_const)) + const3;\n"	  	
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

FRAGMENTSHADER &GetOrCreateEncodingShader(u32 dstFormat, bool isDepth, bool isIntensity)
{
	u32 key = MakeEncoderKey(dstFormat, isDepth, isIntensity);
	_assert_msg_(VIDEO, key < NUM_ENCODING_PROGRAMS, "Bad encoder key");

	if (s_encodingPrograms[key].glprogid == 0)
	{
		const char* shader = TextureConversionShader::GenerateEncodingShader(
			dstFormat, isDepth, isIntensity, API_OPENGL);

#if defined(_DEBUG) || defined(DEBUGFAST)
		if (g_ActiveConfig.iLog & CONF_SAVESHADERS && shader) {
			static int counter = 0;
			char szTemp[MAX_PATH];
			sprintf(szTemp, "%senc_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), counter++);

			SaveData(szTemp, shader);
		}
#endif

		if (!PixelShaderCache::CompilePixelShader(s_encodingPrograms[key], shader)) {
			ERROR_LOG(VIDEO, "Failed to create encoding fragment program");
		}
    }
	return s_encodingPrograms[key];
}

void Init()
{
	glGenFramebuffersEXT(1, &s_texConvFrameBuffer);

	glGenRenderbuffersEXT(1, &s_dstRenderBuffer);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, s_dstRenderBuffer);
	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA, renderBufferWidth, renderBufferHeight);

	s_srcTextureWidth = 0;
	s_srcTextureHeight = 0;

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

	s_rgbToYuyvProgram.Destroy();
	s_yuyvToRgbProgram.Destroy();

	for (unsigned int i = 0; i < NUM_ENCODING_PROGRAMS; i++)
		s_encodingPrograms[i].Destroy();

	s_srcTexture = 0;
	s_dstRenderBuffer = 0;
	s_texConvFrameBuffer = 0;
}

void EncodeToRamUsingShader(FRAGMENTSHADER& shader, GLuint srcTexture, const TargetRectangle& sourceRc,
				            u8* destAddr, int dstWidth, int dstHeight, int readStride, bool toTexture, bool linearFilter)
{

	
	// switch to texture converter frame buffer
	// attach render buffer as color destination
	FramebufferManager::SetFramebuffer(s_texConvFrameBuffer);

	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, s_dstRenderBuffer);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, s_dstRenderBuffer);	
	GL_REPORT_ERRORD();
	
	for (int i = 1; i < 8; ++i)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_TEXTURE_RECTANGLE_ARB);
	}

	// set source texture
	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_RECTANGLE_ARB);
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

	GL_REPORT_ERRORD();

	glViewport(0, 0, (GLsizei)dstWidth, (GLsizei)dstHeight);

	PixelShaderCache::SetCurrentShader(shader.glprogid);

	// Draw...
	glBegin(GL_QUADS);
    glTexCoord2f((float)sourceRc.left, (float)sourceRc.top);     glVertex2f(-1,-1);
	glTexCoord2f((float)sourceRc.left, (float)sourceRc.bottom);  glVertex2f(-1,1);
    glTexCoord2f((float)sourceRc.right, (float)sourceRc.bottom); glVertex2f(1,1);
    glTexCoord2f((float)sourceRc.right, (float)sourceRc.top);    glVertex2f(1,-1);
    glEnd();
	GL_REPORT_ERRORD();

	// .. and then read back the results.
	// TODO: make this less slow.

	int writeStride = bpmem.copyMipMapStrideChannels * 32;

	if (writeStride != readStride && toTexture)
	{
		// writing to a texture of a different size

		int readHeight = readStride / dstWidth;
		readHeight /= 4; // 4 bytes per pixel

		int readStart = 0;
		int readLoops = dstHeight / readHeight;
		for (int i = 0; i < readLoops; i++)
		{
			glReadPixels(0, readStart, (GLsizei)dstWidth, (GLsizei)readHeight, GL_BGRA, GL_UNSIGNED_BYTE, destAddr);
			readStart += readHeight;
			destAddr += writeStride;
		}
	}
	else
		glReadPixels(0, 0, (GLsizei)dstWidth, (GLsizei)dstHeight, GL_BGRA, GL_UNSIGNED_BYTE, destAddr);

	GL_REPORT_ERRORD();
	
}

void EncodeToRam(u8* dst, unsigned int dstFormat, unsigned int srcFormat,
	const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf)
{
	// Clamp srcRect to 640x528. BPS: The Strike tries to encode an 800x600
	// texture, which is invalid.
	EFBRectangle correctSrc = srcRect;
	correctSrc.ClampUL(0, 0, EFB_WIDTH, EFB_HEIGHT);

	// Validate source rect size
	if (correctSrc.GetWidth() <= 0 || correctSrc.GetHeight() <= 0)
		return;

	unsigned int blockW = EFB_COPY_BLOCK_WIDTHS[dstFormat];
	unsigned int blockH = EFB_COPY_BLOCK_HEIGHTS[dstFormat];

	// Round up source dims to multiple of block size
	unsigned int actualWidth = correctSrc.GetWidth() / (scaleByHalf ? 2 : 1);
	actualWidth = (actualWidth + blockW-1) & ~(blockW-1);
	unsigned int actualHeight = correctSrc.GetHeight() / (scaleByHalf ? 2 : 1);
	actualHeight = (actualHeight + blockH-1) & ~(blockH-1);

	unsigned int numBlocksX = actualWidth/blockW;
	unsigned int numBlocksY = actualHeight/blockH;

	unsigned int cacheLinesPerRow;
	if (dstFormat == EFB_COPY_RGBA8) // RGBA takes two cache lines per block; all others take one
		cacheLinesPerRow = numBlocksX*2;
	else
		cacheLinesPerRow = numBlocksX;
	_assert_msg_(VIDEO, cacheLinesPerRow*32 <= EFB_COPY_MAX_BYTES_PER_ROW, "cache lines per row sanity check");

	unsigned int totalCacheLines = cacheLinesPerRow * numBlocksY;
	_assert_msg_(VIDEO, totalCacheLines*32 <= EFB_COPY_MAX_BYTES, "total encode size sanity check");

	FRAGMENTSHADER& texconv_shader = GetOrCreateEncodingShader(dstFormat,
		srcFormat == PIXELFMT_Z24, isIntensity);
	if (texconv_shader.glprogid == 0)
		return;
	
	g_renderer->ResetAPIState();

	GLuint source_texture = (srcFormat == PIXELFMT_Z24)
		? FramebufferManager::ResolveAndGetDepthTarget(srcRect)
		: FramebufferManager::ResolveAndGetRenderTarget(srcRect);

    float sampleStride = scaleByHalf ? 2.f : 1.f;
	TextureConversionShader::SetShaderParameters(
		(float)actualWidth,
		(float)Renderer::EFBToScaledY(actualHeight), // TODO: Why do we scale this?
		(float)Renderer::EFBToScaledX(correctSrc.left),
		(float)Renderer::EFBToScaledY(EFB_HEIGHT - correctSrc.top - actualHeight),
		Renderer::EFBToScaledXf(sampleStride),
		Renderer::EFBToScaledYf(sampleStride));

	u16 samples = TextureConversionShader::GetEncodedSampleCount(dstFormat);

	TargetRectangle scaledSource;
	scaledSource.top = 0;
	scaledSource.bottom = actualHeight;
	scaledSource.left = 0;
	scaledSource.right = actualWidth / samples;
	int cacheBytes = 32;
    if (dstFormat == EFB_COPY_RGBA8)
        cacheBytes = 64;
	
    int readStride = (actualWidth * cacheBytes) / blockW;
	EncodeToRamUsingShader(texconv_shader, source_texture, scaledSource, dst,
		actualWidth / samples, actualHeight, readStride, true, scaleByHalf);

	g_renderer->RestoreAPIState();

	FramebufferManager::SetFramebuffer(0);
    VertexShaderManager::SetViewportChanged();
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	glDisable(GL_TEXTURE_RECTANGLE_ARB);

    GL_REPORT_ERRORD();
}

void EncodeToRamYUYV(GLuint srcTexture, const TargetRectangle& sourceRc, u8* destAddr, int dstWidth, int dstHeight)
{
	g_renderer->ResetAPIState();
	EncodeToRamUsingShader(s_rgbToYuyvProgram, srcTexture, sourceRc, destAddr, dstWidth / 2, dstHeight, 0, false, false);
	FramebufferManager::SetFramebuffer(0);
    VertexShaderManager::SetViewportChanged();
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	glDisable(GL_TEXTURE_RECTANGLE_ARB);
	g_renderer->RestoreAPIState();
    GL_REPORT_ERRORD();
}


// Should be scale free.
void DecodeToTexture(u32 xfbAddr, int srcWidth, int srcHeight, GLuint destTexture)
{
	u8* srcAddr = Memory::GetPointer(xfbAddr);
	if (!srcAddr)
	{
		WARN_LOG(VIDEO, "Tried to decode from invalid memory address");
		return;
	}

	int srcFmtWidth = srcWidth / 2;

	g_renderer->ResetAPIState(); // reset any game specific settings

	// switch to texture converter frame buffer
	// attach destTexture as color destination
	FramebufferManager::SetFramebuffer(s_texConvFrameBuffer);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, destTexture);	
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, destTexture, 0);

	GL_REPORT_FBO_ERROR();

    for (int i = 0; i < 8; ++i)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_TEXTURE_RECTANGLE_ARB);
	}

	// activate source texture
	// set srcAddr as data for source texture
	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_RECTANGLE_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, s_srcTexture);

	// TODO: make this less slow.  (How?)
	if((GLsizei)s_srcTextureWidth == (GLsizei)srcFmtWidth && (GLsizei)s_srcTextureHeight == (GLsizei)srcHeight)
	{
		glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0,0,0,s_srcTextureWidth, s_srcTextureHeight, GL_BGRA, GL_UNSIGNED_BYTE, srcAddr);	
	}
	else
	{	
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8, (GLsizei)srcFmtWidth, (GLsizei)srcHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, srcAddr);	
		s_srcTextureWidth = (GLsizei)srcFmtWidth;
		s_srcTextureHeight = (GLsizei)srcHeight;
	}

	glViewport(0, 0, srcWidth, srcHeight);

	PixelShaderCache::SetCurrentShader(s_yuyvToRgbProgram.glprogid);
	
	GL_REPORT_ERRORD();

    glBegin(GL_QUADS);
	glTexCoord2f((float)srcFmtWidth, (float)srcHeight); glVertex2f(1,-1);
	glTexCoord2f((float)srcFmtWidth, 0); glVertex2f(1,1);
	glTexCoord2f(0, 0); glVertex2f(-1,1);
	glTexCoord2f(0, (float)srcHeight); glVertex2f(-1,-1);
    glEnd();	

	// reset state
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, 0, 0);
	glDisable(GL_TEXTURE_RECTANGLE_ARB);

	VertexShaderManager::SetViewportChanged();

	FramebufferManager::SetFramebuffer(0);

	g_renderer->RestoreAPIState();
    GL_REPORT_ERRORD();
}

}  // namespace

}  // namespace OGL
